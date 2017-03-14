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
 * @file threads_manager.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <chrono>
#include <exception>
#include <iostream>
#include <mutex>
#include <thread>

#include "includes/boost/asio.hpp"

#include "balances.h"
#include "communication.h"
#include "dao_manager.h"
#include "das_manager.h"
#include "entity.h"
#include "globals.h"
#include "keys.h"
#include "ledger.h"
#include "modules_interface.h"
#include "network_manager.h"
#include "node.h"
#include "publisher.h"
#include "slots.h"
#include "threads_manager.h"
#include "transactions_manager.h"
#include "util.h"


void StartTransactionProcessing(bool *IS_OPERATING, TransactionsManager *txManager, Balances *balancesDB, DAOManager *managerDAO, DASManager *managerDAS, Entities *entities, Ledger *ledger, ModulesInterface *interface, Nodes *nodes) {
	txManager->ProcessTransactions(IS_OPERATING, balancesDB, managerDAO, managerDAS, entities, ledger, interface, nodes);
}

void StartRegistrations(bool *IS_OPERATING, TransactionsManager *txManager) {
	txManager->TransactionsRegistration(IS_OPERATING);
}

void StartNMBSupervision(bool *IS_OPERATING, NetworkManager *networkManager) {
	networkManager->StartBlockSupervision(IS_OPERATING);
}

void StartSlotSupervision(bool *IS_OPERATING, NetworkManager *networkManager) {
	networkManager->StartSlotSupervision(IS_OPERATING);
}

void StartLedgerSupervision(bool *IS_OPERATING, bool *IS_SYNCHRONIZED, TransactionsManager *txManager, Ledger *ledger) {
	bool cleaned;
	uint64_t now;
	uint64_t nextLedgerClosure = ledger->GetClosingTime() + LEDGER_CLOSING_INTERVAL;

	while(*IS_OPERATING) {
		now = Util::current_timestamp_nanos();
		if(now >= nextLedgerClosure) {

			if(*IS_SYNCHRONIZED) {
				ledger->CloseLedger();
				ledger->StartConsensus();
				txManager->CleanUp();
				cleaned = false;
			}

			nextLedgerClosure += LEDGER_DURATION;
		}
		else if(!cleaned && now >= nextLedgerClosure - LEDGER_CLOSING_INTERVAL) {
			ledger->CleanConsensus();
			cleaned = true;
		}

		std::this_thread::sleep_for(std::chrono::seconds(60));
	}
}

void StartFeeRedistribution(bool *IS_OPERATING, bool *IS_SYNCHRONIZED, DAOManager *managerDAO, DASManager *managerDAS) {
	uint now;

	while(*IS_OPERATING) {
		now = Util::current_timestamp();

		if(now >= managerDAO->GetNextRedistribution()) managerDAO->RedistributeFees();
		if(now >= managerDAS->GetNextRedistribution()) managerDAS->RedistributeFees();

		std::this_thread::sleep_for(std::chrono::seconds(60));
	}
}

void StartDataBackup(bool *IS_OPERATING, Keys *keysDB, NetworkManager *networkManager, Publisher *publisher, Slots *slotsDB) {
	uint now = Util::current_timestamp();
	uint nextKeysBackup = now + KEYS_BACKUP_INTERVAL;
	uint nextNetworkBackup = now + NETWORK_BACKUP_INTERVAL;
	uint nextPublisherBackup = now + PUBLISHER_BACKUP_INTERVAL;
	uint nextSlotsBackup = now + SLOTS_BACKUP_INTERVAL;

	while(*IS_OPERATING) {
		now = Util::current_timestamp();

		if(now >= nextKeysBackup) {
			if(keysDB->BackupData()) nextKeysBackup += KEYS_BACKUP_INTERVAL;
		}

		if(now >= nextNetworkBackup) {
			networkManager->BackupData();
			nextNetworkBackup += NETWORK_BACKUP_INTERVAL;
		}

		if(now >= nextPublisherBackup) {
			publisher->BackupData();
			nextPublisherBackup += PUBLISHER_BACKUP_INTERVAL;
		}

		if(now >= nextSlotsBackup) {
			if(slotsDB->BackupData()) nextSlotsBackup += SLOTS_BACKUP_INTERVAL;
		}

		std::this_thread::sleep_for(std::chrono::seconds(600));
	}
}

