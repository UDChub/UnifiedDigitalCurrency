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
 * @file database.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <string>
#include <cstdint>
#include <fstream>

#include "includes/rocksdb/db.h"
#include "includes/rocksdb/write_batch.h"
#include "includes/rocksdb/utilities/backupable_db.h"
#include "includes/boost/filesystem.hpp"

#include "codes.h"
#include "database.h"
#include "globals.h"

#include <iostream>
#include <string>


rocksdb::DB* Database::LoadNetworkManagementDB() {
	rocksdb::DB *db;
	rocksdb::Options options;
	//Try to open database
	rocksdb::Status status = rocksdb::DB::Open(options, DATABASE_NETWORK_MANAGEMENT, &db);

	if(!status.ok()) {
		//Otherwise create a new one
		options.create_if_missing = true;
		status = rocksdb::DB::Open(options, DATABASE_NETWORK_MANAGEMENT, &db);
		assert(status.ok());

		//try to restore using an eventual backup database
		rocksdb::BackupEngine* backup_engine;
		status = rocksdb::BackupEngine::Open(rocksdb::Env::Default(), rocksdb::BackupableDBOptions(DATABASE_NETWORK_MANAGEMENT_BACKUP), &backup_engine);
	    if(status.ok()) {
	    	std::cout << "restoring from backup" << std::endl;
		    backup_engine->RestoreDBFromLatestBackup(DATABASE_NETWORK_MANAGEMENT, DATABASE_NETWORK_MANAGEMENT);
		    delete backup_engine;
		}
	}

	return db;
}

rocksdb::DB* Database::LoadBalancesDB() {
	rocksdb::DB *db;
	rocksdb::Options options;

	options.create_if_missing = true;
	//options.merge_operator.reset(new AdditionMergeOperator());
	rocksdb::Status status = rocksdb::DB::Open(options, DATABASE_BALANCES, &db);
	assert(status.ok());

	return db;
}

rocksdb::DB* Database::LoadKeysDB() {
	rocksdb::DB *db;
	rocksdb::Options options;
	//Try to open database
	rocksdb::Status status = rocksdb::DB::Open(options, DATABASE_PUBLIC_KEYS, &db);

	if(!status.ok()) {
		//Otherwise create a new one
		options.create_if_missing = true;
		status = rocksdb::DB::Open(options, DATABASE_PUBLIC_KEYS, &db);
		assert(status.ok());

		//try to restore using an eventual backup database
		rocksdb::BackupEngine* backup_engine;
		status = rocksdb::BackupEngine::Open(rocksdb::Env::Default(), rocksdb::BackupableDBOptions(DATABASE_PUBLIC_KEYS_BACKUP), &backup_engine);
	    if(status.ok()) {
	    	std::cout << "restoring from backup" << std::endl;
		    backup_engine->RestoreDBFromLatestBackup(DATABASE_PUBLIC_KEYS, DATABASE_PUBLIC_KEYS);
		    delete backup_engine;
		}
	}

	return db;
}

rocksdb::DB* Database::LoadSlotsDB() {
	rocksdb::DB *db;
	rocksdb::Options options;
	rocksdb::Status status = rocksdb::DB::Open(options, DATABASE_SLOTS, &db);

	if(!status.ok()) {
		//Create a new database if it doesn't exists
		options.create_if_missing = true;
		status = rocksdb::DB::Open(options, DATABASE_SLOTS, &db);
		assert(status.ok());

		//Repopulate with the Slots backup file
		std::fstream slotsData(LOCAL_DATA_SLOTS, std::fstream::in);
		if(slotsData.good()) {
			std::string key, value;
			while(slotsData >> key >> value) db->Put(rocksdb::WriteOptions(), key, value);
		}
		slotsData.close();
	}

	return db;
}