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
 * @file entry_passport.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <string>

#include "codes.h"
#include "entry_passport.h"


bool PassportEntry::IsEntry() {

	// if(!entry["data"].HasMember("id") || !entry["data"]["id"].IsString()) return false;
	// dataOrder.push_back("id");
	if(!entry["data"].HasMember("identification") || !entry["data"]["identification"].IsString()) return false;
	dataOrder.push_back("identification");

	//Verify format according to the different Entry types
	std::string type = entry["meta"]["type"].GetString();
	
	if(type == ENTRY_TYPE_CREATE) {
		if(entry["data"].HasMember("publicKey") && entry["data"]["publicKey"].IsString()) {

			//verify optional fields
			if(entry["data"].HasMember("designation")) {
				if(entry["data"]["designation"].IsString()) dataOrder.push_back("designation");
				else return false;
			}
			if(entry["data"].HasMember("company")) {
				if(entry["data"]["company"].IsString()) dataOrder.push_back("company");
				else return false;
			}
			if(entry["data"].HasMember("website")) {
				if(entry["data"]["website"].IsString()) dataOrder.push_back("website");
				else return false;
			}
			if(entry["data"].HasMember("certificate")) {
				if(entry["data"]["certificate"].IsString()) dataOrder.push_back("certificate");
				else return false;
			}

			//insert publicKey attribute in the right position
			dataOrder.push_back("publicKey");

			if(entry["data"].HasMember("account")) {
				if(entry["data"]["account"].IsString()) dataOrder.push_back("account");
				else return false;
			}
			if(entry["data"].HasMember("representative")) {
				if(entry["data"]["representative"].IsString()) dataOrder.push_back("representative");
				else return false;
			}
			if(entry["data"].HasMember("email")) {
				if(entry["data"]["email"].IsString()) dataOrder.push_back("email");
				else return false;
			}
			if(entry["data"].HasMember("contact")) {
				if(entry["data"]["contact"].IsString()) dataOrder.push_back("contact");
				else return false;
			}
			if(entry["data"].HasMember("country")) {
				if(entry["data"]["country"].IsString()) dataOrder.push_back("country");
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
		if(entry["data"].HasMember("company")) {
			if(entry["data"]["company"].IsString()) dataOrder.push_back("company");
			else return false;
		}
		if(entry["data"].HasMember("website")) {
			if(entry["data"]["website"].IsString()) dataOrder.push_back("website");
			else return false;
		}
		if(entry["data"].HasMember("certificate")) {
			if(entry["data"]["certificate"].IsString()) dataOrder.push_back("certificate");
			else return false;
		}
		if(entry["data"].HasMember("publicKey")) {
			if(entry["data"]["publicKey"].IsString()) dataOrder.push_back("publicKey");
			else return false;
		}
		if(entry["data"].HasMember("account")) {
			if(entry["data"]["account"].IsString()) dataOrder.push_back("account");
			else return false;
		}
		if(entry["data"].HasMember("representative")) {
			if(entry["data"]["representative"].IsString()) dataOrder.push_back("representative");
			else return false;
		}
		if(entry["data"].HasMember("email")) {
			if(entry["data"]["email"].IsString()) dataOrder.push_back("email");
			else return false;
		}
		if(entry["data"].HasMember("contact")) {
			if(entry["data"]["contact"].IsString()) dataOrder.push_back("contact");
			else return false;
		}
		if(entry["data"].HasMember("country")) {
			if(entry["data"]["country"].IsString()) dataOrder.push_back("country");
			else return false;
		}
		
		//Update type requires at least one field being modified
		return ((type == "update" && dataOrder.size() > 1) || type == "renew");
	}
	else if(type == ENTRY_TYPE_CANCEL) {
		if(!entry["data"].HasMember("designation") && !entry["data"].HasMember("company") && !entry["data"].HasMember("website") && 
			!entry["data"].HasMember("certificate") && !entry["data"].HasMember("publicKey") && !entry["data"].HasMember("account") && 
			!entry["data"].HasMember("representative") && !entry["data"].HasMember("email") && !entry["data"].HasMember("contact") && 
			!entry["data"].HasMember("counry")) 
			return true;
	}

	return false;
}

std::string PassportEntry::GetPassport() {
	// return entry["data"]["id"].GetString();
	return entry["data"]["identification"].GetString();
}

bool PassportEntry::GetDesignation(std::string &out) {
	if(entry["data"].HasMember("designation")) {
		out = entry["data"]["designation"].GetString();
		return true;
	}
	return false;
}

bool PassportEntry::GetCompany(std::string &out) {
	if(entry["data"].HasMember("company")) {
		out = entry["data"]["company"].GetString();
		return true;
	}
	return false;
}

bool PassportEntry::GetWebsite(std::string &out) {
	if(entry["data"].HasMember("website")) {
		out = entry["data"]["website"].GetString();
		return true;
	}
	return false;
}

bool PassportEntry::GetCertificate(std::string &out) {
	if(entry["data"].HasMember("certificate")) {
		out = entry["data"]["certificate"].GetString();
		return true;
	}
	return false;
}

bool PassportEntry::GetPublicKey(std::string &out) {
	if(entry["data"].HasMember("publicKey")) {
		out = entry["data"]["publicKey"].GetString();
		return true;
	}
	return false;
}

bool PassportEntry::GetAccount(std::string &out) {
	if(entry["data"].HasMember("account")) {
		out = entry["data"]["account"].GetString();
		return true;
	}
	return false;
}

bool PassportEntry::GetRepresentative(std::string &out) {
	if(entry["data"].HasMember("representative")) {
		out = entry["data"]["representative"].GetString();
		return true;
	}
	return false;
}

bool PassportEntry::GetEmail(std::string &out) {
	if(entry["data"].HasMember("email")) {
		out = entry["data"]["email"].GetString();
		return true;
	}
	return false;
}

bool PassportEntry::GetContact(std::string &out) {
	if(entry["data"].HasMember("contact")) {
		out = entry["data"]["contact"].GetString();
		return true;
	}
	return false;
}

bool PassportEntry::GetCountry(std::string &out) {
	if(entry["data"].HasMember("country")) {
		out = entry["data"]["country"].GetString();
		return true;
	}
	return false;
}