void StartKeepAlive(bool *IS_OPERATING, Nodes *nodes) {
	while(*IS_OPERATING) {
		nodes->KeepAlive();
		std::this_thread::sleep_for(std::chrono::seconds(NODE_KEEP_ALIVE_INTERVAL));
	}
}
ThreadsManager::ThreadsManager(Balances *balancesDB, DAOManager *managerDAO, DASManager *managerDAS, Entities *entities, Keys *keysDB, Ledger *ledger, ModulesInterface *interface, NetworkManager *networkManager, Nodes *nodes, Publisher *publisher, Slots *slotsDB, TransactionsManager *txManager)
: balancesDB(balancesDB), managerDAO(managerDAO), managerDAS(managerDAS), entities(entities), keysDB(keysDB), ledger(ledger), interface(interface), networkManager(networkManager), nodes(nodes), publisher(publisher), slotsDB(slotsDB), txManager(txManager) {
}

void ThreadsManager::LaunchThreads() {
	processingThread = std::thread(StartTransactionProcessing, &IS_OPERATING, txManager, balancesDB, managerDAO, managerDAS, entities, ledger, interface, nodes);
	processingThread.detach();
	std::cout << "\n - transactions processing thread launched." << std::endl;

	registrationThread = std::thread(StartRegistrations, &IS_OPERATING, txManager);
	registrationThread.detach();
	std::cout << "\n - transactions registration thread launched." << std::endl;

	networkThread = std::thread(StartNMBSupervision, &IS_OPERATING, networkManager);
	networkThread.detach();
	std::cout << "\n - Network Management Blockchain supervision thread launched." << std::endl;

	slotsThread = std::thread(StartSlotSupervision, &IS_OPERATING, networkManager);
	slotsThread.detach();
	std::cout << "\n - Slots' management supervision thread launched." << std::endl;

	ledgerThread = std::thread(StartLedgerSupervision, &IS_OPERATING, &ledger->IS_SYNCHRONIZED, txManager, ledger);
	ledgerThread.detach();
	std::cout << "\n - Ledgers supervision thread launched." << std::endl;

	redistributionThread = std::thread(StartFeeRedistribution, &IS_OPERATING, &ledger->IS_SYNCHRONIZED, managerDAO, managerDAS);
	redistributionThread.detach();
	std::cout << "\n - network fees redistribution thread launched." << std::endl;

	listeningThread = std::thread(Communication::ListenToAnyone, &IS_OPERATING, entities, nodes, publisher);
	listeningThread.detach();
	std::cout << "\n - network thread launched." << std::endl;

	nodes->LoadActiveNodes(this);
	std::cout << "\n - connected to peer nodes." << std::endl;
	publisher->Connect(this);
	std::cout << "\n - connected to subscribers." << std::endl;
	entities->Connect(this);
	std::cout << "\n - connected to managing entities." << std::endl;

	keepAliveThread = std::thread(StartKeepAlive, &IS_OPERATING, nodes);
	keepAliveThread.detach();
	std::cout << "\n - peers keep alive thread launched." << std::endl;

	backupThread = std::thread(StartDataBackup, &IS_OPERATING, keysDB, networkManager, publisher, slotsDB);
	backupThread.detach();
	std::cout << "\n - data backup thread launched." << std::endl;
}

void ThreadsManager::ShutdownServices() {
	dataMutex.lock();
	IS_OPERATING = false;
	dataMutex.unlock();
	
	nodes->Stop();
	entities->Stop();
	publisher->Stop();
 }

bool ThreadsManager::NewNodeThread(NodeStruct *node, boost::asio::ip::tcp::socket *socket) {
	std::lock_guard<std::mutex> lock(dataMutex);
	try {
		nodeThreads.push_back(std::thread(Communication::ListenToNode, &IS_OPERATING, &ledger->IS_SYNCHRONIZED, managerDAO, managerDAS, ledger, networkManager, nodes, txManager, node, socket));
		nodeThreads.back().detach();
	}
	catch(std::exception ex) {
		return false;
	}
	return true;
}

bool ThreadsManager::NewEntityThread(EntityStruct *entity) {
	std::lock_guard<std::mutex> lock(dataMutex);
	try {
		entityThreads.push_back(std::thread(Communication::ListenToEntity, &IS_OPERATING, &ledger->IS_SYNCHRONIZED, managerDAO, managerDAS, keysDB, networkManager, txManager, entity));
		entityThreads.back().detach();
	}
	catch(std::exception ex) {
		return false;
	}
	return true;
}

bool ThreadsManager::NewSubscriberThread(SubscriberStruct *subscriber) {
	std::lock_guard<std::mutex> lock(dataMutex);
	try {
		subscriberThreads.push_back(std::thread(Communication::ListenToSubscriber, &IS_OPERATING, publisher, subscriber));
		subscriberThreads.back().detach();
	}
	catch(std::exception ex) {
		return false;
	}
	return true;
}

void ThreadsManager::RestartListeningThread() {
	listeningThread = std::thread(Communication::ListenToAnyone, &IS_OPERATING, entities, nodes, publisher);
	listeningThread.detach();
}