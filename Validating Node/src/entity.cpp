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
 * @file entity.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <exception>
#include <mutex>
#include <string>
#include <fstream>
#include <unordered_map>
#include <utility>

#include "includes/boost/asio.hpp"
#include "includes/boost/filesystem.hpp"

#include "codes.h"
#include "ecdsa.h"
#include "entity.h"
#include "globals.h"
#include "network.h"
#include "threads_manager.h"


Entities::Entities(std::unordered_map<std::string, EntityStruct> entitiesData) : entitiesData(entitiesData) {
}

void Entities::Stop() {	
	std::lock_guard<std::mutex> lock(dataMutex);

	for(auto it = entitiesData.begin(); it != entitiesData.end(); ++it) {
		if(it->second.socket && it->second.socket->is_open()) {
			Network::WriteMessage(*it->second.socket, NETWORK_EXIT, "", false);
			it->second.socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
			it->second.socket->close();
		}
	}
}

void Entities::Connect(ThreadsManager *threads) {
	this->threads = threads;

	for(auto it = entitiesData.begin(); it != entitiesData.end(); ++it) {
		//Extract data from host
		if(!Network::ResolveHost(it->second.host, it->second.ip, it->second.port)) continue;

		//Connect and launch listening thread
		if(Network::OpenConnection(*it->second.socket, false, it->second.ip, it->second.port) && Network::Identify(*it->second.socket, NETWORK_IDENTIFICATION_ENTITY, it->first)) {
			threads->NewEntityThread(&entitiesData[it->first]);
		}
	}
}

void Entities::NewEntity(std::string entityId, std::string publicKey, std::string host, bool connect /*=true*/) {
	EntityStruct entity;
	entity.entityId = entityId;
	entity.publicKey = publicKey;
	entity.host = host;

	std::lock_guard<std::mutex> lock(dataMutex);
	entitiesData[entityId] = std::move(entity);

	//Extract data from host
	if(!Network::ResolveHost(host, entitiesData[entityId].ip, entitiesData[entityId].port)) return;

	//Connect and launch listening thread
	if(connect && Network::OpenConnection(*entitiesData[entityId].socket, false, entitiesData[entityId].ip, entitiesData[entityId].port) && 
		Network::Identify(*entitiesData[entityId].socket, NETWORK_IDENTIFICATION_ENTITY, entitiesData[entityId].entityId)) {
		threads->NewEntityThread(&entitiesData[entityId]);
	}
}

void Entities::RemoveEntity(std::string entityId) {
	dataMutex.lock();
	if(entitiesData[entityId].socket && entitiesData[entityId].socket->is_open()) entitiesData[entityId].socket->close();
	entitiesData.erase(entityId);
	dataMutex.unlock();
}

bool Entities::AddEntity(std::string entityId, boost::asio::ip::tcp::socket &socket) {
	std::lock_guard<std::mutex> lock(dataMutex);
	if(entitiesData[entityId].socket && entitiesData[entityId].socket->is_open()) return false;

	boost::asio::ip::tcp::socket transfer = std::move(socket);
	entitiesData[entityId].socket = &transfer;
	return true;
}

bool Entities::GetPublicKey(std::string entityId, std::string &publicKey) {
	std::lock_guard<std::mutex> lock(dataMutex);

	auto it = entitiesData.find(entityId);
	if(it == entitiesData.end()) return false;

	publicKey = it->second.publicKey;
	return true;
}

bool Entities::SetPublicKey(std::string entityId, std::string publicKey) {
	std::lock_guard<std::mutex> lock(dataMutex);

	auto it = entitiesData.find(entityId);
	if(it == entitiesData.end()) return false;

	it->second.publicKey = publicKey;
	return true;
}

bool Entities::SetHost(std::string entityId, std::string host, bool connect /*=true*/) {
	std::lock_guard<std::mutex> lock(dataMutex);

	auto it = entitiesData.find(entityId);

	if(it != entitiesData.end()) {
		//Extract data from host
		if(!Network::ResolveHost(host, it->second.ip, it->second.port)) return false;

		//Reconnect if requested
		if(connect && Network::OpenConnection(*it->second.socket, false, it->second.ip, it->second.port) && 
			Network::Identify(*it->second.socket, NETWORK_IDENTIFICATION_ENTITY, it->second.entityId)) {
			it->second.ip = it->second.socket->remote_endpoint().address().to_string();
		}
		return true;
	}
	return false;
}

void Entities::PublicKeyRequest(std::string entityId, std::string account) {
	std::lock_guard<std::mutex> lock(dataMutex);
	
	auto it = entitiesData.find(entityId);
	if(it == entitiesData.end() || !it->second.socket || !it->second.socket->is_open()) return;

	//Send a request asking for the public key associated to that account
	Network::WriteMessage(*it->second.socket, NETWORK_GET_PUBLIC_KEY, account, true);
}

void Entities::TransactionReply(std::string entityId, std::string hash, int errorCode) {
	std::lock_guard<std::mutex> lock(dataMutex);
	auto it = entitiesData.find(entityId);

	if(it != entitiesData.end() && it->second.socket && it->second.socket->is_open()) {
		if(errorCode == VALID) {
			Network::WriteMessage(*it->second.socket, NETWORK_TRANSACTION_ACCEPTED, hash, true);
		}
		else {
			hash += errorCode;
			Network::WriteMessage(*it->second.socket, NETWORK_TRANSACTION_REJECTED, hash, true);
		}
	}
}

bool Entities::RequestBlock(std::string entityId, std::string hash) {
	std::lock_guard<std::mutex> lock(dataMutex);
	auto it = entitiesData.find(entityId);

	if(it != entitiesData.end() && it->second.socket && it->second.socket->is_open()) {
		return Network::WriteMessage(*it->second.socket, NETWORK_GET_BLOCK, hash, true);
	}
	return false;
}

bool Entities::BroadcastConfirmation(std::string hash, int confirmationType /*=CONFIRMATION_BLOCK*/) {
	if(confirmationType != CONFIRMATION_BLOCK) return false;

	uint16_t protocolCode = NETWORK_BLOCK_CONSENSUS;

	int counter = 0;
	dataMutex.lock();
	for(auto it = entitiesData.begin(); it != entitiesData.end(); ++it) {
		if(it->second.socket && it->second.socket->is_open() && Network::WriteMessage(*it->second.socket, protocolCode, hash, true)) counter++; 
	}
	dataMutex.unlock();

	return (counter > 0);
}

void Entities::BroadcastEntry(std::string entry) {
	dataMutex.lock();
	for(auto it = entitiesData.begin(); it != entitiesData.end(); ++it) {
		if(it->second.socket && it->second.socket->is_open()) {
			Network::WriteMessage(*it->second.socket, NETWORK_NEW_ENTRY, entry, true); 
		}
	}
	dataMutex.unlock();
}
