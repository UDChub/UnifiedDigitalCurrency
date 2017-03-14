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
 * @file ledger.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <mutex>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <iomanip>
#include <utility>

#include "includes/boost/filesystem.hpp"
#include "includes/cryptopp/filters.h"
#include "includes/cryptopp/files.h"
#include "includes/cryptopp/ripemd.h"
#include "includes/cryptopp/hex.h"

#include "balances.h"
#include "codes.h"
#include "configurations.h"
#include "globals.h"
#include "transaction.h"
#include "ledger.h"
#include "merkle.h"
#include "network.h"
#include "network_manager.h"
#include "node.h"
#include "publisher.h"
#include "transactions_manager.h"
#include "util.h"


LedgerStruct& LedgerStruct::operator=(const LedgerStruct &ledger) {
	ledgerId = ledger.ledgerId;
	ledgerHash = ledger.ledgerHash;
	previousLedgerId = ledger.previousLedgerId;
	previousLedgerHash = ledger.previousLedgerHash;
	begin = ledger.begin;
	end = ledger.end;
	nAccounts = ledger.nAccounts;
	transactionsList = ledger.transactionsList;
	amountInCirculation = ledger.amountInCirculation; 
	amountTraded = ledger.amountTraded;
	feesCollected = ledger.feesCollected;
	ledgerFile = ledger.ledgerFile;
	accountsFile = ledger.accountsFile;
	transactionsFile = ledger.transactionsFile;
	operationsFile = ledger.operationsFile;
	return *this;
}

Ledger::Ledger(Balances *balancesDB, NetworkManager *networkManager, TransactionsManager *txManager, Nodes *nodes, Publisher *publisher)
 : balancesDB(balancesDB), networkManager(networkManager), txManager(txManager), nodes(nodes), publisher(publisher) {
}

 void Ledger::InitializeLedgers(std::string latestLedger, uint latestId) {
	uint64_t now = Util::current_timestamp_nanos();

	//set current Ledger's info
	if(GENESIS_LEDGER_END > now) {
		currentLedger.ledgerId = GENESIS_LEDGER_ID+1;
		currentLedger.previousLedgerId = GENESIS_LEDGER_ID;
		currentLedger.previousLedgerHash = GENESIS_LEDGER_HASH;
		currentLedger.begin = GENESIS_LEDGER_END+1;
		currentLedger.end = GENESIS_LEDGER_END+LEDGER_DURATION;

		IS_SYNCHRONIZED = true;
		//set filenames and open data output streams
		currentLedger.ledgerFile = LOCAL_DATA_LEDGERS + std::to_string(currentLedger.ledgerId) + LEDGER_EXTENSION;
		currentLedger.accountsFile = LOCAL_DATA_LEDGERS + std::to_string(currentLedger.ledgerId) + LEDGER_ACCOUNTS_EXTENSION;
		currentLedger.transactionsFile = LOCAL_DATA_LEDGERS + std::to_string(currentLedger.ledgerId) + LEDGER_TRANSACTIONS_EXTENSION;
		currentLedger.operationsFile = LOCAL_DATA_LEDGERS + std::to_string(currentLedger.ledgerId) + LEDGER_OPERATIONS_EXTENSION;
		currentLedger.transactionsData.open(currentLedger.transactionsFile, std::fstream::out | std::fstream::trunc);
		currentLedger.operationsData.open(currentLedger.operationsFile, std::fstream::out | std::fstream::trunc);
	}
	else {
		currentLedger.ledgerId = latestId+1;
		currentLedger.previousLedgerId = latestId;
		currentLedger.previousLedgerHash = latestLedger;
		currentLedger.end = GENESIS_LEDGER_END + (currentLedger.ledgerId * LEDGER_DURATION);
		currentLedger.begin = currentLedger.end - LEDGER_DURATION + 1;
		currentLedger.ledgerFile = LOCAL_DATA_LEDGERS + std::to_string(currentLedger.ledgerId) + LEDGER_EXTENSION;
		currentLedger.amountInCirculation = amountInCirculation;
	}

	//Set next Ledger's info
	nextLedger.previousLedgerId = currentLedger.ledgerId;
	nextLedger.ledgerId = nextLedger.previousLedgerId+1;
	nextLedger.begin = currentLedger.begin + LEDGER_DURATION;
	nextLedger.end = currentLedger.end + LEDGER_DURATION;
	
	//set filenames and open data output streams
	nextLedger.ledgerFile = LOCAL_DATA_LEDGERS + std::to_string(nextLedger.ledgerId) + LEDGER_EXTENSION;
	nextLedger.accountsFile = LOCAL_DATA_LEDGERS + std::to_string(nextLedger.ledgerId) + LEDGER_ACCOUNTS_EXTENSION;
	nextLedger.transactionsFile = LOCAL_DATA_LEDGERS + std::to_string(nextLedger.ledgerId) + LEDGER_TRANSACTIONS_EXTENSION;
	nextLedger.operationsFile = LOCAL_DATA_LEDGERS + std::to_string(nextLedger.ledgerId) + LEDGER_OPERATIONS_EXTENSION;
	nextLedger.transactionsData.open(nextLedger.transactionsFile, std::fstream::out | std::fstream::trunc);
	nextLedger.operationsData.open(nextLedger.operationsFile, std::fstream::out | std::fstream::trunc);

	if(!IS_SYNCHRONIZED) {
		//Ensure there's enough time until the current Ledger closes
		if(now < currentLedger.end - SYNCHRONIZATION_MARGIN) WAIT_FOR_NEXT = false;
		else WAIT_FOR_NEXT = true;
	}
}

