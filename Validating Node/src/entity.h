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
 * @file entity.h
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#ifndef ENTITY_H
#define ENTITY_H

#include <mutex>
#include <string>
#include <unordered_map>

#include "includes/boost/asio.hpp"

#include "globals.h"

class ThreadsManager;


struct EntityStruct {
	std::string entityId;
	std::string publicKey;
	std::string host;
	std::string ip;
	uint port;
	boost::asio::ip::tcp::socket *socket;
};

class Entities {
	public:
		Entities(std::unordered_map<std::string, EntityStruct> entitiesData);
		
		void Connect(ThreadsManager *threads);
		void Stop();

		void NewEntity(std::string entityId, std::string publicKey, std::string host, bool connect=true);
		void RemoveEntity(std::string entityId);
		bool AddEntity(std::string entityId, boost::asio::ip::tcp::socket &socket);

		bool GetPublicKey(std::string entityId, std::string &publicKey);
		bool SetPublicKey(std::string entityId, std::string publicKey);
		bool SetHost(std::string entityId, std::string host, bool connect=true);

		void PublicKeyRequest(std::string entityId, std::string account);
		void TransactionReply(std::string entityId, std::string hash, int errorCode);
		bool RequestBlock(std::string entityId, std::string hash);
		bool BroadcastConfirmation(std::string hash, int confirmationType=CONFIRMATION_BLOCK);
		void BroadcastEntry(std::string entry);

	private:
		std::mutex dataMutex;
		ThreadsManager *threads;
		std::unordered_map<std::string, EntityStruct> entitiesData;

};

#endif
