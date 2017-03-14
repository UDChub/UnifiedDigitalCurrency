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
 * @file block.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <chrono>
#include <fstream>
#include <string>
#include <vector>

#include "includes/boost/filesystem.hpp"
#include "includes/cryptopp/filters.h"
#include "includes/cryptopp/ripemd.h"
#include "includes/cryptopp/hex.h"

#include "block.h"
#include "configurations.h"
#include "entry.h"
#include "globals.h"
#include "merkle.h"
#include "network_manager.h"
#include "publisher.h"
#include "util.h"


BlockStruct& BlockStruct::operator=(const BlockStruct &block) {
	blockId = block.blockId;
	previousBlockId = block.previousBlockId;
	blockHash = block.blockHash;
	previousBlockHash = block.previousBlockHash;
	begin = block.begin;
	end = block.end;
	entriesList = block.entriesList;
	blockFile = block.blockFile;
	entriesFile = block.entriesFile;
	return *this;
}


Block::Block(NetworkManager *networkManager, Publisher *publisher, bool IS_SYNCHRONIZED, std::string latestHash, int latestId)
 : networkManager(networkManager), publisher(publisher), IS_SYNCHRONIZED(IS_SYNCHRONIZED)
 {
	//Set BlockStructs
	currentBlock.blockHash = latestHash; //disregard value
	currentBlock.blockId = latestId+1;
	currentBlock.previousBlockHash = latestHash;
	currentBlock.previousBlockId = latestId;
	currentBlock.blockFile = LOCAL_DATA_BLOCKS + std::to_string(currentBlock.blockId) + BLOCK_EXTENSION;
	currentBlock.entriesFile = LOCAL_DATA_BLOCKS + std::to_string(currentBlock.blockId) + BLOCK_ENTRIES_EXTENSION;
	currentBlock.end = GENESIS_BLOCK_END + (currentBlock.blockId * BLOCK_DURATION);
	currentBlock.begin = currentBlock.begin - BLOCK_DURATION + 1;

 	nextBlock.blockId = currentBlock.blockId+1;
 	nextBlock.previousBlockId = currentBlock.blockId;
	nextBlock.blockFile = LOCAL_DATA_BLOCKS + std::to_string(nextBlock.blockId) + BLOCK_EXTENSION;
	nextBlock.entriesFile = LOCAL_DATA_BLOCKS + std::to_string(nextBlock.blockId) + BLOCK_ENTRIES_EXTENSION;
	nextBlock.begin = currentBlock.end + 1;
	nextBlock.end = currentBlock.end + BLOCK_DURATION;

	if(IS_SYNCHRONIZED) {
		currentBlock.entriesData.open(currentBlock.entriesFile, std::fstream::out | std::fstream::trunc);
		nextBlock.entriesData.open(nextBlock.entriesFile, std::fstream::out | std::fstream::trunc);
	}
	else {
		auto now = Util::current_timestamp();

		//Ensure there's enough time until the current Block closes
		if(now < currentBlock.end - SYNCHRONIZATION_MARGIN) {
			WAIT_FOR_NEXT = false;
			nextBlock.entriesData.open(nextBlock.entriesFile, std::fstream::out | std::fstream::trunc);
		}
		else WAIT_FOR_NEXT = true;
	}
}

void Block::CloseBlock() {
	dataMutex.lock();
	if(IS_SYNCHRONIZED) {
		currentBlock.entriesData.close();
		oldEntriesList = currentBlock.entriesList;

		//Build Block
		BuildBlock();
	}
	else if(WAIT_FOR_NEXT) WAIT_FOR_NEXT = false;
	else IS_SYNCHRONIZED = true;

	//Set the previous Block hash of the next Block
	nextBlock.previousBlockHash = currentBlock.blockHash;
	dataMutex.unlock();

	//initialize the new Block
	BlockStruct newBlock;
	newBlock.blockId = nextBlock.blockId+1;
	newBlock.previousBlockId = nextBlock.blockId;
	newBlock.begin = nextBlock.begin + BLOCK_DURATION;
	newBlock.end = nextBlock.end + BLOCK_DURATION;
	newBlock.blockFile = LOCAL_DATA_BLOCKS + std::to_string(newBlock.blockId) + BLOCK_EXTENSION;
	newBlock.entriesFile = LOCAL_DATA_BLOCKS + std::to_string(newBlock.blockId) + BLOCK_ENTRIES_EXTENSION;

	//move Blocks and Entries data streams
	dataMutex.lock();
	nextBlock.entriesData.close();

	currentBlock = nextBlock;
	nextBlock = newBlock;

	currentBlock.entriesData.open(currentBlock.entriesFile, std::fstream::out | std::fstream::app);
	nextBlock.entriesData.open(nextBlock.entriesFile, std::fstream::out | std::fstream::trunc);
	dataMutex.unlock();

	//Initialize the latest Block's consensus process
	StartConsensus();
}

