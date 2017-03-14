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
 * @file network_manager.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include "includes/boost/filesystem.hpp"
#include "includes/boost/regex.hpp"
#include "includes/cryptopp/filters.h"
#include "includes/cryptopp/ripemd.h"
#include "includes/cryptopp/hex.h"
#include "includes/rocksdb/db.h"
#include "includes/rocksdb/iterator.h"
#include "includes/rocksdb/utilities/backupable_db.h"
#include "includes/rocksdb/write_batch.h"

#include "balances.h"
#include "block.h"
#include "codes.h"
#include "configurations.h"
#include "dao_manager.h"
#include "das_manager.h"
#include "ecdsa.h"
#include "entity.h"
#include "entry.h"
#include "entry_account.h"
#include "entry_entity.h"
#include "entry_dao.h"
#include "entry_das.h"
#include "entry_node.h"
#include "entry_passport.h"
#include "entry_resource.h"
#include "entry_slot.h"
#include "globals.h"
#include "keys.h"
#include "merkle.h"
#include "network.h"
#include "network_manager.h"
#include "node.h"
#include "processing.h"
#include "slots.h"
#include "threads_manager.h"
#include "util.h"


NetworkManager::NetworkManager(rocksdb::DB *db, DAOManager *managerDAO, DASManager *managerDAS, Keys *keysDB, Publisher *publisher, Slots *slotsDB, ThreadsManager *threadsManager)
 : db(db), managerDAO(managerDAO), managerDAS(managerDAS), keysDB(keysDB), publisher(publisher), slotsDB(slotsDB), threadsManager(threadsManager) {
	rocksdb::Iterator* dbIterator = db->NewIterator(rocksdb::ReadOptions());
	rocksdb::Status status;
 	std::string id, publicKey, host, version, account, reputation;

 	//Load Managing Entities data
 	std::unordered_map<std::string, EntityStruct> entitiesData;

 	//Use local Entities index
 	entitiesIndex.open(LOCAL_DATA_ENTITIES_INDEX, std::fstream::in);
 	if(entitiesIndex.good()) {
 		while(entitiesIndex >> id) {
 			EntityStruct entity;
 			if(GetEntityData(id, entity)) entitiesData[id] = entity;
 		}
 		entitiesIndex.close();
 		entitiesIndex.open(LOCAL_DATA_NODES_INDEX, std::fstream::app);
 	}
 	//Otherwise iterate through the database for Validating Nodes IDs
 	else {
 		entitiesIndex.close();
 		entitiesIndex.open(LOCAL_DATA_NODES_INDEX, std::fstream::trunc);

 		for (dbIterator->Seek(ITERATOR_ENTITY_FIRST); dbIterator->Valid() && dbIterator->key().ToString() < ITERATOR_ENTITY_END; dbIterator->Next()) {
 			id = dbIterator->key().ToString();

 			EntityStruct entity;
 			if(GetEntityData(id, entity)) {
 				entitiesData[id] = entity;
				//update local index
				entitiesIndex << id << std::endl;
			}
 		}
 	}

 	//Initialize Entities manager
 	this->entities = new Entities(entitiesData);

 	//Load Validating Nodes data
 	std::unordered_map<std::string, NodeStruct> nodesData;

 	//Use local Nodes index
 	nodesIndex.open(LOCAL_DATA_NODES_INDEX, std::fstream::in);
 	if(nodesIndex.good()) {
 		while(nodesIndex >> id) {
 			NodeStruct node;
 			if(GetNodeData(id, node)) nodesData[id] = node;
 		}
 		nodesIndex.close();
 		nodesIndex.open(LOCAL_DATA_NODES_INDEX, std::fstream::app);
 	}
 	//Otherwise iterate through the database for Validating Nodes IDs
 	else {
 		nodesIndex.close();
 		nodesIndex.open(LOCAL_DATA_NODES_INDEX, std::fstream::trunc);
 		for (dbIterator->Seek(ITERATOR_NODE_FIRST); dbIterator->Valid() && dbIterator->key().ToString() < ITERATOR_NODE_END; dbIterator->Next()) {
 			id = dbIterator->key().ToString();
 			NodeStruct node;
 			if(GetNodeData(id, node)) {
 				nodesData[id] = node;
 				nodesIndex << id << std::endl;
 			}
 		}
 	}

 	//Set current number of network peers
 	numPeers = entitiesData.size() + nodesData.size();

 	//Initialize Nodes manager
 	this->nodes = new Nodes(nodesData);

	//Open Blocks index file
	blocksIndex.open(LOCAL_DATA_BLOCKS_INDEX, std::fstream::out | std::fstream::app);
	if(!blocksIndex.good()) {
		blocksIndex.close();
		blocksIndex.open(LOCAL_DATA_BLOCKS_INDEX, std::fstream::out | std::fstream::trunc);
		blocksIndex << GENESIS_BLOCK_ID << " " << GENESIS_BLOCK_HASH << std::endl;
		blocksIndex.flush();
	}

	//Open Ledgers index file
	ledgersIndex.open(LOCAL_DATA_LEDGERS_INDEX, std::fstream::out | std::fstream::app);
	if(!ledgersIndex.good()) {
		ledgersIndex.close();
		ledgersIndex.open(LOCAL_DATA_LEDGERS_INDEX, std::fstream::out | std::fstream::trunc);
		ledgersIndex << GENESIS_LEDGER_ID << " " << GENESIS_LEDGER_HASH << std::endl;
		ledgersIndex.flush();
	}
}

bool NetworkManager::GetEntityData(std::string id, EntityStruct &entity) {
 	entity.entityId = id;

	if(!keysDB->GetPublicKey(id, entity.publicKey, ID_TYPE_ENTITY, false)) return false;

	rocksdb::Status status = db->Get(rocksdb::ReadOptions(), id+NMDB_MASK_HOST, &entity.host);
	if(status.IsNotFound()) return false;

	return true;
}

bool NetworkManager::GetNodeData(std::string id, NodeStruct &node) {
 	node.nodeId = id;

	if(!keysDB->GetPublicKey(id, node.publicKey, ID_TYPE_NODE, false)) return false;

	rocksdb::Status status = db->Get(rocksdb::ReadOptions(), id+NMDB_MASK_HOST, &node.host);
	if(status.IsNotFound()) return false;

	status = db->Get(rocksdb::ReadOptions(), id+NMDB_MASK_VERSION, &node.version);
	if(status.IsNotFound()) return false;

	std::string value;
	status = db->Get(rocksdb::ReadOptions(), id+NMDB_MASK_ACCOUNT, &value);
	if(!status.IsNotFound()) node.account = value;

	status = db->Get(rocksdb::ReadOptions(), id+NMDB_MASK_REPUTATION, &value);
	if(!status.IsNotFound()) node.reputation = std::stol(value);

	return true;
}

void NetworkManager::SynchronizeNMB() {
	int nextBlock = GENESIS_BLOCK_ID;

	rocksdb::Status status = db->Get(rocksdb::ReadOptions(), NMDB_MASK_BLOCK+std::to_string(GENESIS_BLOCK_ID), &latestBlock);
	if(status.IsNotFound()) {
		//Try to execute the Genesis Network Management Block
		SetBlock(LOCAL_DATA_BLOCKS+std::to_string(GENESIS_BLOCK_ID)+BLOCK_EXTENSION);
	}
	else {
		//Find the latest registered Block
		while(!status.IsNotFound()) {
			nextBlock++;
			status = db->Get(rocksdb::ReadOptions(), NMDB_MASK_BLOCK+std::to_string(nextBlock), &latestBlock);
		}
	}

	//Check if the UDC network has already begun operating
	auto now = Util::current_timestamp();
	if(now < GENESIS_BLOCK_END) {
		//If not, we're synchronized
		latestBlock = GENESIS_BLOCK_HASH;

		//Launch Blocks manager
		this->block = new Block(this, publisher, true, latestBlock, 0);
		return;
	}

	//Begin the synchronization process of the NMB
	boost::system::error_code error;
	ConnectToTracker:
		//Try to connect to a Tracker
		boost::asio::ip::tcp::socket socket(NETWORK_SOCKET_SERVICE);
		if(!Network::ConnectToTracker(socket)) {
			//Cannot update
			std::cout << "Network Management Blockchain synchronization process failed: cannot connect to a Data Tracker." << std::endl;
			exit(EXIT_FAILURE);
		}

	//Request latest Network Management Block hash from a Tracker
	if(!Network::WriteMessage(socket, NETWORK_TRACKER_LATEST, std::to_string(TRACKER_CODE_NMB), false)) goto ConnectToTracker;

	//read response
	std::vector<char> res(BLOCK_HASH_LENGTH);
	int n = socket.read_some(boost::asio::buffer(res), error);
	if(error || n != BLOCK_HASH_LENGTH) goto ConnectToTracker;
	std::string latestHash = Util::array_to_string(res, BLOCK_HASH_LENGTH, 0);

	std::string message, localFile;
	std::fstream localData;

	//Fetch all missing Blocks
	while(latestBlock != latestHash) {

		//Search locally for the Block
		localFile = LOCAL_DATA_BLOCKS+std::to_string(nextBlock)+BLOCK_EXTENSION;
		localData.open(localFile, std::fstream::in);
		if(localData.good()) {
			localData.close();

			//Register Block and increment ID for the next request
			if(SetBlock(localFile)) {
				nextBlock++;
				continue;
			}
			else {
				//delete corrupt local Network Management Block
				boost::filesystem::remove(boost::filesystem::path(localFile));
			}
		}
		else localData.close();

		//Request the next Block to a Tracker
		message = std::to_string(TRACKER_CODE_NMB) + std::to_string(TRACKER_GET_BY_ID) + std::to_string(nextBlock);
		if(socket.is_open() && Network::WriteMessage(socket, NETWORK_TRACKER_GET, message, false)) {
			std::vector<char> length(NETWORK_FILE_LENGTH);

			//read response
			int n = socket.read_some(boost::asio::buffer(length), error);
			if(error || n != NETWORK_FILE_LENGTH) continue;
			uint32_t total_length;
			Util::array_to_int(length, NETWORK_FILE_LENGTH, 0, total_length);

			//check if it is a positive response
			if(!total_length) continue;

			//Create a temporary file to receive the Block
			std::string tempFile = LOCAL_TEMP_DATA + std::to_string(nextBlock) + "tracker" + BLOCK_EXTENSION;
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

			//Register Block and increment ID for the next request
			if(SetBlock(tempFile)) nextBlock++;
		}
		else if(!Network::ConnectToTracker(socket)) {
			//Cannot update
			std::cout << "Network Management Blockchain synchronization process failed: cannot connect to a Data Tracker." << std::endl;
			exit(EXIT_FAILURE);
		}
	}

	//Launch Blocks manager
	this->block = new Block(this, publisher, false, latestBlock, nextBlock-1);
}

