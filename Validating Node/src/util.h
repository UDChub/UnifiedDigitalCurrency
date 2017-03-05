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
 * @file util.h
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <unordered_set>
#include <vector>

#include "includes/rapidjson/document.h"


namespace Util {

	static const char* hexa = "0123456789ABCDEF";

	std::string string_to_hex(const std::string &input);
	std::string hex_to_string(const std::string &input);

	std::string string_to_base64(std::string input);
	std::string base64_to_string(std::string input);

	void string_to_array(std::string input, std::vector<char> &output);
	void string_to_array(std::string input, uint size, uint offset, std::vector<char> &output);
	std::string array_to_string(std::vector<char> &input, uint size, uint offset);

	template <typename T>
	void int_to_array(T input, uint size, uint offset, std::vector<char> &output) {
		for(uint i = 0; i < size; i++) {
			if(i+offset < output.size()) output[i+offset] = input >> (i*8);
			else {
				while(output.size() < i+offset) output.push_back(0);
				output.push_back(input >> (i*8));
			}
		}
	}
	template <typename T>
	void array_to_int(std::vector<char> &input, uint size, uint offset, T &output) {
		output = 0;
		for(uint i = 0; i < size && i < input.size(); i++) output = output | ((unsigned char)input[i+offset] << (i*8));
	}

	uint current_timestamp();
	uint64_t current_timestamp_nanos();
	
	std::string timestamp_to_date(uint64_t input);
	std::string timestamp_to_remaining(uint64_t input);


	//Generic Method for copying JSON values between rapidjson::Docucment and/or rapidjson::Value
	template<typename T1, typename T2, typename T3, typename T4>
	void GenericCopier(T1 &from, T2 &to, T3 &iterator, T4 *allocator) {
		rapidjson::Value key;
		key.SetString(iterator->c_str(), *allocator);

		switch(from[key].GetType()) {
			//Sub-Objects
			case rapidjson::kObjectType: {
				rapidjson::Value sub(rapidjson::kObjectType);
				sub.SetObject();
				//Recuperate the sub-object's keys
				std::unordered_set<std::string> subKeys;
				for(auto it = from[key].MemberBegin(); it != from[key].MemberEnd(); ++it) subKeys.insert(it->name.GetString());

				//Copy the sub-object's keys while the iterator points to one of them
				while(subKeys.size()) {
					++iterator;
					auto subIt = subKeys.find(*iterator);
					if(subIt == subKeys.end()) {
						//key not found, corresponds to another attribute
						--iterator;
						break;
					}
					//nested copy of the sub-object's keys
					GenericCopier(from[key], sub, iterator, allocator);
					subKeys.erase(*iterator);
				}

				//Append the sub-object under the initial key name
				to.AddMember(key, sub, *allocator);
				break;
			}

			//Arrays
			case rapidjson::kArrayType: {
				rapidjson::Value array(rapidjson::kArrayType);

				//retrieve all items
				for(int i = 0; i < from[key].Size(); i++) {
					if(from[key][i].GetType() == rapidjson::kStringType) {
						std::string item = from[key][i].GetString();
						array.PushBack(rapidjson::Value(item.c_str(), item.length(), *allocator).Move(), *allocator);
					}
					else array.PushBack(from[key][i], *allocator);
				}

				//Append new array under the corresponding key name
				to.AddMember(key, array, *allocator);
				break;
			}

			//Strings
			case rapidjson::kStringType: {
				std::string value = from[key].GetString();
				to.AddMember(key, rapidjson::Value(value.c_str(), value.length(), *allocator).Move(), *allocator);
				break;
			}

			//All other types
			default: {
				to.AddMember(key, from[key], *allocator);
				break;
			}
		}
	}

}

#endif