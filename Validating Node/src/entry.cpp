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
 * @file entry.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <chrono>
#include <string>
#include <utility>
#include <unordered_set>

#include "includes/cryptopp/filters.h"
#include "includes/cryptopp/ripemd.h"
#include "includes/cryptopp/hex.h"
#include "includes/rapidjson/document.h"
#include "includes/rapidjson/stringbuffer.h"
#include "includes/rapidjson/writer.h"

#include "entry.h"
#include "globals.h"
#include "util.h"


Entry::Entry(const char *rawEntry) {
	entry.Parse(rawEntry);
}

bool Entry::IsEntry() {
	//Ensure it is a valid JSON object and has the top-level keys
	if(entry.IsObject() && entry.HasMember("resource") && entry.HasMember("data") && entry.HasMember("meta") && entry.HasMember("signer") && entry.HasMember("signature")){

		//Check the keys data type
		if(entry["resource"].IsString() && entry["data"].IsObject() && entry["meta"].IsObject() && entry["signer"].IsString() && entry["signature"].IsString()) {

			//Check the metadata format
			if(entry["meta"].HasMember("type") && entry["meta"]["type"].IsString() && 
				entry["meta"].HasMember("date") && entry["meta"]["date"].IsUint() && 
				entry["meta"].HasMember("version") && entry["meta"]["version"].IsString()) return true;
		}
	}
	return false;
}

bool Entry::CheckTimestamp() {
	long long timestamp = entry["meta"]["date"].GetUint();
	auto now = Util::current_timestamp();

	if(timestamp < now && (now - ENTRY_DELAY_NEW) < timestamp) return true;
	return false;
}

void Entry::OrderData() {
	rapidjson::Document temp;
	temp.SetObject();

	temp.AddMember("resource", entry["resource"], temp.GetAllocator());

	//order data sub-object, following custom key order
	rapidjson::Value data(rapidjson::kObjectType);
	for(auto it = dataOrder.begin(); it != dataOrder.end(); ++it) Util::GenericCopier(entry["data"], data, it, &temp.GetAllocator());
	temp.AddMember("data", data, temp.GetAllocator());

	//insert the meta sub-object ordered
	rapidjson::Value meta(rapidjson::kObjectType);
	meta.AddMember("type", entry["meta"]["type"], temp.GetAllocator());
	meta.AddMember("date", entry["meta"]["date"], temp.GetAllocator());
	meta.AddMember("version", entry["meta"]["version"], temp.GetAllocator());
	temp.AddMember("meta", meta, temp.GetAllocator());

	temp.AddMember("signer", entry["signer"], temp.GetAllocator());
	temp.AddMember("signature", entry["signature"], temp.GetAllocator());

	std::swap(entry, temp);
}

std::string Entry::MakeHash() {
	std::string content = GetEntry();
	std::string signer = entry["signer"].GetString();
	std::string signature = entry["signature"].GetString();

	//remove global brackets and the signer and signature object's keys
	content = content.substr(1,content.length()-1-signer.length()-signature.length()-28);

	CryptoPP::RIPEMD160 ripemd;
	CryptoPP::StringSource ss(content, true, new CryptoPP::HashFilter(ripemd, new CryptoPP::HexEncoder(new CryptoPP::StringSink(hash))));
	return hash;
}

std::string Entry::GetEntry() {
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	entry.Accept(writer);
	return buffer.GetString();
}

std::string Entry::GetHash() {
	return hash;
}

std::string Entry::GetResource() {
	return entry["resource"].GetString();
}

bool Entry::GetId(std::string &id) {
	if(entry["data"].HasMember("id")) {
		id = entry["data"]["id"].GetString();
		return true;
	}
	return false;
}

std::string Entry::GetType() {
	return entry["meta"]["type"].GetString();
}

uint Entry::GetTimestamp() {
	return entry["meta"]["date"].GetUint();
}

std::string Entry::GetVersion() {
	return entry["meta"]["version"].GetString();
}

std::string Entry::GetSigner() {
	return entry["signer"].GetString();
}

std::string Entry::GetSignature() {
	return entry["signature"].GetString();
}