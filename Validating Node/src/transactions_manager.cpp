/*
	Copyright (c) 2016-2017, Unified Digital Currency

	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice, this
	list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright notice,
	this list of conditions and the following disclaimer in the documentation
	and/or other materials provided with the distribution.

	3. Neither the name of the copyright holder nor the names of its contributors
	may be used to endorse or promote products derived from this software without
	specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**
 * @file transactions_manager.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include "balances.h"
#include "dao_manager.h"
#include "das_manager.h"
#include "codes.h"
#include "entity.h"
#include "globals.h"
#include "ledger.h"
#include "modules_interface.h"
#include "network.h"
#include "node.h"
#include "processing.h"
#include "publisher.h"
#include "transaction.h"
#include "transaction_dao.h"
#include "transaction_das.h"
#include "transaction_delayed.h"
#include "transaction_future.h"
#include "transactions_manager.h"
#include "util.h"


TransactionsManager::TransactionsManager(Publisher *publisher) : publisher(publisher) {}

void TransactionsManager::CleanUp() {
	std::lock_guard<std::mutex> lock(dataMutex);

	//empty rejection list
	rejectionList.clear();

	//find all the transactions in use
	std::unordered_set<std::string> inUse;

	//Add those awaiting processing
	std::queue<std::string> tempQueue(processingQueue);
	while(!tempQueue.empty()) {
		inUse.insert(tempQueue.front());
		tempQueue.pop();
	}

	//Add Delayed Requests
	inUse.insert(delayedTransactions.begin(), delayedTransactions.end());

	//Add Future Authorizes
	for(auto it = futureTransactions.begin(); it != futureTransactions.end(); ++it) inUse.insert(it->first);

	//Add DAO transactions in use
	std::unordered_set<std::string> daoTransactions = managerDAO->InUse();
	for(auto it = daoTransactions.begin(); it != daoTransactions.end(); ++it) inUse.insert(*it);

	//Add DAS transactions in use
	std::unordered_set<std::string> dasTransactions = managerDAS->InUse();
	for(auto it = dasTransactions.begin(); it != dasTransactions.end(); ++it) inUse.insert(*it);

	//clean up confirmation list
	auto currIt = currentTransactions.end();
	auto now = Util::current_timestamp_nanos();
	
	for(auto it = confirmationList.begin(); it != confirmationList.end(); ++it) {
		//remove registered
		if(it->second.first <= -1) confirmationList.erase(it->first);
		else {
			currIt = currentTransactions.find(it->first);
			if(currIt != currentTransactions.end() && (currIt->second->GetTimestamp() + LEDGER_CLOSING_INTERVAL) < now) {
				//Decrement reputation for the bad confirmations
				nodes->UpdateReputation(it->second.second, false);
				//remove expired unconfirmed
				confirmationList.erase(it->first);
			}
			//Add those awaiting confirmation
			else inUse.insert(it->first);
		}
	}

	//Add those pending registration
	for(auto it = registrationList.begin(); it != registrationList.end(); ++it) inUse.insert(it->second);

	//Finally, remove all that aren't needed
	for(auto it = currentTransactions.begin(); it != currentTransactions.end(); ++it) {
		auto it2 = inUse.find(it->first);
		if(it2 == inUse.end()) {
			//delete the actual transaction object
			delete it->second;
			currentTransactions.erase(it->first);
		}
	}
}

void TransactionsManager::ProcessTransactions(bool *IS_OPERATING, Balances *balancesDB, DAOManager *managerDAO, DASManager *managerDAS, Entities *entities, Ledger *ledger, ModulesInterface *interface, Nodes *nodes) {
	this->balancesDB = balancesDB;
	this->managerDAO = managerDAO;
	this->managerDAS = managerDAS;
	this->ledger = ledger;
	this->nodes = nodes;
	
	std::vector<std::pair<std::string, uint64_t>> senders, receivers;
	std::string hash, event;
	int errorCode;
	uint type;

	while(*IS_OPERATING) {
		if(!processingQueue.empty()) {
			hash = processingQueue.front();
			processingQueue.pop();

			errorCode = VALID;
			type = std::stoul(currentTransactions[hash]->GetType(),nullptr,16);

			//Process transaction accordingly 
			switch(type) {
				case TRANSACTION_BASIC:
					currentTransactions[hash]->Process(interface, errorCode);
					break;

				case TRANSACTION_DELAYED:
					event = dynamic_cast<DelayedTransaction*>(currentTransactions[hash])->GetEvent();
					if(event == TX_DELAYED_REQUEST) currentTransactions[hash]->Process(interface, errorCode);
					else if (event == TX_DELAYED_RELEASE) currentTransactions[hash]->Process(interface, errorCode);
					break;

				case TRANSACTION_FUTURE:
					event = dynamic_cast<FutureTransaction*>(currentTransactions[hash])->GetEvent();
					if(event == TX_FUTURE_AUTHORIZE) currentTransactions[hash]->Process(interface, errorCode);
					else if (event == TX_FUTURE_EXECUTE) currentTransactions[hash]->Process(interface, errorCode);
					break;

				case TRANSACTION_DAO:
					managerDAO->Process(dynamic_cast<DAOTransaction*>(currentTransactions[hash]), errorCode);
					break;

				case TRANSACTION_DAS:
					managerDAS->Process(dynamic_cast<DASTransaction*>(currentTransactions[hash]), errorCode);
					break;

				default:
					errorCode = ERROR_UNSUPPORTED_TYPE;
					break;
			}

			//Execute monetary transfers
			if(errorCode == VALID) {
				//retrieve monetary movements
				currentTransactions[hash]->Execute(senders, receivers);

				//Certify amounts of special transactions
				if(type == TRANSACTION_DAO) {
					if(!managerDAO->Allow(dynamic_cast<DAOTransaction*>(currentTransactions[hash])->GetDAO(), senders, receivers, errorCode)) errorCode = ERROR_INSUFICIENT_FUNDS;
				}
				else if(type == TRANSACTION_DAS) {
					if(!managerDAS->Allow(dynamic_cast<DASTransaction*>(currentTransactions[hash])->GetDAS(), senders, receivers, errorCode)) errorCode = ERROR_INSUFICIENT_FUNDS;
				}

				//update balances
				if(VALID && !balancesDB->UpdateBalances(senders, receivers)) errorCode = ERROR_INSUFICIENT_FUNDS;
				//register operations
				else ledger->RegisterMovements(currentTransactions[hash]->GetTimestamp(), senders, receivers);
			}

			dataMutex.lock();
			//Inform Managing Entity of the validation result
			auto it = submissionList.find(hash);
			if(it != submissionList.end()) {
				entities->TransactionReply(it->second, hash, errorCode);
				submissionList.erase(hash);
			}

			if(errorCode == VALID) {
				switch(type) {
					case TRANSACTION_DELAYED:
						if (dynamic_cast<DelayedTransaction*>(currentTransactions[hash])->GetEvent() == TX_DELAYED_RELEASE) {
							//Remove related Delayed Request
							delayedTransactions.erase(dynamic_cast<ReleaseDelayedTransaction*>(currentTransactions[hash])->GetRequest());
						}
						break;

					case TRANSACTION_FUTURE:
						if(dynamic_cast<FutureTransaction*>(currentTransactions[hash])->GetEvent() == TX_FUTURE_EXECUTE) {
							//Remove related Future Authorize
							futureTransactions.erase(dynamic_cast<ExecuteFutureTransaction*>(currentTransactions[hash])->GetFuture());
						}
						break;

					case TRANSACTION_DAO:
						//Let DAO manager register the validated transaction
						managerDAO->Register(dynamic_cast<DAOTransaction*>(currentTransactions[hash]));
						break;

					case TRANSACTION_DAS:
						//Let DAS manager register the validated transaction
						managerDAS->Register(dynamic_cast<DASTransaction*>(currentTransactions[hash]));
						break;

					default: break;
				}

				//broadcasts confirmation to all peer nodes
				nodes->BroadcastConfirmation(hash, true);
			}
			//Otherwise add the invalid transaction to the rejection list
			else rejectionList.insert(hash);
		}

		else std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void TransactionsManager::TransactionsRegistration(bool *IS_OPERATING) {
	std::string hash;

	while(*IS_OPERATING) {
		if(!registrationList.empty()) {
			auto now = Util::current_timestamp_nanos();

			if(registrationList.front().first + TRANSACTION_DELAY_REGISTRATION > now) {
				hash = registrationList.front().second;
				ledger->RegisterTransaction(currentTransactions[hash]);
				registrationList.pop_front();
			}
		}

		else std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

bool TransactionsManager::AddTransaction(Transaction *transaction, int &errorCode, bool process /*=false*/, uint dispatcher /*=DISPATCHER_NODE*/, std::string dispatcherId /*=""*/) {
	std::lock_guard<std::mutex> lock(dataMutex);
	std::string hash = transaction->GetHash();

	auto it1 = currentTransactions.find(hash);
	if(it1 != currentTransactions.end()) {
		errorCode = ERROR_TRANSACTION_DUPLICATE;
		delete transaction;
		return false;
	}

	auto it2 = rejectionList.find(hash);
	if(it2 != rejectionList.end()) {
		errorCode = ERROR_TRANSACTION_REJECTED;
		delete transaction;
		return false;
	}

	currentTransactions[hash] = transaction;

	if(process) {
		processingQueue.push(hash);
		
		switch(dispatcher) {
			case DISPATCHER_ENTITY:
				nodes->BroadcastNewTransaction(transaction);
				submissionList[hash] = dispatcherId;
				break;

			case DISPATCHER_DAO:
			case DISPATCHER_DAS:
				nodes->BroadcastNewTransaction(transaction);
				break;

			default: break;
		}
	}
	else {
		//Check if we requested it
		auto it3 = missingList.find(hash);
		if(it3 != missingList.end()) {
			missingList.erase(hash);

			//Verify if it already has enough confirmations to be registered
			IsConfirmed(hash);
		}
	}
	return true;
}

