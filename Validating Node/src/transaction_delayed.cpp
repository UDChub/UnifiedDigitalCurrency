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
 * @file transaction_delayed.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <chrono>
#include <string>
#include <utility>
#include <sstream>
#include <vector>

#include "includes/boost/regex.hpp"
#include "includes/cryptopp/filters.h"
#include "includes/cryptopp/ripemd.h"
#include "includes/cryptopp/hex.h"

#include "codes.h"
#include "globals.h"
#include "modules_interface.h"
#include "processing.h"
#include "transaction_delayed.h"
#include "util.h"


//DelayedTransaction Methods Implementation
bool DelayedTransaction::IsTransaction() {
	if(Tx["type"].IsString() && Tx["type"].GetString() == TRANSACTION_TYPE_DELAYED && Tx.HasMember("event") && Tx["event"].IsString()) return true;
	return false;
}

bool DelayedTransaction::CheckTimestamp() {
	long long timestamp = GetTimestamp();
	auto now = Util::current_timestamp_nanos();

	if(timestamp < now && (now - TRANSACTION_DELAY_NEW) < timestamp) return true;
	return false;
}

std::string DelayedTransaction::GetEvent() {
	return Tx["event"].GetString();
}

uint64_t DelayedTransaction::GetTimestamp() {
	return Tx["timestamp"].GetUint64();
}

std::string DelayedTransaction::GetSender() {
	return Tx["sender"].GetString();
}

std::string DelayedTransaction::GetReceiver() {
	return Tx["receiver"].GetString();
}

uint64_t DelayedTransaction::GetAmount() {
	return Tx["amount"].GetUint64();
}


