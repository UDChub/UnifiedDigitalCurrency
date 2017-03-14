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
 * @file processing.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <sstream>

#include "includes/boost/regex.hpp"

#include "balances.h"
#include "codes.h"
#include "dao_manager.h"
#include "das_manager.h"
#include "ecdsa.h"
#include "entry.h"
#include "entry_account.h"
#include "entry_dao.h"
#include "entry_das.h"
#include "entry_entity.h"
#include "entry_node.h"
#include "entry_passport.h"
#include "entry_resource.h"
#include "entry_slot.h"
#include "globals.h"
#include "keys.h"
#include "modules_interface.h"
#include "processing.h"
#include "transaction.h"
#include "transaction_basic.h"
#include "transaction_delayed.h"
#include "transaction_future.h"
#include "transaction_das.h"
#include "transaction_dao.h"


Transaction* Processing::CreateTransaction(DAOManager *managerDAO, DASManager *managerDAS, std::string &rawTx, int &errorCode) {
	Transaction *transaction = new Transaction(rawTx.c_str());
	std::string event;
	errorCode = VALID;

	//Step 1: check if the data corresponds to a generic transaction
	if(!transaction->IsTransaction()) {
		errorCode = ERROR_TRANSACTION_INVALID;
		delete transaction;
		return NULL;
	}
	int type = std::stoul(transaction->GetType(),nullptr,16);
	delete transaction;

	//Step 2: find which transaction type-event it pertains to
	switch(type) {
		case TRANSACTION_BASIC:
			transaction = new BasicTransaction(rawTx.c_str());
			break;

		case TRANSACTION_DELAYED:
			transaction = new DelayedTransaction(rawTx.c_str());
			if(!transaction->IsTransaction()) {
				errorCode = ERROR_TRANSACTION_INVALID;
				delete transaction;
				return NULL;
			}
			event = dynamic_cast<DelayedTransaction*>(transaction)->GetEvent();
			delete transaction;

			//Verify Delayed transaction event
			if(event == TX_DELAYED_REQUEST) transaction = new RequestDelayedTransaction(rawTx.c_str());
			if(event == TX_DELAYED_RELEASE) transaction = new ReleaseDelayedTransaction(rawTx.c_str());
			else {
				errorCode = ERROR_UNSUPPORTED_EVENT;
				return NULL;
			}
			break;

		case TRANSACTION_FUTURE:
			transaction = new FutureTransaction(rawTx.c_str());
			if(!transaction->IsTransaction()) {
				errorCode = ERROR_TRANSACTION_INVALID;
				delete transaction;
				return NULL;
			}
			event = dynamic_cast<FutureTransaction*>(transaction)->GetEvent();
			delete transaction;

			//Verify Future transaction event
			if(event == TX_FUTURE_AUTHORIZE) transaction = new AuthorizeFutureTransaction(rawTx.c_str());
			if(event == TX_FUTURE_EXECUTE) transaction = new ExecuteFutureTransaction(rawTx.c_str());
			else {
				errorCode = ERROR_UNSUPPORTED_EVENT;
				return NULL;
			}
			break;

		case TRANSACTION_DAO:
			transaction = managerDAO->Create(rawTx, errorCode);
			break;

		case TRANSACTION_DAS:
			transaction = managerDAS->Create(rawTx, errorCode);
			break;

		default:
			//Unreconginzed transaction type
			errorCode = ERROR_UNSUPPORTED_TYPE;
			return NULL;
			break;
	}
	//discard if reached an error
	if (errorCode != VALID) {
		delete transaction;
		return NULL;
	}

	//Check if the format corresponds to the transaction type-event specified
	if(!transaction->IsTransaction()) {
		errorCode = ERROR_TRANSACTION_INVALID;
		delete transaction;
		return NULL;
	}

	//Finally order its data and compute the hash
	transaction->OrderData();
	transaction->MakeHash();

	return transaction;
}

Transaction* Processing::PrepareTransaction(DAOManager *managerDAO, DASManager *managerDAS, std::string &rawTx, int &errorCode) {
	//Load transaction
	Transaction *transaction = CreateTransaction(managerDAO, managerDAS, rawTx, errorCode);
	if(errorCode != VALID) return NULL;

	//Verify its timestamp
	if (!transaction->CheckTimestamp()) {
		errorCode = ERROR_TIMESTAMP;
		delete transaction;
		return NULL;
	}

	return transaction;
}

bool Processing::CheckBalance(ModulesInterface *interface, std::string from, uint64_t amount, int &errorCode) {
	if(from == WORLD_BANK_ACCOUNT) return true;

	uint64_t balance = interface->GetBalance(from);

	if(balance < amount) {
		errorCode = ERROR_INSUFICIENT_FUNDS;
		return false;
	}

	return true;
}

Entry* Processing::CreateEntry(std::string &content, int &errorCode) {
	//Create generic NMEntry
	Entry *entry = new Entry(content.c_str());
	errorCode = VALID;

	//check if it has the generic format
	if(!entry->IsEntry()) {
		errorCode = ERROR_ENTRY_INVALID;
		delete entry;
		return NULL;
	}

	//retrieve resource type
	std::string resource = entry->GetResource();
	delete entry;

	//Create specific NMEntry
	if(resource == NMB_RESOURCE) entry = new ResourceEntry(content.c_str());
	else if(resource == NMB_RESOURCE_PASSPORT) entry = new PassportEntry(content.c_str());
	else if(resource == NMB_RESOURCE_ENTITY) entry = new EntityEntry(content.c_str());
	else if(resource == NMB_RESOURCE_NODE) entry = new NodeEntry(content.c_str());
	else if(resource == NMB_RESOURCE_DAO) entry = new DAOEntry(content.c_str());
	else if(resource == NMB_RESOURCE_DAS) entry = new DASEntry(content.c_str());
	else if(resource == NMB_RESOURCE_SLOT) entry = new SlotEntry(content.c_str());
	else if(resource == NMB_RESOURCE_ACCOUNT) entry = new AccountEntry(content.c_str());
	else {
		errorCode = ERROR_RESOURCE;
		delete entry;
		return NULL;
	}

	//verify it follows the specific format
	if(!entry->IsEntry()) {
		errorCode = ERROR_ENTRY_INVALID;
		delete entry;
		return NULL;
	}

	//Order the data and create its hash
	entry->OrderData();
	entry->MakeHash();

	return entry;
}

int Processing::GetIdType(std::string id) {
	boost::regex idRegex;

	switch(id.length()) {
		//Prefixed ID
		case STANDARD_ID_LENGTH:
			idRegex.assign(PATTERN_PASSPORT_ID);
			if(boost::regex_match(id, idRegex)) return ID_TYPE_PASSPORT;
			idRegex.assign(PATTERN_ENTITY_ID);
			if(boost::regex_match(id, idRegex)) return ID_TYPE_ENTITY;
			idRegex.assign(PATTERN_NODE_ID);
			if(boost::regex_match(id, idRegex)) return ID_TYPE_NODE;
			idRegex.assign(PATTERN_DAO_ID);
			if(boost::regex_match(id, idRegex)) return ID_TYPE_DAO;
			idRegex.assign(PATTERN_DAS_ID);
			if(boost::regex_match(id, idRegex)) return ID_TYPE_DAS;
			break;

		case ACCOUNT_LENGTH:
			idRegex.assign(PATTERN_ACCOUNT);
			if(boost::regex_match(id, idRegex)) return ID_TYPE_ACCOUNT;
			break;

		//All remaining valid IDs or any other invalid format
		default: break;
	}

	return ID_TYPE_INVALID;
}