void TransactionsManager::FailedToFetch(std::string hash) {
	//Check if we requested it
	auto it = missingList.find(hash);
	if(it == missingList.end()) return;

	missingList[hash]++;

	if(missingList[hash] > MAX_DATA_REQUESTS) {
		std::string message = std::to_string(TRACKER_CODE_TRANSACTION) + std::to_string(TRACKER_GET_BY_HASH) + hash;

		//Request a Tracker for the transaction
		boost::asio::ip::tcp::socket socket(NETWORK_SOCKET_SERVICE);
		if(Network::ConnectToTracker(socket) && Network::WriteMessage(socket, NETWORK_TRACKER_GET, message, false)) {
			boost::system::error_code error;
			std::vector<char> length(NETWORK_FILE_LENGTH);

			//read response
			int n = socket.read_some(boost::asio::buffer(length), error);
			if(error || n != NETWORK_FILE_LENGTH) goto RequestNode;
			uint32_t total_length;
			Util::array_to_int(length, NETWORK_FILE_LENGTH, 0, total_length);

			//check if it is a positive response
			if(!total_length) goto RequestNode;

			//Read transaction
			std::vector<char> rawTx(total_length);
			n = socket.read_some(boost::asio::buffer(rawTx), error);
			if(error || n != total_length) goto RequestNode;

			//Extract transaction data
			std::string coreTx = Util::array_to_string(rawTx, rawTx.size()-TRANSACTION_HASH_LENGTH-5, TRANSACTION_HASH_LENGTH+4);

			//load transaction and verify it has the same hash
			int errorCode;
			Transaction *transaction = Processing::PrepareTransaction(managerDAO, managerDAS, coreTx, errorCode);
			if(errorCode || hash != transaction->GetHash()) goto RequestNode;

			//Add transaction
			if(!AddTransaction(transaction, errorCode)) goto RequestNode;
			return;
		}
	}

	//Ask a random Node
	RequestNode:
		bool sent = false;
		while(!sent) {
			boost::asio::ip::tcp::socket *socket = nodes->WriteToRandomNode();
			if(Network::WriteMessage(*socket, NETWORK_GET_TRANSACTION, hash, true)) sent = true;
		}
}

