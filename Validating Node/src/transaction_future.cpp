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
 * @file transaction_future.cpp
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

#include "codes.h"
#include "ecdsa.h"
#include "globals.h"
#include "modules_interface.h"
#include "processing.h"
#include "transaction_future.h"
#include "util.h"


//FutureTransaction Methods Implementation
bool FutureTransaction::IsTransaction() {
	if(Tx["type"].IsString() && Tx["type"].GetString() == TRANSACTION_TYPE_FUTURE && Tx.HasMember("event") && Tx["event"].IsString()) return true;
	return false;
}

bool FutureTransaction::CheckTimestamp() {
	long long timestamp = GetTimestamp();
	auto now = Util::current_timestamp_nanos();

	if(timestamp < now && (now - TRANSACTION_DELAY_NEW) < timestamp) return true;
	return false;
}

std::string FutureTransaction::GetEvent() {
	return Tx["event"].GetString();
}

uint64_t FutureTransaction::GetTimestamp() {
	return Tx["timestamp"].GetUint64();
}

std::string FutureTransaction::GetSender() {
	return Tx["sender"].GetString();
}

std::string FutureTransaction::GetReceiver() {
	return Tx["receiver"].GetString();
}

uint64_t FutureTransaction::GetAmount() {
	return Tx["amount"].GetUint64();
}

std::string FutureTransaction::GetSignature() {
	return Tx["sig"].GetString();
}


//AuthorizeFutureTransaction Methods Implementation
bool AuthorizeFutureTransaction::IsTransaction() {
	if (Tx["event"].GetString() != TX_FUTURE_AUTHORIZE) return false;

	if(Tx.HasMember("timestamp") && Tx.HasMember("sender") && Tx.HasMember("validity") && Tx.HasMember("amount") && Tx.HasMember("sig")) {
		if(Tx["timestamp"].IsUint64() && Tx["sender"].IsString() && Tx["validity"].IsUint64() && Tx["amount"].IsUint64() && Tx["sig"].IsString()) {

			if(Tx.HasMember("receiver")) {
				if(Tx["receiver"].IsString()) receiverSet = true;
				else return false;
			}
			if(Tx.HasMember("slot")) {
				//reject if both receiver and slot are specified
				if(receiverSet) return false;
				if(Tx["slot"].IsString()) slotSet = true;
				else return false;
			}
			//or if none was
			else if (!receiverSet) return false;

			//Format is valid, set the correct key order, also enabling to ignoring external keys
			dataOrder.push_back("type");
			dataOrder.push_back("event");
			dataOrder.push_back("timestamp");
			dataOrder.push_back("validity");
			dataOrder.push_back("sender");
			if(receiverSet) dataOrder.push_back("receiver");
			else dataOrder.push_back("slot");
			dataOrder.push_back("amount");
			dataOrder.push_back("sig");
			return true;
		}
	}
	return false;
}

bool AuthorizeFutureTransaction::CheckTimestamp() {
	long long timestamp = GetTimestamp();
	auto now = Util::current_timestamp_nanos();

	if(timestamp < now && (now - TRANSACTION_DELAY_NEW) < timestamp && now < GetValidity()) return true;
	return false;
}


bool AuthorizeFutureTransaction::Process(ModulesInterface *interface, int &errorCode) {
	//Check that sender, and eventually the receiver, are valid accounts
	boost::regex accRegex(PATTERN_ACCOUNT);
	if(!boost::regex_match(GetSender(), accRegex)) {
		errorCode = ERROR_ACCOUNT_SENDER;
		return false;
	}
	if(receiverSet && !boost::regex_match(GetReceiver(), accRegex)) {
		errorCode = ERROR_ACCOUNT_RECEIVER;
		return false;
	}

	//If a slot was specified, check that it is valid
	boost::regex slotRegex(PATTERN_SLOT);
	if(slotSet && !boost::regex_match(GetSlot(), slotRegex)) {
		errorCode = ERROR_SLOT;
		return false;
	}

	//Verify sender's signature
	std::string publicKey;
	if(!interface->GetPublicKey(GetSender(), publicKey) || !Crypto::Verify(GetCore(), GetSignature(), publicKey)) {
		errorCode = ERROR_SIGNATURE_SENDER;
		return false;
	}

	return true;
}

uint64_t AuthorizeFutureTransaction::GetValidity() {
	return Tx["validity"].GetUint64();
}

std::string AuthorizeFutureTransaction::GetSlot() {
	return Tx["slot"].GetString();
}

std::string AuthorizeFutureTransaction::GetCore() {
	std::stringstream ss;
	ss << GetType() << GetEvent() << GetTimestamp() << GetValidity() << GetSender();
	if(receiverSet) ss << GetReceiver();
	else if (slotSet) ss << GetSlot();
	ss << GetAmount();
	return ss.str();
}

bool AuthorizeFutureTransaction::HasReceiver() {
	return receiverSet;
}

bool AuthorizeFutureTransaction::HasSlot() {
	return slotSet;
}


