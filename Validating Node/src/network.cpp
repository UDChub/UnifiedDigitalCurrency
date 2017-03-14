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
 * @file network.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <mutex>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

#include "includes/boost/asio.hpp"
#include "includes/boost/regex.hpp"

#include "codes.h"
#include "configurations.h"
#include "ecdsa.h"
#include "globals.h"
#include "network.h"
#include "node.h"
#include "transaction.h"
#include "util.h"


bool Network::RegisterWithWorldBank(std::string publicKey, std::string challenge, std::string& signature, std::string& timestamp) {	
	boost::system::error_code error;
	boost::asio::ip::tcp::socket socket(NETWORK_SOCKET_SERVICE);
	boost::asio::ip::tcp::resolver resolver(NETWORK_SOCKET_SERVICE);

	boost::asio::ip::tcp::resolver::query query(WORLD_BANK.first, WORLD_BANK.second);
	boost::asio::ip::tcp::resolver::iterator endpointIt = resolver.resolve(query, error);
	boost::asio::ip::tcp::resolver::iterator end;

	//establish a connection with the World Bank
	while(error && endpointIt != end) {
		socket.close();
  		socket.connect(*endpointIt++, error);
	}
	if(error) return false;

	//sign the challenge
	signature = Crypto::Sign(challenge);

	//uint data_length = NETWORK_CODE_LENGTH+NETWORK_DATA_LENGTH+NODE_ID_LENGTH+NETWORK_PUBLIC_KEY_LENGTH+publicKey.length()+NETWORK_SIGNATURE_LENGTH+signature.length();
	std::vector<char> message;
	int offset = 0;

	//add protocol code
	Util::int_to_array(NETWORK_REGISTRATION, NETWORK_CODE_LENGTH, offset, message);
	offset += NETWORK_CODE_LENGTH;

	//add data total length
	Util::int_to_array(NODE_ID_LENGTH+NETWORK_PUBLIC_KEY_LENGTH+publicKey.length()+NETWORK_SIGNATURE_LENGTH+signature.length(), NETWORK_DATA_LENGTH, offset, message);
	offset += NETWORK_DATA_LENGTH;

	//add public key's length
	Util::int_to_array(publicKey.length(), NETWORK_PUBLIC_KEY_LENGTH, offset, message);
	offset += NETWORK_PUBLIC_KEY_LENGTH;
	//add public key
	Util::string_to_array(publicKey, message);
	offset += publicKey.length();

	//add signature length
	Util::int_to_array(signature.length(), NETWORK_SIGNATURE_LENGTH, offset, message);
	offset += NETWORK_SIGNATURE_LENGTH;
	//add signature
	Util::string_to_array(signature, message);
	offset += signature.length();

	//send message
	boost::asio::write(socket, boost::asio::buffer(message), error);
	if(error) return false;

	//read response
	std::vector<char> code(NETWORK_CODE_LENGTH);
	int n = socket.read_some(boost::asio::buffer(code), error);
	if(error || n != NETWORK_CODE_LENGTH) return false;
	uint16_t protocolCode;
	Util::array_to_int(code, NETWORK_CODE_LENGTH, 0, protocolCode);

	//check if positive response
	if(protocolCode != NETWORK_REGISTRATION_SUCCESS) return false;

	//retrieve response's content length
	std::vector<char> total_length(NETWORK_DATA_LENGTH);
	n = socket.read_some(boost::asio::buffer(total_length), error);
	if(error || n != NETWORK_DATA_LENGTH) return false;
	uint16_t length;
	Util::array_to_int(total_length, NETWORK_DATA_LENGTH, 0, length);

	std::vector<char> response(length);
	n = socket.read_some(boost::asio::buffer(response), error);
	if(error || n != NETWORK_DATA_LENGTH) return false;

	//get signature length
	offset = 0;
	uint16_t sig_length;
	Util::array_to_int(response, NETWORK_SIGNATURE_LENGTH, offset, sig_length);
	offset += NETWORK_SIGNATURE_LENGTH;

	//retrieve signature
	signature = Util::array_to_string(response, sig_length, offset);

	//verify signature
	///can't verify from here, since no access to keysDB nor have yet registered the genesis block

	return true;
}