void TransactionsManager::AddConfirmation(std::string hash, std::string node) {
	std::lock_guard<std::mutex> lock(dataMutex);
	confirmationList[hash].first++;
	confirmationList[hash].second.push_back(node);

	//Request the transaction if we don't have it
	auto it = currentTransactions.find(hash);
	if(it == currentTransactions.end()) {
		missingList[hash] = 0;
		int errorCode;
		boost::asio::ip::tcp::socket *socket = nodes->WriteToNode(node, errorCode);
		if(!errorCode) Network::WriteMessage(*socket, NETWORK_GET_TRANSACTION, hash, true);
		return;
	}

	IsConfirmed(hash);
}

bool TransactionsManager::IsConfirmed(std::string hash) {
	int threshold, type = std::stoul(currentTransactions[hash]->GetType(), nullptr, 16);
	switch(type) {
		case TRANSACTION_DAO:
			threshold = managerDAO->ConfirmationThreshold(dynamic_cast<DAOTransaction*>(currentTransactions[hash])->GetDAO());
			break;

		case TRANSACTION_DAS:
			threshold = managerDAS->ConfirmationThreshold(dynamic_cast<DASTransaction*>(currentTransactions[hash])->GetDAS());
			break;

		default:
			threshold = nodes->ConfirmationThreshold();
			break;
	}

	std::lock_guard<std::mutex> lock(dataMutex);

	if(confirmationList[hash].first >= threshold) {
		confirmationList[hash].first = -32767;

		//insert the transaction ordered by its timestamp
		if(!registrationList.empty()) {
			for(auto regIt = registrationList.end(); regIt != registrationList.begin(); --regIt) {
				if(regIt->first <= currentTransactions[hash]->GetTimestamp()) {
					registrationList.insert(regIt, {currentTransactions[hash]->GetTimestamp(), hash});
					break;
				}
			}
		}
		else registrationList.push_front({currentTransactions[hash]->GetTimestamp(), hash});

		bool keep = false;
		switch(type) {
			case TRANSACTION_DELAYED:
				if(dynamic_cast<DelayedTransaction*>(currentTransactions[hash])->GetEvent() == TX_DELAYED_REQUEST) {
					delayedTransactions.insert(hash);
					keep = true;
				}
				break;

			case TRANSACTION_FUTURE:
				if(dynamic_cast<FutureTransaction*>(currentTransactions[hash])->GetEvent() == TX_FUTURE_AUTHORIZE) {
					futureTransactions[hash] = dynamic_cast<AuthorizeFutureTransaction*>(currentTransactions[hash])->GetValidity();
					keep = true;
				}
				break;

			default: break;
		}

		//Let DAOs and DASs lookup the transaction, delete if its unused
		if(!keep && !managerDAO->Enroll(currentTransactions[hash]) && !managerDAS->Enroll(currentTransactions[hash])) delete currentTransactions[hash];

		//Increment reputation of the nodes that correctly confirmed this transaction
		nodes->UpdateReputation(confirmationList[hash].second, true);
		confirmationList[hash].second.clear();

		//publish transaction
		int counter = 0;
		while(!publisher->PublishTransaction("{\"" + hash + "\":" + currentTransactions[hash]->GetTransaction()+"}") && counter < 3) counter++;

		return true;
	}
	return false;
}

