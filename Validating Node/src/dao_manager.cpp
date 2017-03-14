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
 * @file dao_manager.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <algorithm>
#include <map>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "codes.h"
#include "dao_manager.h"
#include "dao_module.h"
#include "globals.h"
#include "modules_interface.h"
#include "network.h"
#include "node.h"
#include "transaction.h"
#include "transaction_dao.h"
#include "transactions_manager.h"

///Include all modules
//#include "[DAO modules dir]/[module]/[module].h"


DAOManager::DAOManager(TransactionsManager *txManager) : txManager(txManager) {
}

void DAOManager::SetReferences(Nodes *nodes) {
	this->nodes = nodes;
}

void DAOManager::Launch(ModulesInterface *interface) {
	this->interface = interface;
	//Initialize and Launch all DAOs

	//Add us to modules support
}

void DAOManager::Stop() {
	//Stop all DAOs
	for(auto it = modules.begin(); it != modules.end(); ++it) it->second.module->Stop();
}

Transaction* DAOManager::Create(std::string &rawTx, int &errorCode) {
	Transaction *transaction = new DAOTransaction(rawTx.c_str());
	if(transaction->IsTransaction()) {
		errorCode = ERROR_TRANSACTION_INVALID;
		return transaction;
	}

	auto it = modules.find(dynamic_cast<DAOTransaction*>(transaction)->GetDAO());
	if(it != modules.end()) {
		delete transaction;
		return it->second.module->Create(rawTx, errorCode);
	}
	else if(dynamic_cast<DAOTransaction*>(transaction)->GetDAO() == DAO_BASE_ID) {
		return new FeeRedistributionDAOTransaction(rawTx.c_str());
	}
	errorCode = ERROR_UNSUPPORTED_SERVICE;
	return transaction;
}

bool DAOManager::Process(DAOTransaction *transaction, int &errorCode) {
	auto it = modules.find(transaction->GetDAO());
	if(it != modules.end()) return it->second.module->Process(transaction, errorCode);

	else if(transaction->GetDAO() == DAO_BASE_ID) {
		//Process Fee Redistibution Transaction
		FeeRedistributionDAOTransaction *frTx = dynamic_cast<FeeRedistributionDAOTransaction*>(transaction);

		if(!frTx->IsTransaction()) {
			errorCode = ERROR_TRANSACTION_INVALID;
			return false;
		}

		//If we're behind, redistribute compensation
		if(frTx->GetEnd() == currentEnding) RedistributeFees();

		//Check if we generated the same transaction
		if(frTx->GetHash() == latestDistribution) return true;

		//Verify its period
		if(frTx->GetEnd() != previousEnding) {
			errorCode = ERROR_TIMESTAMP;
			return false;
		}

		//Verify reported funds
		if(frTx->GetReported() != reportedAmount) {
			errorCode = ERROR_CONTENT;
			return false;
		}

		//Check shares
		std::map<std::string, std::pair<std::string, uint64_t>> shares = frTx->GetShares();
		uint64_t total = 0;
		for(auto it2 = shares.begin(); it2 != shares.end(); ++it2) {
			if(nodesAccounts[it2->first] != it2->second.first || currentDistribution[it2->first] != it2->second.second) {
				errorCode = ERROR_CONTENT;
				return false;
			}
			total += it2->second.second;
		}

		//Verify if aggregated compensation funds match
		if(frTx->GetTotal() != total + reportedAmount) {
			errorCode = ERROR_CONTENT;
			return false;
		}
		return true;
	}

	errorCode = ERROR_UNSUPPORTED_SERVICE;
	return false;
}

void DAOManager::Execute(DAOTransaction *transaction, std::vector<std::pair<std::string, uint64_t>> &from, std::vector<std::pair<std::string, uint64_t>> &to) {
	auto it = modules.find(transaction->GetDAO());
	if(it != modules.end()) it->second.module->Execute(transaction, from, to);

	else if(transaction->GetDAO() == DAO_BASE_ID) {
		//Execute Fee Redistibution Transaction
		transaction->Execute(from, to);
	}
}

