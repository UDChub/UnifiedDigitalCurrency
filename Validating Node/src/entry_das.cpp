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
 * @file entry_das.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <string>

#include "codes.h"
#include "entry_das.h"


bool DASEntry::IsEntry() {

	if(!entry["data"].HasMember("id") || !entry["data"]["id"].IsString()) return false;

	dataOrder.push_back("id");

	//Verify format according to the different Entry types
	std::string type = entry["meta"]["type"].GetString();
	
	if(type == ENTRY_TYPE_CREATE) {
		if(entry["data"].HasMember("designation") && entry["data"]["designation"].IsString() && entry["data"].HasMember("manager") && 
			entry["data"]["manager"].IsString() && entry["data"].HasMember("documentation") && entry["data"]["documentation"].IsString() && 
			entry["data"].HasMember("networkVersion") && entry["data"]["networkVersion"].IsString()) {

			dataOrder.push_back("designation");
			dataOrder.push_back("manager");
			dataOrder.push_back("documentation");
			dataOrder.push_back("networkVersion");

			if(entry["data"].HasMember("account")) {
				if(entry["data"]["account"].IsString()) dataOrder.push_back("account");
				else return false;
			}

			return true;
		}
	}
	else if(type == ENTRY_TYPE_UPDATE || type == ENTRY_TYPE_RENEW) {
		//verify optional fields
		if(entry["data"].HasMember("designation")) {
			if(entry["data"]["designation"].IsString()) dataOrder.push_back("designation");
			else return false;
		}
		if(entry["data"].HasMember("manager")) {
			if(entry["data"]["manager"].IsString()) dataOrder.push_back("manager");
			else return false;
		}
		if(entry["data"].HasMember("documentation")) {
			if(entry["data"]["documentation"].IsString()) dataOrder.push_back("documentation");
			else return false;
		}
		if(entry["data"].HasMember("networkVersion")) {
			if(entry["data"]["networkVersion"].IsString()) dataOrder.push_back("networkVersion");
			else return false;
		}
		if(entry["data"].HasMember("account")) {
			if(entry["data"]["account"].IsString()) dataOrder.push_back("account");
			else return false;
		}
		
		//Update type requires at least one field being modified
		return ((type == "update" && dataOrder.size() > 1) || type == "renew");
	}
	else if(type == ENTRY_TYPE_CANCEL) {
		if(!entry["data"].HasMember("designation") && !entry["data"].HasMember("manager") && !entry["data"].HasMember("documentation") && 
			!entry["data"].HasMember("networkVersion") && !entry["data"].HasMember("account")) 
			return true;
	}

	return false;
}

std::string DASEntry::GetDAS() {
	return entry["data"]["id"].GetString();
}

bool DASEntry::GetDesignation(std::string &out) {
	if(entry["data"].HasMember("designation")) {
		out = entry["data"]["designation"].GetString();
		return true;
	}
	return false;
}

bool DASEntry::GetManager(std::string &out) {
	if(entry["data"].HasMember("manager")) {
		out = entry["data"]["manager"].GetString();
		return true;
	}
	return false;
}

bool DASEntry::GetDocumentation(std::string &out) {
	if(entry["data"].HasMember("documentation")) {
		out = entry["data"]["documentation"].GetString();
		return true;
	}
	return false;
}

bool DASEntry::GetNetworkVersion(std::string &out) {
	if(entry["data"].HasMember("networkVersion")) {
		out = entry["data"]["networkVersion"].GetString();
		return true;
	}
	return false;
}

bool DASEntry::GetAccount(std::string &out) {
	if(entry["data"].HasMember("account")) {
		out = entry["data"]["account"].GetString();
		return true;
	}
	return false;
}