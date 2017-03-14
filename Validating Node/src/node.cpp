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
 * @file node.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "includes/boost/asio.hpp"
#include "includes/boost/filesystem.hpp"
#include "includes/boost/regex.hpp"

#include "configurations.h"
#include "network.h"
#include "node.h"
#include "globals.h"
#include "codes.h"
#include "communication.h"
#include "threads_manager.h"
#include "transaction.h"
#include "util.h"


Nodes::Nodes(std::unordered_map<std::string, NodeStruct> nodesData) : nodesData(nodesData) {
	UpdateRatios();
}

void Nodes::Stop() {
	std::lock_guard<std::mutex> lock(dataMutex);

	for(auto it = activeNodes.begin(); it != activeNodes.end(); ++it) {
		Network::WriteMessage(*it->second, NETWORK_EXIT, "", false);
		it->second->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
		it->second->close();
	}
}

void Nodes::LoadActiveNodes(ThreadsManager *threads) {
	this->threads = threads;

	for(auto it = nodesData.begin(); it != nodesData.end(); ++it) {
		auto pos = it->second.host.find(":");
		if(pos == std::string::npos) continue;
		std::string address = it->second.host.substr(0,pos);

		//Set port
		it->second.port = std::stoul(it->second.host.substr(pos+1, it->second.host.length()-1));

		boost::regex ipRegex(PATTERN_IPV4);

		//Save IP address or resolve from hostname
		if(boost::regex_match(address, ipRegex)) it->second.ip = address;
		else {
			boost::asio::ip::tcp::resolver resolver(NETWORK_SOCKET_SERVICE);
			boost::asio::ip::tcp::resolver::query query(address, "");
			auto endpointIt = resolver.resolve(query);
			//No endpoint found
			if(endpointIt == boost::asio::ip::tcp::resolver::iterator()) continue;
			it->second.ip = (*endpointIt).endpoint().address().to_string();
		}

		//Try to open a connection with the peer node
		boost::asio::ip::tcp::socket *socket = new boost::asio::ip::tcp::socket(NETWORK_SOCKET_SERVICE);
		if(Network::OpenConnection(*socket, false, it->second.ip, it->second.port) && Network::Identify(*socket, NETWORK_IDENTIFICATION_NODE, it->second.nodeId)) {
			activeNodes[it->first] = socket;

			//Launch listening thread
			threads->NewNodeThread(&nodesData[it->first], activeNodes[it->first]);
		}
	}
}

void Nodes::NewNode(std::string nodeId, std::string publicKey, std::string host, std::string version, std::string account, int reputation, bool connect /*=true*/) {
	NodeStruct newNode;
	newNode.nodeId = nodeId;
	newNode.publicKey = publicKey;
	newNode.host = host;
	newNode.version = version;
	newNode.account = account;
	newNode.reputation = reputation;

	dataMutex.lock();
	if(connect && Network::ResolveHost(host, newNode.ip, newNode.port)) {
		boost::asio::ip::tcp::socket *socket = new boost::asio::ip::tcp::socket(NETWORK_SOCKET_SERVICE);
		if(Network::OpenConnection(*socket, false, newNode.ip, newNode.port) &&	Network::Identify(*socket, NETWORK_IDENTIFICATION_NODE, newNode.nodeId)) {
			//Keep openned connection
			activeNodes[nodeId] = socket;
			//Launch listening thread
			threads->NewNodeThread(&nodesData[nodeId], activeNodes[nodeId]);
		}
	}

	nodesData[nodeId] = newNode;
	dataMutex.unlock();

	//Update consensus ratios
	UpdateRatios();
}

bool Nodes::RemoveNode(std::string nodeId) {
	std::lock_guard<std::mutex> lock(dataMutex);

	//Close connection if open
	DropNode(nodeId);

	auto it = nodesData.find(nodeId);
	if(it != nodesData.end()) {
		//Remove Node
		nodesData.erase(nodeId);

		UpdateRatios();
		return true;
	}
	return false;
}

