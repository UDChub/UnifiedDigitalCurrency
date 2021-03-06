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
 * @file transactions_manager.h
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#ifndef TRANSACTIONS_MANAGER_H
#define TRANSACTIONS_MANAGER_H

#include <mutex>
#include <queue>
#include <list>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "globals.h"

class AuthorizeFutureTransaction;
class Balances;
class DAOManager;
class DASManager;
class Entities;
class Ledger;
class ModulesInterface;
class Nodes;
class Publisher;
class RequestDelayedTransaction;
class Transaction;


class TransactionsManager {
	public:
		TransactionsManager(Publisher *publisher);
		void CleanUp();

		void ProcessTransactions(bool *IS_OPERATING, Balances *balancesDB, DAOManager *managerDAO, DASManager *managerDAS, Entities *entities, Ledger *ledger, ModulesInterface *interface, Nodes *nodes);
		void TransactionsRegistration(bool *IS_OPERATING);

		bool AddTransaction(Transaction *transaction, int &errorCode, bool process=false, uint dispatcher=DISPATCHER_NODE, std::string dispatcherId="");
		void FailedToFetch(std::string hash);
		void AddConfirmation(std::string hash, std::string node);

		void RollbackTransaction(Transaction *transaction);
		bool RollbackLedger(std::string newLedger, std::string oldLedger, std::set<std::string> executedTransactions);
		bool RegisterLedger(std::string ledgerFile);

		bool GetTransaction(std::string hash, std::string &transactionOut);
		bool GetTransaction(std::string hash, Transaction *&transactionPtr);
		bool FindRequest(std::string hash, RequestDelayedTransaction *&transaction, int &errorCode);
		bool FindAuthorize(std::string hash, AuthorizeFutureTransaction *&transaction, int &errorCode);

	private:
		std::mutex dataMutex;
		Balances *balancesDB;
		DAOManager *managerDAO;
		DASManager *managerDAS;
		Ledger *ledger;
		Nodes *nodes;
		Publisher *publisher;

		std::unordered_map<std::string, Transaction*> currentTransactions;
		std::unordered_set<std::string> rejectionList;

		std::unordered_map<std::string, int> missingList;
		std::unordered_map<std::string, std::string> submissionList;
		std::queue<std::string> processingQueue;

		std::unordered_map<std::string, std::pair<int,std::vector<std::string>>> confirmationList;
		std::list<std::pair<uint64_t,std::string>> registrationList;

		std::unordered_set<std::string> delayedTransactions;
		std::unordered_map<std::string, uint64_t> futureTransactions;

		bool IsConfirmed(std::string hash);
};

#endif