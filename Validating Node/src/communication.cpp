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
 * @file communication.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <mutex>
#include <string>
#include <sstream>

#include "includes/rapidjson/document.h"
#include "includes/boost/asio.hpp"
#include "includes/boost/filesystem.hpp"
#include "includes/boost/regex.hpp"

#include "codes.h"
#include "communication.h"
#include "configurations.h"
#include "dao_manager.h"
#include "das_manager.h"
#include "ecdsa.h"
#include "entity.h"
#include "globals.h"
#include "keys.h"
#include "ledger.h"
#include "network.h"
#include "network_manager.h"
#include "node.h"
#include "processing.h"
#include "publisher.h"
#include "transaction.h"
#include "transactions_manager.h"
#include "util.h"


void Communication::ListenToNode(bool *IS_OPERATING, bool *IS_SYNCHRONIZED, DAOManager *managerDAO, DASManager *managerDAS, Ledger *ledger, NetworkManager *networkManager, Nodes *nodes, TransactionsManager *txManager, NodeStruct *node, boost::asio::ip::tcp::socket *socket) {
	bool connected = true;
	int n, errorCode;
	std::mutex dataMutex;

	while(connected && *IS_OPERATING) {
		boost::system::error_code error;
		n = errorCode = 0;

		//retrieve message's protocol code
		std::vector<char> code(NETWORK_CODE_LENGTH);
		dataMutex.lock();
		n = socket->read_some(boost::asio::buffer(code), error);
		dataMutex.unlock();

		if(error || n != NETWORK_CODE_LENGTH) continue;
		uint16_t protocolCode;
		Util::array_to_int(code, NETWORK_CODE_LENGTH, 0, protocolCode);

		//ignore KEEP_ALIVE messages
		if(protocolCode == NETWORK_KEEP_ALIVE) continue;

		//process NETWORK_EXIT messages
		if (protocolCode == NETWORK_EXIT) {
			nodes->DropNode(node->nodeId);
			connected = false;
			continue;
		}

		//process separately incomming Ledgers or Blocks
		if (protocolCode == NETWORK_LEDGER || protocolCode == NETWORK_BLOCK) {
			//retrieve total length of message's data
			std::vector<char> total_length(NETWORK_DATA_LENGTH_BIG);
			dataMutex.lock();
			n = socket->read_some(boost::asio::buffer(total_length), error);
			if(error || n != NETWORK_DATA_LENGTH_BIG) {
				dataMutex.unlock();
				continue;
			}
			uint32_t length;
			Util::array_to_int(total_length, NETWORK_DATA_LENGTH_BIG, 0, length);

			//file signature's length
			char block[NETWORK_FILE_BLOCK_SIZE];
			n = socket->read_some(boost::asio::buffer(block, NETWORK_SIGNATURE_LENGTH), error);
			if(error || n != NETWORK_SIGNATURE_LENGTH) {
				dataMutex.unlock();
				continue;
			}
			uint16_t sig_length;
			Util::array_to_int(total_length, NETWORK_SIGNATURE_LENGTH, 0, length);

			//file signature
			char sig[sig_length];
			n = socket->read_some(boost::asio::buffer(sig, sig_length), error);
			if(error || n != sig_length) {
				dataMutex.unlock();
				continue;
			}
			std::string signature(sig);

			//file length
			n = socket->read_some(boost::asio::buffer(block, NETWORK_DATA_LENGTH_BIG), error);
			dataMutex.unlock();
			if(error || n != NETWORK_DATA_LENGTH_BIG) continue;
			uint32_t file_length;
			Util::array_to_int(total_length, NETWORK_DATA_LENGTH_BIG, 0, file_length);


			std::string tempName = LOCAL_TEMP_DATA + node->nodeId + "temp";
			if (protocolCode == NETWORK_LEDGER) tempName += LEDGER_EXTENSION;
			else if (protocolCode == NETWORK_BLOCK) tempName += BLOCK_EXTENSION;
			std::fstream tempFile(tempName, std::fstream::out | std::fstream::trunc);

			//file data
			uint received = 0;
			dataMutex.lock();
			while(received < file_length) {
				if (file_length - received > NETWORK_FILE_BLOCK_SIZE) n = socket->read_some(boost::asio::buffer(block, NETWORK_FILE_BLOCK_SIZE), error);
				else n = socket->read_some(boost::asio::buffer(block, file_length - received), error);

				if(!error && n > 0) {
					tempFile << block;
					received += n;
				}
				else break;
			}
			dataMutex.unlock();
			tempFile.close();

			//verify file's signature and register it
			if(Crypto::Verify(tempName, signature, node->publicKey, true)) {
				if (protocolCode == NETWORK_LEDGER) ledger->SetLedger(tempName);
				else if (protocolCode == NETWORK_BLOCK) networkManager->SetBlock(tempName);
			}
			else {
				boost::filesystem::remove(boost::filesystem::path(tempName));
				if(protocolCode == NETWORK_BLOCK) networkManager->FailedToFetch();
			}
			continue;
		}

		//retrieve message length
		std::vector<char> total_length(NETWORK_DATA_LENGTH);
		dataMutex.lock();
		n = socket->read_some(boost::asio::buffer(total_length), error);
		if(error || n != NETWORK_DATA_LENGTH) {
			dataMutex.unlock();
			continue;
		}
		uint16_t length;
		Util::array_to_int(total_length, NETWORK_DATA_LENGTH, 0, length);

		//retrieve message's data
		std::vector<char> data(length);
		n = socket->read_some(boost::asio::buffer(data), error);
		dataMutex.unlock();
		if(error || n != length) continue;

		///warning: remaining data length is unchecked before extracting individual fields

		//message's signature
		uint16_t sig_length;
		Util::array_to_int(data, NETWORK_SIGNATURE_LENGTH, 0, sig_length);
		int offset = NETWORK_SIGNATURE_LENGTH;
		std::string signature = Util::array_to_string(data, sig_length, offset);

		//process accordingly
		offset += sig_length;
		switch (protocolCode) {

			case NETWORK_BROADCAST_CONFIRMATION: {
				std::string hash = Util::array_to_string(data, TRANSACTION_HASH_LENGTH, offset);
				if(Crypto::Verify(hash, signature, node->publicKey)) txManager->AddConfirmation(hash, node->nodeId);
				break;
			}

			case NETWORK_BROADCAST_TRANSACTION: {
				if(*IS_SYNCHRONIZED) {
					int tx_length;
					Util::array_to_int(data, NETWORK_TRANSACTION_LENGTH, offset, tx_length);
					offset += NETWORK_TRANSACTION_LENGTH;
					
					if(tx_length >= TRANSACTION_MIN_LENGTH && tx_length <= TRANSACTION_MAX_LENGTH) {
						std::string rawTx = Util::array_to_string(data, tx_length, offset);
						if(Crypto::Verify(rawTx, signature, node->publicKey)) {
							//Load transaction and send for validation
							Transaction *transaction = Processing::PrepareTransaction(managerDAO, managerDAS, rawTx, errorCode);
							if(!errorCode) txManager->AddTransaction(transaction, errorCode, true);
						}
					}
				}
				break;
			}

			case NETWORK_LEDGER_CONSENSUS: {
				std::string hash = Util::array_to_string(data, LEDGER_HASH_LENGTH, offset);
				if(Crypto::Verify(hash, signature, node->publicKey)) ledger->AddConfirmation(hash, node->nodeId);
				break;
			}

			case NETWORK_GET_TRANSACTION: {
				std::string hash = Util::array_to_string(data, TRANSACTION_HASH_LENGTH, offset);
				if(Crypto::Verify(hash, signature, node->publicKey)) {
					std::string transaction;
					//retrieve transaction requested
					if(txManager->GetTransaction(hash, transaction)) {
						transaction = "{\"" + hash + "\":" + transaction + "}";

						//send it back
						dataMutex.lock();
						Network::WriteMessage(*socket, NETWORK_TRANSACTION, "", transaction, NETWORK_TRANSACTION_LENGTH, true, true);
						dataMutex.unlock();
					}
				}
				break;
			}

			case NETWORK_TRANSACTION: {
				int tx_length;
				Util::array_to_int(data, NETWORK_TRANSACTION_LENGTH, offset, tx_length);
				offset += NETWORK_TRANSACTION_LENGTH;

				if(tx_length >= TRANSACTION_MIN_LENGTH+TRANSACTION_HASH_LENGTH && tx_length <= TRANSACTION_MAX_LENGTH) {
					std::string rawTx = Util::array_to_string(data, tx_length, offset);

					if(Crypto::Verify(rawTx, signature, node->publicKey)) {
						std::string hash, coreTx;
						hash = rawTx.substr(2, TRANSACTION_HASH_LENGTH);
						coreTx = rawTx.substr(4+TRANSACTION_HASH_LENGTH, tx_length-5-TRANSACTION_HASH_LENGTH);

						//load transaction and verify it has the same hash
						Transaction *transaction = Processing::PrepareTransaction(managerDAO, managerDAS, coreTx, errorCode);
						if(!errorCode && hash == transaction->GetHash()) txManager->AddTransaction(transaction, errorCode);
						else txManager->FailedToFetch(hash);
					}
				}
				break;
			}

			case NETWORK_GET_LEDGER: {
				std::string hash = Util::array_to_string(data, LEDGER_HASH_LENGTH, offset);
				if(Crypto::Verify(hash, signature, node->publicKey)) {
					std::string ledgerFile;

					dataMutex.lock();
					//Send back Ledger
					if(ledger->GetLedger(hash, ledgerFile)) Network::SendFile(*socket, NETWORK_LEDGER, ledgerFile);
					//or inform it was not found
					else Network::WriteMessage(*socket, NETWORK_LEDGER_NOT_FOUND, hash, true);
					dataMutex.unlock();
				}
				break;
			}

			case NETWORK_LEDGER_NOT_FOUND: {
				std::string hash = Util::array_to_string(data, LEDGER_HASH_LENGTH, offset);
				//Request another Node
				if(Crypto::Verify(hash, signature, node->publicKey)) ledger->FailedToFetch(hash);
			}

			case NETWORK_NEW_ENTRY: {
				int entry_length;
				Util::array_to_int(data, NETWORK_ENTRY_LENGTH, offset, entry_length);
				offset += NETWORK_ENTRY_LENGTH;

				if(entry_length >= ENTRY_MIN_LENGTH) {
					std::string entry = Util::array_to_string(data, entry_length, offset);

					if(Crypto::Verify(entry, signature, node->publicKey)) {
						//extract its hash for the response
						std::string hash = entry.substr(2, ENTRY_HASH_LENGTH);
						std::string entry = entry.substr(5+ENTRY_HASH_LENGTH, entry.length()-6-ENTRY_HASH_LENGTH);

						//submit entry and send back appropriate response
						///thinking AddEntry(rawEntry, errorCode, newEntry=true)
						if(networkManager->AddEntry(hash, entry, errorCode)) Network::WriteMessage(*socket, NETWORK_ENTRY_ACCEPTED, hash, true);
						else Network::WriteMessage(*socket, NETWORK_ENTRY_REJECTED, hash+std::to_string(errorCode), true);
					}
				}
			}

			case NETWORK_ENTRY_ACCEPTED: {
				//ignore?
			}

			case NETWORK_ENTRY_REJECTED: {
				//ignore?
			}

			case NETWORK_GET_ENTRY: {
				std::string hash = Util::array_to_string(data, ENTRY_HASH_LENGTH, offset);
				if(Crypto::Verify(hash, signature, node->publicKey)) {
					//fetch entry
				}
			}

			case NETWORK_ENTRY: {
				int entry_length;
				Util::array_to_int(data, NETWORK_ENTRY_LENGTH, offset, entry_length);
				offset += NETWORK_ENTRY_LENGTH;

				if(entry_length >= ENTRY_MIN_LENGTH) {
					std::string entry = Util::array_to_string(data, entry_length, offset);

					//register entry
					if(Crypto::Verify(entry, signature, node->publicKey)) {						
						std::string hash = entry.substr(2, ENTRY_HASH_LENGTH);
						entry = entry.substr(5+ENTRY_HASH_LENGTH, entry.length()-6-ENTRY_HASH_LENGTH);
						networkManager->AddEntry(hash, entry, errorCode, false);
					}
				}

			}

			case NETWORK_BLOCK_CONSENSUS: {
				std::string hash = Util::array_to_string(data, BLOCK_HASH_LENGTH, offset);
				if(Crypto::Verify(hash, signature, node->publicKey)) networkManager->AddConfirmation(hash, node->nodeId);
				break;
			}

			case NETWORK_GET_BLOCK: {
				std::string hash = Util::array_to_string(data, BLOCK_HASH_LENGTH, offset);
				if(Crypto::Verify(hash, signature, node->publicKey)) {
					//Send back Block if found
					std::string block;
					if(!networkManager->GetBlock(hash, block) || !Network::SendFile(*socket, NETWORK_BLOCK, block)) {
						Network::WriteMessage(*socket, NETWORK_BLOCK_NOT_FOUND, hash, true);
					}
				}
			}

			case NETWORK_BLOCK_NOT_FOUND: {
				std::string hash = Util::array_to_string(data, BLOCK_HASH_LENGTH, offset);
				//Request another Node
				if(Crypto::Verify(hash, signature, node->publicKey)) networkManager->FailedToFetch(hash);
			}

			default:
				break;
		}
	}
}