void TransactionsManager::RollbackTransaction(Transaction *transaction) {
	auto it = currentTransactions.find(transaction->GetHash());
	if(it != currentTransactions.end()) {
		delete it->second;
		currentTransactions.erase(transaction->GetHash());
	}
	std::vector<std::pair<std::string, uint64_t>> from, to;
	std::string event;

	//Rollback operations
	//Step 1: retrieve transaction's monetary transfers
	//Step 2: register inverse transfer into the Ledger
	//Step 3: revert transfers within balances database
	//Step 4: undo transaction's actions

	switch(std::stoul((it->second)->GetType(), nullptr, 16)) {
		case TRANSACTION_BASIC:
			transaction->Execute(from, to); //Step 1
			ledger->RegisterMovements(transaction->GetTimestamp(), to, from); //Step 2
			balancesDB->RollbackBalances(from, to); //Step 3
			break;

		case TRANSACTION_DELAYED:
			event = dynamic_cast<DelayedTransaction*>(transaction)->GetEvent();
			if(event == TX_DELAYED_REQUEST) delayedTransactions.erase(transaction->GetHash()); //Step 4

			transaction->Execute(from, to); //Step 1
			ledger->RegisterMovements(transaction->GetTimestamp(), to, from); //Step 2
			balancesDB->RollbackBalances(from, to); //Step 3
			break;

		case TRANSACTION_FUTURE:
			event = dynamic_cast<FutureTransaction*>(transaction)->GetEvent();
			if(event == TX_FUTURE_AUTHORIZE) futureTransactions.erase(transaction->GetHash()); //Step 4

			else if(event == TX_FUTURE_EXECUTE) {
				//Step 4
				auto it2 = currentTransactions.find(dynamic_cast<ExecuteFutureTransaction*>(transaction)->GetFuture());
				if(it2 != currentTransactions.end()) {
					//Restore related Authorize if still valid
					auto now = Util::current_timestamp_nanos();
					uint64_t validity = dynamic_cast<AuthorizeFutureTransaction*>(it2->second)->GetValidity();
					if(validity > now) futureTransactions[transaction->GetHash()] = validity;
				}
			}
			transaction->Execute(from, to); //Step 1
			ledger->RegisterMovements(transaction->GetTimestamp(), to, from); //Step 2
			balancesDB->RollbackBalances(from, to); //Step 3
			break;

		case TRANSACTION_DAO:
			managerDAO->Execute(dynamic_cast<DAOTransaction*>(transaction), from, to); //Step 1
			ledger->RegisterMovements(transaction->GetTimestamp(), to, from); //Step 2
			managerDAO->Rollback(dynamic_cast<DAOTransaction*>(transaction)); //Steps 3 and 4
			break;

		case TRANSACTION_DAS:
			managerDAS->Execute(dynamic_cast<DASTransaction*>(transaction), from, to); //Step 1
			ledger->RegisterMovements(transaction->GetTimestamp(), to, from); //Step 2
			managerDAS->Rollback(dynamic_cast<DASTransaction*>(transaction)); //Steps 3 and 4
			break;

		default: break;
	}
}