bool DAOManager::Allow(std::string DAO, std::vector<std::pair<std::string, uint64_t>> &from, std::vector<std::pair<std::string, uint64_t>> &to, int &errorCode) {
	auto it = modules.find(DAO);
	if(it != modules.end()) {
		errorCode = ERROR_UNSUPPORTED_SERVICE;
		return false;
	}

	uint64_t payout = 0;
	for(auto f = from.begin(); f != from.end(); ++f) {
		if(f->first == it->second.account) payout += f->second;
	}

	//Check if payout is possible: DAO account funds - current exposure of unresolved events >= payout
	if(payout > (interface->GetBalance(it->second.account) - it->second.module->GetExposure())) {
		errorCode = ERROR_INSUFICIENT_FUNDS;
		return false;
	}
	return true;
}

void DAOManager::Register(DAOTransaction *transaction) {
	auto it = modules.find(transaction->GetDAO());
	if(it != modules.end()) it->second.module->Register(transaction);

	else if(transaction->GetDAO() == DAO_BASE_ID) {
		//Register Fee Redistibution Transaction
		currentDistribution = nextDistribution;
		nextDistribution.clear();
	}

	//Retrieve network fees
	currentDistribution[transaction->GetDAO()] += transaction->GetNetworkFees();
}

bool DAOManager::Rollback(DAOTransaction *transaction) {
	auto it = modules.find(transaction->GetDAO());
	if(it != modules.end()) return it->second.module->Rollback(transaction);

	else if(transaction->GetDAO() == DAO_BASE_ID) {
		//Rollback Fee Redistibution Transaction
		if(reportedAmount >= dynamic_cast<FeeRedistributionDAOTransaction*>(transaction)->GetReported()) {
			//Subtract reported funds
			reportedAmount -= dynamic_cast<FeeRedistributionDAOTransaction*>(transaction)->GetReported();
			return true;
		}
	}
	return false;
}

bool DAOManager::Enroll(Transaction *transaction) {
	bool keep = false;
	for(auto it = modules.begin(); it != modules.end(); ++it) if(it->second.module->Enroll(transaction)) keep = true;
	return keep;
}

bool DAOManager::AddModuleTransaction(DAOTransaction *transaction, int &errorCode) {
	int counter = 0;
	int threshold = BroadcastThreshold(transaction->GetDAO());

	//broadcast transaction to the network
	std::vector<std::string> possibleNodes(modules[transaction->GetDAO()].support.begin(), modules[transaction->GetDAO()].support.end());
	std::random_shuffle(possibleNodes.begin(), possibleNodes.end());
	while(counter < threshold && possibleNodes.size()) {
		errorCode = VALID;
		boost::asio::ip::tcp::socket *socket = nodes->WriteToNode(possibleNodes.back(), errorCode);
		if(!errorCode && Network::WriteMessage(*socket, NETWORK_BROADCAST_TRANSACTION, transaction->GetTransaction(), true)) counter++;
		possibleNodes.pop_back();
	}

	//Send to transactions manager for processing
	return txManager->AddTransaction(transaction, errorCode, true, DISPATCHER_DAO);
}

bool DAOManager::GetModule(std::string DAO, DAOModule *&module) {
	auto it = modules.find(DAO);
	if(it != modules.end()) {
		module = it->second.module;
		return true;
	}
	return false;
}

std::unordered_set<std::string> DAOManager::InUse() {
	std::unordered_set<std::string> totalInUse;

	for(auto it = modules.begin(); it != modules.end(); ++it) {
		std::unordered_set<std::string> inUse = it->second.module->InUse();
		totalInUse.insert(inUse.begin(), inUse.end());
	}

	return totalInUse;
}

void DAOManager::SetNodeVersion(std::string nodeId, std::string version) {
	bool update = false;
	auto it = nodesVersion.find(nodeId);
	if(it != nodesVersion.end()) update = true;

	for(auto itMod = modules.begin(); itMod != modules.end(); ++itMod) {
		if(update && itMod->second.version <= it->second) itMod->second.support.erase(nodeId);
		if(itMod->second.version <= version) itMod->second.support.insert(nodeId);
	}

	nodesVersion[nodeId] = version;
}