Ledger* NetworkManager::SynchronizeLedgers(Balances *balancesDB, TransactionsManager *txManager) {
	//Create Ledgers manager
	ledger = new Ledger(balancesDB, this, txManager, nodes, publisher);

	latestLedger = GENESIS_LEDGER_HASH;
	uint nextId = GENESIS_LEDGER_ID;
	std::string hash;

	rocksdb::Status status = db->Get(rocksdb::ReadOptions(), NMDB_MASK_LEDGER+std::to_string(GENESIS_LEDGER_ID), &hash);
	if(status.IsNotFound()) {

		//Verify Genesis Ledger file, create if it doesn't exist
		std::string genesisFile = LOCAL_DATA_LEDGERS+std::to_string(GENESIS_LEDGER_ID)+LEDGER_EXTENSION;
		std::fstream genesisLedger(genesisFile, std::fstream::in);
		if(!genesisLedger.good()) {
			genesisLedger.close();
			genesisLedger.open(genesisFile, std::fstream::out | std::fstream::trunc);
			genesisLedger << GENESIS_LEDGER;
			genesisLedger.flush();
		}
		genesisLedger.close();

		//Try to execute the Genesis Ledger
		//ledger->SetLedger(LOCAL_DATA_LEDGERS+GENESIS_LEDGER_ID+LEDGER_EXTENSION);

		//Start Ledger chain inside the database
		rocksdb::WriteBatch batch;
		batch.Put(NMDB_MASK_LEDGER+std::to_string(GENESIS_LEDGER_ID), GENESIS_LEDGER_HASH);
		batch.Put(NMDB_MASK_LEDGER+GENESIS_LEDGER_HASH, std::to_string(GENESIS_LEDGER_ID));
		batch.Put(NMDB_MASK_LEDGER+GENESIS_LEDGER_HASH+NMDB_MASK_OPEN, "0");
		batch.Put(NMDB_MASK_LEDGER+GENESIS_LEDGER_HASH+NMDB_MASK_CLOSE, std::to_string(GENESIS_LEDGER_END));
		db->Write(rocksdb::WriteOptions(), &batch);
	}
	else {
		while(!status.IsNotFound()) {
			latestLedger = hash;
			nextId++;
			status = db->Get(rocksdb::ReadOptions(), NMDB_MASK_LEDGER+std::to_string(nextId), &hash);
		}
	}

	//Check if the UDC network has already begun operating
	auto now = Util::current_timestamp();
	if(now < GENESIS_LEDGER_END) {
		//If not, we're synchronized
		latestLedger = GENESIS_LEDGER_HASH;

		//Initialize LedgerStructs of the manager
		ledger->InitializeLedgers(latestLedger, GENESIS_LEDGER_ID);
		return ledger;
	}

	//Begin the synchronization process of the currency
	boost::system::error_code error;
	ConnectToTracker:
		//Try to connect to a Tracker
		boost::asio::ip::tcp::socket socket(NETWORK_SOCKET_SERVICE);
		if(!Network::ConnectToTracker(socket)) {
			//Cannot update
			std::cout << "Currency's Ledgers synchronization process failed: cannot connect to a Data Tracker." << std::endl;
			exit(EXIT_FAILURE);
		}

	//Request latest Ledger hash from a Tracker
	if(!Network::WriteMessage(socket, NETWORK_TRACKER_LATEST, std::to_string(TRACKER_CODE_LEDGER), false)) goto ConnectToTracker;

	//read response
	std::vector<char> res(LEDGER_HASH_LENGTH);
	int n = socket.read_some(boost::asio::buffer(res), error);
	if(error || n != LEDGER_HASH_LENGTH) goto ConnectToTracker;
	std::string latestHash = Util::array_to_string(res, LEDGER_HASH_LENGTH, 0);

	std::string message, localFile;
	std::fstream localData;

	//Fetch all missing Ledgers
	while(latestLedger != latestHash) {
		//Search locally for the Ledger
		localFile = LOCAL_DATA_LEDGERS+std::to_string(nextId)+LEDGER_EXTENSION;
		localData.open(localFile, std::fstream::in);
		if(localData.good()) {
			localData.close();

			//Register Ledger and increment ID for the next request
			if(ledger->SetLedger(localFile)) {
				nextId++;
				continue;
			}
			else {
				//delete corrupt local Ledger
				boost::filesystem::remove(boost::filesystem::path(localFile));
			}
		}
		else localData.close();

		//Request the next Ledger to a Tracker
		message = std::to_string(TRACKER_CODE_LEDGER) + std::to_string(TRACKER_GET_BY_ID) + std::to_string(nextId);
		if(socket.is_open() && Network::WriteMessage(socket, NETWORK_TRACKER_GET, message, false)) {
			std::vector<char> length(NETWORK_FILE_LENGTH);

			//read response
			int n = socket.read_some(boost::asio::buffer(length), error);
			if(error || n != NETWORK_FILE_LENGTH) continue;
			uint32_t total_length;
			Util::array_to_int(length, NETWORK_FILE_LENGTH, 0, total_length);

			//check if it is a positive response
			if(!total_length) continue;

			//Create a temporary file to receive the Ledger
			std::string tempFile = LOCAL_TEMP_DATA + std::to_string(nextId) + "tracker" + LEDGER_EXTENSION;
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

			//Register Ledger and increment ID for the next request
			if(ledger->SetLedger(tempFile)) nextId++;
		}
		else if(!Network::ConnectToTracker(socket)) {
			//Cannot update
			std::cout << "Currency's Ledgers synchronization process failed: cannot connect to a Data Tracker." << std::endl;
			exit(EXIT_FAILURE);
		}
	}

	//Initialize LedgerStructs of the manager
	ledger->InitializeLedgers(latestLedger, nextId-1);
	return ledger;
}

void NetworkManager::CheckSelf(std::string publicKey /*=NULL*/, bool changed /*=false*/) {
	std::string value;
	bool account, version;

	rocksdb::Status status = db->Get(rocksdb::ReadOptions(), _SELF+NMDB_MASK_ACCOUNT, &value);
	if(!status.IsNotFound() && _ACCOUNT != value) {
		db->Put(rocksdb::WriteOptions(), _SELF+NMDB_MASK_ACCOUNT, _ACCOUNT);
		account = true;
	}

	status = db->Get(rocksdb::ReadOptions(), _SELF+NMDB_MASK_VERSION, &value);
	if(!status.IsNotFound() && CURRENT_VERSION != value) {
		db->Put(rocksdb::WriteOptions(), _SELF+NMDB_MASK_VERSION, CURRENT_VERSION);
		version = true;
	}

	if(!changed && !account && !version) return;

	//Create a Network Management Entry of resource "NODE" and type "update"
	std::stringstream contents;
	contents << "{\"resource\":\"" << NMB_RESOURCE_NODE << "\",\"data\":{";
	if(changed) contents << "\"publicKey\":\"" << publicKey << "\"";
	if(version) {
		if(changed) contents << ",";
		contents << "\"networkVersion\":\"" << CURRENT_VERSION << "\"";
	}
	if(account) {
		if(contents || (changed && !contents)) contents << ",";
		contents << "\"account\":\"" << _ACCOUNT << "\"";
	}
	contents << "},\"meta\":{\"type\":\"update\",\"date\":" << Util::current_timestamp();
	contents << ",\"version\":\"" << CURRENT_NMB_VERSION << "\"}";

	//calculate hash and sign Entry's contents
	std::string hash;
	CryptoPP::RIPEMD160 ripemd;
	CryptoPP::StringSource ss(contents.str(), true, new CryptoPP::HashFilter(ripemd, new CryptoPP::HexEncoder(new CryptoPP::StringSink(hash))));
	std::string signature = Crypto::Sign(hash);

	contents << ",\"signer\":\"" << _SELF << "\",\"signature\":\"" << "\"}}";
	std::string entry = "{" + contents.str();

	//Broadcast Entry
	nodes->BroadcastEntry(entry);
	entities->BroadcastEntry(entry);
}

bool NetworkManager::BackupData() {
	//Update Nodes with latest reputations
	std::unordered_map<std::string, int> rep = nodes->GetReputation();
	rocksdb::WriteBatch batch;
	for(auto it = rep.begin(); it != rep.end(); ++it) batch.Put(it->first+NMDB_MASK_REPUTATION, std::to_string(it->second));

	std::lock_guard<std::mutex> lock(dataMutex);
	rocksdb::Status status = db->Write(rocksdb::WriteOptions(), &batch);

	//Create a new database backup
	rocksdb::BackupEngine* backup_engine;
    status = rocksdb::BackupEngine::Open(rocksdb::Env::Default(), rocksdb::BackupableDBOptions(DATABASE_NETWORK_MANAGEMENT_BACKUP), &backup_engine);

    if(status.ok()) {
    	backup_engine->CreateNewBackup(db);
    	delete backup_engine;
    	return true;
    }
    return false;
}

Entities* NetworkManager::GetEntitiesManager() {
	return entities;
}

Nodes* NetworkManager::GetNodesManager() {
	return nodes;
}

void NetworkManager::StartBlockSupervision(bool *IS_OPERATING) {
	bool cleaned = true;
	uint now;

	while(*IS_OPERATING) {
		now = Util::current_timestamp();

		if(now >= block->GetClosingTime() + BLOCK_CLOSING_INTERVAL) {
			//Close current Network Management Block
			if(block->IS_SYNCHRONIZED) {
				block->CloseBlock();
				block->StartConsensus();
				cleaned = false;
			}
		}
		else if(!cleaned && now >= block->GetClosingTime()) {
			//Clean up latest Block consensus data
			block->CleanConsensus();
			cleaned = true;
		}

		std::this_thread::sleep_for(std::chrono::seconds(60));
	}
}