void Communication::ListenToEntity(bool *IS_OPERATING, bool *IS_SYNCHRONIZED, DAOManager *managerDAO, DASManager *managerDAS, Keys *keysDB, NetworkManager *networkManager, TransactionsManager *txManager, EntityStruct *entity) {	
	bool connected = true;
	int n, errorCode;
	std::mutex dataMutex;

	while(connected && *IS_OPERATING) {
		boost::system::error_code error;
		n = errorCode = 0;

		//retrieve message's protocol code
		std::vector<char> code(NETWORK_CODE_LENGTH);
		dataMutex.lock();
		n = entity->socket->read_some(boost::asio::buffer(code), error);
		dataMutex.unlock();

		if(error || n != NETWORK_CODE_LENGTH) continue;
		uint16_t protocolCode;
		Util::array_to_int(code, NETWORK_CODE_LENGTH, 0, protocolCode);

		//ignore KEEP_ALIVE messages
		if(protocolCode == NETWORK_KEEP_ALIVE) continue;

		//process NETWORK_EXIT messages
		if (protocolCode == NETWORK_EXIT) {
			dataMutex.lock();
			if(entity->socket->is_open()) entity->socket->close();
			dataMutex.unlock();

			connected = false;
			continue;
		}

		//process separately incomming Network Management Blocks
		if (protocolCode == NETWORK_BLOCK) {
			//retrieve total length of message's data
			std::vector<char> total_length(NETWORK_DATA_LENGTH_BIG);
			dataMutex.lock();
			n = entity->socket->read_some(boost::asio::buffer(total_length), error);
			if(error || n != NETWORK_DATA_LENGTH_BIG) {
				dataMutex.unlock();
				continue;
			}
			uint32_t length;
			Util::array_to_int(total_length, NETWORK_DATA_LENGTH_BIG, 0, length);

			//file signature's length
			char block[NETWORK_FILE_BLOCK_SIZE];
			n = entity->socket->read_some(boost::asio::buffer(block, NETWORK_SIGNATURE_LENGTH), error);
			if(error || n != NETWORK_SIGNATURE_LENGTH) {
				dataMutex.unlock();
				continue;
			}
			uint16_t sig_length;
			Util::array_to_int(total_length, NETWORK_SIGNATURE_LENGTH, 0, length);

			//file signature
			char sig[sig_length];
			n = entity->socket->read_some(boost::asio::buffer(sig, sig_length), error);
			if(error || n != sig_length) {
				dataMutex.unlock();
				continue;
			}
			std::string signature(sig);

			//file length
			n = entity->socket->read_some(boost::asio::buffer(block, NETWORK_DATA_LENGTH_BIG), error);
			dataMutex.unlock();
			if(error || n != NETWORK_DATA_LENGTH_BIG) continue;
			uint32_t file_length;
			Util::array_to_int(total_length, NETWORK_DATA_LENGTH_BIG, 0, file_length);

			std::string tempName = LOCAL_TEMP_DATA + entity->entityId + "temp" + BLOCK_EXTENSION;
			std::fstream tempFile(tempName, std::fstream::out | std::fstream::trunc);

			//file data
			uint received = 0;
			dataMutex.lock();
			while(received < file_length) {
				if (file_length - received > NETWORK_FILE_BLOCK_SIZE) n = entity->socket->read_some(boost::asio::buffer(block, NETWORK_FILE_BLOCK_SIZE), error);
				else n = entity->socket->read_some(boost::asio::buffer(block, file_length - received), error);

				if(!error && n > 0) {
					tempFile << block;
					received += n;
				}
				else break;
			}
			dataMutex.unlock();
			tempFile.close();

			//verify file's signature
			if(Crypto::Verify(tempName, signature, entity->publicKey, true)) networkManager->SetBlock(tempName);
			else {
				boost::filesystem::remove(boost::filesystem::path(tempName));
				networkManager->FailedToFetch();
			}
			continue;
		}

		//retrieve data length
		std::vector<char> total_length(NETWORK_DATA_LENGTH);
		dataMutex.lock();
		n = entity->socket->read_some(boost::asio::buffer(total_length), error);
		if(error || n != NETWORK_DATA_LENGTH) {
			dataMutex.unlock();
			continue;
		}
		uint16_t length;
		Util::array_to_int(total_length, NETWORK_DATA_LENGTH, 0, length);

		//retrieve message's data
		std::vector<char> data(length);
		n = entity->socket->read_some(boost::asio::buffer(data), error);
		dataMutex.unlock();
		if(error || n != length) continue;

		///warning: remaining data length is unchecked before extracting individual fields

		//message's signature
		uint16_t sig_length;
		Util::array_to_int(data, NETWORK_SIGNATURE_LENGTH, 0, sig_length);
		int offset = NETWORK_SIGNATURE_LENGTH;
		std::string signature = Util::array_to_string(data, sig_length, offset);

		//process accordingly
		offset += sig_length;
		switch (protocolCode) {

			case NETWORK_NEW_TRANSACTION: {
				if(*IS_SYNCHRONIZED) {
					std::string hash = Util::array_to_string(data, TRANSACTION_HASH_LENGTH, offset);
					offset += TRANSACTION_HASH_LENGTH;
					int tx_length;
					Util::array_to_int(data, NETWORK_TRANSACTION_LENGTH, offset, tx_length);
					offset += NETWORK_TRANSACTION_LENGTH;

					if(tx_length >= TRANSACTION_MIN_LENGTH+TRANSACTION_HASH_LENGTH && tx_length <= TRANSACTION_MAX_LENGTH) {
						std::string newTx = Util::array_to_string(data, tx_length, offset);

						if(Crypto::Verify(hash+newTx, signature, entity->publicKey)) {
							Transaction *transaction = Processing::PrepareTransaction(managerDAO, managerDAS, newTx, errorCode);

							if(!errorCode) {
								if(hash != transaction->GetHash()) errorCode = ERROR_HASH;
								else if(txManager->AddTransaction(transaction, errorCode, true, DISPATCHER_ENTITY, entity->entityId)) {
									dataMutex.lock();
									Network::WriteMessage(*entity->socket, NETWORK_TRANSACTION_ACCEPTED, hash, true);
									dataMutex.unlock();
								}
							}

							if(errorCode) {
								dataMutex.lock();
								Network::WriteMessage(*entity->socket, NETWORK_TRANSACTION_REJECTED, hash+std::to_string(errorCode), true);
								dataMutex.unlock();
							}
						}
					}
				}
				else {
					dataMutex.lock();
					Network::WriteMessage(*entity->socket, NETWORK_UNSYNCHRONIZED, "", false);
					dataMutex.unlock();
				}
				break;
			}

			case NETWORK_PUBLIC_KEY: {	
				std::string account = Util::array_to_string(data, ACCOUNT_LENGTH, offset);
				offset += ACCOUNT_LENGTH;

				int key_length;
				Util::array_to_int(data, NETWORK_PUBLIC_KEY_LENGTH, offset, key_length);
				offset += NETWORK_PUBLIC_KEY_LENGTH;

				std::string publicKey = Util::array_to_string(data, key_length, offset);

				if(Crypto::Verify(account+publicKey, signature, entity->publicKey)) keysDB->SetPublicKey(account, publicKey);
				break;
			}

			case NETWORK_KEY_NOT_FOUND: {
				//ingore?
				break;
			}

			case NETWORK_NEW_ENTRY: {
				int entry_length;
				Util::array_to_int(data, NETWORK_ENTRY_LENGTH, offset, entry_length);
				offset += NETWORK_ENTRY_LENGTH;

				if(entry_length >= ENTRY_MIN_LENGTH) {
					std::string entry = Util::array_to_string(data, entry_length, offset);

					if(Crypto::Verify(entry, signature, entity->publicKey)) {
						//extract its hash for the response
						std::string hash = entry.substr(2, ENTRY_HASH_LENGTH);
						entry = entry.substr(4+ENTRY_HASH_LENGTH, entry.length()-5-ENTRY_HASH_LENGTH);

						if(networkManager->AddEntry(hash, entry, errorCode)) Network::WriteMessage(*entity->socket, NETWORK_ENTRY_ACCEPTED, hash, true);
						else Network::WriteMessage(*entity->socket, NETWORK_ENTRY_REJECTED, hash+std::to_string(errorCode), true);
					}
				}
				break;
			}

			case NETWORK_ENTRY_ACCEPTED: {
				//ignore?
				break;
			}

			case NETWORK_ENTRY_REJECTED: {
				//ignore?
				break;
			}

			case NETWORK_GET_ENTRY: {
				std::string hash = Util::array_to_string(data, ENTRY_HASH_LENGTH, offset);
				if(Crypto::Verify(hash, signature, entity->publicKey)) {
					//fetch Entry
				}
				break;
			}

			case NETWORK_ENTRY: {
				int entry_length;
				Util::array_to_int(data, NETWORK_ENTRY_LENGTH, offset, entry_length);
				offset += NETWORK_ENTRY_LENGTH;

				if(entry_length >= ENTRY_MIN_LENGTH) {
					std::string entry = Util::array_to_string(data, entry_length, offset);

					//verify signature
					if(Crypto::Verify(entry, signature, entity->publicKey)) {
						//extract its hash for the response
						std::string hash = entry.substr(2, ENTRY_HASH_LENGTH);
						entry = entry.substr(4+ENTRY_HASH_LENGTH, entry.length()-5-ENTRY_HASH_LENGTH);

						//register Entry
						networkManager->AddEntry(hash, entry, errorCode, false);
					}
				}
				break;
			}

			case NETWORK_BLOCK_CONSENSUS: {
				std::string hash = Util::array_to_string(data, BLOCK_HASH_LENGTH, offset);
				if(Crypto::Verify(hash, signature, entity->publicKey)) networkManager->AddConfirmation(hash, entity->entityId);
				break;
			}

			case NETWORK_GET_BLOCK: {
				std::string hash = Util::array_to_string(data, BLOCK_HASH_LENGTH, offset);
				if(Crypto::Verify(hash, signature, entity->publicKey)) {
					//Send back Block if found
					std::string block;
					if(!networkManager->GetBlock(hash, block) || !Network::SendFile(*entity->socket, NETWORK_BLOCK, block)) {
						Network::WriteMessage(*entity->socket, NETWORK_BLOCK_NOT_FOUND, hash, true);
					}
				}
				break;
			}

			case NETWORK_BLOCK_NOT_FOUND: {
				std::string hash = Util::array_to_string(data, BLOCK_HASH_LENGTH, offset);
				if(Crypto::Verify(hash, signature, entity->publicKey)) networkManager->FailedToFetch(hash);
				break;
			}

			default:
				break;
		}
	}
}


