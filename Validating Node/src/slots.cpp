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
 * @file slots.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <mutex>
#include <string>
#include <fstream>

#include "includes/rocksdb/db.h"
#include "includes/boost/regex.hpp"

#include "globals.h"
#include "slots.h"


Slots::Slots(rocksdb::DB* slotsDB) : db(slotsDB) {
	backupFile.open(LOCAL_DATA_SLOTS, std::fstream::out | std::fstream::app);
}

bool Slots::BackupData() {
	//replace current backup file
	backupFile.close();
	backupFile.open(LOCAL_DATA_SLOTS, std::fstream::out | std::fstream::trunc);

	auto iter = db->NewIterator(rocksdb::ReadOptions());
	iter->SeekToFirst();

	while(iter->Valid()) {
		backupFile << (iter->key()).data() << " " << (iter->value()).data() << std::endl;
		iter->Next();
	}

	//reopen backup file in append mode
	backupFile.open(LOCAL_DATA_SLOTS, std::fstream::out | std::fstream::app);
	return backupFile.good();
}

void Slots::BackupData(std::string slot, std::string entity) {
	if(backupFile.good()) backupFile << slot << " " << entity << std::endl;
}

bool Slots::GetEntity(std::string account, std::string &entity, bool fromAccount /*=true*/) {
	std::string slot;

	//Extract Slot from account number
	if(fromAccount) slot = account.substr(0, SLOT_LENGTH);
	else slot = account;

	std::lock_guard<std::mutex> lock(dbMutex);
	//Fetch Managing Entity's ID
	rocksdb::Status status = db->Get(rocksdb::ReadOptions(), slot, &entity);
	return !status.IsNotFound();
}

bool Slots::SetEntity(std::string slot, std::string entity) {
	boost::regex slotRegex(PATTERN_SLOT);
	boost::regex idRegex(PATTERN_ENTITY_ID);

	//Check data formats
	if(!boost::regex_match(slot, slotRegex) || !boost::regex_match(entity, idRegex)) return false;

	//Upsert the Slot's Managing Entity
	rocksdb::Status status = db->Put(rocksdb::WriteOptions(), slot, entity);
	if(status.ok()) {
		BackupData(slot, entity);
		return true;
	}
	return false;
}

bool Slots::IsActivated(std::string slot) {
	std::string value;
	rocksdb::Status status = db->Get(rocksdb::ReadOptions(), slot, &value);
	return !status.IsNotFound();
}

bool Slots::RemoveSlot(std::string slot) {
	boost::regex slotRegex(PATTERN_SLOT);

	if(boost::regex_match(slot, slotRegex)) {
		rocksdb::Status status = db->Delete(rocksdb::WriteOptions(), slot);
		return status.ok();
	}
	return false;
}