void NetworkManager::StartSlotSupervision(bool *IS_OPERATING) {
	uint now;

	while(*IS_OPERATING) {
		now = Util::current_timestamp();

		//Activate Slots
		dataMutex.lock();
		for(auto itAct = slotsActivation.begin(); itAct != slotsActivation.end() && now >= itAct->first; ++itAct) {
			//Set next managers
			for(auto it = itAct->second.begin(); it != itAct->second.end(); ++it) slotsDB->SetEntity(it->first, it->second);
			slotsActivation.erase(itAct->first);
		}
		dataMutex.unlock();

		//Remove Slots' management
		dataMutex.lock();
		for(auto itExp = slotsExpiration.begin(); itExp != slotsExpiration.end() && now >= itExp->first; ++itExp) {
			//Rollback management to its Supervisor
			for(auto it = itExp->second.begin(); it != itExp->second.end(); ++it) RevertSlotManager(itExp->first, it->first, it->second);
			slotsExpiration.erase(itExp->first);
		}
		dataMutex.unlock();

		//Execute pending transfers
		dataMutex.lock();
		for (auto it = slotsTransfers.begin(); it != slotsTransfers.end() && now >= it->first; ++it) {
			for(SlotTransfer transfer: it->second) ExecuteSlotTransfer(transfer);
			slotsTransfers.erase(it->first);
		}

		std::this_thread::sleep_for(std::chrono::seconds(60));
	}
}

void NetworkManager::RevertSlotManager(uint now, std::string slot, std::string manager) {
	rocksdb::Status status1, status2;
	std::string validity, next;

	rocksdb::WriteBatch batch;

	while(true) {
		//Retrieve validity and supervisor
		status1 = db->Get(rocksdb::ReadOptions(), slot+NMDB_MASK_DELIMITER+manager+NMDB_MASK_VALIDITY, &validity);
		status2 = db->Get(rocksdb::ReadOptions(), slot+NMDB_MASK_DELIMITER+manager+NMDB_MASK_SUPERVISOR, &next);

		//No validity speficied or still within accepted period
		if(status1.IsNotFound() || std::stoul(validity) > now) {
			//Set it as current manager
			slotsDB->SetEntity(slot, manager);
			break;
		}
		//Validity expired and no supervisor found
		else if(status2.IsNotFound()) {
			//Deactivate Slot
			slotsDB->RemoveSlot(slot);
			break;
		}
		//Expired but has a supervisor
		else {
			//Remove unnecessary data
			batch.Delete(slot+NMDB_MASK_DELIMITER+manager+NMDB_MASK_START);
			batch.Delete(slot+NMDB_MASK_DELIMITER+manager+NMDB_MASK_VALIDITY);
			batch.Delete(slot+NMDB_MASK_DELIMITER+manager+NMDB_MASK_SUPERVISOR);
			batch.Delete(slot+NMDB_MASK_DELIMITER+manager+NMDB_MASK_SUPERVISING);
			batch.Delete(slot+NMDB_MASK_DELIMITER+next+NMDB_MASK_SUPERVISING);
			//Check its supervisor
			manager = next;
		}
	}

	//Execute removals 
	db->Write(rocksdb::WriteOptions(), &batch);
}

bool NetworkManager::AddEntry(std::string hash, std::string content, int &errorCode, bool process /*=true*/) {
	Entry *newEntry = Processing::CreateEntry(content, errorCode);

	//Compare hashes to ensure it is correct
	if(hash != newEntry->GetHash()) {
		errorCode = ERROR_HASH;
		delete newEntry;
		return false;
	}

	//Verify if it was already rejected
	auto it = rejectionList.find(newEntry->GetHash());
	if(it != rejectionList.end()) {
		errorCode = ERROR_ENTRY_REJECTED;
		delete newEntry;
		return false;
	}

	//Or is a duplicate
	if(block->HasEntry(newEntry->GetHash())) {
		errorCode = ERROR_ENTRY_DUPLICATE;
		delete newEntry;
		return false;
	}

	//If new Entry
	if(process) {
		//Check its creation date
		if(!newEntry->CheckTimestamp()) {
			errorCode = ERROR_DATE;
			goto Reject;
		}

		//Retrieve public key linked to signer
		std::string publicKey;
		int idType = Processing::GetIdType(newEntry->GetSigner());
		if(!idType || !keysDB->GetPublicKey(newEntry->GetSigner(), publicKey, idType)) {
			errorCode = ERROR_SIGNER;
			goto Reject;
		}

		//Verify signature
		if(!Crypto::Verify(newEntry->GetHash(), newEntry->GetSignature(), publicKey)) {
			errorCode = ERROR_UNAUTHORIZED;
			goto Reject;
		}

		//Check if the Entry's type is supported by the resource
		std::string value;
		rocksdb::Status status = db->Get(rocksdb::ReadOptions(), newEntry->GetResource()+NMDB_MASK_ENTRY_TYPE+newEntry->GetType(), &value);
		if(status.IsNotFound()) {
			errorCode = ERROR_UNSUPPORTED;
			goto Reject;
		}

		//Check if the signer is authorized to submit this entry
		if(!IsAuthorized(newEntry, errorCode)) {
			errorCode = ERROR_UNAUTHORIZED;
			goto Reject;
		}
		goto Execute;
	}
	//Missing Entry
	else {
		//Verify if we actually requested it
		auto it3 = missingList.find(newEntry->GetHash());
		if(it3 != missingList.end()) {
			errorCode = ERROR_UNAUTHORIZED;
			goto Reject;
		}

		missingList.erase(newEntry->GetHash());
		goto Execute;
	}

	Reject:
		rejectionList.insert(newEntry->GetHash());
		delete newEntry;
		return false;

	Execute:
		if(ExecuteEntry(newEntry, process, errorCode)) RegisterEntry(newEntry);

	delete newEntry;
	return (errorCode == VALID);
}

bool NetworkManager::IsAuthorized(Entry *newEntry, int &errorCode) {
	//Verify if signer is the resource's supervisor
	std::string supervisor;
	db->Get(rocksdb::ReadOptions(), newEntry->GetResource()+NMDB_MASK_SUPERVISOR, &supervisor);
	if(newEntry->GetSigner() == supervisor) return true;

	//otherwise, if he has the appropriate privileges
	std::string protection, value;
	rocksdb::Status status = db->Get(rocksdb::ReadOptions(), newEntry->GetResource()+NMDB_MASK_PROTECTION, &protection);
	if(status.IsNotFound()) protection = DEFAULT_RESOURCE_PROTECTION;

	//Private resource, only its supervisor can use it
	if(protection == RESOURCE_PROTECTION_PRIVATE) return false;

	//Slot (restricted) resource
	if(newEntry->GetResource() == NMB_RESOURCE_SLOT) {
		//Create is restricted to supervisor
		if(newEntry->GetType() != ENTRY_TYPE_CREATE) return false;

		//Retrieve the current Managing Entity of the Slot
		std::string slot = dynamic_cast<SlotEntry*>(newEntry)->GetSlot();
		std::string manager;
		if(!slotsDB->GetEntity(slot, manager, false)) return false;
		uint time;

		//The current manager can only update
		if(newEntry->GetSigner() == manager) {
			if(newEntry->GetType() != ENTRY_TYPE_UPDATE) return false;
		}
		//otherwise, check if signer is a supervisor
		else {
			//For cancel and renew entries, signer must be the specified manager's immediate supervisor
			status = db->Get(rocksdb::ReadOptions(), slot+NMDB_MASK_DELIMITER+newEntry->GetSigner()+NMDB_MASK_SUPERVISING, &supervisor);
			if(newEntry->GetType() != ENTRY_TYPE_UPDATE) {
				if(status.IsNotFound() || dynamic_cast<SlotEntry*>(newEntry)->GetManager() != supervisor) return false;
				//Since cancel doesn't have optional fields, it is authorized
				if(newEntry->GetType() != ENTRY_TYPE_CANCEL) return true;

				//For renew type, if start is specified, entry's creation date must be after the specified manager's start date
				if(dynamic_cast<SlotEntry*>(newEntry)->GetStart(time)) {
					status = db->Get(rocksdb::ReadOptions(), slot+NMDB_MASK_DELIMITER+dynamic_cast<SlotEntry*>(newEntry)->GetManager()+NMDB_MASK_START, &value);
					if(!status.IsNotFound() && newEntry->GetTimestamp() > std::stoul(value)) return false;
				}
			}
			//For update entries, check Slot's hierarchy
			else {
				bool isSupervisor = false;
				supervisor = manager;
				status = db->Get(rocksdb::ReadOptions(), slot+NMDB_MASK_DELIMITER+supervisor+NMDB_MASK_SUPERVISOR, &supervisor);
				while(!status.IsNotFound()) {
					if(newEntry->GetSigner() == supervisor) {
						isSupervisor = true;
						break;
					}
					status = db->Get(rocksdb::ReadOptions(), slot+NMDB_MASK_DELIMITER+supervisor+NMDB_MASK_SUPERVISOR, &supervisor);
				}
				//then verify if it is
				if(!isSupervisor) return false;
			}
		}
		//Check remaining optional fields

		//If a start date was specified, check that it respects the signer's own start date
		if(dynamic_cast<SlotEntry*>(newEntry)->GetStart(time)) {
			status = db->Get(rocksdb::ReadOptions(), slot+NMDB_MASK_DELIMITER+newEntry->GetSigner()+NMDB_MASK_START, &value);
			if(!status.IsNotFound() && time < std::stoul(value)) return false;
		}

		//If a validity date was specified, check that it doesn't surpass the signer's privilege
		if(dynamic_cast<SlotEntry*>(newEntry)->GetValidity(time)) {
			status = db->Get(rocksdb::ReadOptions(), slot+NMDB_MASK_DELIMITER+newEntry->GetSigner()+NMDB_MASK_VALIDITY, &value);
			if(!status.IsNotFound() && time > std::stoul(value)) return false;
		}
		return true;
	}

	//Remaining restricted resources, only update is accepted
	if(protection == RESOURCE_PROTECTION_RESTRICTED) return (newEntry->GetType() == ENTRY_TYPE_UPDATE);

	//Account (public) resource, only current manager can submit create entries
	if(newEntry->GetResource() == NMB_RESOURCE_ACCOUNT) {
		std::string entity;
		return (newEntry->GetType() == ENTRY_TYPE_CREATE && slotsDB->GetEntity(dynamic_cast<AccountEntry*>(newEntry)->GetAccount(), entity) && entity == newEntry->GetSigner());
	}

	//Remaining public resources
	std::string id;
	if(newEntry->GetId(id)) {
		status = db->Get(rocksdb::ReadOptions(), newEntry->GetResource()+NMDB_MASK_DELIMITER+id, &value);
		//Existing ID for this resource, ensure it matches the signer
		if(!status.IsNotFound()) return (newEntry->GetSigner() == value);
	}
	//New ID for a public resource
	return true;
}