void Communication::ListenToSubscriber(bool *IS_OPERATING, Publisher *publisher, SubscriberStruct *subscriber) {
	bool connected = true;
	int n, errorCode;
	std::mutex dataMutex;

	while(connected && *IS_OPERATING) {
		boost::system::error_code error;
		n = errorCode = 0;

		//retrieve message's protocol code
		std::vector<char> code(NETWORK_CODE_LENGTH);
		dataMutex.lock();
		n = subscriber->socket.read_some(boost::asio::buffer(code), error);
		dataMutex.unlock();

		if(error || n != NETWORK_CODE_LENGTH) continue;
		uint16_t protocolCode;
		Util::array_to_int(code, NETWORK_CODE_LENGTH, 0, protocolCode);

		switch(protocolCode) {

			case NETWORK_SUBSCRIPTIONS: {
				//retrieve data length
				std::vector<char> data_length(NETWORK_SUBSCRIPTION_LENGTH);
				dataMutex.lock();
				n = subscriber->socket.read_some(boost::asio::buffer(data_length), error);
				if(error || n != NETWORK_SUBSCRIPTION_LENGTH) {
					dataMutex.unlock();
					continue;
				}
				uint16_t length;
				Util::array_to_int(data_length, NETWORK_SUBSCRIPTION_LENGTH, 0, length);

				//retrieve subscriptions
				char subs[length];
				n = subscriber->socket.read_some(boost::asio::buffer(subs, length), error);
				dataMutex.unlock();
				if(error || n != length) continue; 
				
				//extract and verify json contents
				rapidjson::Document subscriptions;
				subscriptions.Parse(subs);
				if(subscriptions.IsObject() && subscriptions.HasMember("transaction") && subscriptions["transaction"].IsBool() && subscriptions.HasMember("ledger") && 
					subscriptions["ledger"].IsBool() && subscriptions.HasMember("networkManagementBlock") && subscriptions["networkManagementBlock"].IsBool()) {

					//update subscriptions
					subscriber->transaction = subscriptions["transaction"].GetBool();
					subscriber->ledger = subscriptions["ledger"].GetBool();
					subscriber->block = subscriptions["networkManagementBlock"].GetBool();
				}
				break;
			}

			case NETWORK_UNSUBSCRIBE: {
				publisher->RemoveSubscriber(subscriber->name);
				connected = false;
				break;
			}

			default:
				connected = false;
				break;
		}
	}
}