void Ledger::RegisterMovements(uint64_t timestamp, std::vector<std::pair<std::string, uint64_t>> &from, std::vector<std::pair<std::string, uint64_t>> &to) {
	//selects the appropriate ledger
	LedgerStruct *ledger;
	if(IS_SYNCHRONIZED) {
		if(timestamp >= currentLedger.begin && timestamp <= currentLedger.end) {
		ledger = &currentLedger;
		}
		else if(timestamp <= nextLedger.end) {
			ledger = &nextLedger;
		}
		else return;
	}
	else if(!WAIT_FOR_NEXT && timestamp >= nextLedger.begin && timestamp <= nextLedger.end) {
		ledger = &nextLedger;
	}
	else return;

	dataMutex.lock();

	//updates ledger's operations sheet and currency's metadata
	for(auto movement : from) {
		ledger->operationsData << movement.first << " - " << movement.second << std::endl;
		ledger->amountTraded += movement.second;
		if(movement.first == WORLD_BANK_ACCOUNT) ledger->amountInCirculation += movement.second;
	}
	for(auto movement : to) {
		ledger->operationsData << movement.first << " + " << movement.second << std::endl;
		if(movement.first == WORLD_BANK_ACCOUNT) ledger->amountInCirculation -= movement.second;
	}
	ledger->operationsData.flush();

	dataMutex.unlock();
}

void Ledger::RegisterTransaction(Transaction *Tx) {
	LedgerStruct *ledgerPtr;
	std::lock_guard<std::mutex> lock(dataMutex);

	//selects the appropriate ledger
	if(Tx->GetTimestamp() >= currentLedger.begin && Tx->GetTimestamp() <= currentLedger.end) {
		ledgerPtr = &currentLedger;
		if(!IS_SYNCHRONIZED) return;
	}
	else if(Tx->GetTimestamp() <= currentLedger.end) {
		ledgerPtr = &nextLedger;
	}
	else return;

	//updates ledger's meta data
	ledgerPtr->feesCollected += Tx->GetFees();

	//inserts the transaction
	ledgerPtr->transactionsList.insert(Tx->GetHash());
	std::string transaction = "\"" + Tx->GetHash() + "\":" + Tx->GetTransaction();
	ledgerPtr->transactionsData << "," << transaction;
	ledgerPtr->transactionsData.flush();

	// //publish transaction
	// int counter = 0;
	// while(!publisher->PublishTransaction("{"+transaction+"}") && counter < 3) counter++;
}