//if(newEntry) Execute the Entry while checking for inconsistencies
//else Execute the Entry while ignoring inconsistencies as it was already approved by the network
bool NetworkManager::ExecuteEntry(Entry *entry, bool newEntry, int &errorCode) {
	std::string resource = entry->GetResource();

	//Execute according to its resource type
	if(resource == NMB_RESOURCE) ExecuteResourceEntry(dynamic_cast<ResourceEntry*>(entry), newEntry, errorCode);
	else if(resource == NMB_RESOURCE_PASSPORT) ExecutePassportEntry(dynamic_cast<PassportEntry*>(entry), newEntry, errorCode);
	else if(resource == NMB_RESOURCE_ENTITY) ExecuteEntityEntry(dynamic_cast<EntityEntry*>(entry), newEntry, errorCode);
	else if(resource == NMB_RESOURCE_NODE) ExecuteNodeEntry(dynamic_cast<NodeEntry*>(entry), newEntry, errorCode);
	else if(resource == NMB_RESOURCE_DAO) ExecuteDAOEntry(dynamic_cast<DAOEntry*>(entry), newEntry, errorCode);
	else if(resource == NMB_RESOURCE_DAS) ExecuteDASEntry(dynamic_cast<DASEntry*>(entry), newEntry, errorCode);
	else if(resource == NMB_RESOURCE_SLOT) ExecuteSlotEntry(dynamic_cast<SlotEntry*>(entry), newEntry, errorCode);
	else if(resource == NMB_RESOURCE_ACCOUNT) ExecuteAccountEntry(dynamic_cast<AccountEntry*>(entry), newEntry, errorCode);
	else if(entry->GetType() == ENTRY_TYPE_CREATE) {
		///Warning: verify existence of the resource, its protection and supervisor first
		///since it may not have been properly handled by IsAuthorized and will lead to registration of unauthorized NMEs
		std::string id;
		if(entry->GetId(id)) {
			rocksdb::Status status = db->Put(rocksdb::WriteOptions(), entry->GetResource()+NMDB_MASK_DELIMITER+id, id);
			if(status.ok()) errorCode = ERROR_UNSUPPORTED; 
		}
		else errorCode = ERROR_UNSUPPORTED;
	}
	else errorCode = ERROR_UNAUTHORIZED;

	return (errorCode == VALID);
}

bool NetworkManager::ExecuteResourceEntry(ResourceEntry *entry, bool newEntry, int &errorCode) {
	rocksdb::Status status;
	boost::regex valueRegex;
	std::string key = NMB_RESOURCE+NMDB_MASK_DELIMITER+entry->GetDesignation();

	//If cancel, remove resource designation
	if(entry->GetType() == ENTRY_TYPE_CANCEL) db->Delete(rocksdb::WriteOptions(), key);

	else {
		if(entry->GetType() == ENTRY_TYPE_CREATE || entry->GetType() == ENTRY_TYPE_RENEW) {
			//Keep resource designation
			status = db->Put(rocksdb::WriteOptions(), key, entry->GetDesignation());
			if(!status.ok()) {
				errorCode = ERROR_UNSUPPORTED;
				return false;
			}
		}
		//Update values atomically
		rocksdb::WriteBatch batch;
		std::string value;
		if(entry->GetSupervisor(value)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_PASSPORT_ID);
				if(!boost::regex_match(value, valueRegex)) {
					errorCode = ERROR_DATA_CONTENT;
					return false;
				}
			}
			batch.Put(entry->GetDesignation()+NMDB_MASK_SUPERVISOR, value);
		}
		if(entry->GetProtection(value)) {
			if(newEntry && value != RESOURCE_PROTECTION_PRIVATE && value != RESOURCE_PROTECTION_RESTRICTED && value != RESOURCE_PROTECTION_PUBLIC) {
				errorCode = ERROR_DATA_CONTENT;
				return false;
			}
			batch.Put(entry->GetDesignation()+NMDB_MASK_PROTECTION, value);
		}

		std::vector<std::string> list = entry->GetIdentifier();
		if(list.size()) {
			value = "";
			for(auto it = list.begin(); it != list.end(); ++it) value += *it+NMDB_MASK_DELIMITER;
			batch.Put(entry->GetDesignation()+NMDB_MASK_IDENTIFIER, value);
		}

		list = entry->GetOperation();
		if(list.size()) {
			if(list.size() > 4) {
				errorCode = ERROR_DATA_CONTENT;
				return false;
			}
			for(auto it = list.begin(); it != list.end(); ++it) {
				if(newEntry && *it != ENTRY_TYPE_CREATE && *it != ENTRY_TYPE_UPDATE && *it != ENTRY_TYPE_RENEW && *it != ENTRY_TYPE_CANCEL) {
					errorCode = ERROR_DATA_CONTENT;
					return false;
				}
				batch.Put(entry->GetDesignation()+NMDB_MASK_ENTRY_TYPE+*it, *it);
			}
		}

		status = db->Write(rocksdb::WriteOptions(), &batch);
		if(!status.ok()) errorCode = ERROR_DATA_CONTENT;
	}

	return (errorCode == VALID);
}

bool NetworkManager::ExecutePassportEntry(PassportEntry *entry, bool newEntry, int &errorCode) {
	rocksdb::Status status;
	std::string key = NMB_RESOURCE_PASSPORT+NMDB_MASK_DELIMITER+entry->GetPassport();

	//If cancel, remove Passport ID
	if(entry->GetType() == ENTRY_TYPE_CANCEL) db->Delete(rocksdb::WriteOptions(), key);

	else {
		if(entry->GetType() == ENTRY_TYPE_CREATE || entry->GetType() == ENTRY_TYPE_RENEW) {
			//Keep passport ID
			status = db->Put(rocksdb::WriteOptions(), key, entry->GetPassport());
			if(!status.ok()) {
				errorCode = ERROR_UNSUPPORTED;
				return false;
			}
		}

		//Verify account format if specified
		boost::regex accRegex(PATTERN_ACCOUNT);
		std::string account;
		if(entry->GetAccount(account) && !boost::regex_match(account, accRegex)) {
			errorCode = ERROR_DATA_CONTENT;
			return false;
		}

		//Verify if it has a public key to register
		std::string publicKey;
		if(entry->GetPublicKey(publicKey)) {
			if(!keysDB->SetPublicKey(entry->GetPassport(), publicKey, ID_TYPE_PASSPORT)) errorCode = ERROR_DATA_CONTENT;
		}
	}

	return (errorCode == VALID);
}

bool NetworkManager::ExecuteEntityEntry(EntityEntry *entry, bool newEntry, int &errorCode) {
	rocksdb::Status status;
	std::string key = NMB_RESOURCE_ENTITY+NMDB_MASK_DELIMITER+entry->GetEntity();

	//If cancel, remove Entity ID
	if(entry->GetType() == ENTRY_TYPE_CANCEL) {
		db->Delete(rocksdb::WriteOptions(), key);
		numPeers--;
	}
	else if(entry->GetType() == ENTRY_TYPE_CREATE || entry->GetType() == ENTRY_TYPE_RENEW) {
		//Keep Entity ID
		status = db->Put(rocksdb::WriteOptions(), key, entry->GetEntity());
		if(!status.ok()) {
			errorCode = ERROR_UNSUPPORTED;
			return false;
		}

		//Preemptively set possible error code
		errorCode = ERROR_DATA_CONTENT;

		//Update values atomically, while validating content if it's a new entry
		rocksdb::WriteBatch batch;
		boost::regex valueRegex;
		std::string value, host, publicKey;

		//Verify Passport ID
		if(entry->GetPassport(value)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_PASSPORT_ID);
				if(!boost::regex_match(value, valueRegex)) return false;
			}
			batch.Put(entry->GetEntity()+NMDB_MASK_PASSPORT, value);
		}

		//Retrieve host
		if(entry->GetHost(host)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_HOST);
				if(!boost::regex_match(host, valueRegex)) return false;
			}
			batch.Put(entry->GetEntity()+NMDB_MASK_HOST, host);
		}
		else {
			status = db->Get(rocksdb::ReadOptions(), entry->GetEntity()+NMDB_MASK_HOST, &host);
			if(status.IsNotFound()) return false;
		}

		//Verify official account
		if(entry->GetAccount(value)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_ACCOUNT);
				if(!boost::regex_match(value, valueRegex)) return false;
			}
			batch.Put(entry->GetEntity()+NMDB_MASK_ACCOUNT, value);
		}

		//Retrieve public key
		if(entry->GetPublicKey(publicKey) && !keysDB->SetPublicKey(entry->GetEntity(), publicKey, ID_TYPE_ENTITY)) return false;
		else if(!keysDB->GetPublicKey(entry->GetEntity(), publicKey, ID_TYPE_ENTITY, false)) return false;

		//Commit updates
		status = db->Write(rocksdb::WriteOptions(), &batch);
		if(!status.ok()) return false;

		//Add the Managing Entity to its manager
		entities->NewEntity(entry->GetEntity(), publicKey, host);
		numPeers++;
	}
	else { //Entry type update
		rocksdb::WriteBatch batch;
		boost::regex valueRegex;
		std::string value;

		//Preemptively set possible error code
		errorCode = ERROR_DATA_CONTENT;

		if(entry->GetPassport(value)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_PASSPORT_ID);
				if(!boost::regex_match(value, valueRegex)) return false;
			}
			batch.Put(entry->GetEntity()+NMDB_MASK_PASSPORT, value);
		}
		if(entry->GetHost(value)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_HOST);
				if(!boost::regex_match(value, valueRegex)) return false;
			}
			batch.Put(entry->GetEntity()+NMDB_MASK_HOST, value);
			//Update host directly into Entities manager as well
			entities->SetHost(entry->GetEntity(), value, newEntry);
		}
		if(entry->GetAccount(value)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_ACCOUNT);
				if(!boost::regex_match(value, valueRegex)) return false;
			}
			batch.Put(entry->GetEntity()+NMDB_MASK_ACCOUNT, value);
		}

		//Execute updates
		status = db->Write(rocksdb::WriteOptions(), &batch);
		if(!status.ok()) return false;

		//Verify if it has a public key to register
		if(entry->GetPublicKey(value)) {
			if(!keysDB->SetPublicKey(entry->GetEntity(), value, ID_TYPE_ENTITY)) return false;
			entities->SetPublicKey(entry->GetEntity(), value);
		}
	}

	errorCode = VALID;
	return true;
}

