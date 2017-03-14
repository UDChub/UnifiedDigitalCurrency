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
 * @file transaction.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <chrono>
#include <string>
#include <utility>

#include "includes/cryptopp/filters.h"
#include "includes/cryptopp/ripemd.h"
#include "includes/cryptopp/hex.h"
#include "includes/rapidjson/stringbuffer.h"
#include "includes/rapidjson/writer.h"

#include "modules_interface.h"
#include "transaction.h"
#include "util.h"


Transaction::Transaction(const char *rawTx) {
	Tx.Parse(rawTx);
}

Transaction::Transaction(const Transaction &Tx2) {
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	Tx2.Tx.Accept(writer);
	std::string temp = buffer.GetString();
	Tx.Parse<0>(temp.c_str());
	hash = Tx2.hash;
}

Transaction& Transaction::operator=(Transaction& other) {
	std::swap(Tx, other.Tx);
	std::swap(hash, other.hash);
	return *this;
}

bool Transaction::IsTransaction() {
	if(Tx.IsObject() && Tx.HasMember("type") && Tx["type"].IsString()) return true;
	return false;
}

void Transaction::OrderData() {
	rapidjson::Document temp;
	temp.SetObject();
	for(auto it = dataOrder.begin(); it != dataOrder.end(); ++it) Util::GenericCopier(Tx, temp, it, &temp.GetAllocator());
	std::swap(Tx, temp);
}

bool Transaction::CheckTimestamp() {
	return false;
}

std::string Transaction::MakeHash() {
	CryptoPP::RIPEMD160 ripemd;
	CryptoPP::StringSource ss(GetTransaction(), true, new CryptoPP::HashFilter(ripemd, new CryptoPP::HexEncoder(new CryptoPP::StringSink(hash))));
	return hash;
}

bool Transaction::Process(ModulesInterface *interface, int &errorCode) {
	return false;
}

std::string Transaction::GetTransaction() {
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	Tx.Accept(writer);
	return buffer.GetString();
}

std::string Transaction::GetHash() {
	return hash;
}

std::string Transaction::GetType() {
	return Tx["type"].GetString();
}

uint64_t Transaction::GetTimestamp() {
	return Tx["timestamp"].GetUint64();
}

uint64_t Transaction::GetFees() {
	return 0;
}