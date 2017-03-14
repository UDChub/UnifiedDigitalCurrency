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
 * @file entry_slot.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <chrono>
#include <string>
#include <vector>

#include "codes.h"
#include "entry_slot.h"
#include "globals.h"
#include "util.h"


bool SlotEntry::IsEntry() {
	// if(!entry["data"].HasMember("slot") || !entry["data"]["slot"].IsString()) return false;
	// dataOrder.push_back("slot");
	if(!entry["data"].HasMember("id") || !entry["data"]["id"].IsString()) return false;
	dataOrder.push_back("id");

	if(entry["data"].HasMember("manager") && entry["data"]["manager"].IsString()) return false;
	dataOrder.push_back("manager");

	//Verify format according to the different Entry types
	std::string type = entry["meta"]["type"].GetString();
	bool hasValidity = false;
	
	if(type == ENTRY_TYPE_CANCEL) {
		return (!entry["data"].HasMember("manager") && !entry["data"].HasMember("supervisor") && 
			!entry["data"].HasMember("start") && !entry["data"].HasMember("validity"));
	}

	//verify optional fields
	if(entry["data"].HasMember("start")) {
		if(entry["data"]["start"].IsUint()) dataOrder.push_back("start");
		else return false;
	}
	if(entry["data"].HasMember("validity")) {
		hasValidity = true;
		if(entry["data"]["validity"].IsUint()) dataOrder.push_back("validity");
		else return false;
	}
	else if(entry["data"].HasMember("transfer")) {
		//Ensure that if a validity is specified, transfer is set to false
		if(entry["data"]["transfer"].IsBool()) {
			bool transfer = entry["data"]["transfer"].GetBool();
			if((transfer && !hasValidity) || (!transfer && hasValidity)) dataOrder.push_back("transfer");
			else return false;
		}
		else return false;
	}

	//renew type requires at least one field being modified
	if(type == "renew") return (dataOrder.size() > 2);
	return true;
}

bool SlotEntry::CheckTimestamp() {
	uint now = Util::current_timestamp();
	uint timestamp = GetTimestamp();
	uint end, start;
	if(!GetStart(start)) start = timestamp;

	if(GetValidity(end)) if(timestamp < now && (now - ENTRY_DELAY_NEW) < timestamp && timestamp <= start && start < end) return true;
	else if(timestamp < now && (now - ENTRY_DELAY_NEW) < timestamp && timestamp <= start) return true;

	return false;
}

std::string SlotEntry::GetSlot() {
	// return entry["data"]["slot"].GetString();
	return entry["data"]["id"].GetString();
}

std::string SlotEntry::GetManager() {
	return entry["data"]["manager"].GetString();;
}

bool SlotEntry::GetStart(uint &start) {
	if(entry["data"].HasMember("start")) {
		start = entry["data"]["start"].GetUint();
		return true;
	}
	return false;
}

bool SlotEntry::GetValidity(uint &validity) {
	if(entry["data"].HasMember("validity")) {
		validity = entry["data"]["validity"].GetUint();
		return true;
	}
	return false;
}

bool SlotEntry::IsTransfer() {
	if(entry["data"].HasMember("transfer")) return entry["data"]["transfer"].GetBool();
	return false;
}