void Ledger::CloseLedger() {

	if(IS_SYNCHRONIZED) {
		dataMutex.lock();
		//close data streams of the current ledger
		currentLedger.transactionsData.close();
		currentLedger.operationsData.close();
		dataMutex.unlock();

		CalculateBalances();
		BuildLedger();
		//latest ledger is built
	}

	std::lock_guard<std::mutex> lock(dataMutex);

	//updates the remaining data of the next ledger
	nextLedger.previousLedgerHash = currentLedger.ledgerHash;

	//keep useful data in case of having the wrong Ledger
	amountInCirculation = currentLedger.amountInCirculation;
	oldTransactionsList = currentLedger.transactionsList;

	//initialize the new ledger's data
	LedgerStruct newLedger;
	newLedger.ledgerId = nextLedger.ledgerId+1;
	newLedger.previousLedgerId = nextLedger.ledgerId;
	newLedger.begin = nextLedger.begin + LEDGER_DURATION;
	newLedger.end = nextLedger.end + LEDGER_DURATION;
	newLedger.ledgerFile = LOCAL_DATA_LEDGERS + std::to_string(newLedger.ledgerId) + LEDGER_EXTENSION;
	newLedger.accountsFile = LOCAL_DATA_LEDGERS + std::to_string(newLedger.ledgerId) + LEDGER_ACCOUNTS_EXTENSION;
	newLedger.transactionsFile = LOCAL_DATA_LEDGERS + std::to_string(newLedger.ledgerId) + LEDGER_TRANSACTIONS_EXTENSION;
	newLedger.operationsFile = LOCAL_DATA_LEDGERS + std::to_string(newLedger.ledgerId) + LEDGER_OPERATIONS_EXTENSION;

	//moves ledgers and data streams 
	nextLedger.transactionsData.close();
	nextLedger.operationsData.close();

	currentLedger = nextLedger;
	nextLedger = newLedger;

	currentLedger.transactionsData.open(currentLedger.transactionsFile, std::fstream::out | std::fstream::app);
	currentLedger.operationsData.open(currentLedger.operationsFile, std::fstream::out | std::fstream::app);
	nextLedger.transactionsData.open(nextLedger.transactionsFile, std::fstream::out | std::fstream::trunc);
	nextLedger.operationsData.open(nextLedger.operationsFile, std::fstream::out | std::fstream::trunc);

	IS_SYNCHRONIZED = true;
}

void Ledger::BuildLedger() {
	std::lock_guard<std::mutex> lock(dataMutex);

	std::string accountsHash, transactionsRoot;

	//calculate acccounts state hash
	CryptoPP::RIPEMD160 ripemd;
	CryptoPP::FileSource fs(currentLedger.accountsFile.c_str(), true, new CryptoPP::HashFilter(ripemd, new CryptoPP::HexEncoder(new CryptoPP::StringSink(accountsHash))));

	//calculate the root of the transactions' Merkle tree
	transactionsRoot = Crypto::MerkleRoot(currentLedger.transactionsList);

	//Compute Ledger's hash
	CryptoPP::StringSource ss(std::to_string(currentLedger.ledgerId)+currentLedger.previousLedgerHash+accountsHash+transactionsRoot, true, new CryptoPP::HashFilter(ripemd, new CryptoPP::HexEncoder(new CryptoPP::StringSink(currentLedger.ledgerHash))));

	//create Ledger file
	std::fstream ledgerFile(currentLedger.ledgerFile, std::fstream::out | std::fstream::trunc);
	ledgerFile << "{\"ledgerId\":" << currentLedger.ledgerId;
	ledgerFile << ",\"ledgerHash\":\"" << currentLedger.ledgerHash;
	ledgerFile << "\",\"previousLedgerId\":" << currentLedger.previousLedgerId;
	ledgerFile << ",\"previousLedgerHash\":\"" << currentLedger.previousLedgerHash;
	ledgerFile << "\",\"accountsHash\":\"" << accountsHash;
	ledgerFile << "\",\"transactionsRoot\":\"" << transactionsRoot;
	ledgerFile << "\",\"opening\":" << currentLedger.begin;
	ledgerFile << ",\"closing\":" << currentLedger.end;
	ledgerFile << ",\"numberOfAccounts\":" << currentLedger.nAccounts;
	ledgerFile << ",\"numberOfTransactions\":" << currentLedger.transactionsList.size();
	ledgerFile << ",\"amountInCirculation\":" << currentLedger.amountInCirculation;
	ledgerFile << ",\"amountTraded\":" << currentLedger.amountTraded;
	ledgerFile << ",\"feesCollected\":" << currentLedger.feesCollected;

	//insert the accounts with their final balance
	currentLedger.accountsData.open(currentLedger.accountsFile, std::fstream::in);
	ledgerFile << ",\"accounts\":" << currentLedger.accountsData.rdbuf();
	currentLedger.accountsData.close();

	//insert all registered transactions
	currentLedger.transactionsData.open(currentLedger.transactionsFile, std::fstream::in);
	//ignore the initial extra comma
	currentLedger.transactionsData.seekg(1, std::ios_base::cur);
	ledgerFile << ",\"transactions\":[" << currentLedger.transactionsData.rdbuf() << "]";
	currentLedger.transactionsData.close();

	//finally, insert the closing bracket and close the file
	ledgerFile << "}";
	ledgerFile.close();
}