void Communication::ListenToAnyone(bool *IS_OPERATING, Entities *entities, Nodes *nodes, Publisher *publisher) {
	std::string entity, node, publicKey, signature;
	int n, errorCode;
	std::mutex dataMutex;

	boost::asio::ip::tcp::acceptor acceptor(NETWORK_SOCKET_SERVICE, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), _PORT));

	while(*IS_OPERATING) {
		n = errorCode = 0;
		boost::system::error_code error;
		// boost::asio::ip::tcp::socket socket(NETWORK_SOCKET_SERVICE);
		boost::asio::ip::tcp::socket *socket = new boost::asio::ip::tcp::socket(NETWORK_SOCKET_SERVICE);
     	acceptor.accept(*socket);

		//retrieve message's protocol code
		std::vector<char> code(NETWORK_CODE_LENGTH);
		n = socket->read_some(boost::asio::buffer(code), error);
		if(error || n != NETWORK_CODE_LENGTH) continue;
		uint16_t protocolCode;
		Util::array_to_int(code, NETWORK_CODE_LENGTH, 0, protocolCode);

		//process new subcriber-related messages separately
		if(protocolCode == NETWORK_SUBSCRIBE || protocolCode == NETWORK_SUBSCRIPTIONS || protocolCode == NETWORK_UNSUBSCRIBE) {

			//retrieve data length
			std::vector<char> data_length(NETWORK_SUBSCRIPTION_LENGTH);
			n = socket->read_some(boost::asio::buffer(data_length), error);
			if(error || n != NETWORK_SUBSCRIPTION_LENGTH) continue;
			uint16_t length;
			Util::array_to_int(data_length, NETWORK_SUBSCRIPTION_LENGTH, 0, length);

			//retrieve subscriptions
			char sub[length];
			n = socket->read_some(boost::asio::buffer(sub, length), error);
			dataMutex.unlock();
			if(error || n != length) continue; 

			//extract and verify json content
			rapidjson::Document subscriber;
			subscriber.Parse(sub);
			if(!subscriber.IsObject() || !subscriber.HasMember("name") || !subscriber["name"].IsString()) continue;

			//Execute unsubscription
			if(protocolCode == NETWORK_UNSUBSCRIBE) publisher->RemoveSubscriber(subscriber["name"].GetString());

			if(subscriber.HasMember("transaction") && subscriber.HasMember("ledger") && subscriber.HasMember("block") && 
				subscriber["transaction"].IsBool() && subscriber["ledger"].IsBool() && subscriber["block"].IsBool())
			{
				if(protocolCode == NETWORK_SUBSCRIBE) {
					////check if ip/name is in the approval list////
					////check if it can have another subcriber////
					//keep subscriber
					publisher->AddSubscriber(subscriber["name"].GetString(), *socket, subscriber["transaction"].GetBool(), subscriber["ledger"].GetBool(), subscriber["block"].GetBool());
				}
				else if(protocolCode == NETWORK_SUBSCRIPTIONS) {
					//Update subscriptions
					publisher->UpdateSubscriptions(subscriber["name"].GetString(), subscriber["transaction"].GetBool(), subscriber["ledger"].GetBool(), subscriber["block"].GetBool());
				}
			}
			continue;
		}

		//retrieve data length
		std::vector<char> total_length(NETWORK_DATA_LENGTH);
		n = socket->read_some(boost::asio::buffer(total_length), error);
		if(error || n != NETWORK_DATA_LENGTH) continue;
		uint16_t length;
		Util::array_to_int(total_length, NETWORK_DATA_LENGTH, 0, length);

		//retrieve message's data
		std::vector<char> data(length);
		n = socket->read_some(boost::asio::buffer(data), error);
		if(error || n != length) continue;

		//process message accordingly
		int offset = 0;
		switch(protocolCode) {

			case NETWORK_IDENTIFICATION_ENTITY: {
				//retrieve signature length
				uint16_t sig_length;
				Util::array_to_int(data, NETWORK_SIGNATURE_LENGTH, offset, sig_length);
				offset += NETWORK_SIGNATURE_LENGTH;
				//retrieve signature
				signature = Util::array_to_string(data, sig_length, offset);
				offset += sig_length;

				//retrieve Entity ID
				entity = Util::array_to_string(data, ENTITY_ID_LENGTH, offset);
				offset += ENTITY_ID_LENGTH;
				//validate ID
				boost::regex idRegex(PATTERN_ENTITY_ID);
				if(!boost::regex_match(entity, idRegex)) continue;

				//retrieve and verify timestamp used
				uint64_t now = Util::current_timestamp();
				uint64_t timestamp = std::stoull(Util::array_to_string(data, NETWORK_TIMESTAMP_LENGTH, offset));
				if(!timestamp || timestamp > now || timestamp < now - NETWORK_IDENTIFICATION_DELAY) {
					// Network::WriteMessage(*socket, NETWORK_IDENTIFICATION_FAILURE, _SELF, true);
					Network::WriteMessage(*socket, NETWORK_IDENTIFICATION_FAILURE, "", false);
				}

				//retrieve public key and verify signature
				if(!entities->GetPublicKey(entity, publicKey) && Crypto::Verify(_SELF+std::to_string(timestamp), signature, publicKey)) {
					entities->AddEntity(entity, *socket);
					// Network::WriteMessage(*socket, NETWORK_IDENTIFICATION_SUCCESS, _SELF, true);
					Network::WriteMessage(*socket, NETWORK_IDENTIFICATION_SUCCESS, "", false);
					break;
				}

				// Network::WriteMessage(*socket, NETWORK_IDENTIFICATION_FAILURE, _SELF, true);
				Network::WriteMessage(*socket, NETWORK_IDENTIFICATION_FAILURE, "", false);
				break;
			}

			case NETWORK_IDENTIFICATION_NODE: {
				//retrieve signature length
				uint16_t sig_length;
				Util::array_to_int(data, NETWORK_SIGNATURE_LENGTH, offset, sig_length);
				offset += NETWORK_SIGNATURE_LENGTH;
				//retrieve signature
				signature = Util::array_to_string(data, sig_length, offset);
				offset += sig_length;

				//retrieve Node ID
				node = Util::array_to_string(data, NODE_ID_LENGTH, offset);
				offset += NODE_ID_LENGTH;
				//validate ID
				boost::regex idRegex(PATTERN_NODE_ID);
				if(!boost::regex_match(entity, idRegex)) continue;

				//retrieve and verify timestamp used
				uint64_t now = Util::current_timestamp();
				uint64_t timestamp = std::stoull(Util::array_to_string(data, NETWORK_TIMESTAMP_LENGTH, offset));
				if(!timestamp || timestamp > now || timestamp < now - NETWORK_IDENTIFICATION_DELAY) {
					// Network::WriteMessage(*socket, NETWORK_IDENTIFICATION_FAILURE, _SELF, true);
					Network::WriteMessage(*socket, NETWORK_IDENTIFICATION_FAILURE, "", false);
				}

				//retrieve public key and verify signature
				if(!nodes->GetPublicKey(node, publicKey) && Crypto::Verify(_SELF+std::to_string(timestamp), signature, publicKey)) {
					nodes->AddNode(node, socket);
					// Network::WriteMessage(*socket, NETWORK_IDENTIFICATION_SUCCESS, _SELF, true);
					Network::WriteMessage(*socket, NETWORK_IDENTIFICATION_SUCCESS, "", false);
					break;
				}

				// Network::WriteMessage(*socket, NETWORK_IDENTIFICATION_FAILURE, _SELF, true);
				Network::WriteMessage(*socket, NETWORK_IDENTIFICATION_FAILURE, "", false);
				break;
			}

			///Replaced by NMEs signed by the WorldBank
			// case NETWORK_NEW_ENTITY: {
			// 	//retrieve Entity ID
			// 	entity = Util::array_to_string(data, ENTITY_ID_LENGTH, offset);
			// 	offset += ENTITY_ID_LENGTH;
			// 	//validate ID
			// 	boost::regex idRegex(PATTERN_ENTITY_ID);
			// 	if(!boost::regex_match(entity, idRegex)) continue;

			// 	//retrieve public key length
			// 	uint16_t key_length;
			// 	Util::array_to_int(data, NETWORK_PUBLIC_KEY_LENGTH, offset, key_length);
			// 	offset += NETWORK_PUBLIC_KEY_LENGTH;
			// 	//retrieve public key
			// 	std::string key = Util::array_to_string(data, key_length, offset);
			// 	offset += key_length;

			// 	//retrieve signature length
			// 	uint16_t sig_length;
			// 	Util::array_to_int(data, NETWORK_SIGNATURE_LENGTH, offset, sig_length);
			// 	offset += NETWORK_SIGNATURE_LENGTH;
			// 	//retrieve signature
			// 	signature = Util::array_to_string(data, sig_length, offset);
			// 	offset += sig_length;

			// 	//retrieve and verify timestamp used
			// 	uint64_t now = Util::current_timestamp();
			// 	uint64_t timestamp = std::stoull(Util::array_to_string(data, NETWORK_TIMESTAMP_LENGTH, offset));
			// 	if(!timestamp || timestamp > now || timestamp < now - NETWORK_REGISTRATION_DELAY) {
			// 		Network::WriteMessage(*socket, NETWORK_REGISTRATION_FAILURE, _SELF, true);
			// 	}

			// 	//retrieve World Bank's public key
			// 	entities->GetPublicKey(WORLD_BANK_ENTITY, publicKey);

			// 	//verify contents signature
			// 	if(Crypto::Verify(entity+key+std::to_string(timestamp), signature, publicKey)) {
			// 		//Extract host
			// 		boost::asio::ip::tcp::endpoint endpoint = socket->remote_endpoint();
			// 		std::string host = endpoint.address().to_string()+":"+std::to_string(endpoint.port());

			// 		//Add data and move socket
			// 		entities->NewEntity(entity, key, host, false);
			// 		entities->AddEntity(entity, *socket);

			// 		Network::WriteMessage(*socket, NETWORK_REGISTRATION_SUCCESS, _SELF, true);
			// 		break;
			// 	}

			// 	Network::WriteMessage(*socket, NETWORK_REGISTRATION_FAILURE, _SELF, true);
			// 	break;
			// }

			// case NETWORK_NEW_NODE: {
			// 	//retrieve Node ID
			// 	node = Util::array_to_string(data, NODE_ID_LENGTH, offset);
			// 	offset += NODE_ID_LENGTH;
			// 	//validate ID
			// 	boost::regex idRegex(PATTERN_NODE_ID);
			// 	if(!boost::regex_match(node, idRegex)) continue;

			// 	//retrieve public key length
			// 	uint16_t key_length;
			// 	Util::array_to_int(data, NETWORK_PUBLIC_KEY_LENGTH, offset, key_length);
			// 	offset += NETWORK_PUBLIC_KEY_LENGTH;
			// 	//retrieve public key
			// 	publicKey = Util::array_to_string(data, key_length, offset);
			// 	offset += key_length;

			// 	//retrieve signature length
			// 	uint16_t sig_length;
			// 	Util::array_to_int(data, NETWORK_SIGNATURE_LENGTH, offset, sig_length);
			// 	offset += NETWORK_SIGNATURE_LENGTH;
			// 	//retrieve signature
			// 	signature = Util::array_to_string(data, sig_length, offset);
			// 	offset += sig_length;

			// 	//retrieve and verify timestamp used
			// 	uint64_t now = Util::current_timestamp();
			// 	uint64_t timestamp = std::stoull(Util::array_to_string(data, NETWORK_TIMESTAMP_LENGTH, offset));
			// 	if(!timestamp || timestamp > now || timestamp < now - NETWORK_REGISTRATION_DELAY) {
			// 		Network::WriteMessage(*socket, NETWORK_REGISTRATION_FAILURE, _SELF, true);
			// 	}

			// 	//retrieve World Bank's public key
			// 	std::string wbKey;
			// 	entities->GetPublicKey(WORLD_BANK_ENTITY, wbKey);

			// 	//verify contents signature
			// 	if(Crypto::Verify(node+publicKey+std::to_string(timestamp), signature, wbKey)) {
			// 		//Extract host
			// 		boost::asio::ip::tcp::endpoint endpoint = socket->remote_endpoint();
			// 		std::string host = endpoint.address().to_string()+":"+std::to_string(endpoint.port());

			// 		//Add data and move socket
			// 		nodes->NewNode(node, publicKey, host, CURRENT_VERSION, WORLD_BANK_NODE_ACCOUNT, NODE_REPUTATION_DEFAULT, false);
			// 		nodes->AddNode(node, socket);

			// 		Network::WriteMessage(*socket, NETWORK_REGISTRATION_SUCCESS, _SELF, true);
			// 		break;
			// 	}

			// 	Network::WriteMessage(*socket, NETWORK_REGISTRATION_FAILURE, _SELF, true);
			// 	break;
			// }

			default: 
				Network::WriteMessage(*socket, NETWORK_ERROR, _SELF, true);
				break;

		}
	}
}