// void Network::RegisterWithAll(std::string publicKey, std::string wbSignature, std::string timestamp) {
// 	//create registration message
// 	//uint data_length = NETWORK_CODE_LENGTH+NETWORK_DATA_LENGTH+NODE_ID_LENGTH+NETWORK_PUBLIC_KEY_LENGTH+publicKey.length()+NETWORK_SIGNATURE_LENGTH+wbSignature.length()+NETWORK_TIMESTAMP_LENGTH;
// 	std::vector<char> message;
// 	int offset = 0;

// 	//add protocol code
// 	Util::int_to_array(NETWORK_NEW_NODE, NETWORK_CODE_LENGTH, offset, message);
// 	offset += NETWORK_CODE_LENGTH;

// 	//add content length
// 	Util::int_to_array(NODE_ID_LENGTH+NETWORK_PUBLIC_KEY_LENGTH+publicKey.length()+NETWORK_SIGNATURE_LENGTH+wbSignature.length()+NETWORK_TIMESTAMP_LENGTH, NETWORK_DATA_LENGTH, offset, message);
// 	offset += NETWORK_DATA_LENGTH;

// 	//add own ID
// 	Util::string_to_array(_SELF, message);
// 	offset += NODE_ID_LENGTH;

// 	//add public key's length
// 	Util::int_to_array(publicKey.length(), NETWORK_PUBLIC_KEY_LENGTH, offset, message);
// 	offset += NETWORK_PUBLIC_KEY_LENGTH;
// 	//add public key
// 	Util::string_to_array(publicKey, message);
// 	offset += publicKey.length();

// 	//add signature length
// 	Util::int_to_array(wbSignature.length(), NETWORK_SIGNATURE_LENGTH, offset, message);
// 	offset += NETWORK_SIGNATURE_LENGTH;
// 	//add signature
// 	Util::string_to_array(wbSignature, message);
// 	offset += wbSignature.length();

// 	//add timestamp used
// 	Util::string_to_array(timestamp, message);

// 	boost::system::error_code error;
// 	std::string id, key, ip, name;
// 	int port, reputation;

// 	//contact known Validating Nodes
// 	std::fstream localData(LOCAL_DATA_NODES, std::fstream::in);
// 	if(localData.good()) {
// 		while(localData >> id >> key >> ip >> port >> reputation) {
// 			boost::asio::ip::tcp::socket socket(NETWORK_SOCKET_SERVICE);
// 			if(OpenConnection(socket, false, ip, port)) boost::asio::write(socket, boost::asio::buffer(message), error);
// 		}
// 	}
// 	localData.close();

// 	//contact known Signing Nodes
// 	localData.open(LOCAL_DATA_ENTITIES);
// 	if(localData.good()) {
// 		while(localData >> id >> name >> key >> ip >> port) {
// 			boost::asio::ip::tcp::socket socket(NETWORK_SOCKET_SERVICE);
// 			if(OpenConnection(socket, false, ip, port)) boost::asio::write(socket, boost::asio::buffer(message), error);
// 		}
// 	}
// 	localData.close();
// }

bool Network::OpenConnection(boost::asio::ip::tcp::socket &socket, bool hostname, std::string host, int port) {
	boost::system::error_code error;

	//create endpoint from ip address and port of the peer node
	if(!hostname) {
		boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(host), port);
		socket.connect(endpoint, error);
	}
	else {
		boost::asio::ip::tcp::resolver resolver(NETWORK_SOCKET_SERVICE);
		boost::asio::ip::tcp::resolver::query query(host, std::to_string(port));
		auto endpointIt = resolver.resolve(query, error);
		//if host was not found, return false
		if(error) return false;

		socket.connect(*endpointIt, error);
	}

	//verifies if an error occured
	if(error) return false;

	//adds the keep_alive option for managing the active nodes
	boost::asio::socket_base::keep_alive option(true);
	socket.set_option(option);

	return true;
}