void Ledger::CalculateBalances() {
	//Use an unordered map for the insertion and update speed performance relative to an ordered map
	std::unordered_map<std::string,uint64_t> accountsList;
	bool start = false, end = false, str = false;
	std::string account, value, operation;
	uint64_t amount;
	char c;

	//retrieve the state of the accounts from the previous ledger
	std::fstream previousLedger(LOCAL_DATA_LEDGERS + std::to_string(currentLedger.previousLedgerId) + LEDGER_EXTENSION, std::fstream::in);

	//skip until we reach the accounts array
	while(!start) {
		previousLedger.get(c);
		if(c == '[') start = true;
	}

	while(!end) {
		previousLedger.get(c);

		switch(c) {
			//reached last account, adds it and stops loop
			case ']':
				accountsList[account] = stoull(value);
				end = true;
				break;

			//reached beginning or ending of the key
			case '\"':
				str = !str;
				break;

			case ':':
				break;

			case ',':
				accountsList[account] = stoull(value);
				account = value = "";
				break;

			default:
				if(str) account += c;
				else value += c;
		}
	}
	previousLedger.close();

	//update balances by computing the movements occurred during the new ledger
	currentLedger.operationsData.open(currentLedger.operationsFile, std::fstream::in);

	while(currentLedger.operationsData >> account >> operation >> amount) {
		if(operation == "+") accountsList[account] += amount;
		else accountsList[account] -= amount;
	}
	currentLedger.operationsData.close();

	//ignores worldbank account
	accountsList.erase(WORLD_BANK_ACCOUNT);

	//order the list while excluding null balances
	std::map<std::string,uint64_t> orderedList;
	for(auto it = accountsList.begin(); it != accountsList.end(); ++it) {
		if(it->second > 0) orderedList[it->first] = it->second;
	}

	//writes accounts with balances to file
	currentLedger.accountsData.open(currentLedger.accountsFile, std::fstream::out | std::fstream::trunc);
	auto it = orderedList.begin();

	//processes first entry individually so there's no extra initial comma
	if(it->second != 0) {
		currentLedger.accountsData << "[\"" << it->first << "\":" << it->second;
		currentLedger.nAccounts++;
		++it;
	}

	for(it; it != orderedList.end(); ++it) {
		if(it->second != 0) {
			currentLedger.accountsData << ",\"" << it->first << "\":" << it->second;
			currentLedger.nAccounts++;
		}
	}

	//insert closing bracket to complete the array and close the file
	currentLedger.accountsData << "]";
	currentLedger.accountsData.close();
}

bool Ledger::GetLedger(std::string hash, std::string &ledgerFile) {
	uint id;
	if(networkManager->GetLedgerId(hash, id)) {
		ledgerFile = LOCAL_DATA_LEDGERS + std::to_string(id) + LEDGER_EXTENSION;
		return true;
	}
	return false;
}