bool NetworkManager::ExecuteNodeEntry(NodeEntry *entry, bool newEntry, int &errorCode) {
	rocksdb::Status status;
	rocksdb::WriteBatch batch;
	boost::regex valueRegex;
	std::string value, key = NMB_RESOURCE_NODE+NMDB_MASK_DELIMITER+entry->GetNode();

	//Verify if it's about this Node
	if(entry->GetNode() == _SELF) {
		if(entry->GetType() == ENTRY_TYPE_CREATE) return false;
		if(entry->GetType() == ENTRY_TYPE_CANCEL) threadsManager->ShutdownServices();
		if(entry->GetType() == ENTRY_TYPE_UPDATE || entry->GetType() == ENTRY_TYPE_RENEW) {

			//Retrieve network version it is running
			if(entry->GetNetworkVersion(value)) {
				if(newEntry) {
					valueRegex.assign(PATTERN_NETWORK_VERSION);
					if(!boost::regex_match(value, valueRegex)) return false;
				}
				//Update our listening port
				_PORT = std::stoul(value);
				//relaunch our listening thread
				threadsManager->RestartListeningThread();
			}
			//Retrieve network version it is running
			if(entry->GetNetworkVersion(value)) {
				if(newEntry) {
					valueRegex.assign(PATTERN_NETWORK_VERSION);
					if(!boost::regex_match(value, valueRegex)) return false;
				}
				batch.Put(entry->GetNode()+NMDB_MASK_VERSION, value);
				//Update our version
				CURRENT_VERSION = value;
			}

			//Retrieve our new private account
			if(entry->GetAccount(value)) {
				if(newEntry) {
					valueRegex.assign(PATTERN_ACCOUNT);
					if(!boost::regex_match(value, valueRegex)) return false;
				}
				batch.Put(entry->GetNode()+NMDB_MASK_ACCOUNT, value);
				_ACCOUNT = value;
			}

			//Commit updates
			status = db->Write(rocksdb::WriteOptions(), &batch);
			if(!status.ok()) return false;
		}
	}

	//If cancel, remove Node ID
	else if(entry->GetType() == ENTRY_TYPE_CANCEL) {
		db->Delete(rocksdb::WriteOptions(), key);
		numPeers--;
	}
	else if(entry->GetType() == ENTRY_TYPE_CREATE || entry->GetType() == ENTRY_TYPE_RENEW) {
		//Keep Node ID
		status = db->Put(rocksdb::WriteOptions(), key, entry->GetNode());
		if(!status.ok()) {
			errorCode = ERROR_UNSUPPORTED;
			return false;
		}

		//Preemptively set possible error code
		errorCode = ERROR_DATA_CONTENT;

		//Update values atomically, while validating content if it's a new entry
		std::string publicKey, host, version, account;
		int reputation;

		//Verify Passport ID
		if(entry->GetPassport(value)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_PASSPORT_ID);
				if(!boost::regex_match(value, valueRegex)) return false;
			}
			batch.Put(entry->GetNode()+NMDB_MASK_PASSPORT, value);
		}

		//Retrieve host
		if(entry->GetHost(host)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_HOST);
				if(!boost::regex_match(host, valueRegex)) return false;
			}
			batch.Put(entry->GetNode()+NMDB_MASK_HOST, host);
		}
		else {
			status = db->Get(rocksdb::ReadOptions(), entry->GetNode()+NMDB_MASK_HOST, &host);
			if(status.IsNotFound()) return false;
		}

		//Retrieve network version it is running
		if(entry->GetNetworkVersion(version)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_NETWORK_VERSION);
				if(!boost::regex_match(version, valueRegex)) return false;
			}
			batch.Put(entry->GetNode()+NMDB_MASK_VERSION, version);
		}
		else {
			status = db->Get(rocksdb::ReadOptions(), entry->GetNode()+NMDB_MASK_VERSION, &version);
			if(status.IsNotFound()) version = CURRENT_VERSION;
		}

		//Retrieve official account
		if(entry->GetAccount(account)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_ACCOUNT);
				if(!boost::regex_match(account, valueRegex)) return false;
			}
			batch.Put(entry->GetNode()+NMDB_MASK_ACCOUNT, account);
		}
		else {
			status = db->Get(rocksdb::ReadOptions(), entry->GetNode()+NMDB_MASK_ACCOUNT, &account);
			if(status.IsNotFound()) account = WORLD_BANK_NODE_ACCOUNT;
		}

		//Verify if it has a public key to register
		if(entry->GetPublicKey(value) && !keysDB->SetPublicKey(entry->GetNode(), value, ID_TYPE_NODE)) return false;
		else if(!keysDB->GetPublicKey(entry->GetNode(), publicKey, ID_TYPE_NODE, false)) return false;

		//Retrieve reputation if applicable
		status = db->Get(rocksdb::ReadOptions(), entry->GetNode()+NMDB_MASK_REPUTATION, &value);
		if(status.IsNotFound()) reputation = NODE_REPUTATION_DEFAULT;
		else reputation = std::stol(value);

		//Commit updates
		status = db->Write(rocksdb::WriteOptions(), &batch);
		if(!status.ok()) return false;

		//Add the Validating Node to its manager
		nodes->NewNode(entry->GetNode(), publicKey, host, version, account, reputation);
		numPeers++;
	}
	else {//Entry type update
		//Update values atomically, while validating content if it's a new entry
		boost::regex valueRegex;
		std::string value;

		//Preemptively set possible error code
		errorCode = ERROR_DATA_CONTENT;

		if(entry->GetPassport(value)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_PASSPORT_ID);
				if(!boost::regex_match(value, valueRegex)) return false;
			}
			batch.Put(entry->GetNode()+NMDB_MASK_PASSPORT, value);
		}
		if(entry->GetHost(value)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_HOST);
				if(!boost::regex_match(value, valueRegex)) return false;
			}
			batch.Put(entry->GetNode()+NMDB_MASK_HOST, value);
			//Update host directly into Nodes manager as well
			nodes->SetHost(entry->GetNode(), value, newEntry);
		}
		if(entry->GetNetworkVersion(value)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_NETWORK_VERSION);
				if(!boost::regex_match(value, valueRegex)) return false;
			}
			batch.Put(entry->GetNode()+NMDB_MASK_VERSION, value);
			//Update network version directly into Nodes manager as well
			nodes->SetNetworkVersion(entry->GetNode(), value);
		}
		if(entry->GetAccount(value)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_ACCOUNT);
				if(!boost::regex_match(value, valueRegex)) return false;
			}
			batch.Put(entry->GetNode()+NMDB_MASK_ACCOUNT, value);
			//Update Node's account directly into the Nodes manager as well
			nodes->SetAccount(entry->GetNode(), value);
		}

		//Commit updates
		status = db->Write(rocksdb::WriteOptions(), &batch);
		if(!status.ok()) return false;

		//Verify if it has a public key to register
		if(entry->GetPublicKey(value)) {
			if(!keysDB->SetPublicKey(entry->GetNode(), value, ID_TYPE_NODE)) return false;
			nodes->SetPublicKey(entry->GetNode(), value);
		}
	}

	errorCode = VALID;
	return true;
}

bool NetworkManager::ExecuteDAOEntry(DAOEntry *entry, bool newEntry, int &errorCode) {
	rocksdb::Status status;
	std::string key = NMB_RESOURCE_DAO+NMDB_MASK_DELIMITER+entry->GetDAO();

	//If cancel, remove DAO ID
	if(entry->GetType() == ENTRY_TYPE_CANCEL) db->Delete(rocksdb::WriteOptions(), key);

	else {
		if(entry->GetType() == ENTRY_TYPE_CREATE || entry->GetType() == ENTRY_TYPE_RENEW) {
			//Keep DAO ID
			status = db->Put(rocksdb::WriteOptions(), key, entry->GetDAO());
			if(!status.ok()) {
				errorCode = ERROR_UNSUPPORTED;
				return false;
			}
		}

		//Update values atomically, while validating content if it's a new entry
		rocksdb::WriteBatch batch;
		boost::regex valueRegex;
		std::string value;

		if(entry->GetSupervisor(value)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_PASSPORT_ID);
				if(!boost::regex_match(value, valueRegex)) {
					errorCode = ERROR_DATA_CONTENT;
					return false;
				}
			}
			batch.Put(entry->GetDAO()+NMDB_MASK_SUPERVISOR, value);
			//Update DAO's supervisor directly into the manager
			managerDAO->SetSupervisor(entry->GetDAO(), value);
		}
		if(entry->GetNetworkVersion(value)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_NETWORK_VERSION);
				if(!boost::regex_match(value, valueRegex)) {
					errorCode = ERROR_DATA_CONTENT;
					return false;
				}
			}
			batch.Put(entry->GetDAO()+NMDB_MASK_VERSION, value);
			//Update network version directly into the DAO manager
			managerDAO->SetVersion(entry->GetDAO(), value);
		}
		if(entry->GetAccount(value)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_ACCOUNT);
				if(!boost::regex_match(value, valueRegex)) {
					errorCode = ERROR_DATA_CONTENT;
					return false;
				}
			}
			batch.Put(entry->GetDAO()+NMDB_MASK_ACCOUNT, value);
			//Update the official account directly into the DAO manager
			managerDAO->SetAccount(entry->GetDAO(), value);
		}

		status = db->Write(rocksdb::WriteOptions(), &batch);
		if(!status.ok()) errorCode = ERROR_DATA_CONTENT;
	}

	return (errorCode == VALID);
}

