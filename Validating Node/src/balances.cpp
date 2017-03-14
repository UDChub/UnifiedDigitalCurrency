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
 * @file balances.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "includes/rocksdb/db.h"
#include "includes/rocksdb/write_batch.h"

#include "balances.h"
#include "globals.h"
#include "transaction.h"


Balances::Balances(rocksdb::DB* balancesDB) {
	db = balancesDB;
}

uint64_t Balances::Get(std::string key) {
	std::string temp;
    rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, &temp);

	if(status.IsNotFound()) return DEFAULT_BALANCE;
	return std::stoull(temp);
}

bool Balances::Set(std::string key, uint64_t value) {
    rocksdb::Status status = db->Put(rocksdb::WriteOptions(), key, std::to_string(value));
	return status.ok();
}

bool Balances::Batch(rocksdb::WriteBatch batch) {
	rocksdb::Status status = db->Write(rocksdb::WriteOptions(), &batch);
	return status.ok();
}

uint64_t Balances::GetBalance(std::string account) {
	dbMutex.lock();
	return Get(account);
	dbMutex.unlock();
}
  
bool Balances::UpdateBalances(std::vector<std::pair<std::string, uint64_t>> &from, std::vector<std::pair<std::string, uint64_t>> &to) {
	bool updated = false;
	std::unordered_map<std::string, uint64_t> updatedBalances;
	uint64_t balance, totalFrom = 0, totalTo = 0;
	dbMutex.lock();

	//Ensure senders have enough funds, update balance if they do
	for(auto f = from.begin(); f != from.end(); ++f) {
		totalFrom += f->second;

		balance = Get(f->first);
		if(f->first == WORLD_BANK_ACCOUNT || f->second <= balance) {
			updatedBalances[f->first] = balance - f->second;
			continue;
		}
		dbMutex.unlock();
		return false;
	}

	//Update receivers' balances
	for(auto t = to.begin(); t != to.end(); ++t) {
		totalTo += t->second;
		
		balance = Get(t->first);
		updatedBalances[t->first] = balance + t->second;
	}

	//Ensure equilibrium is maintained
	if(totalTo != totalFrom) {
		dbMutex.unlock();
		return false;
	}

	//Execute update
	rocksdb::WriteBatch batch;
	for(auto op = updatedBalances.begin(); op != updatedBalances.end(); ++op) batch.Put(op->first, std::to_string(op->second));
	if(Batch(batch)) updated = true;

	dbMutex.unlock();
	return updated;
}

bool Balances::RollbackBalances(std::vector<std::pair<std::string, uint64_t>> &from, std::vector<std::pair<std::string, uint64_t>> &to, bool add /*=false*/) {
	bool updated = false;
	std::unordered_map<std::string, uint64_t> updatedBalances;
	uint64_t balance;
	dbMutex.lock();

	//Check if its an unregistered transaction
	if (add) {
		//Update senders' balances
		for(auto t = to.begin(); t != to.end(); ++t) {
			balance = Get(t->first);
			updatedBalances[t->first] = balance - t->second;
		}
		//Update receivers' balances
		for(auto t = to.begin(); t != to.end(); ++t) {
			balance = Get(t->first);
			updatedBalances[t->first] = balance + t->second;
		}
	}
	else {
		//Update senders' balances
		for(auto t = to.begin(); t != to.end(); ++t) {
			balance = Get(t->first);
			updatedBalances[t->first] = balance + t->second;
		}
		//Update receivers' balances
		for(auto t = to.begin(); t != to.end(); ++t) {
			balance = Get(t->first);
			updatedBalances[t->first] = balance - t->second;
		}
	}

	//Execute update
	rocksdb::WriteBatch batch;
	for(auto op = updatedBalances.begin(); op != updatedBalances.end(); ++op) batch.Put(op->first, std::to_string(op->second));
	if(Batch(batch)) updated = true;
	//else "what if rollback fails?"

	dbMutex.unlock();
	return updated;
}

void Balances::UpdateFromLedger(std::unordered_map<std::string,uint64_t> balances) {
	dbMutex.lock();
	for(auto it = balances.begin(); it != balances.end(); ++it) Set(it->first, it->second);
	dbMutex.unlock();
}
