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
 * @file das_module.h
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#ifndef DAS_MODULE_H
#define DAS_MODULE_H

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "includes/rocksdb/db.h"

class DASTransaction;
class ModulesInterface;
class Transaction;


class DASModule {

	public:
		DASModule(ModulesInterface *interface) : interface(interface) {}

		virtual bool Launch();
		virtual bool Stop();
		void SetAccount(std::string account) {this->account = account;}
		std::string GetAccount() {return account;}
		void SetManager(std::string manager) {this->manager = manager;}
		std::string GetManager() {return manager;}

		virtual Transaction* Create(std::string &rawTx, int &errorCode);
		virtual bool Process(DASTransaction *transaction, int &errorCode);
		virtual void Execute(DASTransaction *transaction, std::vector<std::pair<std::string, uint64_t>> &from, std::vector<std::pair<std::string, uint64_t>> &to);
		virtual void Register(DASTransaction *transaction);
		virtual bool Rollback(DASTransaction *transaction);
		virtual bool Enroll(Transaction *transaction);

		virtual uint64_t GetExposure();
		virtual std::unordered_set<std::string> InUse();

	private:
		rocksdb::DB* db;
		ModulesInterface *interface;

		std::string account;
		std::string manager;
};


#endif