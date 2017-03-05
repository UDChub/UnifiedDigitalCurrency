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
 * @file util.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <algorithm>
#include <chrono>
#include <ctime>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

#include "includes/cryptopp/base64.h"
#include "includes/cryptopp/filters.h"

#include "util.h"


std::string Util::string_to_hex(const std::string &input) {
	std::string output;

	size_t len = input.length();
	output.reserve(2 * len);

	for (size_t i = 0; i < len; ++i) {
		const unsigned char c = input[i];
		output.push_back(hexa[c >> 4]);
		output.push_back(hexa[c  &15]);
	}
	return output;
}

std::string Util::hex_to_string(const std::string &input) {
	std::string output;

	size_t len = input.length();
	output.reserve(len / 2);

	for (size_t i = 0; i < len; i += 2) {
		char a = input[i];
		char b = input[i + 1];

		const char* p = std::lower_bound(hexa, hexa + 16, a);
		const char* q = std::lower_bound(hexa, hexa + 16, b);

		output.push_back(((p - hexa) << 4) | (q - hexa));
	}
	return output;
}

std::string Util::string_to_base64(std::string input) {
	std::string output;
	std::vector<unsigned char> bytes(input.begin(), input.end());

	CryptoPP::Base64Encoder encoder(NULL, false);
	encoder.Put(bytes.data(), bytes.size());
	encoder.MessageEnd();

	CryptoPP::word64 size = encoder.MaxRetrievable();
	if(size)
	{
		output.resize(size);	   
		encoder.Get((byte*) output.data(), output.size());
	}
	return output;
}


std::string Util::base64_to_string(std::string input) {
	std::string output;
    std::vector<unsigned char> bytes(input.begin(), input.end());

	CryptoPP::Base64Decoder decoder;
	decoder.Put((byte*) bytes.data(), bytes.size());
	decoder.MessageEnd();

	CryptoPP::word64 size = decoder.MaxRetrievable();
	if(size && size <= SIZE_MAX)
	{
		output.resize(size);
		decoder.Get((byte*) output.data(), output.size());
	}
	return output;
}

void Util::string_to_array(std::string input, std::vector<char> &output) {
	for(char c: input) output.push_back(c);
}

void Util::string_to_array(std::string input, uint size, uint offset, std::vector<char> &output) {
	for(uint i = 0; i < size; i++) {
		if(i+offset < output.size()) output[i+offset] = input[i];
		else {
			while(output.size() < i+offset) output.push_back(0);
			output.push_back(input[i]);
		}
	}
}

std::string Util::array_to_string(std::vector<char> &input, uint size, uint offset) {
	std::string output;
	for(uint i = offset; i < offset+size && i < input.size(); i++) output += input[i];
	return output;
}

// template <typename T>
// void Util::int_to_array(T input, uint size, uint offset, std::vector<char> &output) {
// 	for(uint i = 0; i < size; i++) {
// 		if(i+offset < output.size()) output[i+offset] = input >> (i*8);
// 		else {
// 			while(output.size() < i+offset) output.push_back(0);
// 			output.push_back(input >> (i*8));
// 		}
// 	}
// }

// template <typename T>
// void Util::array_to_int(std::vector<char> &input, uint size, uint offset, T &output) {
// 	output = 0;
// 	for(uint i = 0; i < size && i < input.size(); i++) output = output | ((unsigned char)input[i+offset] << (i*8));
// }

uint Util::current_timestamp() {
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

uint64_t Util::current_timestamp_nanos() {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string Util::timestamp_to_date(uint64_t input) {
	long seconds;

	//trim to seconds
	if(input > 10000000000) seconds = input/(1000*1000*1000);
	else seconds = input;

	time_t timer = time(&seconds);
	struct tm date = *localtime(&timer);

	//extract date
	std::stringstream output;
	output << (date.tm_year + 1900) << '-' << (date.tm_mon + 1) << '-' <<  date.tm_mday << ", ";
	output << date.tm_hour << ":" << date.tm_min << ":" << date.tm_sec << std::endl;

	return output.str();
}

std::string Util::timestamp_to_remaining(uint64_t input) {
	int minutes = 0, seconds = 0;

	//extract difference in seconds
	if(input > 10000000000) input = (input-current_timestamp_nanos())/(1000*1000*1000);
	else input = input - current_timestamp();

	minutes = input/60;
	seconds = input%60;

	std::stringstream output;
	output << minutes << "m" << seconds << "s";
	return output.str();
}