//ExecuteFutureTransaction Methods Implementation
bool ExecuteFutureTransaction::IsTransaction() {
	if (Tx["event"].GetString() != TX_FUTURE_EXECUTE) return false;

	if(Tx.HasMember("timestamp") && Tx.HasMember("future") && Tx.HasMember("sender") && Tx.HasMember("receiver") && 
		Tx.HasMember("amount") && Tx.HasMember("sig") && Tx.HasMember("ob") && Tx.HasMember("ib")) {

		if(Tx["ob"].HasMember("account") && Tx["ob"].HasMember("fee") && Tx["ob"].HasMember("sig") && 
			Tx["ib"].HasMember("account") && Tx["ib"].HasMember("fee") && Tx["ib"].HasMember("sig")) {

			if(Tx["timestamp"].IsUint64() && Tx["future"].IsString() && Tx["sender"].IsString() && Tx["receiver"].IsString() && 
				Tx["amount"].IsUint64() && Tx["sig"].IsString() && Tx["ob"]["account"].IsString() && Tx["ob"]["fee"].IsUint64() &&  
				Tx["ob"]["sig"].IsString() && Tx["ib"]["account"].IsString() && Tx["ib"]["fee"].IsUint64() && Tx["ib"]["sig"].IsString()) 
			{
				//Format is valid, set the correct key order, also enabling to ignoring external keys
				dataOrder.push_back("type");
				dataOrder.push_back("event");
				dataOrder.push_back("timestamp");
				dataOrder.push_back("future");
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

bool ExecuteFutureTransaction::Process(ModulesInterface *interface, int &errorCode) {
	//Step 1: check amounts are correct
	if(!Processing::CheckAmounts(this, errorCode)) return false;

	//Step 2: verify if accounts are valid
	if(!Processing::CheckAccounts(this, errorCode)) return false;

	//Step 3: ensure future contains a hash
	boost::regex hashRegex(PATTERN_HASH);
	if(!boost::regex_match(GetFuture(), hashRegex)) {
		errorCode = ERROR_HASH;
		return false;
	}

	//Step 4: verify signatures	
	std::stringstream ss;
	std::string message, publicKey;

	//Verify sender's signature
	if(!interface->GetPublicKey(GetSender(), publicKey) || !Crypto::Verify(GetFuture(), GetSignature(), publicKey)) {
		errorCode = ERROR_SIGNATURE_SENDER;
		return false;
	}
	//Verify Outbound signature
	ss << GetCore() << GetSignature() << GetOutboundAccount() << GetOutboundFee();
	message = ss.str();
	if(!interface->GetManagingEntityKey(GetOutboundAccount(), publicKey) || !Crypto::Verify(message, GetOutboundSignature(), publicKey)) {
		errorCode = ERROR_SIGNATURE_OUTBOUND;
		return false;
	}
	//Verify Inbound signature
	ss.str();
	ss << GetOutboundSignature() << GetInboundAccount() << GetInboundFee();
	message = ss.str();
	if(!interface->GetManagingEntityKey(GetInboundAccount(), publicKey) || !Crypto::Verify(message, GetInboundSignature(), publicKey)) {
		errorCode = ERROR_SIGNATURE_INBOUND;
		return false;
	}

	//Step 5: check authorization	
	AuthorizeFutureTransaction *authorization;
	if (!interface->FindAuthorize(GetFuture(), authorization, errorCode)) return false;
	//Ensure it is still valid
	if (authorization->GetValidity() < GetTimestamp()) errorCode = ERROR_TIMESTAMP;
	//Check that if a Slot was specified, the receiver corresponds to it
	else if(authorization->HasSlot() && authorization->GetSlot() != GetReceiver().substr(0, SLOT_LENGTH)) errorCode = ERROR_SLOT;
	//Ensure the remaining content matches the authorization
	else if(authorization->GetSender() != GetSender() || (authorization->HasReceiver() && authorization->GetReceiver() != GetReceiver()) || authorization->GetAmount() < GetAmount()) errorCode = ERROR_CONTENT;
	if(errorCode != VALID) return false;

	//Step 6: verify if the sender has sufficient funds
	if(!Processing::CheckBalance(interface, GetSender(), GetAmount(), errorCode)) return false;

	return true;
}

void ExecuteFutureTransaction::Execute(std::vector<std::pair<std::string, uint64_t>> &from, std::vector<std::pair<std::string, uint64_t>> &to) {
	from.push_back(std::make_pair(GetSender(), GetAmount()));
	to.push_back(std::make_pair(GetReceiver(), GetNetAmount()));
	to.push_back(std::make_pair(GetOutboundAccount(), GetOutboundFee()));
	to.push_back(std::make_pair(GetInboundAccount(), GetInboundFee()));
}

std::string ExecuteFutureTransaction::GetFuture() {
	return Tx["future"].GetString();
}

std::string ExecuteFutureTransaction::GetCore() {
	std::stringstream ss;
	ss << GetType() << GetEvent() << GetTimestamp() << GetSender() << GetReceiver() << GetAmount() << GetFuture();
	return ss.str();
}

std::string ExecuteFutureTransaction::GetOutboundAccount() {
	return Tx["ob"]["account"].GetString();
}

uint64_t ExecuteFutureTransaction::GetOutboundFee() {
	return Tx["ob"]["fee"].GetUint64();
}

std::string ExecuteFutureTransaction::GetOutboundSignature() {
	return Tx["ob"]["sig"].GetString();
}

std::string ExecuteFutureTransaction::GetInboundAccount() {
	return Tx["ib"]["account"].GetString();
}

uint64_t ExecuteFutureTransaction::GetInboundFee() {
	return Tx["ib"]["fee"].GetUint64();
}

std::string ExecuteFutureTransaction::GetInboundSignature() {
	return Tx["ib"]["sig"].GetString();
}

uint64_t ExecuteFutureTransaction::GetFees() {
	return GetOutboundFee()+GetInboundFee();
}

uint64_t ExecuteFutureTransaction::GetNetAmount() {
	return GetAmount()-GetFees();
}