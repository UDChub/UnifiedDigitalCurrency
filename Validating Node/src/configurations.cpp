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
 * @file configurations.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <fstream>
#include <chrono>
#include <string>
#include <iostream>
#include <thread>
#include <string>

#include "includes/boost/regex.hpp"

#include "configurations.h"
#include "ecdsa.h"
#include "globals.h"
#include "network.h"


bool LoadConfigurations() {
	Configurations config;

	std::fstream file(CONFIGURATION_FILE, std::fstream::in | std::fstream::binary);
	if(!file.good()) return false;

	file.read((char*)&config, sizeof(config));
	file.close();

	_SELF = config.id;
	_PORT = config.port;
	_ECDSA_PRIVATE_KEY = config.privateKey;
	_ECDSA_PUBLIC_KEY = config.publicKey;
	_ACCOUNT = config.account;

	return true;
}

void SaveConfigurations() {
	Configurations config = {_SELF, _PORT, _ECDSA_PRIVATE_KEY, _ECDSA_PUBLIC_KEY, _ACCOUNT};

	std::fstream file(CONFIGURATION_FILE, std::fstream::out | std::fstream::binary | std::fstream::trunc);
	file.write((char*)&config, sizeof(config));
	file.close();
}

bool CreateProfile() {
	bool correct = false;

	boost::regex idRegex(PATTERN_NODE_ID);
	while(!correct) {
		std::string id;
		std::cout << "\n\nPlease enter the identification key of this node (e.g. 4E1B2D3F): ";
		std::cin >> id;
		std::cin.clear();

		if(boost::regex_match(id, idRegex)) {
			correct = true;
			_SELF = id;
		}
		else {
			std::cout << "\nInvalid identification key given: " << id << std::endl;
		}
	}

	boost::regex portRegex(PATTERN_PORT);
	correct = false;
	while(!correct) {
		std::string port;
		std::cout << "\n\nPlease enter the port number to be used by this node (e.g. 4200): ";
		std::cin >> port;
		std::cin.clear();
		std::cin.ignore(10000,'\n');

		if(boost::regex_match(port, portRegex)) {
			correct = true;
			_PORT = stoul(port);
		}
		else {
			std::cout << "\nInvalid port number: " << port << std::endl;
		}
	}

	boost::regex pathRegex(PATTERN_PATH);
	std::string path = "";
	correct = false;
	while(!correct) {
		std::cout << "\n\nPlease specify the path for the private key of this node or leave blank for default (" << _ECDSA_PRIVATE_KEY << "): ";

		std::getline(std::cin, path);
		std::cin.clear();

		if(path.length() == 0) correct = true;
		else if(boost::regex_match(path, pathRegex)) {
			correct = true;
			_ECDSA_PRIVATE_KEY = path;
		}
		else {
			std::cout << "\nInvalid path: " << path << std::endl;
		}
	}

	path = "";
	correct = false;
	while(!correct) {
		std::cout << "\n\nPlease specify the path for the public key of this node or leave blank for default (" << _ECDSA_PUBLIC_KEY << "): ";

		std::getline(std::cin, path);
		std::cin.clear();

		if(path.length() == 0) correct = true;
		else if(boost::regex_match(path, pathRegex)) {
			correct = true;
			_ECDSA_PUBLIC_KEY = path;
		}
		else {
			std::cout << "\nInvalid path: " << path << std::endl;
		}
	}

	boost::regex accRegex(PATTERN_ACCOUNT);
	correct = false;
	while(!correct) {
		std::string account;
		std::cout << "\n\nPlease enter the private account of this node (e.g. UDC1234567) or leave blank if it doesn't have one: ";
		std::cin >> account;
		std::cin.clear();

		if(account.length() == 0) correct = true;
		if(boost::regex_match(account, accRegex)) {
			correct = true;
			_ACCOUNT = account;
		}
		else {
			std::cout << "\nInvalid account number given: " << account << std::endl;
		}
	}

	std::cout << "Profile completed." << std::endl << std::endl;
	SaveConfigurations();

	std::string choice;
	std::cout << "Generate a new key pair and register with WorldBank? [Y/N]\n (WARNING: this will overwrite any existing key, decline if you're already registered)\n";
	std::cin >> choice;
	std::cin.clear();
	std::cin.ignore(10000,'\n');

	if(choice == "Y" || choice == "y" || choice == "yes") {			
		std::cout << "\n\nStarting registration process." << std::endl;

		assert(Crypto::GenerateKeys());
		std::string publicKey;
		assert(Crypto::GetPublicKey(publicKey));

		//Register public key at WorldBank
		std::string challenge;
		std::cout << "\nPlease enter the registration challenge given by the WorldBank: ";
		std::cin >> challenge;
		std::cin.clear();

		std::string signature, timestamp;
		assert(Network::RegisterWithWorldBank(publicKey, challenge, signature, timestamp));
		std::cout << "\n\nRegistration with WorldBank completed. Awaiting dissemination across the network." << std::endl << std::endl;

		// Network::RegisterWithAll(publicKey, signature, timestamp);
		// std::cout << "\n\nRegistration process completed. This node is now validated." << std::endl << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(5));
		std::cout << "Meanwhile, initiation synchronization." << std::endl;
		system("clear");

		return true;
	}
	return false;
}