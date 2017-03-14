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
 * @file node.h
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#ifndef NODE_H
#define NODE_H

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "includes/boost/asio.hpp"

#include "globals.h"

class ThreadsManager;
class Transaction;


struct NodeStruct {
	std::string nodeId;
	std::string publicKey;
	std::string version;
	std::string account = WORLD_BANK_NODE_ACCOUNT;
	std::string host;
	std::string ip;
	uint port;
	int reputation = NODE_REPUTATION_DEFAULT;
};

class Nodes {
	public:
		Nodes(std::unordered_map<std::string, NodeStruct> nodesData);

		void LoadActiveNodes(ThreadsManager *threads);
		void Stop();

		void NewNode(std::string nodeId, std::string publicKey, std::string host, std::string version, std::string account, int reputation, bool connect=true);
		bool RemoveNode(std::string nodeId);
		void AddNode(std::string nodeId, boost::asio::ip::tcp::socket *socket);
		bool DropNode(std::string nodeId);

		void UpdateReputation(std::vector<std::string> nodes, bool valid, bool transaction=true);
		std::unordered_map<std::string, int> GetReputation();

		bool GetPublicKey(std::string nodeId, std::string &publicKey);
		bool SetPublicKey(std::string nodeId, std::string publicKey);
		bool SetHost(std::string nodeId, std::string host, bool connect);
		bool SetNetworkVersion(std::string nodeId, std::string version);
		std::unordered_map<std::string, std::string> GetAccounts();
		bool GetAccount(std::string nodeId, std::string &account);
		bool SetAccount(std::string nodeId, std::string account);

		void UpdateRatios();
		int BroadcastNumber();
		int ConfirmationThreshold(bool transaction=true);

		bool BroadcastNewTransaction(Transaction *Tx);
		bool BroadcastConfirmation(std::string hash, int confirmationType=CONFIRMATION_TRANSACTION);
		void BroadcastEntry(std::string entry);
		boost::asio::ip::tcp::socket* WriteToNode(std::string nodeId, int &errorCode);
		boost::asio::ip::tcp::socket* WriteToRandomNode();
		void KeepAlive();

	private:
		std::mutex dataMutex;
		ThreadsManager *threads;

		std::unordered_map<std::string, NodeStruct> nodesData;
		std::unordered_map<std::string, boost::asio::ip::tcp::socket*> activeNodes;
		std::unordered_map<std::string, int> retries;

		int broadcastNumber;
		int confirmationThresholdTx;
		int confirmationThresholdLedger;

		uint nextUpdate;
};

#endif