bool NetworkManager::ExecuteDASEntry(DASEntry *entry, bool newEntry, int &errorCode) {
	rocksdb::Status status;
	std::string key = NMB_RESOURCE_DAS+NMDB_MASK_DELIMITER+entry->GetDAS();

	//If cancel, remove DAS ID
	if(entry->GetType() == ENTRY_TYPE_CANCEL) db->Delete(rocksdb::WriteOptions(), key);

	else {
		if(entry->GetType() == ENTRY_TYPE_CREATE || entry->GetType() == ENTRY_TYPE_RENEW) {
			//Keep DAS ID
			status = db->Put(rocksdb::WriteOptions(), key, entry->GetDAS());
			if(!status.ok()) {
				errorCode = ERROR_UNSUPPORTED;
				return false;
			}
		}

		//Update values atomically, while validating content if it's a new entry
		rocksdb::WriteBatch batch;
		boost::regex valueRegex;
		std::string value;

		if(entry->GetManager(value)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_PASSPORT_ID);
				if(!boost::regex_match(value, valueRegex)) {
					errorCode = ERROR_DATA_CONTENT;
					return false;
				}
			}
			batch.Put(entry->GetDAS()+NMDB_MASK_MANAGER, value);
			//Update DAS's manager directly into the manager
			managerDAS->SetManager(entry->GetDAS(), value);
		}
		if(entry->GetNetworkVersion(value)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_NETWORK_VERSION);
				if(!boost::regex_match(value, valueRegex)) {
					errorCode = ERROR_DATA_CONTENT;
					return false;
				}
			}
			batch.Put(entry->GetDAS()+NMDB_MASK_VERSION, value);
			//Update network version directly into the DAS manager
			managerDAS->SetVersion(entry->GetDAS(), value);
		}
		if(entry->GetAccount(value)) {
			if(newEntry) {
				valueRegex.assign(PATTERN_ACCOUNT);
				if(!boost::regex_match(value, valueRegex)) {
					errorCode = ERROR_DATA_CONTENT;
					return false;
				}
			}
			batch.Put(entry->GetDAS()+NMDB_MASK_ACCOUNT, value);
			//Update the official account directly into the DAS manager
			managerDAS->SetAccount(entry->GetDAS(), value);
		}

		status = db->Write(rocksdb::WriteOptions(), &batch);
		if(!status.ok()) errorCode = ERROR_DATA_CONTENT;
	}

	return (errorCode == VALID);
}

bool NetworkManager::ExecuteSlotEntry(SlotEntry *entry, bool newEntry, int &errorCode) {
	rocksdb::Status status;
	rocksdb::WriteBatch batch;
	boost::regex valueRegex;
	std::string manager, value, next;
	uint timestamp, now = Util::current_timestamp(), startDate = 0;

	if(newEntry) {
		//Validate the manager ID specified
		valueRegex.assign(PATTERN_ENTITY_ID);
		if(!boost::regex_match(entry->GetManager(), valueRegex)) {
			errorCode = ERROR_DATA_CONTENT;
			return false;
		}
	}

	//If create, activate Slot
	if(entry->GetType() == ENTRY_TYPE_CREATE) {
		//Set start date
		if(!entry->GetStart(timestamp)) timestamp = entry->GetTimestamp();
		batch.Put(entry->GetSlot()+NMDB_MASK_DELIMITER+entry->GetManager()+NMDB_MASK_START, std::to_string(timestamp));

		//Set activation
		if(now >= timestamp && !slotsDB->SetEntity(entry->GetSlot(), entry->GetManager())) {
			errorCode = ERROR_DATA_CONTENT;
			return false;
		}
		else slotsActivation[timestamp].push_back(std::make_pair(entry->GetSlot(), entry->GetManager()));

		//Set validity
		if(entry->GetValidity(timestamp)) {
			batch.Put(entry->GetSlot()+NMDB_MASK_DELIMITER+entry->GetManager()+NMDB_MASK_VALIDITY, std::to_string(timestamp));
			slotsExpiration[timestamp].push_back(std::make_pair(entry->GetSlot(), entry->GetManager()));
		}
		return true;
	}

	//If cancel, remove all subleases
	else if(entry->GetType() == ENTRY_TYPE_CANCEL) {
		//if signer is not the current one, change it
		if(!slotsDB->GetEntity(entry->GetSlot(), manager, false) && entry->GetSigner() != manager && !slotsDB->SetEntity(entry->GetSlot(), entry->GetSigner())) {
			errorCode = ERROR_DATA_CONTENT;
			return false;
		}

		//Remove subleases
		batch.Delete(entry->GetSlot()+NMDB_MASK_DELIMITER+entry->GetSigner()+NMDB_MASK_SUPERVISING);
		status = db->Get(rocksdb::ReadOptions(), entry->GetSlot()+NMDB_MASK_DELIMITER+entry->GetSigner()+NMDB_MASK_SUPERVISING, &next);
		while(!status.IsNotFound()) {
			//delete data
			batch.Delete(entry->GetSlot()+NMDB_MASK_DELIMITER+next+NMDB_MASK_START);
			batch.Delete(entry->GetSlot()+NMDB_MASK_DELIMITER+next+NMDB_MASK_VALIDITY);
			batch.Delete(entry->GetSlot()+NMDB_MASK_DELIMITER+next+NMDB_MASK_SUPERVISOR);
			batch.Delete(entry->GetSlot()+NMDB_MASK_DELIMITER+next+NMDB_MASK_SUPERVISING);

			//Retrieve start date and remove from activation list
			status = db->Get(rocksdb::ReadOptions(), entry->GetSlot()+NMDB_MASK_DELIMITER+next+NMDB_MASK_START, &value);
			if(!status.IsNotFound() && std::stoul(value) >= now) RemoveSlotUpdate(std::stoul(value), entry->GetSlot(), entry->GetManager());

			//Retrieve validity and remove from deactivation list
			status = db->Get(rocksdb::ReadOptions(), entry->GetSlot()+NMDB_MASK_DELIMITER+next+NMDB_MASK_VALIDITY, &value);
			if(!status.IsNotFound()) RemoveSlotUpdate(std::stoul(value), entry->GetSlot(), entry->GetManager(), false);

			//fetch next entity
			status = db->Get(rocksdb::ReadOptions(), entry->GetSlot()+NMDB_MASK_DELIMITER+next+NMDB_MASK_SUPERVISING, &next);
		}
	}

	//If renew, enact updates to current manager
	else if(entry->GetType() == ENTRY_TYPE_RENEW) {

		//Update validity
		if(entry->GetValidity(timestamp)) {
			//Retrieve current validity
			status = db->Get(rocksdb::ReadOptions(), entry->GetSlot()+NMDB_MASK_DELIMITER+entry->GetManager()+NMDB_MASK_VALIDITY, &value);
			//remove from deactivation list
			if(!status.IsNotFound()) RemoveSlotUpdate(std::stoul(value), entry->GetSlot(), entry->GetManager(), false);

			//set new validity
			batch.Put(entry->GetSlot()+NMDB_MASK_DELIMITER+entry->GetManager()+NMDB_MASK_VALIDITY, std::to_string(timestamp));
			slotsExpiration[timestamp].push_back(std::make_pair(entry->GetSlot(), entry->GetManager()));
		}
		else {
			//retrieve manager's current start date
			status = db->Get(rocksdb::ReadOptions(), entry->GetSlot()+NMDB_MASK_DELIMITER+entry->GetManager()+NMDB_MASK_START, &value);
			if(!status.IsNotFound()) startDate = std::stoul(value);

			//Update start date
			if(entry->GetStart(timestamp)) {
				//remove current one, if found above
				if(startDate > 0) RemoveSlotUpdate(startDate, entry->GetSlot(), entry->GetManager());

				//set new start date
				batch.Put(entry->GetSlot()+NMDB_MASK_DELIMITER+entry->GetManager()+NMDB_MASK_START, std::to_string(timestamp));
				//and activation
				if(now >= timestamp && !slotsDB->SetEntity(entry->GetSlot(), entry->GetManager())) {
					errorCode = ERROR_DATA_CONTENT;
					return false;
				}
				else slotsActivation[timestamp].push_back(std::make_pair(entry->GetSlot(), entry->GetManager()));
			}
			else timestamp = entry->GetTimestamp();

			//Process transfer
			if(entry->IsTransfer()) {
				SlotTransfer newTransfer;
				newTransfer.slot = entry->GetSlot();
				newTransfer.from = entry->GetSigner();
				newTransfer.to = entry->GetManager();

				//Transfer is immediate or become effective in the meantime
				if(now >= timestamp) ExecuteSlotTransfer(newTransfer);
				//otheriwse, keep transfer for later
				else slotsTransfers[timestamp].push_back(newTransfer);
			}
			//or cancel any pending transfer
			else RemoveSlotTransfer(startDate, entry->GetSlot());
		}
	}

	//Update type
	else {
		//Remove subleases
		batch.Delete(entry->GetSlot()+NMDB_MASK_DELIMITER+entry->GetSigner()+NMDB_MASK_SUPERVISING);
		status = db->Get(rocksdb::ReadOptions(), entry->GetSlot()+NMDB_MASK_DELIMITER+entry->GetSigner()+NMDB_MASK_SUPERVISING, &next);
		while(!status.IsNotFound()) {
			//delete data
			batch.Delete(entry->GetSlot()+NMDB_MASK_DELIMITER+next+NMDB_MASK_START);
			batch.Delete(entry->GetSlot()+NMDB_MASK_DELIMITER+next+NMDB_MASK_VALIDITY);
			batch.Delete(entry->GetSlot()+NMDB_MASK_DELIMITER+next+NMDB_MASK_SUPERVISOR);
			batch.Delete(entry->GetSlot()+NMDB_MASK_DELIMITER+next+NMDB_MASK_SUPERVISING);

			//Retrieve start date and remove from activation list
			status = db->Get(rocksdb::ReadOptions(), entry->GetSlot()+NMDB_MASK_DELIMITER+next+NMDB_MASK_START, &value);
			if(!status.IsNotFound() && std::stoul(value) >= now) RemoveSlotUpdate(std::stoul(value), entry->GetSlot(), entry->GetManager());

			//Retrieve validity and remove from deactivation list
			status = db->Get(rocksdb::ReadOptions(), entry->GetSlot()+NMDB_MASK_DELIMITER+next+NMDB_MASK_VALIDITY, &value);
			if(!status.IsNotFound()) RemoveSlotUpdate(std::stoul(value), entry->GetSlot(), entry->GetManager(), false);

			//fetch next entity
			status = db->Get(rocksdb::ReadOptions(), entry->GetSlot()+NMDB_MASK_DELIMITER+next+NMDB_MASK_SUPERVISING, &next);
		}

		//Update link
		batch.Put(entry->GetSlot()+NMDB_MASK_DELIMITER+entry->GetSigner()+NMDB_MASK_SUPERVISING, entry->GetManager());
		
		//Retrieve effective date
		if(!entry->GetStart(timestamp)) timestamp = entry->GetTimestamp();
		batch.Put(entry->GetSlot()+NMDB_MASK_DELIMITER+entry->GetSigner()+NMDB_MASK_START, std::to_string(timestamp));

		//Check date
		if(now >= timestamp) {
			//Set Slot's manager to the specified Managing Entity
			if(!slotsDB->SetEntity(entry->GetSlot(), entry->GetManager())) {
				errorCode = ERROR_DATA_CONTENT;
				return false;
			}
		}
		//otherwise set an activation date
		else slotsActivation[timestamp].push_back(std::make_pair(entry->GetSlot(), entry->GetManager()));

		//Set validity, if speficied
		if(entry->GetValidity(timestamp)) {
			slotsExpiration[timestamp].push_back(std::make_pair(entry->GetSlot(), entry->GetSigner()));
			batch.Put(entry->GetSlot()+NMDB_MASK_DELIMITER+entry->GetSigner()+NMDB_MASK_VALIDITY, std::to_string(timestamp));
		}
		else {
			//or retrieve from signer's
			status = db->Get(rocksdb::ReadOptions(), entry->GetSlot()+NMDB_MASK_DELIMITER+entry->GetSigner()+NMDB_MASK_SUPERVISING, &value);
			if(!status.IsNotFound()) {
				slotsExpiration[std::stoul(value)].push_back(std::make_pair(entry->GetSlot(), entry->GetSigner()));
				batch.Put(entry->GetSlot()+NMDB_MASK_DELIMITER+entry->GetSigner()+NMDB_MASK_VALIDITY, value);
			}
		}
	}

	//Execute changes
	status = db->Write(rocksdb::WriteOptions(), &batch);
	if(!status.ok()) errorCode = ERROR_DATA_CONTENT;
	return (errorCode == VALID);
}