void Nodes::AddNode(std::string nodeId, boost::asio::ip::tcp::socket *socket) {
	std::lock_guard<std::mutex> lock(dataMutex);
	activeNodes[nodeId] = socket;

	//Retrieve host information
	boost::asio::ip::tcp::endpoint remoteEndpoint = activeNodes[nodeId]->remote_endpoint();
	nodesData[nodeId].ip = remoteEndpoint.address().to_string();
	nodesData[nodeId].port = (uint)remoteEndpoint.port();
	nodesData[nodeId].host = nodesData[nodeId].ip+":"+std::to_string(nodesData[nodeId].port);

	//Launch listening thread
	threads->NewNodeThread(&nodesData[nodeId], activeNodes[nodeId]);
}

bool Nodes::DropNode(std::string nodeId) {
	std::lock_guard<std::mutex> lock(dataMutex);

	auto it = activeNodes.find(nodeId);
	if(it != activeNodes.end()) {
		//Close socket
		it->second->close();
		delete it->second;
		activeNodes.erase(nodeId);
		return true;
	}
	return false;
}

void Nodes::UpdateReputation(std::vector<std::string> nodes, bool valid, bool transaction /*=true*/) {
	std::lock_guard<std::mutex> lock(dataMutex);

	for(std::string node: nodes) {
		if(valid) {
			if(transaction) nodesData[node].reputation += NODE_REPUTATION_INCREMENT;
			else nodesData[node].reputation += NODE_REPUTATION_INCREMENT * NODE_REPUTATION_LEDGER_MUX;
		}

		else {
			if(transaction) nodesData[node].reputation -= NODE_REPUTATION_DECREMENT;
			else nodesData[node].reputation -= NODE_REPUTATION_DECREMENT * NODE_REPUTATION_LEDGER_MUX;
		}
	}
}

std::unordered_map<std::string, int> Nodes::GetReputation() {
	std::unordered_map<std::string, int> reputations;
	for(auto it = nodesData.begin(); it != nodesData.end(); ++it) reputations[it->first] = it->second.reputation;
	return reputations;
}

bool Nodes::GetPublicKey(std::string nodeId, std::string &publicKey) {
	auto it = nodesData.find(nodeId);
	if(it != nodesData.end()) {
		publicKey = it->second.publicKey;
		return true;
	}
	return false;
}

bool Nodes::SetPublicKey(std::string nodeId, std::string publicKey) {
	std::lock_guard<std::mutex> lock(dataMutex);

	auto it = nodesData.find(nodeId);
	if(it != nodesData.end()) {
		it->second.publicKey = publicKey;
		return true;
	}
	return false;
}

bool Nodes::SetHost(std::string nodeId, std::string host, bool connect) {
	std::lock_guard<std::mutex> lock(dataMutex);

	auto it = nodesData.find(nodeId);

	if(it != nodesData.end()) {
		it->second.host = host;
		//Extract data from host
		if(!Network::ResolveHost(it->second.host, it->second.ip, it->second.port)) return false;

		//Reconnect if requested
		if(connect) {
			auto activeIt = activeNodes.find(nodeId);
			if(activeIt != activeNodes.end()) activeIt->second->close();

			boost::asio::ip::tcp::socket *socket = new boost::asio::ip::tcp::socket(NETWORK_SOCKET_SERVICE);
			if(Network::OpenConnection(*socket, false, it->second.ip, it->second.port) && Network::Identify(*socket, NETWORK_IDENTIFICATION_NODE, it->second.nodeId)) {
				activeIt->second = socket;
			}
		}
		return true;
	}
	return false;
}

bool Nodes::SetNetworkVersion(std::string nodeId, std::string version) {
	std::lock_guard<std::mutex> lock(dataMutex);

	auto it = nodesData.find(nodeId);
	if(it != nodesData.end()) {
		it->second.version = version;
		return true;
	}
	return false;
}

std::unordered_map<std::string, std::string> Nodes::GetAccounts() {
	std::unordered_map<std::string, std::string> accounts;
	
	std::lock_guard<std::mutex> lock(dataMutex);
	for(auto it = nodesData.begin(); it != nodesData.end(); ++it) accounts[it->first] = it->second.account;

	//Add our account
	accounts[_SELF] = _ACCOUNT;
	return accounts;
}

bool Nodes::GetAccount(std::string nodeId, std::string &account) {
	auto it = nodesData.find(nodeId);
	if(it != nodesData.end()) {
		account = it->second.account;
		return true;
	}
	return false;
}

