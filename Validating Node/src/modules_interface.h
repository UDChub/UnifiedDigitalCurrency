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
 * @file modules_interface.h
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#ifndef MODULES_INTERFACE_H
#define MODULES_INTERFACE_H

#include <string>
#include <unordered_map>

#include "codes.h"

class AuthorizeFutureTransaction;
class Balances;
class DAOManager;
class DAOModule;
class DAOTransaction;
class DASManager;
class DASModule;
class DASTransaction;
class Entities;
class Keys;
class Ledger;
class NetworkManager;
class Nodes;
class RequestDelayedTransaction;
class Transaction;
class TransactionsManager;
class Slots;


class ModulesInterface {

	public:
		///Only add what is necessary
		ModulesInterface(Balances *balancesDB, DAOManager *managerDAO, DASManager *managerDAS, Entities *entities, Keys *keysDB, NetworkManager *networkManager, Nodes *nodes, TransactionsManager *txManager, Slots *slotsDB);
		void SetReferences(Ledger *ledger);

		//Balances Database
		uint64_t GetBalance(std::string account);

		//DAOManager
		bool AddModuleTransaction(DAOTransaction *transaction, int &errorCode);
		bool GetModule(std::string DAO, DAOModule *&module);

		//DASManager
		bool AddModuleTransaction(DASTransaction *transaction, int &errorCode);
		bool GetModule(std::string DAS, DASModule *&module);

		//Entities

		//Keys Database
		bool GetPublicKey(std::string id, std::string &publicKey, int idType=ID_TYPE_ACCOUNT, bool request=true);
		bool GetManagingEntityKey(std::string account, std::string &key);

		//Ledger
		bool GetLedger(std::string hash, std::string &ledgerFile);
		uint64_t GetEstimatedAmountInCirculation();
		uint64_t GetEstimatedVolume(bool current=true);
		uint64_t GetEstimatedFeesCollected(bool current=true);
		uint64_t GetClosingTime(bool current=true);
		std::string LastClosedLedgerHash();
		uint64_t LastClosedLedgerId();

		//Network Management Blockchain
		bool GetBlock(std::string hash, std::string &blockFile);
		uint64_t GetClosingTime();

		//Nodes
		std::unordered_map<std::string, int> GetReputation();
		std::unordered_map<std::string, std::string> GetAccounts();
		bool GetAccount(std::string nodeId, std::string &account);

		//Transactions Manager
		bool GetTransaction(std::string hash, std::string &transactionOut);
		bool GetTransaction(std::string hash, Transaction *&transactionPtr);
		bool FindRequest(std::string hash, RequestDelayedTransaction *&transaction, int &errorCode);
		bool FindAuthorize(std::string hash, AuthorizeFutureTransaction *&transaction, int &errorCode);

		//Slots Database
		bool GetEntity(std::string account, std::string &entity, bool fromAccount=true);


	private:
		Balances *balancesDB;
		DAOManager *managerDAO;
		DASManager *managerDAS;
		Entities *entities;
		Keys *keysDB;
		Ledger *ledger;
		NetworkManager *networkManager;
		Nodes *nodes;
		TransactionsManager *txManager;
		Slots *slotsDB;


};


#endif