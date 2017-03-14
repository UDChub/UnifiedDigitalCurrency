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
 * @file transaction_basic.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <chrono>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "includes/rapidjson/document.h"

#include "codes.h"
#include "globals.h"
#include "modules_interface.h"
#include "processing.h"
#include "transaction_basic.h"
#include "util.h"


//BasicTransaction methods implementation

bool BasicTransaction::IsTransaction() {
	//Check if basic transaction type
	if(Tx["type"].GetString() != TRANSACTION_TYPE_BASIC) return false;

	//Verify JSON data format is correct
	if(Tx.HasMember("timestamp") && Tx.HasMember("sender") && Tx.HasMember("receiver") && Tx.HasMember("amount") && 
		Tx.HasMember("sig") && Tx.HasMember("ob") && Tx.HasMember("ib") && Tx["ob"].IsObject() && Tx["ib"].IsObject())
	{
		if(Tx["ob"].HasMember("account") && Tx["ob"].HasMember("fee") && Tx["ob"].HasMember("sig") && 
			Tx["ib"].HasMember("account") && Tx["ib"].HasMember("fee") && Tx["ib"].HasMember("sig"))
		{
			if(Tx["timestamp"].IsUint64() && Tx["sender"].IsString() && Tx["receiver"].IsString() && Tx["amount"].IsUint64() && 
				Tx["sig"].IsString() && Tx["ob"]["account"].IsString() && Tx["ob"]["fee"].IsUint64() && Tx["ob"]["sig"].IsString() &&
				Tx["ib"]["account"].IsString() && Tx["ib"]["fee"].IsUint64() && Tx["ib"]["sig"].IsString())
			{
				dataOrder.push_back("type");
				dataOrder.push_back("timestamp");
				dataOrder.push_back("sender");
				dataOrder.push_back("receiver");
				dataOrder.push_back("amount");
				dataOrder.push_back("sig");
				dataOrder.push_back("ob");
				dataOrder.push_back("account");
				dataOrder.push_back("fee");
				dataOrder.push_back("sig");
				dataOrder.push_back("ib");
				dataOrder.push_back("account");
				dataOrder.push_back("fee");
				dataOrder.push_back("sig");
				return true;
			}
		}
	}
	return false;
}

bool BasicTransaction::CheckTimestamp() {
	// long long timestamp = GetTimestamp();
	// auto now = Util::current_timestamp_nanos();

	// if(timestamp < now && (now - TRANSACTION_DELAY_NEW) < timestamp) return true;
	// return false;
	return true;
}

bool BasicTransaction::Process(ModulesInterface *interface, int &errorCode) {
	//Step 1: check amounts are correct
	if(!Processing::CheckAmounts(this, errorCode)) return false;

	//Step 2: verify if accounts are valid
	if(!Processing::CheckAccounts(this, errorCode)) return false;

	//Step 3: check if sender has enough funds
	if(!Processing::CheckBalance(interface, GetSender(), GetAmount(), errorCode)) return false;

	//Step 4: verify signatures
	if(!Processing::CheckSignatures(interface, this, errorCode)) return false;

	return true;
}

void BasicTransaction::Execute(std::vector<std::pair<std::string, uint64_t>> &from, std::vector<std::pair<std::string, uint64_t>> &to) {
	from.push_back(std::make_pair(GetSender(), GetAmount()));
	to.push_back(std::make_pair(GetReceiver(), GetNetAmount()));
	to.push_back(std::make_pair(GetOutboundAccount(), GetOutboundFee()));
	to.push_back(std::make_pair(GetInboundAccount(), GetInboundFee()));
}

uint64_t BasicTransaction::GetTimestamp() {
	return Tx["timestamp"].GetUint64();
}

std::string BasicTransaction::GetSender() {
	return Tx["sender"].GetString();
}

std::string BasicTransaction::GetReceiver() {
	return Tx["receiver"].GetString();
}

uint64_t BasicTransaction::GetAmount() {
	return Tx["amount"].GetUint64();
}

std::string BasicTransaction::GetSignature() {
	return Tx["sig"].GetString();
}

std::string BasicTransaction::GetCore() {
	std::stringstream ss;
	ss << GetType() << GetTimestamp() << GetSender() << GetReceiver() << GetAmount();
	return ss.str();
}

std::string BasicTransaction::GetOutboundAccount() {
	return Tx["ob"]["account"].GetString();
}

uint64_t BasicTransaction::GetOutboundFee() {
	return Tx["ob"]["fee"].GetUint64();
}

std::string BasicTransaction::GetOutboundSignature() {
	return Tx["ob"]["sig"].GetString();
}

std::string BasicTransaction::GetInboundAccount() {
	return Tx["ib"]["account"].GetString();
}

uint64_t BasicTransaction::GetInboundFee() {
	return Tx["ib"]["fee"].GetUint64();
}
std::string BasicTransaction::GetInboundSignature() {
	return Tx["ib"]["sig"].GetString();
}

uint64_t BasicTransaction::GetFees() {
	return GetOutboundFee()+GetInboundFee();
}

uint64_t BasicTransaction::GetNetAmount() {
	return  GetAmount()-GetFees();
}