bool TransactionsManager::RollbackLedger(std::string newLedger, std::string oldLedger, std::set<std::string> executedTransactions) {
	std::unordered_set<std::string> toRollback;
	std::vector<std::pair<std::string, uint64_t>> from, to;
	Transaction *transaction;
	std::string hash, content, event;
	char c;
	std::vector<char> buffer;
	buffer.resize(STANDARD_HASH_LENGTH);
	int count, errorCode;
	bool outside;

	auto it = executedTransactions.end();

	//open correct ledger
	std::fstream data(newLedger, std::fstream::in);
	if(!data.good()) return false;

	//forward to transactions
	data.seekg(LEDGER_SKIP_METADATA, std::ios_base::cur);
	while(c != ']') data.get(c);
	data.seekg(16, std::ios_base::cur);

	//iterate through its transactions
	data.get(c);
	while(c != ']') {
		//skip opening quotation marks
		data.seekg(1, std::ios_base::cur);
		//get hash
		data.read(&buffer.front(), TRANSACTION_HASH_LENGTH);
		hash = Util::array_to_string(buffer, TRANSACTION_HASH_LENGTH, 0);
		data.seekg(3, std::ios_base::cur);
		count = 1;
		outside = true;

		it = executedTransactions.find(hash);

		//transaction correctly registered
		if(it != executedTransactions.end()) {
			//remove from list and skip its content
			executedTransactions.erase(hash);
			while(c != '}' && count > 0) {
				data.get(c);
				if(c == '"') outside = !outside;
				if(outside) {
					if(c == '{') count++;
					else if(c == '}') count--;
				}
			}
		}
		//unregistered transaction
		else {
			//fetch its contents
			content = '{';
			while(c != '}' && count > 0) {
				data.get(c);
				if(c == '"') outside = !outside;
				if(outside) {
					if(c == '{') count++;
					else if(c == '}') count--;
				}
				content += c;
			}

			//load the transaction
			transaction = Processing::CreateTransaction(managerDAO, managerDAS, content, errorCode);
			if(errorCode == VALID) {
				//Publish it
				publisher->PublishTransaction("{\""+hash+"\",:"+transaction->GetTransaction()+"}");

				//Register it and compute the monetary transfers
				bool keep = false;
				switch(std::stoul(transaction->GetType(), nullptr, 16)) {
					case TRANSACTION_BASIC:
						transaction->Execute(from, to);
						break;

					case TRANSACTION_DELAYED:
						transaction->Execute(from, to);
						event = dynamic_cast<DelayedTransaction*>(transaction)->GetEvent();

						if(event == TX_DELAYED_REQUEST) {
							delayedTransactions.insert(hash);
							currentTransactions[hash] = transaction;
							keep = true;
						}
						else if(event == TX_DELAYED_RELEASE) {
							delayedTransactions.erase(hash);
							currentTransactions.erase(hash);
						}
						break;

					case TRANSACTION_FUTURE:
						transaction->Execute(from, to);
						event = dynamic_cast<FutureTransaction*>(transaction)->GetEvent();

						if(event == TX_FUTURE_AUTHORIZE) {
							futureTransactions[hash] = dynamic_cast<AuthorizeFutureTransaction*>(transaction)->GetValidity();
							currentTransactions[hash] = transaction;
							keep = true;
						}
						else if(event == TX_FUTURE_EXECUTE) {
							futureTransactions.erase(hash);
							currentTransactions.erase(hash);
						}
						break;

					case TRANSACTION_DAO:
						managerDAO->Execute(dynamic_cast<DAOTransaction*>(transaction), from, to);
						managerDAO->Register(dynamic_cast<DAOTransaction*>(transaction));
						break;

					case TRANSACTION_DAS:
						managerDAS->Execute(dynamic_cast<DASTransaction*>(transaction), from, to);
						managerDAS->Register(dynamic_cast<DASTransaction*>(transaction));
						break;

					default: break;
				}
				//If the transaction is not required, delete it
				if(!keep && !managerDAO->Enroll(transaction) && !managerDAS->Enroll(transaction)) delete transaction;

				//Register monetary movements
				ledger->RegisterMovements(transaction->GetTimestamp(), from, to);
				//Update balances
				balancesDB->RollbackBalances(from, to, true);

				from.clear();
				to.clear();
			}
		}
		//read separation comma or transactions array's end bracket
		data.get(c);
	}
	data.close();

	//open incorrect ledger
	data.open(oldLedger, std::fstream::in);
	if(!data.good()) return false;

	//forward to transactions
	data.seekg(LEDGER_SKIP_METADATA, std::ios_base::cur);
	while(c != ']') data.get(c);
	data.seekg(16, std::ios_base::cur);

	//iterate through its transactions
	data.get(c);
	while(c != ']') {
		//skip opening quotation marks
		data.seekg(1, std::ios_base::cur);
		//get hash
		data.read(&buffer.front(), TRANSACTION_HASH_LENGTH);
		hash = Util::array_to_string(buffer, TRANSACTION_HASH_LENGTH, 0);
		data.seekg(3, std::ios_base::cur);
		count = 1;

		it = executedTransactions.find(hash);

		//transaction incorrectly registered
		if(it != executedTransactions.end()) {
			//remove from list
			executedTransactions.erase(hash);

			//fetch its contents
			content = '{';
			while(c != '}' && count > 0) {
				data.get(c);
				if(c == '"') outside = !outside;
				if(outside) {
					if(c == '{') count++;
					else if(c == '}') count--;
				}
				content += c;
			}

			//load the transaction and rollback it
			transaction = Processing::CreateTransaction(managerDAO, managerDAS, content, errorCode);
			if(errorCode == VALID) RollbackTransaction(transaction);
			delete transaction;
		}
		//otherwise skip it
		else while(c != '}' && count > 0) {
			data.get(c);
			if(c == '"') outside = !outside;
			if(outside) {
				if(c == '{') count++;
				else if(c == '}') count--;
			}
		}

		//read separation comma or transactions array's end bracket
		data.get(c);
	}

	data.close();
	return true;
}