bool Nodes::SetAccount(std::string nodeId, std::string account) {
	std::lock_guard<std::mutex> lock(dataMutex);

	auto it = nodesData.find(nodeId);
	if(it != nodesData.end()) {
		it->second.account = account;
		return true;
	}
	return false;
}

void Nodes::UpdateRatios() {
	std::lock_guard<std::mutex> lock(dataMutex);

	broadcastNumber = (nodesData.size()*TRANSACTION_BROADCAST_RATIO) + 1;
	confirmationThresholdTx = (nodesData.size()*TRANSACTION_CONFIRMATION_RATIO) + 1;
	confirmationThresholdLedger = (nodesData.size()*LEDGER_CONFIRMATION_RATIO) + 1;
}

int Nodes::BroadcastNumber() {
	return broadcastNumber;
}

int Nodes::ConfirmationThreshold(bool transaction /*=true*/) {
	if(transaction) return confirmationThresholdTx;
	return confirmationThresholdLedger;
}

bool Nodes::BroadcastNewTransaction(Transaction *Tx) {
	std::vector<std::string> goodNodes, badNodes;
	int counter = 0, threshold = ConfirmationThreshold();
	std::string rawTx = Tx->GetTransaction();

	//sort active nodes by reputation
	for(auto it = nodesData.begin(); it != nodesData.end(); ++it) {
		if(it->second.reputation >= NODE_REPUTATION_TRESHOLD) goodNodes.push_back(it->first);
		else badNodes.push_back(it->first);
	}

	//broadcast to good peer nodes first
	if(goodNodes.size() > broadcastNumber) std::random_shuffle(goodNodes.begin(), goodNodes.end());
	while(counter < threshold && goodNodes.size()) {
		if(Network::WriteMessage(*activeNodes[goodNodes.back()], NETWORK_BROADCAST_TRANSACTION, rawTx, true)) counter++;
		goodNodes.pop_back();
	}

	//stops if enough nodes received the transaction
	if(counter >= threshold) return true;

	//complete with bad nodes if necessary
	std::random_shuffle(badNodes.begin(), badNodes.end());
	while(counter < threshold && badNodes.size()) {
		if(Network::WriteMessage(*activeNodes[badNodes.back()], NETWORK_BROADCAST_TRANSACTION, rawTx, true)) counter++;
		badNodes.pop_back();
	}

	//verifies enough nodes received the transaction
	return (counter >= threshold);
}

bool Nodes::BroadcastConfirmation(std::string hash, int confirmationType /*=CONFIRMATION_TRANSACTION*/) {
	int counter = 0;
	uint16_t protocolCode;

	switch(confirmationType) {
		case CONFIRMATION_TRANSACTION:
			protocolCode = NETWORK_BROADCAST_CONFIRMATION;
			break;

		case CONFIRMATION_LEDGER:
			protocolCode = NETWORK_LEDGER_CONSENSUS;
			break;
			
		case CONFIRMATION_BLOCK:
			protocolCode = NETWORK_BLOCK_CONSENSUS;
			break;

		default:return false;
	}

	for(auto it = activeNodes.begin(); it != activeNodes.end(); ++it) {
		if(Network::WriteMessage(*it->second, protocolCode, hash, true)) counter++; 
	}

	return (counter > 0);
}

void Nodes::BroadcastEntry(std::string entry) {
	for(auto it = activeNodes.begin(); it != activeNodes.end(); ++it) {
		Network::WriteMessage(*it->second, NETWORK_NEW_ENTRY, entry, true); 
	}
}

boost::asio::ip::tcp::socket* Nodes::WriteToNode(std::string nodeId, int &errorCode) {
	auto activeIt = activeNodes.find(nodeId);
	if(activeIt != activeNodes.end()) {
		errorCode = ERROR_NODE_OFFLINE;
		return NULL;
	}
	return activeNodes[nodeId];
}

boost::asio::ip::tcp::socket* Nodes::WriteToRandomNode() {
	auto it = activeNodes.begin();
	std::advance(it, Util::current_timestamp() % activeNodes.size());
	return it->second;
}

void Nodes::KeepAlive() {
	for(auto it = activeNodes.begin(); it != activeNodes.end(); ++it) {
		if(Network::SendKeepAlive(*it->second)) retries[it->first] = 0;
		else {
			retries[it->first]++;
			if(retries[it->first] > NODE_MAX_RETRIES) DropNode(it->first);
		}
	}
}