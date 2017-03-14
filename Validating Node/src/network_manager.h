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
 * @file network_manager.h
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <unordered_set>
#include <utility>

#include "includes/rocksdb/db.h"

#include "ledger.h"
#include "threads_manager.h"

class AccountEntry;
class Balances;
class Block;
class DAOEntry;
class DAOManager;
class DASEntry;
class DASManager;
class Entities;
class EntityEntry;
class Entry;
class Keys;
class NodeEntry;
class Nodes;
class Publisher;
class PassportEntry;
class ResourceEntry;
class SlotEntry;
class Slots;
class TransactionsManager;

struct EntityStruct;
struct NodeStruct;

struct SlotTransfer {
	std::string slot;
	std::string from;
	std::string to;
};


class NetworkManager {

	public:
		NetworkManager(rocksdb::DB *db, DAOManager *managerDAO, DASManager *managerDAS, Keys *keysDB, Publisher *publisher, Slots *slotsDB, ThreadsManager *threadsManager);

		void SynchronizeNMB();
		Ledger* SynchronizeLedgers(Balances *balancesDB, TransactionsManager *txManager);
		void CheckSelf(std::string publicKey=NULL, bool changed=false);
		bool BackupData();

		Entities* GetEntitiesManager();
		Nodes* GetNodesManager();

		void StartBlockSupervision(bool *IS_OPERATING);
		void StartSlotSupervision(bool *IS_OPERATING);
		
		bool AddEntry(std::string hash, std::string content, int &errorCode, bool process=true);
		void AddConfirmation(std::string hash, std::string node);
		void BroadcastConfirmation(std::string hash);
		int ConfirmationThreshold();

		bool RequestBlock(std::vector<std::string> peers, std::string hash, bool newRequest=true);
		void NewBlock(uint id, std::string hash);
		bool GetBlock(std::string hash, std::string &blockFile);
		bool SetBlock(std::string blockFile);
		void FailedToFetch(std::string hash="");

		void NewLedger(uint id, std::string hash);
		bool GetLedgerId(std::string hash, uint &id);

		uint64_t GetClosingTime();
		bool IsSynchronized();
		uint64_t ExpectedSynchronizationEnd();

	private:
		bool GetEntityData(std::string id, EntityStruct &entity);
		bool GetNodeData(std::string id, NodeStruct &node);

		bool IsAuthorized(Entry *newEntry, int &errorCode);
		bool RegisterEntry(Entry *entry);
		void ExecuteBlock(std::string newBlock, bool latest);

		bool ExecuteEntry(Entry *entry, bool newEntry, int &errorCode);
		bool ExecuteResourceEntry(ResourceEntry *entry, bool newEntry, int &errorCode);
		bool ExecutePassportEntry(PassportEntry *entry, bool newEntry, int &errorCode);
		bool ExecuteEntityEntry(EntityEntry *entry, bool newEntry, int &errorCode);
		bool ExecuteNodeEntry(NodeEntry *entry, bool newEntry, int &errorCode);
		bool ExecuteDAOEntry(DAOEntry *entry, bool newEntry, int &errorCode);
		bool ExecuteDASEntry(DASEntry *entry, bool newEntry, int &errorCode);
		bool ExecuteSlotEntry(SlotEntry *entry, bool newEntry, int &errorCode);
		bool ExecuteAccountEntry(AccountEntry *entry, bool newEntry, int &errorCode);

		void RevertSlotManager(uint now, std::string slot, std::string manager);
		bool RemoveSlotUpdate(uint timestamp, std::string slot, std::string entity, bool activation=true);
		bool ExecuteSlotTransfer(SlotTransfer transfer);
		bool RemoveSlotTransfer(uint timestamp, std::string slot);

		std::mutex dataMutex;

		rocksdb::DB *db;
		Block *block;
		DAOManager *managerDAO;
		DASManager *managerDAS;
		Entities *entities;
		Ledger *ledger;
		Keys *keysDB;
		Nodes *nodes;
		Publisher *publisher;
		Slots *slotsDB;
		ThreadsManager *threadsManager;
		uint numPeers;

		std::fstream blocksIndex;
		std::fstream ledgersIndex;
		std::fstream entitiesIndex;
		std::fstream nodesIndex;

		std::string latestBlock;
		std::string latestLedger;

		std::unordered_set<std::string> rejectionList;
		std::unordered_set<std::string> missingList;
		std::unordered_map<std::string, int> missingBlocks;

		std::map<uint, std::vector<std::pair<std::string, std::string>>> slotsActivation;
		std::map<uint, std::vector<std::pair<std::string, std::string>>> slotsExpiration;
		std::map<uint, std::vector<SlotTransfer>> slotsTransfers;
};


#endif