bool TransactionsManager::RegisterLedger(std::string ledgerFile) {
	Transaction *transaction;
	std::string hash, content, event;
	char c;
	std::vector<char> buffer;
	buffer.resize(STANDARD_HASH_LENGTH);
	int count, errorCode;
	bool outside;

	//open correct ledger
	std::fstream data(ledgerFile, std::fstream::in);
	if(!data.good()) return false;

	//forward to transactions
	data.seekg(LEDGER_SKIP_METADATA, std::ios_base::cur);
	while(c != ']') data.get(c);
	data.seekg(17, std::ios_base::cur);

	//iterate through its transactions
	while(c != ']') {
		//skip opening quotation marks
		data.seekg(1, std::ios_base::cur);
		//get hash
		data.read(&buffer.front(), TRANSACTION_HASH_LENGTH);
		hash = Util::array_to_string(buffer, TRANSACTION_HASH_LENGTH, 0);
		data.seekg(3, std::ios_base::cur);
		count = 1;
		outside = true;

		//fetch its contents
		content = '{';
		while(c != '}' && count > 0) {
			data.get(c);
			if(c == '"') outside = !outside;
			if(outside) {
				if(c == '{') count++;
				else if(c == '}') count--;
			}
			content += c;
		}

		//load the transaction
		transaction = Processing::CreateTransaction(managerDAO, managerDAS, content, errorCode);
		if(errorCode == VALID) {
			//Publish it
			publisher->PublishTransaction("{\""+hash+"\",:"+transaction->GetTransaction()+"}");

			//Register it
			bool keep = false;
			switch(std::stoul(transaction->GetType(), nullptr, 16)) {
				case TRANSACTION_BASIC:	break;

				case TRANSACTION_DELAYED:
					event = dynamic_cast<DelayedTransaction*>(transaction)->GetEvent();
					if(event == TX_DELAYED_REQUEST) {
						delayedTransactions.insert(hash);
						currentTransactions[hash] = transaction;
						keep = true;
					}
					else if(event == TX_DELAYED_RELEASE) {
						delayedTransactions.erase(hash);
						currentTransactions.erase(hash);
					}
					break;

				case TRANSACTION_FUTURE:
					event = dynamic_cast<FutureTransaction*>(transaction)->GetEvent();
					if(event == TX_FUTURE_AUTHORIZE) {
						futureTransactions[hash] = dynamic_cast<RequestDelayedTransaction*>(transaction)->GetExecution();
						currentTransactions[hash] = transaction;
						keep = true;
					}
					else if(event == TX_FUTURE_EXECUTE) {
						futureTransactions.erase(hash);
						currentTransactions.erase(hash);
					}
					break;

				case TRANSACTION_DAO:
					managerDAO->Register(dynamic_cast<DAOTransaction*>(transaction));
					break;

				case TRANSACTION_DAS:
					managerDAS->Register(dynamic_cast<DASTransaction*>(transaction));
					break;

				default:  break;
			}
			//If the transaction is not required, delete it
			if(!keep && !managerDAO->Enroll(transaction) && !managerDAS->Enroll(transaction)) delete transaction;
		}
		//read separation comma or transactions array's end bracket
		data.get(c);
	}
	data.close();

	return true;
}

