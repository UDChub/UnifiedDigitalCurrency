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
 * @file dao_manager.h
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#ifndef DAO_MANAGER_H
#define DAO_MANAGER_H

#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class DAOModule;
class DAOTransaction;
class ModulesInterface;
class Nodes;
class Transaction;
class TransactionsManager;


struct DAOStruct {
	std::string moduleId;
	DAOModule *module;
	std::string version;
	std::string account;
	std::string supervisor;
	std::unordered_set<std::string> support;
};

class DAOManager {

	public:
		DAOManager(TransactionsManager *txManager);
		void SetReferences(Nodes *nodes);
		void Launch(ModulesInterface *interface);
		void Stop();

		Transaction* Create(std::string &rawTx, int &errorCode);
		bool Process(DAOTransaction *transaction, int &errorCode);
		void Execute(DAOTransaction *transaction, std::vector<std::pair<std::string, uint64_t>> &from, std::vector<std::pair<std::string, uint64_t>> &to);
		bool Allow(std::string DAO, std::vector<std::pair<std::string, uint64_t>> &from, std::vector<std::pair<std::string, uint64_t>> &to, int &errorCode);
		void Register(DAOTransaction *transaction);
		bool Rollback(DAOTransaction *transaction);
		bool Enroll(Transaction *transaction);

		bool AddModuleTransaction(DAOTransaction *transaction, int &errorCode);
		bool GetModule(std::string DAO, DAOModule *&module);
		
		uint64_t GetNextRedistribution();
		void RedistributeFees();

		std::unordered_set<std::string> InUse();

		void SetNodeVersion(std::string nodeId, std::string version);
		void SetVersion(std::string DAO, std::string version);
		void SetAccount(std::string DAO, std::string account);
		void SetSupervisor(std::string DAO, std::string supervisor);
		int BroadcastThreshold(std::string DAO);
		int ConfirmationThreshold(std::string DAO);

	private:
		ModulesInterface *interface;
		Nodes *nodes;
		TransactionsManager *txManager;
		std::map<std::string, DAOStruct> modules;

		std::unordered_map<std::string, std::string> nodesVersion;
		std::unordered_map<std::string, std::string> nodesAccounts;

		std::unordered_map<std::string, uint64_t> currentDistribution;
		std::unordered_map<std::string, uint64_t> nextDistribution;
		uint64_t reportedAmount;
		uint64_t currentEnding;
		uint64_t previousEnding;
		std::string latestDistribution;
};


#endif