bool NetworkManager::RemoveSlotUpdate(uint timestamp, std::string slot, std::string entity, bool activation/*=true*/) {
	std::map<uint, std::vector<std::pair<std::string, std::string>>> *selection;
	if(activation) selection = &slotsActivation;
	else selection = &slotsExpiration;

	auto it = selection->find(timestamp);
	//remove from selected list
	if(it != selection->end()) {
		int i = 0;
		for(auto it2 = it->second.begin(); it2 != it->second.end(); ++it2, i++) {
			if(it2->first == slot && it2->second == entity) {
				(*selection)[it->first].erase(it->second.begin()+i);
				return true;
			}
		}
	}
	return false;
}

bool NetworkManager::ExecuteSlotTransfer(SlotTransfer transfer) {
	rocksdb::Status status;
	rocksdb::WriteBatch batch;
	std::string value;
	
	//Set manager's start date equal to the signer's
	status = db->Get(rocksdb::ReadOptions(), transfer.slot+NMDB_MASK_DELIMITER+transfer.from+NMDB_MASK_START, &value);
	if(status.IsNotFound()) return false;
	batch.Put(transfer.slot+NMDB_MASK_DELIMITER+transfer.to+NMDB_MASK_START, value);

	//Set manager's validity equal to the signer's 
	status = db->Get(rocksdb::ReadOptions(), transfer.slot+NMDB_MASK_DELIMITER+transfer.from+NMDB_MASK_VALIDITY, &value);
	if(!status.IsNotFound()) batch.Put(transfer.slot+NMDB_MASK_DELIMITER+transfer.to+NMDB_MASK_VALIDITY, value);

	status = db->Get(rocksdb::ReadOptions(), transfer.slot+NMDB_MASK_DELIMITER+transfer.from+NMDB_MASK_SUPERVISOR, &value);
	if(!status.IsNotFound()) {
		//Keep signer's supervisor as the manager's supervisor
		batch.Put(transfer.slot+NMDB_MASK_DELIMITER+transfer.to+NMDB_MASK_SUPERVISOR, value);
		//Link the manager with this supervisor
		batch.Put(transfer.slot+NMDB_MASK_DELIMITER+value+NMDB_MASK_SUPERVISOR, transfer.to);
	}

	//retrieve Slot's current manager
	std::string manager;
	if(!slotsDB->GetEntity(transfer.slot, manager, false)) return false;
	//if signer is the current manager, change it
	if(transfer.from == manager && !slotsDB->SetEntity(transfer.slot, transfer.to)) return false;

	//Remove signer's data
	batch.Delete(transfer.slot+NMDB_MASK_DELIMITER+transfer.from+NMDB_MASK_START);
	batch.Delete(transfer.slot+NMDB_MASK_DELIMITER+transfer.from+NMDB_MASK_VALIDITY);
	batch.Delete(transfer.slot+NMDB_MASK_DELIMITER+transfer.from+NMDB_MASK_SUPERVISOR);
	batch.Delete(transfer.slot+NMDB_MASK_DELIMITER+transfer.from+NMDB_MASK_SUPERVISING);

	//Execute changes
	status = db->Write(rocksdb::WriteOptions(), &batch);
	return status.ok();
}

bool NetworkManager::RemoveSlotTransfer(uint timestamp, std::string slot) {
	auto it = slotsTransfers.find(timestamp);
	if(it != slotsTransfers.end()) {
		int i = 0;
		for(auto it2 = it->second.begin(); it2 != it->second.end(); ++it2, i++) {
			if(it2->slot == slot) {
				slotsTransfers[it->first].erase(it->second.begin()+i);
				return true;
			}
		}
	}
	return false;
}

bool NetworkManager::ExecuteAccountEntry(AccountEntry *entry, bool newEntry, int &errorCode) {
	if(entry->GetType() == ENTRY_TYPE_CREATE) {
		if(newEntry) {
			std::string value;
			if(keysDB->GetPublicKey(entry->GetAccount(), value, ID_TYPE_ACCOUNT, false)) {
				//Account is already activated
				errorCode = ERROR_UNAUTHORIZED;
				return false;
			}
		}
		if(keysDB->SetPublicKey(entry->GetAccount(), entry->GetPublicKey())) return true;
		//Invalid public key
		errorCode = ERROR_DATA_CONTENT;
	}
	else errorCode = ERROR_UNSUPPORTED;

	return false;
}

bool NetworkManager::RegisterEntry(Entry *entry) {
	return block->NewEntry(entry);
}

void NetworkManager::AddConfirmation(std::string hash, std::string node) {
	block->AddConfirmation(hash, node);
}

void NetworkManager::BroadcastConfirmation(std::string hash) {
	//Broadcast confirmation to Validating Nodes
	nodes->BroadcastConfirmation(hash, CONFIRMATION_BLOCK);

	//Broadcast confirmation to Managing Entities
	entities->BroadcastConfirmation(hash);
}

int NetworkManager::ConfirmationThreshold() {
	return (numPeers * BLOCK_CONFIRMATION_RATIO) + 1;
}

bool NetworkManager::RequestBlock(std::vector<std::string> peers, std::string hash, bool newRequest /*=true*/) {
	if(newRequest) missingBlocks[hash] = 0;

	int errorCode = 0;
	bool sent = false;
	std::string peer;

	while(!sent && peers.size()) {
		peer = peers.back();

		if(peer >= FIRST_NODE_ID) {
			//Validating Node
			boost::asio::ip::tcp::socket *socket = nodes->WriteToNode(peer, errorCode);
			if(!errorCode && Network::WriteMessage(*socket, NETWORK_GET_BLOCK, hash, true)) sent = true;
		}
		//Managing Entity
		else if(entities->RequestBlock(peer, hash)) sent = true;

		peers.pop_back();
		errorCode = 0;
	}
	return sent;
}

void NetworkManager::NewBlock(uint id, std::string hash) {
	std::string value;
	rocksdb::Status status = db->Get(rocksdb::ReadOptions(), NMDB_MASK_BLOCK+hash, &value);
	rocksdb::Status status2 = db->Get(rocksdb::ReadOptions(), NMDB_MASK_BLOCK+std::to_string(id), &value);

	//Ensure no other block with that hash has already been submitted
	if(status.IsNotFound()) {
		rocksdb::WriteBatch batch;
		status = db->Get(rocksdb::ReadOptions(), NMDB_MASK_BLOCK+std::to_string((id-1)), &value);
		//correctly link the Block it with the rest of the chain
		if(!status.IsNotFound()) {
			batch.Put(NMDB_MASK_BLOCK+value+NMDB_MASK_NEXT, hash);
			batch.Put(NMDB_MASK_BLOCK+hash+NMDB_MASK_PREVIOUS, value);
		}

		//Check if it's the latest closed Block
		status = db->Get(rocksdb::ReadOptions(), NMDB_MASK_BLOCK+hash+NMDB_MASK_NEXT, &value);
		if(status.IsNotFound() || status2.IsNotFound()) latestBlock = hash;

		batch.Put(NMDB_MASK_BLOCK+std::to_string(id), hash);
		batch.Put(NMDB_MASK_BLOCK+hash, std::to_string(id));

		uint closing = GENESIS_BLOCK_END + (id * BLOCK_DURATION);
		uint opening = closing - BLOCK_DURATION + 1;
		batch.Put(NMDB_MASK_BLOCK+hash+NMDB_MASK_CLOSE, std::to_string(closing));
		batch.Put(NMDB_MASK_BLOCK+hash+NMDB_MASK_OPEN, std::to_string(opening));

		//Comming updates and persist to local index file
		db->Write(rocksdb::WriteOptions(), &batch);
		blocksIndex << id << " " << hash << std::endl;
	}
}