bool TransactionsManager::GetTransaction(std::string hash, std::string &transactionOut) {
	std::lock_guard<std::mutex> lock(dataMutex);
	
	auto it = currentTransactions.find(hash);
	if(it != currentTransactions.end()) {
		transactionOut = it->second->GetTransaction();
		return true;
	}
	return false;
}

bool TransactionsManager::GetTransaction(std::string hash, Transaction *&transactionPtr) {
	std::lock_guard<std::mutex> lock(dataMutex);
	
	auto it = currentTransactions.find(hash);
	if(it != currentTransactions.end()) {
		transactionPtr = it->second;
		return true;
	}
	return false;
}

bool TransactionsManager::FindRequest(std::string hash, RequestDelayedTransaction *&transaction, int &errorCode) {	
	std::lock_guard<std::mutex> lock(dataMutex);
	
	auto it = delayedTransactions.find(hash);
	if(it != delayedTransactions.end()) {
		transaction = dynamic_cast<RequestDelayedTransaction*>(currentTransactions[*it]);
		return true;
	}
	errorCode = ERROR_HASH;
	return false;
}

bool TransactionsManager::FindAuthorize(std::string hash, AuthorizeFutureTransaction *&transaction, int &errorCode) {
	std::lock_guard<std::mutex> lock(dataMutex);
	
	auto it = futureTransactions.find(hash);
	if(it != futureTransactions.end()) {
		auto now = Util::current_timestamp_nanos();

		//Verify its validity
		if((it->second + TRANSACTION_DELAY_NEW) < now) {
			transaction = dynamic_cast<AuthorizeFutureTransaction*>(currentTransactions[hash]);
			return true;
		}
		else {
			//delete if it expired
			futureTransactions.erase(hash);
			delete currentTransactions[hash];
			currentTransactions.erase(hash);

			errorCode = ERROR_TIMESTAMP;
		}
	}
	else errorCode = ERROR_HASH;

	return false;
}