bool Network::Identify(boost::asio::ip::tcp::socket &socket, int protocolCode, std::string id) {
	std::string now = std::to_string(Util::current_timestamp());
	std::string signature = Crypto::Sign(id+now);

	//set message content
	std::vector<char> message;

	//add network protocol code
	Util::int_to_array(protocolCode, NETWORK_CODE_LENGTH, 0, message);
	uint offset = NETWORK_CODE_LENGTH+NETWORK_DATA_LENGTH;

	//add own ID
	Util::string_to_array(_SELF, NODE_ID_LENGTH, offset, message);
	offset += NODE_ID_LENGTH;

	//add signature length
	Util::int_to_array(signature.length(), NETWORK_SIGNATURE_LENGTH, offset, message);
	offset += NETWORK_SIGNATURE_LENGTH;

	//add signature
	Util::string_to_array(signature, signature.length(), offset, message);
	offset += signature.length();

	//add timestamp used
	Util::string_to_array(now, NETWORK_TIMESTAMP_LENGTH, offset, message);
	offset += NETWORK_TIMESTAMP_LENGTH;

	//set content length
	uint length = NODE_ID_LENGTH+NETWORK_SIGNATURE_LENGTH+signature.length()+NETWORK_TIMESTAMP_LENGTH;
	Util::int_to_array(length, NETWORK_DATA_LENGTH, NETWORK_CODE_LENGTH, message);

	//Send message
	boost::system::error_code error;
	boost::asio::write(socket, boost::asio::buffer(message), error);
	if(error) return false;

	//Receive response
	std::vector<char> code(NETWORK_CODE_LENGTH);
	int n = socket.read_some(boost::asio::buffer(code), error);

	if(!error && n == NETWORK_CODE_LENGTH) {
		uint16_t protocolCode;
		Util::array_to_int(code, NETWORK_CODE_LENGTH, 0, protocolCode);
		if(protocolCode == NETWORK_IDENTIFICATION_SUCCESS) return true;
	}
	return false;
}

bool Network::WriteMessage(boost::asio::ip::tcp::socket &socket, int protocolCode, std::string content, bool sign) {
	std::vector<char> message;

	//add protocol code
	Util::int_to_array(protocolCode, NETWORK_CODE_LENGTH, 0, message);
	uint offset = NETWORK_CODE_LENGTH;

	if(sign) {
		//Sign content
		std::string signature(Crypto::Sign(content));

		//add data total length
		Util::int_to_array(NETWORK_SIGNATURE_LENGTH+signature.length()+content.length(), NETWORK_DATA_LENGTH, offset, message);
		offset += NETWORK_DATA_LENGTH;

		//add signature length
		Util::int_to_array(signature.length(), NETWORK_SIGNATURE_LENGTH, offset, message);
		offset += NETWORK_SIGNATURE_LENGTH;
		//add signature
		Util::string_to_array(signature, signature.length(), offset, message);
		offset += signature.length();
	}
	else {
		//add data total length
		Util::int_to_array(content.length(), NETWORK_DATA_LENGTH, offset, message);
		offset += NETWORK_DATA_LENGTH;
	}
	//add content
	Util::string_to_array(content, content.length(), offset, message);

	//send the message to the peer node
	boost::system::error_code error;
	boost::asio::write(socket, boost::asio::buffer(message), boost::asio::transfer_all(), error);

	//verify it there was an error
	return !error;
}

bool Network::WriteMessage(boost::asio::ip::tcp::socket &socket, int protocolCode, std::string content, std::string variableContent, int variableLength, bool variableFirst, bool sign) {
	std::vector<char> message;

	//add protocol code
	Util::int_to_array(protocolCode, NETWORK_CODE_LENGTH, 0, message);
	uint offset = NETWORK_CODE_LENGTH;

	if(sign) {
		//Sign content
		std::string signature(Crypto::Sign(content));

		//add data total length
		Util::int_to_array(NETWORK_SIGNATURE_LENGTH+signature.length()+content.length()+variableLength+variableContent.length(), NETWORK_DATA_LENGTH, offset, message);
		offset += NETWORK_DATA_LENGTH;

		//add signature length
		Util::int_to_array(signature.length(), NETWORK_SIGNATURE_LENGTH, offset, message);
		offset += NETWORK_SIGNATURE_LENGTH;
		//add signature
		Util::string_to_array(signature, signature.length(), offset, message);
		offset += signature.length();
	}
	else {
		//add data total length
		Util::int_to_array(content.length()+variableLength+variableContent.length(), NETWORK_DATA_LENGTH, offset, message);
		offset += NETWORK_DATA_LENGTH;
	}

	//add message's content
	if(variableFirst) {
		//add variable content's length
		Util::int_to_array(variableContent.length(), variableLength, offset, message);
		offset += variableLength;

		//add variable content
		Util::string_to_array(variableContent, variableContent.length(), offset, message);
		offset += variableContent.length();

		//add content
		Util::string_to_array(content, content.length(), offset, message);
	}
	else {
		//add content
		Util::string_to_array(content, content.length(), offset, message);
		offset += content.length();
	}

	//send the message to the peer node
	boost::system::error_code error;
	boost::asio::write(socket, boost::asio::buffer(message), error);

	//verify it there was an error
	return !error;
}

