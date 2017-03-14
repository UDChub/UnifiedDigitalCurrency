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
 * @file threads_manager.h
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#ifndef UPDATES_MANAGER_H
#define UPDATES_MANAGER_H

#include <mutex>
#include <thread>
#include <list>

#include "includes/boost/asio.hpp"

class Balances;
class DAOManager;
class DASManager;
class Entities;
class Keys;
class Ledger;
class ModulesInterface;
class NetworkManager;
class Nodes;
class Publisher;
class Slots;
class TransactionsManager;

struct EntityStruct;
struct NodeStruct;
struct SubscriberStruct;


void StartTransactionProcessing(bool *IS_OPERATING, TransactionsManager *txManager, Balances *balancesDB, DAOManager *managerDAO, DASManager *managerDAS, Entities *entities, Ledger *ledger, ModulesInterface *interface, Nodes *nodes);
void StartRegistrations(bool *IS_OPERATING, TransactionsManager *txManager);
void StartNMBSupervision(bool *IS_OPERATING, NetworkManager *networkManager);
void StartSlotSupervision(bool *IS_OPERATING, NetworkManager *networkManager);
void StartLedgerSupervision(bool *IS_OPERATING, bool *IS_SYNCHRONIZED, TransactionsManager *txManager, Ledger *ledger);
void StartFeeRedistribution(bool *IS_OPERATING, bool *IS_SYNCHRONIZED, DAOManager *managerDAO, DASManager *managerDAS);
void StartDataBackup(bool *IS_OPERATING, Keys *keysDB, NetworkManager *networkManager, Publisher *publisher, Slots *slotsDB);
void StartKeepAlive(bool *IS_OPERATING, Nodes *nodes);

class ThreadsManager {
	public:
		ThreadsManager(Balances *balancesDB, DAOManager *managerDAO, DASManager *managerDAS, Entities *entities, Keys *keysDB, Ledger *ledger, ModulesInterface *interface, NetworkManager *networkManager, Nodes *nodes, Publisher *publisher, Slots *slotsDB, TransactionsManager *txManager);
		void LaunchThreads();	
		void ShutdownServices();

		bool NewNodeThread(NodeStruct *node, boost::asio::ip::tcp::socket *socket);
		bool NewEntityThread(EntityStruct *entity);
		bool NewSubscriberThread(SubscriberStruct *subscriber);

		void RestartListeningThread();

		bool IS_OPERATING = true;

	private:
		std::mutex dataMutex;
		Balances *balancesDB;
		DAOManager *managerDAO;
		DASManager *managerDAS;
		Entities *entities;
		Keys *keysDB;
		Ledger *ledger;
		ModulesInterface *interface;
		NetworkManager *networkManager;
		Nodes *nodes;
		Publisher *publisher;
		Slots *slotsDB;
		TransactionsManager *txManager;

		std::thread processingThread;
		std::thread registrationThread;
		std::thread networkThread;
		std::thread slotsThread;
		std::thread ledgerThread;
		std::thread redistributionThread;

		std::thread backupThread;
		std::thread keepAliveThread;
		std::thread listeningThread;
		
		std::list<std::thread> nodeThreads;
		std::list<std::thread> entityThreads;
		std::list<std::thread> subscriberThreads;
};

#endif