bool Ledger::SetLedger(std::string tempLedger) {
	std::string hash, accountsHash, transactionsRoot, value;
	uint id;
	char c;
	std::vector<char> buffer;
	buffer.resize(STANDARD_HASH_LENGTH);

	//read id, hash, accountshash, root, accounts, txhashes
	std::fstream in(tempLedger, std::fstream::in);

	//Get Ledger ID
	in.seekg(12, std::ios_base::cur);
	while(c != ',') {
		in.get(c);
		value += c;
	}
	id = std::stoul(value);

	//Get Ledger Hash
	in.seekg(14, std::ios_base::cur);
	in.read(&buffer.front(), LEDGER_HASH_LENGTH);
	hash = Util::array_to_string(buffer, LEDGER_HASH_LENGTH, 0);

	//Verify that we requested this Ledger
	auto it = missingLedgers.find(hash);
	if(IS_SYNCHRONIZED && it == missingLedgers.end()) return false;

	//Get accounts state hash
	in.seekg(100, std::ios_base::cur);
	while(c != ':') in.get(c);
	in.get(c);
	in.read(&buffer.front(), ACCOUNTS_HASH_LENGTH);
	accountsHash = Util::array_to_string(buffer, ACCOUNTS_HASH_LENGTH, 0);

	//Get transactions' Merkle tree root
	in.seekg(22, std::ios_base::cur);
	in.read(&buffer.front(), MERKLE_HASH_LENGTH);
	transactionsRoot = Util::array_to_string(buffer, MERKLE_HASH_LENGTH, 0);

	//Retrieve the monetary amount in circulation
	in.seekg(100, std::ios_base::cur);
	while(c != 'C') in.get(c);
	in.seekg(12, std::ios_base::cur);
	value = "";
	while(c != ',') {
		in.get(c);
		value += c;
	}
	amountInCirculation = std::stoull(value);

	//forward to accounts data
	in.seekg(40, std::ios_base::cur);
	while(c != '[') in.get(c);

	//save accounts state into a file for verification
	std::string stateData = LOCAL_DATA_LEDGERS + std::to_string(id) + LEDGER_EXTENSION + ".state";
	std::fstream out(stateData, std::fstream::out | std::fstream::trunc);
	out << '[';
	while(c != ']') {
		in.get(c);
		out << c;
	}
	out.close();

	//ensure it matches the accounts state hash retrieved
	CryptoPP::RIPEMD160 ripemd;
	CryptoPP::FileSource fs(stateData.c_str(), true, new CryptoPP::HashFilter(ripemd, new CryptoPP::HexEncoder(new CryptoPP::StringSink(value))));
	if(accountsHash != value) {
		FailedToFetch(hash);
		return false;
	}

	//forward to the transactions and retrieve all hashes
	in.seekg(16, std::ios_base::cur);
	std::set<std::string> transactionsList;

	int count;
	bool outside;
	while(c != ']') {
		//skip opening quotation marks
		in.seekg(1, std::ios_base::cur);
		//get hash
		in.read(&buffer.front(), TRANSACTION_HASH_LENGTH);
		value = Util::array_to_string(buffer, TRANSACTION_HASH_LENGTH, 0);
		transactionsList.insert(value);
		in.seekg(3, std::ios_base::cur);

		//skip its contents
		count = 1;
		outside = true;
		while(count > 0) {
			in.get(c);
			if(c == '"') outside = !outside;
			if(outside) {
				if(c == '{') count++;
				else if(c == '}') count--;
			}
		}

		//read separation comma or transactions array's end bracket
		in.get(c);
	}
	in.close();

	//verify the Merkle tree root is correct
	if(transactionsRoot != Crypto::MerkleRoot(transactionsList)) {
		FailedToFetch(hash);
		return false;
	}

	//compute hash to check if it matches the one provided
	CryptoPP::StringSource ss(std::to_string(id)+hash+accountsHash+transactionsRoot, true, new CryptoPP::HashFilter(ripemd, new CryptoPP::HexEncoder(new CryptoPP::StringSink(value))));
	if(hash != value) {
		FailedToFetch(hash);
		return false;
	}

	//Ledger validated, register it
	std::string ledgerFile = LOCAL_DATA_LEDGERS + std::to_string(id) + LEDGER_EXTENSION;

	//Update the amount of currency in circulation
	currentLedger.amountInCirculation += amountInCirculation;

	//Check if its the latest closed Ledger
	if(hash == currentLedger.previousLedgerHash && IS_SYNCHRONIZED) {
		///read new ledger and rollback previous operations
		txManager->RollbackLedger(tempLedger, ledgerFile, oldTransactionsList);

		//revove unnecessary files
		std::string previousFiles = LOCAL_DATA_LEDGERS + std::to_string(id);
		std::string file = previousFiles + LEDGER_OPERATIONS_EXTENSION;
		boost::filesystem::remove(boost::filesystem::path(file));
		file = previousFiles + LEDGER_ACCOUNTS_EXTENSION;
		boost::filesystem::remove(boost::filesystem::path(file));
		file = previousFiles + LEDGER_TRANSACTIONS_EXTENSION;
		boost::filesystem::remove(boost::filesystem::path(file));

		//replace old Ledger
		boost::filesystem::rename(boost::filesystem::path(tempLedger), boost::filesystem::path(ledgerFile));
	}
	//still synchronizing
	else if(!IS_SYNCHRONIZED) {
		//Set correct balances
		std::unordered_map<std::string,uint64_t> balances;
		LoadBalances(stateData, balances);
		balancesDB->UpdateFromLedger(balances);

		//save Ledger
		boost::filesystem::rename(boost::filesystem::path(tempLedger), boost::filesystem::path(ledgerFile));
		//Register its transactions
		txManager->RegisterLedger(ledgerFile);

		if(WAIT_FOR_NEXT) WAIT_FOR_NEXT = false;
	}
	else {
		//save Ledger
		boost::filesystem::rename(boost::filesystem::path(tempLedger), boost::filesystem::path(ledgerFile));
		//register the transactions it contains
		txManager->RegisterLedger(ledgerFile);
	}

	//finally, publish it
	publisher->PublishLedger(ledgerFile);

	//delete file with the accounts state
	boost::filesystem::remove(boost::filesystem::path(stateData));

	//Update Ledgers index
	networkManager->NewLedger(id, hash);

	missingLedgers.erase(hash);
	return true;
}