//RequestDelayedTransaction Methods Implementation
bool RequestDelayedTransaction::IsTransaction() {
	if (Tx["event"].GetString() != TX_DELAYED_REQUEST) return false;

	if(Tx.HasMember("timestamp") && Tx.HasMember("execution") && Tx.HasMember("sender") && Tx.HasMember("receiver") && 
		Tx.HasMember("amount") && Tx.HasMember("sig") && Tx.HasMember("ob") && Tx.HasMember("ib"))
	{
		if(Tx["ob"].HasMember("account") && Tx["ob"].HasMember("fee") && Tx["ob"].HasMember("sig") && 
			Tx["ib"].HasMember("account") && Tx["ib"].HasMember("fee") && Tx["ib"].HasMember("sig"))
		{
			if(Tx["timestamp"].IsUint64() && Tx["execution"].IsUint64() && Tx["sender"].IsString() && Tx["receiver"].IsString() && 
				Tx["amount"].IsUint64() && Tx["sig"].IsString() && Tx["ob"]["account"].IsString() && Tx["ob"]["fee"].IsUint64() && 
				Tx["ob"]["sig"].IsString() && Tx["ib"]["account"].IsString() && Tx["ib"]["fee"].IsUint64() && Tx["ib"]["sig"].IsString()) 
			{
				//Format is valid, set the correct key order, also enabling to ignoring external keys
				dataOrder.push_back("type");
				dataOrder.push_back("event");
				dataOrder.push_back("timestamp");
				dataOrder.push_back("execution");
				dataOrder.push_back("sender");
				dataOrder.push_back("receiver");
				dataOrder.push_back("amount");
				dataOrder.push_back("sig");
				//Order ooutbound data
				dataOrder.push_back("ob");
				dataOrder.push_back("account");
				dataOrder.push_back("fee");
				dataOrder.push_back("sig");
				//Order inbound data
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

bool RequestDelayedTransaction::CheckTimestamp() {
	long long timestamp = GetTimestamp();
	auto now = Util::current_timestamp_nanos();

	if(timestamp < now && (now - TRANSACTION_DELAY_NEW) < timestamp && now < GetExecution()) return true;
	return false;
}

bool RequestDelayedTransaction::Process(ModulesInterface *interface, int &errorCode) {
	//Step 1: check amounts are correct
	if(!Processing::CheckAmounts(this, errorCode)) return false;

	//Step 2: verify if accounts are valid
	if(!Processing::CheckAccounts(this, errorCode)) return false;

	//Step 3: check if has enough funds for fees
	if(!Processing::CheckBalance(interface, GetSender(), GetFees(), errorCode)) return false;

	//Step 3: verify signatures
	if(!Processing::CheckSignatures(interface, this, errorCode)) return false;

	return true;
}

void RequestDelayedTransaction::Execute(std::vector<std::pair<std::string, uint64_t>> &from, std::vector<std::pair<std::string, uint64_t>> &to) {
	from.push_back(std::make_pair(GetSender(), GetFees()));
	to.push_back(std::make_pair(GetOutboundAccount(), GetOutboundFee()));
	to.push_back(std::make_pair(GetInboundAccount(), GetInboundFee()));
}

uint64_t RequestDelayedTransaction::GetExecution() {
	return Tx["execution"].GetUint64();
}

std::string RequestDelayedTransaction::GetSignature() {
	return Tx["sig"].GetString();
}

std::string RequestDelayedTransaction::GetCore() {
	std::stringstream ss;
	ss << GetType() << GetEvent() << GetTimestamp() << GetExecution() << GetSender() << GetReceiver() << GetAmount();
	return ss.str();
}

std::string RequestDelayedTransaction::GetOutboundAccount() {
	return Tx["ob"]["account"].GetString();
}

uint64_t RequestDelayedTransaction::GetOutboundFee() {
	return Tx["ob"]["fee"].GetUint64();
}

std::string RequestDelayedTransaction::GetOutboundSignature() {
	return Tx["ob"]["sig"].GetString();
}

std::string RequestDelayedTransaction::GetInboundAccount() {
	return Tx["ib"]["account"].GetString();
}

uint64_t RequestDelayedTransaction::GetInboundFee() {
	return Tx["ib"]["fee"].GetUint64();
}

std::string RequestDelayedTransaction::GetInboundSignature() {
	return Tx["ib"]["sig"].GetString();
}

uint64_t RequestDelayedTransaction::GetFees() {
	return GetOutboundFee()+GetInboundFee();
}

uint64_t RequestDelayedTransaction::GetNetAmount() {
	return GetAmount()-GetFees();
}


//ReleaseDelayedTransaction Methods Implementation
bool ReleaseDelayedTransaction::IsTransaction() {
	if (Tx["event"].GetString() != TX_DELAYED_RELEASE) return false;

	if(Tx.HasMember("timestamp") && Tx.HasMember("request") && Tx.HasMember("sender") && Tx.HasMember("receiver") && Tx.HasMember("amount")) {
		if(Tx["timestamp"].IsUint64() && Tx["request"].IsString() && Tx["sender"].IsString() && Tx["receiver"].IsString() && Tx["amount"].IsUint64()) {
			//Format is valid, set the correct key order, also enabling to ignoring external keys
			dataOrder.push_back("type");
			dataOrder.push_back("event");
			dataOrder.push_back("timestamp");
			dataOrder.push_back("request");
			dataOrder.push_back("sender");
			dataOrder.push_back("receiver");
			dataOrder.push_back("amount");
			return true;
		}
	}
	return false;
}

bool ReleaseDelayedTransaction::Process(ModulesInterface *interface, int &errorCode) {
	//Ensure request is a valid hash
	boost::regex hashRegex(PATTERN_HASH);
	if(!boost::regex_match(GetRequest(), hashRegex)) {
		errorCode = ERROR_HASH;
		return false;
	}

	//Check that sender and receiver are valid accounts
	boost::regex accRegex(PATTERN_ACCOUNT);
	if(!boost::regex_match(GetSender(), accRegex)) {
		errorCode = ERROR_ACCOUNT_SENDER;
		return false;
	}
	if(!boost::regex_match(GetReceiver(), accRegex)) {
		errorCode = ERROR_ACCOUNT_RECEIVER;
		return false;
	}

	//Verify if the Request exists and is still pending
	RequestDelayedTransaction *request;
	if (!interface->FindRequest(GetRequest(), request, errorCode)) return false;

	//Verify that the Release contents correspond to the Request
	if(request->GetExecution() != GetTimestamp() || request->GetSender() != GetSender() || request->GetReceiver() != GetReceiver() || request->GetNetAmount() != GetAmount()) {
		errorCode = ERROR_CONTENT;
		return false;
	}

	//Check if sender has sufficient funds
	if(Processing::CheckBalance(interface, GetSender(), GetAmount(), errorCode)) return false;

	return true;
}

void ReleaseDelayedTransaction::Execute(std::vector<std::pair<std::string, uint64_t>> &from, std::vector<std::pair<std::string, uint64_t>> &to) {
	from.push_back(std::make_pair(GetSender(), GetAmount()));
	to.push_back(std::make_pair(GetReceiver(), GetAmount()));
}

std::string ReleaseDelayedTransaction::GetRequest() {
	return Tx["request"].GetString();
}