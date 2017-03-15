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
 * @file main.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <chrono>
#include <iostream>
#include <csignal>

#include "includes/boost/filesystem.hpp"
#include "includes/rocksdb/db.h"

#include "balances.h"
#include "codes.h"
#include "communication.h"
#include "configurations.h"
#include "dao_manager.h"
#include "das_manager.h"
#include "database.h"
#include "ecdsa.h"
#include "entity.h"
#include "globals.h"
#include "keys.h"
#include "ledger.h"
#include "modules_interface.h"
#include "network.h"
#include "network_manager.h"
#include "node.h"
#include "processing.h"
#include "publisher.h"
#include "slots.h"
#include "threads_manager.h"
#include "transaction.h"
#include "transaction_basic.h"
#include "transactions_manager.h"
#include "util.h"


ThreadsManager *threadsManager;

void SIGNAL_HANDLER(int param) {
	std::cout << "\n\nWARNING: Node shutdown requested! Please confirm to continue [Y/N]: ";
	std::string entered;
	std::cin >> entered;

	if(entered == "Y" || entered == "y" || entered == "yes") {
		std::cout << "\n\nShutdown process initiated, please wait..." << std::endl;
		std::cout << "\nClosing connections and stopping threads." << std::endl;
		threadsManager->ShutdownServices();
		std::cout << "\nShutdown process completed. Exiting..." << std::endl;
	}
	else {
		std::cout << "\n\nShutdown canceled.";
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}
}

int main() {
	
	//create directories if they don't exist
	boost::filesystem::create_directory(boost::filesystem::path(LOCAL_DATA));
	boost::filesystem::create_directory(boost::filesystem::path(LOCAL_DATA_DATABASES));
	boost::filesystem::create_directory(boost::filesystem::path(DATABASE_PUBLIC_KEYS));
	boost::filesystem::create_directory(boost::filesystem::path(DATABASE_PUBLIC_KEYS_BACKUP));
	boost::filesystem::create_directory(boost::filesystem::path(DATABASE_SLOTS));
	boost::filesystem::create_directory(boost::filesystem::path(DATABASE_BALANCES));
	boost::filesystem::create_directory(boost::filesystem::path(DATABASE_NETWORK_MANAGEMENT));
	boost::filesystem::create_directory(boost::filesystem::path(DATABASE_NETWORK_MANAGEMENT_BACKUP));
	boost::filesystem::create_directory(boost::filesystem::path(LOCAL_DATA_LEDGERS));
	boost::filesystem::create_directory(boost::filesystem::path(LOCAL_DATA_BLOCKS));
	boost::filesystem::create_directory(boost::filesystem::path(LOCAL_DATA_KEYS));
	boost::filesystem::create_directory(boost::filesystem::path(LOCAL_TEMP_DATA));

	//check for an existing profile
	bool fresh = false;
	if(!LoadConfigurations()) {
		std::cout << "No profile found, creating a new one..." << std::endl;
		fresh = CreateProfile();
	}

	std::cout << "Starting up node..." << std::endl << std::endl;

	//load private key
	assert(Crypto::LoadPrivateKey());
	std::cout << "Node's Private Key has been loaded." << std::endl << std::endl;

//**********************synchronize ledger and transactions**********************

	//start operation
	std::cout << "\nLoading databases and creating data structures..." << std::endl;

	//create accounts' balances database
	Balances balancesDB(Database::LoadBalancesDB());

	//load public keys database
	Keys keysDB(Database::LoadKeysDB());

	//load slots database
	Slots slotsDB(Database::LoadSlotsDB());

	//load subscribers data
	Publisher publisher;

	//load transactions manager
	TransactionsManager txManager(&publisher);

	//load DAO manager
	DAOManager managerDAO(&txManager);

	//load DAS manager
	DASManager managerDAS(&txManager);

	//load network management database
	NetworkManager networkManager(Database::LoadNetworkManagementDB(), &managerDAO, &managerDAS, &keysDB, &publisher, &slotsDB, threadsManager);

	//Get reference to Managing Entities data
	Entities *entities = networkManager.GetEntitiesManager();

	//Get reference to Validating Nodes data
	Nodes *nodes = networkManager.GetNodesManager();

	//Set needed references
	keysDB.SetReferences(entities, nodes, &slotsDB);
	managerDAO.SetReferences(nodes);
	managerDAS.SetReferences(nodes);

	//Synchronize Network Management Blockchain
	std::cout << "Starting Network Management Blockchain update process..." << std::endl;
	networkManager.SynchronizeNMB();
	std::cout << "Network Management Blockchain is up to date." << std::endl;

	while(!networkManager.IsSynchronized()) {
		system("clear");
		std::cout << "WARNING: Validating Node is not synchronized!\n\nStarting Network Management Blockchain synchronization process..." << std::endl << std::endl;
		std::cout <<"Time remaining for complete synchronization: " << Util::timestamp_to_remaining(networkManager.ExpectedSynchronizationEnd()) << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	std::cout << "\nSynchronization process complete.\nNetwork Management Blockchain is up to date." << std::endl;

	//load Modules Interface
	ModulesInterface interface(&balancesDB, &managerDAO, &managerDAS, entities, &keysDB, &networkManager, nodes, &txManager, &slotsDB);

	//launch all DAOs
	managerDAO.Launch(&interface);

	//Launch all DASs
	managerDAS.Launch(&interface);

	//load Ledgers manager
	std::cout << "Starting UDC's Ledgers update process..." << std::endl;
	Ledger *ledger = networkManager.SynchronizeLedgers(&balancesDB, &txManager);
	std::cout << "Currency's Ledgers are up to date." << std::endl;
	interface.SetReferences(ledger);

	//start threads
	std::cout << "Starting threads..." << std::endl;
	threadsManager = new ThreadsManager(&balancesDB, &managerDAO, &managerDAS, entities, &keysDB, ledger, &interface, &networkManager, nodes, &publisher, &slotsDB, &txManager);
	threadsManager->LaunchThreads();

	//Broadcast any changes we might have since last startup
	std::string publicKey;
	if(fresh && Crypto::GetPublicKey(publicKey)) networkManager.CheckSelf(publicKey, true);
	else networkManager.CheckSelf();

	while(!ledger->IS_SYNCHRONIZED) {
		system("clear");
		std::cout << "WARNING: Validating Node is not synchronized!\nStarting UDC's synchronization process..." << std::endl << std::endl;

		uint64_t closingTime = ledger->GetClosingTime();
		std::cout << "Current ledger's closing time: " << Util::timestamp_to_date(closingTime) << "(local time)";

		closingTime += LEDGER_CLOSING_INTERVAL;
		if(ledger->WAIT_FOR_NEXT) closingTime += LEDGER_DURATION;
		std::cout <<"\nTime remaining for complete synchronization: " << Util::timestamp_to_remaining(closingTime) << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	std::cout << "\nSynchronization completed.";
	std::cout << "\nValidating Node is up and running!";

	//listen to shutdown signals
	signal(SIGINT, SIGNAL_HANDLER);
	signal(SIGTSTP, SIGNAL_HANDLER);
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	while(true) {
		system("clear");
		std::cout << "\n########################################################################" << std::endl;
		std::cout << "#Unified Digital Currency - WorldBank's Official Validation Node v0.1.0#" << std::endl;
		std::cout << "########################################################################" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(60));
	}



	return 1;
}

