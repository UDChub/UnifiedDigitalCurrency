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
 * @file entry_resource.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <string>

#include "codes.h"
#include "entry_resource.h"


bool ResourceEntry::IsEntry() {
	//Check that it specifies a resource
	if(!entry["data"].HasMember("designation") || !entry["data"]["designation"].IsString()) return false;

	dataOrder.push_back("designation");

	//Verify format according to the different Entry types
	std::string type = entry["meta"]["type"].GetString();
	
	if(type == ENTRY_TYPE_CREATE) {
		if(entry["data"].HasMember("supervisor") && entry["data"]["supervisor"].IsString() && entry["data"].HasMember("protection") && entry["data"]["protection"].IsString()) {
			dataOrder.push_back("supervisor");
			dataOrder.push_back("protection");

			//verify optional fields
			if(entry["data"].HasMember("identifier")) {
				if(entry["data"]["identifier"].IsArray()) dataOrder.push_back("identifier");
				else return false;
			}
			if(entry["data"].HasMember("operation")) {
				if(entry["data"]["operation"].IsArray()) dataOrder.push_back("operation");
				else return false;
			}

			return true;
		}
	}
	else if(type == ENTRY_TYPE_UPDATE || type == ENTRY_TYPE_RENEW) {
		//verify optional fields
		if(entry["data"].HasMember("supervisor")) {
			if(entry["data"]["supervisor"].IsArray()) dataOrder.push_back("supervisor");
			else return false;
		}
		if(entry["data"].HasMember("protection")) {
			if(entry["data"]["protection"].IsArray()) dataOrder.push_back("protection");
			else return false;
		}
		if(entry["data"].HasMember("identifier")) {
			if(entry["data"]["identifier"].IsArray()) dataOrder.push_back("identifier");
			else return false;
		}
		if(entry["data"].HasMember("operation")) {
			if(entry["data"]["operation"].IsArray()) dataOrder.push_back("operation");
			else return false;
		}
		
		//Update type requires at least one field being modified
		return ((type == "update" && dataOrder.size() > 1) || type == "renew");
	}
	else if(type == ENTRY_TYPE_CANCEL) {
		if(!entry["data"].HasMember("supervisor") && !entry["data"].HasMember("protection") && 
			!entry["data"].HasMember("identifier") && !entry["data"].HasMember("operation")) 
			return true;
	}

	return false;
}

std::string ResourceEntry::GetDesignation() {
	return entry["data"]["designation"].GetString();
}

bool ResourceEntry::GetSupervisor(std::string &out) {
	if(entry["data"].HasMember("supervisor")) {
		out = entry["data"]["supervisor"].GetString();
		return true;
	}
	return false;
}

bool ResourceEntry::GetProtection(std::string &out) {
	if(entry["data"].HasMember("protection")) {
		out = entry["data"]["protection"].GetString();
		return true;
	}
	return false;
}

std::vector<std::string> ResourceEntry::GetIdentifier() {
	std::vector<std::string> identifiers;
	if(entry["data"].HasMember("identifier")) {
		for(rapidjson::Value::ConstValueIterator it = entry["data"]["identifier"].Begin(); it != entry["data"]["identifier"].End(); ++it) identifiers.push_back(it->GetString());
	}
	return identifiers;
}

std::vector<std::string> ResourceEntry::GetOperation() {
	std::vector<std::string> operations;
	if(entry["data"].HasMember("operation")) {
		for(rapidjson::Value::ConstValueIterator it = entry["data"]["operation"].Begin(); it != entry["data"]["operation"].End(); ++it) operations.push_back(it->GetString());
	}
	return operations;
}
