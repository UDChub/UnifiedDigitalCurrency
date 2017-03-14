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
 * @file network.h
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <string>

#include "includes/boost/asio.hpp"


namespace Network {

	bool RegisterWithWorldBank(std::string publicKey, std::string challenge, std::string& signature, std::string& timestamp);
	// void RegisterWithAll(std::string publicKey, std::string wbSignature, std::string timestamp);

	bool OpenConnection(boost::asio::ip::tcp::socket &socket, bool hostname, std::string host, int port);
	bool Identify(boost::asio::ip::tcp::socket &socket, int protocolCode, std::string id);

	bool WriteMessage(boost::asio::ip::tcp::socket &socket, int protocolCode, std::string message, bool sign);
	bool WriteMessage(boost::asio::ip::tcp::socket &socket, int protocolCode, std::string content, std::string variableContent, int variableLength, bool variableFirst, bool sign);
	bool SendFile(boost::asio::ip::tcp::socket &socket, int protocolCode, std::string file);
	bool SendKeepAlive(boost::asio::ip::tcp::socket &socket);

	bool ConnectToTracker(boost::asio::ip::tcp::socket &socket);

	bool ResolveHost(std::string host, std::string &ip, uint &port);
}

#endif