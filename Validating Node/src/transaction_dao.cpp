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
 * @file transaction_dao.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <chrono>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>

#include "includes/rapidjson/document.h"

#include "codes.h"
#include "globals.h"
#include "transaction_dao.h"
#include "util.h"


//DAOTransaction Methods Implementation
bool DAOTransaction::IsTransaction() {
	if (Tx.HasMember("type") && Tx["type"].IsString() && Tx["type"].GetString() == TRANSACTION_TYPE_DAO && Tx.HasMember("DAO") && Tx["DAO"].IsString()) return true;
	return false;
}

std::string DAOTransaction::GetDAO() {
	return Tx["DAO"].GetString();
}

uint64_t DAOTransaction::GetNetworkFees() {
	if(Tx.HasMember("serviceFees") && Tx["serviceFees"].IsObject() && Tx["serviceFees"].HasMember("networkFees") && Tx["serviceFees"]["networkFees"].IsUint64()) {
		return Tx["serviceFees"]["networkFees"].GetUint64();
	}
	return 0;
}


//FeeRedistributionDAOTransaction Methods Implementation
FeeRedistributionDAOTransaction::FeeRedistributionDAOTransaction(uint64_t start, uint64_t end, uint64_t total, uint64_t reported, std::map<std::string, uint64_t> shares, std::unordered_map<std::string, std::string> accounts) {
	Tx.SetObject();

	//Set the metadata
	rapidjson::Value value;

	value.SetString(TRANSACTION_TYPE_DAO.c_str(), Tx.GetAllocator());
	Tx.AddMember("type", value, Tx.GetAllocator());
	value.SetString(DAO_BASE_ID, Tx.GetAllocator());
	Tx.AddMember("DAO", value, Tx.GetAllocator());
	Tx.AddMember("start", start, Tx.GetAllocator());
	Tx.AddMember("end", end, Tx.GetAllocator());
	Tx.AddMember("total", total, Tx.GetAllocator());
	Tx.AddMember("reported", reported, Tx.GetAllocator());

	rapidjson::Value sub(rapidjson::kArrayType);

	for(auto it = shares.begin(); it != shares.end(); ++it) {
		//Create a new sub-object for each share
		rapidjson::Value share(rapidjson::kObjectType);

		//Set the Validating Node ID
		value.SetString(it->first.c_str(), Tx.GetAllocator());
		share.AddMember("node", value, Tx.GetAllocator());
		//Set the account
		value.SetString(accounts[it->first].c_str(), Tx.GetAllocator());
		share.AddMember("account", value, Tx.GetAllocator());
		//Set the amount
		share.AddMember("share", it->second, Tx.GetAllocator());

		//append into the shares array
		sub.PushBack(share, Tx.GetAllocator());
	}
	//Add the shares array into the transaction
	Tx.AddMember("shares", sub, Tx.GetAllocator());
}

bool FeeRedistributionDAOTransaction::IsTransaction() {
	if(Tx["DAO"].GetString() != DAO_BASE_ID) return false;

	//Lookup transaction structure
	if(Tx.HasMember("start") && Tx["start"].IsUint64() && Tx.HasMember("end") && Tx["end"].IsUint64() && Tx.HasMember("total") && 
		Tx["total"].IsUint64() && Tx.HasMember("reported") && Tx["reported"].IsUint64() && Tx.HasMember("shares") && Tx["shares"].IsArray())
	{
		for(rapidjson::Value::ConstValueIterator it = Tx["shares"].Begin(); it != Tx["shares"].End(); ++it) {
			//Check each share's format
			if(!it->IsObject() || !it->HasMember("node") || !it->HasMember("account") || !it->HasMember("share")) return false;
			if(!(*it)["node"].IsString() || !(*it)["account"].IsString() || !(*it)["share"].IsUint64()) return false;
		}
		return true;
	}
	return false;
}

bool FeeRedistributionDAOTransaction::CheckTimestamp() {
	if(GetStart() != (GetEnd() - DAO_FEE_REDISTRIBUTION_INTERVAL + 1)) return false;

	auto now = Util::current_timestamp_nanos();
	return (now - GetStart() < TRANSACTION_DELAY_NEW);
}

void FeeRedistributionDAOTransaction::Execute(std::vector<std::pair<std::string, uint64_t>> &from, std::vector<std::pair<std::string, uint64_t>> &to) {
	for(rapidjson::Value::ConstValueIterator it = Tx["shares"].Begin(); it != Tx["shares"].End(); ++it) {
		from.push_back(std::make_pair(DAO_BASE_ACCOUNT, (*it)["share"].GetUint64()));
		to.push_back(std::make_pair((*it)["account"].GetString(), (*it)["share"].GetUint64()));
	}
}

uint64_t FeeRedistributionDAOTransaction::GetTimestamp() {
	return GetEnd();
}

uint64_t FeeRedistributionDAOTransaction::GetStart() {
	return Tx["start"].GetUint64();
}

uint64_t FeeRedistributionDAOTransaction::GetEnd() {
	return Tx["end"].GetUint64();
}

uint64_t FeeRedistributionDAOTransaction::GetTotal() {
	return Tx["total"].GetUint64();
}

uint64_t FeeRedistributionDAOTransaction::GetReported() {
	return Tx["reported"].GetUint64();
}

std::map<std::string, std::pair<std::string, uint64_t>> FeeRedistributionDAOTransaction::GetShares() {
	std::map<std::string, std::pair<std::string, uint64_t>> shares;
	for(rapidjson::Value::ConstValueIterator it = Tx["shares"].Begin(); it != Tx["shares"].End(); ++it) {
		shares[(*it)["node"].GetString()] = std::make_pair((*it)["account"].GetString(), (*it)["share"].GetUint64());
	}
	return shares;
}