bool Network::SendFile(boost::asio::ip::tcp::socket &socket, int protocolCode, std::string file) {
	std::fstream data(file, std::fstream::in);
	if(data.good()) {
		boost::system::error_code error;
		std::vector<char> header;

		//sign file
		data.close();
		std::string signature = Crypto::Sign(file, true);

		//add protocol code
		Util::int_to_array(protocolCode, NETWORK_CODE_LENGTH, 0, header);
		uint offset = NETWORK_CODE_LENGTH;

		//add data total length
		data.open(file, std::fstream::in);
		data.seekg(0, data.end);
		Util::int_to_array(NETWORK_SIGNATURE_LENGTH+signature.length()+NETWORK_FILE_LENGTH+data.tellg(), NETWORK_DATA_LENGTH_BIG, offset, header);
		offset += NETWORK_DATA_LENGTH_BIG;

		//add signature length
		Util::int_to_array(signature.length(), NETWORK_SIGNATURE_LENGTH, offset, header);
		offset += NETWORK_SIGNATURE_LENGTH;
		//add signature
		Util::string_to_array(signature, signature.length(), offset, header);
		offset += signature.length();

		//add file size
		Util::int_to_array(data.tellg(), NETWORK_FILE_LENGTH, offset, header);

		//send header
		boost::asio::write(socket, boost::asio::buffer(header), error);
		if(error) return false;

		//send file in 4KB blocks of data
		char block[4096];

		data.seekg(0, data.beg);
		while(!data.eof()) {
			data >> block;
			boost::asio::write(socket, boost::asio::buffer(block, 4096), error);
			if(error) return false;
		}

		data.close();
		return true;
	}
	data.close();
	return false;
}

bool Network::SendKeepAlive(boost::asio::ip::tcp::socket &socket) {
	boost::system::error_code error;

	//send keep alive network code
	boost::asio::write(socket, boost::asio::buffer((void*) NETWORK_KEEP_ALIVE, NETWORK_CODE_LENGTH), error);
	return !error;
}

bool Network::ConnectToTracker(boost::asio::ip::tcp::socket &socket) {
    boost::asio::ip::tcp::resolver resolver(NETWORK_SOCKET_SERVICE);
    boost::system::error_code error;

    std::mutex networkMutex;
	std::lock_guard<std::mutex> lock(networkMutex);
	
	for(std::pair<std::string, std::string> host: TRACKERS) {
    	error = boost::asio::error::host_not_found;
    	
		boost::asio::ip::tcp::resolver::query query(host.first, host.second);
		boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		boost::asio::ip::tcp::resolver::iterator end;

		while(error && endpoint_iterator != end) {
			socket.close();
      		socket.connect(*endpoint_iterator++, error);
		}

		return !error;
	}
	return false;
}

bool Network::ResolveHost(std::string host, std::string &ip, uint &port) {
	//Extract data from host
	auto pos = host.find(":");
	if(pos == std::string::npos) return false;
	std::string address = host.substr(0,pos);

	//Retrieve port
	port = std::stoul(host.substr(pos+1, host.length()-1));

	boost::regex ipRegex(PATTERN_IPV4);

	//Retrieve IP address or resolve from hostname
	if(boost::regex_match(address, ipRegex)) ip = address;
	else {
		boost::asio::ip::tcp::resolver resolver(NETWORK_SOCKET_SERVICE);
		boost::asio::ip::tcp::resolver::query query(address, "");
		auto endpointIt = resolver.resolve(query);
		//Check if an endpoint was found
		if(endpointIt == boost::asio::ip::tcp::resolver::iterator()) return false;
		ip = endpointIt->endpoint().address().to_string();
	}
	return true;
}