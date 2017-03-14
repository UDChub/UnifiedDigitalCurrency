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
 * @file modules_interface.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <string>

#include "balances.h"
#include "dao_manager.h"
#include "das_manager.h"
#include "entity.h"
#include "keys.h"
#include "ledger.h"
#include "modules_interface.h"
#include "network_manager.h"
#include "node.h"
#include "slots.h"
#include "transaction.h"
#include "transaction_dao.h"
#include "transaction_das.h"
#include "transaction_delayed.h"
#include "transaction_future.h"
#include "transactions_manager.h"


ModulesInterface::ModulesInterface(Balances *balancesDB, DAOManager *managerDAO, DASManager *managerDAS, Entities *entities, Keys *keysDB, NetworkManager *networkManager, Nodes *nodes, TransactionsManager *txManager, Slots *slotsDB)
 : balancesDB(balancesDB), managerDAO(managerDAO), managerDAS(managerDAS), entities(entities), keysDB(keysDB), networkManager(networkManager), nodes(nodes), txManager(txManager), slotsDB(slotsDB) {
 }

 void ModulesInterface::SetReferences(Ledger *ledger) {
 	this->ledger = ledger;
 }

//Balances Database
uint64_t ModulesInterface::GetBalance(std::string account) {
	return balancesDB->GetBalance(account);
}

//DAOManager
bool ModulesInterface::AddModuleTransaction(DAOTransaction *transaction, int &errorCode) {
	return managerDAO->AddModuleTransaction(transaction, errorCode);
}
bool ModulesInterface::GetModule(std::string DAO, DAOModule *&module) {
	return managerDAO->GetModule(DAO, module);
}

//DASManager
bool ModulesInterface::AddModuleTransaction(DASTransaction *transaction, int &errorCode) {
	return managerDAS->AddModuleTransaction(transaction, errorCode);
}
bool ModulesInterface::GetModule(std::string DAS, DASModule *&module) {
	return managerDAS->GetModule(DAS, module);
}

//Keys Database
bool ModulesInterface::GetPublicKey(std::string id, std::string &publicKey, int idType /*=ID_TYPE_ACCOUNT*/, bool request /*=true*/) {
	return keysDB->GetPublicKey(id, publicKey, idType, request);
}
bool ModulesInterface::GetManagingEntityKey(std::string account, std::string &key) {
	return keysDB->GetManagingEntityKey(account, key);
}

//Ledger
bool ModulesInterface::GetLedger(std::string hash, std::string &ledgerFile) {
	return ledger->GetLedger(hash, ledgerFile);
}
uint64_t ModulesInterface::GetEstimatedAmountInCirculation() {
	return ledger->GetEstimatedAmountInCirculation();
}
uint64_t ModulesInterface::GetEstimatedVolume(bool current /*=true*/) {
	return ledger->GetEstimatedVolume(current);
}
uint64_t ModulesInterface::GetEstimatedFeesCollected(bool current /*=true*/) {
	return ledger->GetEstimatedFeesCollected(current);
}
uint64_t ModulesInterface::GetClosingTime(bool current /*=true*/) {
	return ledger->GetClosingTime(current);
}
std::string ModulesInterface::LastClosedLedgerHash() {
	return ledger->LastClosedLedgerHash();
}
uint64_t ModulesInterface::LastClosedLedgerId() {
	return ledger->LastClosedLedgerId();
}

//Network Management Blockchain
bool ModulesInterface::GetBlock(std::string hash, std::string &blockFile) {
	return networkManager->GetBlock(hash, blockFile);
}
uint64_t ModulesInterface::GetClosingTime() {
	return networkManager->GetClosingTime();
}

//Nodes
std::unordered_map<std::string, int> ModulesInterface::GetReputation() {
	return nodes->GetReputation();
}
std::unordered_map<std::string, std::string> ModulesInterface::GetAccounts() {
	return nodes->GetAccounts();
}
bool ModulesInterface::GetAccount(std::string nodeId, std::string &account) {
	return nodes->GetAccount(nodeId, account);
}

//Slots Database
bool ModulesInterface::GetEntity(std::string account, std::string &entity, bool fromAccount /*=true*/) {
	return slotsDB->GetEntity(account, entity, fromAccount);
}

//Transactions Manager
bool ModulesInterface::GetTransaction(std::string hash, std::string &transactionOut) {
	return txManager->GetTransaction(hash, transactionOut);
}
bool ModulesInterface::GetTransaction(std::string hash, Transaction *&transactionPtr) {
	return txManager->GetTransaction(hash, transactionPtr);
}
bool ModulesInterface::FindRequest(std::string hash, RequestDelayedTransaction *&transaction, int &errorCode) {
	return txManager->FindRequest(hash, transaction, errorCode);
}
bool ModulesInterface::FindAuthorize(std::string hash, AuthorizeFutureTransaction *&transaction, int &errorCode) {
	return txManager->FindAuthorize(hash, transaction, errorCode);
}