void DAOManager::SetVersion(std::string DAO, std::string version) {
	auto it = modules.find(DAO);
	if(it != modules.end()) {
		//Update list of Validating Nodes supporting the DAO
		it->second.support.clear();
		for(auto it2 = nodesVersion.begin(); it2 != nodesVersion.end(); ++it2) {
			if(version <= it2->second) it->second.support.insert(it->first);
		}
		//Update version
		it->second.version = version;
	}
}

void DAOManager::SetAccount(std::string DAO, std::string account) {
	auto it = modules.find(DAO);
	if(it != modules.end()) it->second.account = account;
}

void DAOManager::SetSupervisor(std::string DAO, std::string supervisor) {
	auto it = modules.find(DAO);
	if(it != modules.end()) it->second.supervisor = supervisor;
}

int DAOManager::BroadcastThreshold(std::string DAO) {
	auto it = modules.find(DAO);
	if(it != modules.end()) return (it->second.support.size() * TRANSACTION_BROADCAST_RATIO);
	return nodesVersion.size();
}

int DAOManager::ConfirmationThreshold(std::string DAO) {
	auto it = modules.find(DAO);
	if(it != modules.end()) return (it->second.support.size() * TRANSACTION_CONFIRMATION_RATIO) + 1;
	return nodesVersion.size();
}

uint64_t DAOManager::GetNextRedistribution() {
	return currentEnding;
}

void DAOManager::RedistributeFees() {
	std::map<std::string, uint64_t> shares;
	uint64_t total = reportedAmount, toReport = 0, share = 0;

	//First, redistribute compensation from current period
	for(auto it = modules.begin(); it != modules.end(); ++it) {
		total += currentDistribution[it->first];

		//Calculate equal shares and amount to be reported for each DAO
		share = currentDistribution[it->first] / it->second.support.size();
		toReport += currentDistribution[it->first] % it->second.support.size();

		for (auto it2 = it->second.support.begin(); it2 != it->second.support.end(); ++it2) shares[*it2] += share;
	}

	//Then, redistribute reported funds
	toReport += reportedAmount;
	share = toReport / nodesVersion.size();
	toReport = toReport % nodesVersion.size();
	for (auto it = nodesVersion.begin(); it != nodesVersion.end(); ++it) shares[it->first] += share;

	//Save remaining amount to report
	reportedAmount = toReport;

	//Retrieve all current private accounts
	nodesAccounts = nodes->GetAccounts();

	//Create the Fee Redistribution Transaction
	uint64_t start = currentEnding - DAO_FEE_REDISTRIBUTION_INTERVAL + 1;
	DAOTransaction *frTx = new FeeRedistributionDAOTransaction(start, currentEnding, total, toReport, shares, nodesAccounts);
	//Keep its hash
	frTx->MakeHash();
	latestDistribution = frTx->GetHash();

	int counter = 0, errorCode;
	int threshold = nodesVersion.size() * TRANSACTION_BROADCAST_RATIO;
	std::vector<std::string> possibleNodes;
	for(auto it = nodesVersion.begin(); it != nodesVersion.end(); ++it) possibleNodes.push_back(it->first);

	//broadcast transaction to the network
	std::random_shuffle(possibleNodes.begin(), possibleNodes.end());
	while(counter < threshold && possibleNodes.size()) {
		errorCode = VALID;
		boost::asio::ip::tcp::socket *socket = nodes->WriteToNode(possibleNodes.back(), errorCode);
		if(!errorCode && Network::WriteMessage(*socket, NETWORK_BROADCAST_TRANSACTION, frTx->GetTransaction(), true)) counter++;
		possibleNodes.pop_back();
	}

	//Send it for processing
	txManager->AddTransaction(frTx, errorCode, true, DISPATCHER_DAO);

	//Switch to next period
	previousEnding = currentEnding;
	currentEnding += DAO_FEE_REDISTRIBUTION_INTERVAL;
}