bool NetworkManager::GetBlock(std::string hash, std::string &blockFile) {
	std::string id;
	rocksdb::Status status = db->Get(rocksdb::ReadOptions(), NMDB_MASK_BLOCK+hash, &id);

	if(status.IsNotFound()) return false;
	blockFile = LOCAL_DATA_BLOCKS + id + BLOCK_EXTENSION;
	return true;
}

bool NetworkManager::SetBlock(std::string blockFile) {
	std::fstream in(blockFile, std::fstream::in);

	if(!in.good()) {
		auto it = missingBlocks.find(latestBlock);
		if(it != missingBlocks.end()) FailedToFetch();
		return false;
	}

	std::set<std::string> entriesList;
	std::string value, hash, blockHash, previousHash, entriesRoot;
	uint id;
	char c;
	std::vector<char> buffer;
	buffer.resize(STANDARD_HASH_LENGTH);

	//Retrieve Block ID
	in.seekg(11, std::ios_base::beg);
	in.get(c);
	while(c != ',') {
		value += c;
		in.get(c);
	}
	id = std::stoul(value);

	//Retrieve the Block hash
	in.seekg(14, std::ios_base::cur);
	in.read(&buffer.front(), BLOCK_HASH_LENGTH);
	blockHash = Util::array_to_string(buffer, BLOCK_HASH_LENGTH, 0);

	//Verify that we requested this Block
	auto it2 = missingBlocks.find(blockHash);
	if(block->IS_SYNCHRONIZED && it2 == missingBlocks.end()) return false;

	//Retrieve previous Block hash
	in.seekg(23, std::ios_base::cur);
	in.read(&buffer.front(), BLOCK_HASH_LENGTH);
	previousHash = Util::array_to_string(buffer, BLOCK_HASH_LENGTH, 0);

	//Retrieve the Entries Merkle tree root
	in.seekg(35, std::ios_base::cur);
	in.read(&buffer.front(), STANDARD_HASH_LENGTH);
	entriesRoot = Util::array_to_string(buffer, STANDARD_HASH_LENGTH, 0);

	//Compute Block hash and compare it with the one stated
	CryptoPP::RIPEMD160 ripemd;
	CryptoPP::StringSource ss(std::to_string(id)+previousHash+entriesRoot, true, new CryptoPP::HashFilter(ripemd, new CryptoPP::HexEncoder(new CryptoPP::StringSink(hash))));
	if(blockHash != hash) {
		FailedToFetch(blockHash);
		return false;
	}

	//Retrieve all Entries hashes
	in.seekg(10, std::ios_base::cur);
	int count;
	while(c != '}') {
		//skip opening quotation marks
		in.seekg(1, std::ios_base::cur);
		//get hash
		in.read(&buffer.front(), ENTRY_HASH_LENGTH);
		value = Util::array_to_string(buffer, ENTRY_HASH_LENGTH, 0);
		entriesList.insert(value);

		//skip its contents
		in.seekg(3, std::ios_base::cur);
		count = 1;
		bool outside = true;
		while(count > 0) {
			in.get(c);
			//ensure curly brackets present inside strings are ignored
			if(c == '"') outside = !outside;
			if(outside) {
				if(c == '{') count++;
				else if(c == '}') count--;
			}
		}

		//read separation comma or data object's end bracket
		in.get(c);
	}
	in.close();

	//Compute the Entries Merkle tree root and compare it with the one stated
	if(entriesRoot != Crypto::MerkleRoot(entriesList)) {
		FailedToFetch(blockHash);
		return false;
	}

	//Save Block
	std::string correctFile = LOCAL_DATA_BLOCKS+std::to_string(id)+BLOCK_EXTENSION;
	boost::filesystem::rename(boost::filesystem::path(blockFile), boost::filesystem::path(correctFile));
	NewBlock(id, blockHash);

	//Execute Network Management Block
	ExecuteBlock(correctFile, blockHash == latestBlock);
	return true;
}

void NetworkManager::ExecuteBlock(std::string newBlock, bool latest) {
	Entry *entry;
	std::string content, hash;
	char c;
	std::vector<char> buffer;
	buffer.resize(STANDARD_HASH_LENGTH);

	//Retrieve Entries already executed
	std::set<std::string> oldEntries;
	if(block->IS_SYNCHRONIZED && latest) oldEntries = block->GetLatestEntries();

	std::fstream in(newBlock, std::fstream::in);
	//forward to Entries
	in.seekg(200, std::ios_base::beg);
	while(c != '{') in.get(c);

	//Read and execute all missing Entries
	int count, errorCode;
	bool outside;
	while(c != '}') {
		//skip opening quotation marks
		in.seekg(1, std::ios_base::cur);
		//get hash
		in.read(&buffer.front(), ENTRY_HASH_LENGTH);
		hash = Util::array_to_string(buffer, ENTRY_HASH_LENGTH, 0);
		in.seekg(3, std::ios_base::cur);
		
		count = 1;
		outside = true;

		//Verify if it's missing
		auto it = oldEntries.find(hash);
		if(it != oldEntries.end()) {
			//skip its contents
			while(count > 0) {
				in.get(c);
				//ensure curly brackets inside strings are ignored
				if(c == '"') outside = !outside;
				if(outside) {
					if(c == '{') count++;
					else if(c == '}') count--;
				}
			}
		}
		else {
			//extract content
			content = "";
			errorCode = VALID;
			while(count > 0) {
				in.get(c);
				//ensure curly brackets inside strings are ignored
				if(c == '"') outside = !outside;
				if(outside) {
					if(c == '{') count++;
					else if(c == '}') count--;
				}
				content += c;
			}

			//Execute Entry if it is valid
			entry = Processing::CreateEntry(content, errorCode);
			if(errorCode == VALID && hash == entry->GetHash()) ExecuteEntry(entry, false, errorCode);

			delete entry;
		}

		//read separation comma or transactions array's end bracket
		in.get(c);
	}
	in.close();
}

void NetworkManager::FailedToFetch(std::string hash /*=""*/) {
	if(hash.length() == 0) hash = latestBlock;
	
	missingBlocks[hash]++;

	if(missingBlocks[hash] > MAX_DATA_REQUESTS) {
		std::string message = std::to_string(TRACKER_CODE_NMB) + std::to_string(TRACKER_GET_BY_HASH) + hash;

		//Request a Tracker for the Block
		boost::asio::ip::tcp::socket socket(NETWORK_SOCKET_SERVICE);
		if(Network::ConnectToTracker(socket) && Network::WriteMessage(socket, NETWORK_TRACKER_GET, message, false)) {
			boost::system::error_code error;
			std::vector<char> length(NETWORK_FILE_LENGTH);

			//read response
			int n = socket.read_some(boost::asio::buffer(length), error);
			if(error || n != NETWORK_FILE_LENGTH) goto RequestPeer;
			uint32_t total_length;
			Util::array_to_int(length, NETWORK_FILE_LENGTH, 0, total_length);

			//check if it is a positive response
			if(!total_length) goto RequestPeer;

			//Create a temporary file to receive the Block
			std::string tempFile = LOCAL_TEMP_DATA + hash + "tracker" + BLOCK_EXTENSION;
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

			//Register Block
			SetBlock(tempFile);
			return;
		}
	}

	RequestPeer:
		bool sent = false;
		//If it's the latest closed Block, ask one of the peers that confirmed it
		std::vector<std::string> peers;
		if(block->GetCorrectNodes(hash, peers) && RequestBlock(peers, hash, false)) sent = true;
		
		//Otherwise ask a random Node
		while(!sent) {
			boost::asio::ip::tcp::socket *socket = nodes->WriteToRandomNode();
			if(Network::WriteMessage(*socket, NETWORK_GET_BLOCK, hash, true)) sent = true;
		}
}

void NetworkManager::NewLedger(uint id, std::string hash) {
	std::string value;
	rocksdb::Status status = db->Get(rocksdb::ReadOptions(), NMDB_MASK_LEDGER+hash, &value);
	rocksdb::Status status2 = db->Get(rocksdb::ReadOptions(), NMDB_MASK_LEDGER+std::to_string(id), &value);

	//Ensure no other Ledger with that hash has already been submitted
	if(status.IsNotFound()) {
		rocksdb::WriteBatch batch;
		status = db->Get(rocksdb::ReadOptions(), NMDB_MASK_LEDGER+std::to_string((id-1)), &value);
		//correctly link the Ledger with the rest of the chain
		if(!status.IsNotFound()) {
			batch.Put(NMDB_MASK_LEDGER+value+NMDB_MASK_NEXT, hash);
			batch.Put(NMDB_MASK_LEDGER+hash+NMDB_MASK_PREVIOUS, value);
		}

		//Check if it's the latest closed Ledger
		status = db->Get(rocksdb::ReadOptions(), NMDB_MASK_LEDGER+hash+NMDB_MASK_NEXT, &value);
		if(status.IsNotFound() || status2.IsNotFound()) latestLedger = hash;

		batch.Put(NMDB_MASK_LEDGER+std::to_string(id), hash);
		batch.Put(NMDB_MASK_LEDGER+hash, std::to_string(id));

		uint closing = GENESIS_LEDGER_END + (id * LEDGER_DURATION);
		uint opening = closing - LEDGER_DURATION + 1;
		batch.Put(NMDB_MASK_LEDGER+hash+NMDB_MASK_CLOSE, std::to_string(closing));
		batch.Put(NMDB_MASK_LEDGER+hash+NMDB_MASK_OPEN, std::to_string(opening));

		//Comming updates and persist to local index file
		db->Write(rocksdb::WriteOptions(), &batch);
		ledgersIndex << id << " " << hash << std::endl;
	}
}

bool NetworkManager::GetLedgerId(std::string hash, uint &id) {
	std::string value;
	rocksdb::Status status = db->Get(rocksdb::ReadOptions(), NMDB_MASK_LEDGER+hash, &value);
	if(status.IsNotFound()) return false;

	id = std::stoul(value);
	return true; 
}

uint64_t NetworkManager::GetClosingTime() {
	return block->GetClosingTime();
}

bool NetworkManager::IsSynchronized() {
	return block->IS_SYNCHRONIZED;
}

uint64_t NetworkManager::ExpectedSynchronizationEnd() {
	return block->ExpectedSynchronizationEnd();
}