void Ledger::LoadBalances(std::string stateData, std::unordered_map<std::string,uint64_t> &balances) {
	bool end = false, str = false;
	std::string account, value;
	char c;

	dataMutex.lock();
	std::fstream data(stateData, std::fstream::in);

	//skip opening bracket
	data.get(c);

	while(end) {
		data.get(c);

		switch(c) {
			case ']':
				balances[account] = stoull(value);
				end = true;
				break;

			case '\"':
				str = !str;
				break;

			case ':':
				break;

			case ',':
				balances[account] = stoull(value);
				account = value = "";
				break;

			default:
				if(str) account += c;
				else value += c;
		}
	}
	data.close();
}

void Ledger::FailedToFetch(std::string hash /*=""*/) {
	if(hash.length() == 0) hash = currentLedger.previousLedgerHash;
	
	missingLedgers[hash]++;

	if(!IS_SYNCHRONIZED || missingLedgers[hash] > MAX_DATA_REQUESTS) {
		std::string message = std::to_string(TRACKER_CODE_LEDGER) + std::to_string(TRACKER_GET_BY_HASH) + hash;

		//Request a Tracker for the Ledger
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

			//Create a temporary file to receive the Ledger
			std::string tempFile = LOCAL_TEMP_DATA + hash + "tracker" + LEDGER_EXTENSION;
			std::fstream data(tempFile, std::fstream::out | std::fstream::trunc);

			uint32_t received = 0;
			char buffer[4096];
			//Read data
			while(!error && received != total_length) {
				if (total_length - received > 4096) n = socket.read_some(boost::asio::buffer(buffer, 4096), error);
				else n = socket.read_some(boost::asio::buffer(buffer, total_length - received), error);

				if(!error && n > 0) {
					data << buffer;
					received += n;
				}
				else break;
			}
			data.close();

			//Register Ledger
			SetLedger(tempFile);
			return;
		}
	}

	RequestNode:
		bool sent = false;
		int errorCode;
		//If it's the latest closed Ledger, ask one of the Nodes that confirmed it
		auto it = ledgerConsensus.find(hash);
		if(it != ledgerConsensus.end()) {
			std::vector<std::string> goodNodes = ledgerConsensus[hash].second;
			while(!sent && goodNodes.size()) {
				boost::asio::ip::tcp::socket *socket = nodes->WriteToNode(goodNodes.back(), errorCode);
				if(!errorCode && Network::WriteMessage(*socket, NETWORK_GET_LEDGER, hash, true)) sent = true;
				goodNodes.pop_back();
				errorCode = 0;
			}
		}
		//Otherwise ask a random Node
		while(IS_SYNCHRONIZED && !sent) {
			boost::asio::ip::tcp::socket *socket = nodes->WriteToRandomNode();
			if(Network::WriteMessage(*socket, NETWORK_GET_LEDGER, hash, true)) sent = true;
		}
}

