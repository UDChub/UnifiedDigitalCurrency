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
 * @file keys.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <mutex>
#include <string>

#include "includes/boost/regex.hpp"
#include "includes/rocksdb/db.h"
#include "includes/rocksdb/utilities/backupable_db.h"

#include "codes.h"
#include "ecdsa.h"
#include "entity.h"
#include "globals.h"
#include "keys.h"
#include "node.h"
#include "slots.h"
#include "util.h"


Keys::Keys(rocksdb::DB* keysDB)	: db(keysDB) {}

void Keys::SetReferences(Entities *entities, Nodes *nodes, Slots* slotsDB) {
	this->entities = entities;
	this->nodes = nodes;
	this->slotsDB = slotsDB;
	HAS_REFERENCES = true;
}

bool Keys::BackupData() {
	rocksdb::BackupEngine* backup_engine;

	std::lock_guard<std::mutex> lock(dbMutex);
    rocksdb::Status status = rocksdb::BackupEngine::Open(rocksdb::Env::Default(), rocksdb::BackupableDBOptions(DATABASE_PUBLIC_KEYS_BACKUP), &backup_engine);

    if(status.ok()) {
    	backup_engine->CreateNewBackup(db);
    	delete backup_engine;
    	return true;
    }

    return false;
}

bool Keys::GetPublicKey(std::string id, std::string &publicKey, int idType /*=ID_TYPE_ACCOUNT*/, bool request /*=true*/) {
	rocksdb::Status status = db->Get(rocksdb::ReadOptions(), id, &publicKey);
	std::string entity;

	//return if key was found
	if(!status.IsNotFound()) return true;

	//otherwise, try to retrieve it from another source
	if(request && HAS_REFERENCES) {
		switch(idType) {
			case ID_TYPE_ACCOUNT:
				//find the managing entity and send a request for the public key
				if(slotsDB->GetEntity(id, entity)) entities->PublicKeyRequest(entity, id);
				return false;

			case ID_TYPE_ENTITY:
				if(!entities->GetPublicKey(id, publicKey)) return false;

			case ID_TYPE_NODE:
				if(!nodes->GetPublicKey(id, publicKey)) return false;

			default: return false;
		}

		//save it
		db->Put(rocksdb::WriteOptions(), id, publicKey);
		return true;
	}
	return false;
}

bool Keys::SetPublicKey(std::string id, std::string publicKey, int idType /*=ID_TYPE_ACCOUNT*/) {
	boost::regex keyRegex(PATTERN_BASE64);
	boost::regex idRegex;
	std::string yPoint;

	//verify the key format
	if(!boost::regex_match(publicKey, keyRegex)) return false;
	publicKey = Util::base64_to_string(publicKey);
	keyRegex.assign(PATTERN_PUBLIC_KEY);
	if(!boost::regex_match(publicKey, keyRegex)) return false;

	switch(publicKey.at(0)) {
		//Compressed key
		case 0x02:
		case 0x03:
			publicKey = publicKey.substr(1);
			if(!Crypto::SolveYPoint(Util::string_to_hex(publicKey), yPoint)) return false;
			publicKey += Util::hex_to_string(yPoint);
			break;

		//Uncompressed key
		case 0x04:
			publicKey = publicKey.substr(1);
			break;

		default: return false;
	}

	//ensure key uses an accepted NIST curve
	if(!Crypto::IsNIST(publicKey)) return false;

	//Verify the id format
	switch(idType) {
			case ID_TYPE_ACCOUNT:
				idRegex.assign(PATTERN_ACCOUNT);
				break;

			case ID_TYPE_PASSPORT:
				idRegex.assign(PATTERN_PASSPORT_ID);
				//Requires at least secp384r1
				if(publicKey.size() < NETWORK_MIN_KEY_SIZE) return false;
				break;

			case ID_TYPE_ENTITY:
				idRegex.assign(PATTERN_ENTITY_ID);
				//Requires at least secp384r1
				if(publicKey.size() < NETWORK_MIN_KEY_SIZE) return false;
				break;

			case ID_TYPE_NODE:
				idRegex.assign(PATTERN_NODE_ID);
				//Requires at least secp384r1
				if(publicKey.size() < NETWORK_MIN_KEY_SIZE) return false;
				break;

			default: return false;
	}
	if(!boost::regex_match(id, idRegex)) return false;

	std::lock_guard<std::mutex> lock(dbMutex);
	//insert into database
	rocksdb::Status status = db->Put(rocksdb::WriteOptions(), id, publicKey);
	return status.ok();
}

bool Keys::GetManagingEntityKey(std::string account, std::string &publicKey) {
	std::string entity;

	//fetch the Managing Entity's public key using an account number
	if(!slotsDB->GetEntity(account, entity)) return false;
	
	return GetPublicKey(entity, publicKey, ID_TYPE_ENTITY);
}