void Block::BuildBlock() {
	//Compute the root of the Entries Merkle tree
	std::string entriesRoot = Crypto::MerkleRoot(currentBlock.entriesList);

	//Compute the Block hash
	CryptoPP::RIPEMD160 ripemd;
	std::string toHash = std::to_string(currentBlock.blockId)+currentBlock.previousBlockHash+entriesRoot;
	CryptoPP::StringSource ss(toHash, true, new CryptoPP::HashFilter(ripemd, new CryptoPP::HexEncoder(new CryptoPP::StringSink(currentBlock.blockHash))));

	//Create the Block file
	std::fstream blockFile(currentBlock.blockFile, std::fstream::out | std::fstream::trunc);
	blockFile << "{\"blockId\":" << currentBlock.blockId;
	blockFile << ",\"blockHash\":\"" << currentBlock.blockHash;
	blockFile << "\",\"previousBlockHash\":\"" << currentBlock.previousBlockHash;
	blockFile << "\",\"date\":" << currentBlock.end;
	blockFile << ",\"entriesRoot\":\"" << entriesRoot;

	//Append the Entries data
	currentBlock.entriesData.open(currentBlock.entriesFile, std::fstream::in);
	//skip initial extra comma
	currentBlock.entriesData.seekg(1, std::ios_base::beg);
	blockFile << "\",\"data\":{" << currentBlock.entriesData.rdbuf();
	currentBlock.entriesData.close();

	//Write the closing brackets and close the Block file
	blockFile << "}}";
	blockFile.close();
}

bool Block::NewEntry(Entry *entry) {
	BlockStruct *blockPtr;
	if(entry->GetTimestamp() < currentBlock.begin) return false;
	if(!IS_SYNCHRONIZED) {
		if(!WAIT_FOR_NEXT && entry->GetTimestamp() >= nextBlock.begin && entry->GetTimestamp() <= nextBlock.end) blockPtr = &nextBlock;
		else return true;
	}
	else if(entry->GetTimestamp() <= currentBlock.end) blockPtr = &currentBlock;
	else if(entry->GetTimestamp() <= nextBlock.end) blockPtr = &nextBlock;
	else return false;

	blockPtr->entriesList.insert(entry->GetHash());
	blockPtr->entriesData << ",{\"" << entry->GetHash() << "\':" << entry->GetEntry() << "}";
	return true;
}

bool Block::HasEntry(std::string hash) {
	auto it = currentBlock.entriesList.find(hash);
	if(it != currentBlock.entriesList.end()) return true;

	auto it2 = nextBlock.entriesList.find(hash);
	if(it2 != nextBlock.entriesList.end()) return true;

	return false;
}

std::set<std::string> Block::GetLatestEntries() {
	return oldEntriesList;
}

void Block::StartConsensus() {
	networkManager->BroadcastConfirmation(currentBlock.previousBlockHash);

	std::lock_guard<std::mutex> lock(dataMutex);
	blockConsensus[currentBlock.previousBlockHash].first++;

	if(blockConsensus[currentBlock.previousBlockHash].first >= networkManager->ConfirmationThreshold()) {
		EndConsensus(currentBlock.previousBlockHash, _SELF);
	}
}

void Block::AddConfirmation(std::string hash, std::string node) {
	std::lock_guard<std::mutex> lock(dataMutex);
	
	blockConsensus[hash].first++;
	blockConsensus[hash].second.push_back(node);

	if(blockConsensus[hash].first >= networkManager->ConfirmationThreshold()) {
		EndConsensus(hash, node);
	} 
}

void Block::EndConsensus(std::string hash, std::string node) {
	//If we built an incorrect Block
	if(hash != currentBlock.previousBlockHash) {
		//keep the correct hash
		currentBlock.previousBlockHash = hash;

		//request correct Block from any of the peers that voted for it
		networkManager->RequestBlock(blockConsensus[hash].second, hash);
	}
	else {
		//irectly publish Block
		std::string BlockFile = LOCAL_DATA_BLOCKS + std::to_string(currentBlock.previousBlockId) + BLOCK_EXTENSION;
		publisher->PublishBlock(BlockFile);

		//remove related Entries file
		std::string file = LOCAL_DATA_BLOCKS + std::to_string(currentBlock.previousBlockId) + BLOCK_ENTRIES_EXTENSION;
		boost::filesystem::remove(boost::filesystem::path(file));
	}

	//update the Blocks index
	networkManager->NewBlock(currentBlock.previousBlockId, hash);
}

void Block::CleanConsensus() {
	///For Monitoring, keep track of good and bad confirmations
	//update reputation of the other nodes according to their votes for the latest Block
	// nodes->UpdateReputation(true, false, blockConsensus[currentBlock.previousBlockHash]);
	// blockConsensus.erase(hash);

	// for(auto it = blockConsensus.begin(); it != blockConsensus.end(); ++it) {
	// 	nodes->UpdateReputation(false, false, it->second.second);
	// }

	blockConsensus.clear();
}

bool Block::GetCorrectNodes(std::string hash, std::vector<std::string> &nodes) {
	auto it = blockConsensus.find(hash);
	if(it != blockConsensus.end()) {
		nodes = it->second.second;
		return true;
	}
	return false;
}

uint Block::GetClosingTime() {
	return currentBlock.end;
}

uint Block::ExpectedSynchronizationEnd() {
	if(WAIT_FOR_NEXT) return nextBlock.end;
	return currentBlock.end;
}