void Ledger::StartConsensus() {
	nodes->BroadcastConfirmation(currentLedger.previousLedgerHash, false);

	std::lock_guard<std::mutex> lock(dataMutex);
	ledgerConsensus[currentLedger.previousLedgerHash].first++;

	if(ledgerConsensus[currentLedger.previousLedgerHash].first >= nodes->ConfirmationThreshold(false)) {
		EndConsensus(currentLedger.previousLedgerHash, _SELF);
	}
}

void Ledger::EndConsensus(std::string hash, std::string node) {
	std::vector<std::string> goodNodes = ledgerConsensus[hash].second;

	if(hash != currentLedger.previousLedgerHash) {
		//we built an incorrect Ledger
		currentLedger.previousLedgerHash = hash;

		int errorCode = 0;
		bool sent = false;

		//request correct Ledger from any of the nodes that voted for it
		while(!sent && goodNodes.size()) {
			boost::asio::ip::tcp::socket *socket = nodes->WriteToNode(goodNodes.back(), errorCode);
			if(!errorCode && Network::WriteMessage(*socket, NETWORK_GET_LEDGER, hash, true)) sent = true;
			goodNodes.pop_back();
			errorCode = 0;
		}
		missingLedgers[hash] = 0;
	}
	else {
		//update the amount of currency in circulation
		currentLedger.amountInCirculation += amountInCirculation;

		std::string previousFiles = LOCAL_DATA_LEDGERS + std::to_string(currentLedger.previousLedgerId);

		//publish ledger
		std::string ledgerFile = previousFiles + LEDGER_EXTENSION;
		publisher->PublishLedger(ledgerFile);

		//revove unnecessary files
		std::string file = previousFiles + LEDGER_ACCOUNTS_EXTENSION;
		boost::filesystem::remove(boost::filesystem::path(file));
		file = previousFiles + LEDGER_TRANSACTIONS_EXTENSION;
		boost::filesystem::remove(boost::filesystem::path(file));
		file = previousFiles + LEDGER_OPERATIONS_EXTENSION;
		boost::filesystem::remove(boost::filesystem::path(file));
	}

	//update the ledgers index
	networkManager->NewLedger(currentLedger.previousLedgerId, hash);
}

void Ledger::CleanConsensus() {	
	//update reputation of the other nodes according to their votes for the latest Ledger
	nodes->UpdateReputation(ledgerConsensus[currentLedger.previousLedgerHash].second, true, false);
	ledgerConsensus.erase(currentLedger.previousLedgerHash);

	for(auto it = ledgerConsensus.begin(); it != ledgerConsensus.end(); ++it) {
		nodes->UpdateReputation(it->second.second, false, false);
	}

	ledgerConsensus.clear();
}

void Ledger::AddConfirmation(std::string hash, std::string node) {
	std::lock_guard<std::mutex> lock(dataMutex);
	
	ledgerConsensus[hash].first++;
	ledgerConsensus[hash].second.push_back(node);

	if(ledgerConsensus[hash].first >= nodes->ConfirmationThreshold(false)) {
		EndConsensus(hash, node);
	} 
}

uint64_t Ledger::GetEstimatedAmountInCirculation() {
	return currentLedger.amountInCirculation + nextLedger.amountInCirculation;
}

uint64_t Ledger::GetEstimatedVolume(bool current /*=true*/) {
	if(current) return currentLedger.feesCollected;
	return nextLedger.feesCollected;
}

uint64_t Ledger::GetEstimatedFeesCollected(bool current /*=true*/) {
	if(current) return currentLedger.feesCollected;
	return nextLedger.feesCollected;
}

uint64_t Ledger::GetClosingTime(bool current /*=true*/) {
	if(current) return currentLedger.end;
	return nextLedger.end;
}

std::string Ledger::LastClosedLedgerHash() {
	return currentLedger.previousLedgerHash;
}

uint64_t Ledger::LastClosedLedgerId() {
	return currentLedger.previousLedgerId;
}