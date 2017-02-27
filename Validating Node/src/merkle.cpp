#include <set>
#include <string>
#include <vector>

#include "includes/cryptopp/filters.h"
#include "includes/cryptopp/ripemd.h"
#include "includes/cryptopp/hex.h"

#include "globals.h"
#include "merkle.h"


std::string Crypto::MerkleRoot(std::set<std::string> &list) {
	int num = list.size();

	//return zero-length hash if empty list
	if (num == 0) return RIPEMD160_NULL_HASH;

	CryptoPP::RIPEMD160 ripemd;
	std::vector<std::string> parents, children;
	std::string content, parent;
	bool pair = false;

	//find shortest depth that can take all items
	int maxLeaves = 1;
	while (maxLeaves < num) maxLeaves = maxLeaves * 2;
	std::vector<int> leavesIndex;

	//find number of leaves per tree level, in decreasing level order
	while(num > 1) {
		if(num == maxLeaves - num <= 1) {
			leavesIndex.push_back(maxLeaves);
			num = maxLeaves/2;
		}
		else {
			int l = 0, m = maxLeaves/2, rest = 0;
			while(num-l > m/2) {
				l += m;
				m = m/2;
			}
			leavesIndex.push_back(l);
			rest = num - l;
			num = l/2 + rest;
		}
		maxLeaves = maxLeaves/2;
	}

	//add all leaves and compute the internal nodes
	auto listIterator = list.begin();
	for(auto parentsIterator = leavesIndex.begin(); parentsIterator != leavesIndex.end(); ++parentsIterator) {
		uint i = 0;
		//create parents from any existing non-leaf child nodes
		for (auto childIterator = children.begin(); childIterator != children.end(); ++childIterator, i++) {
			if (pair) {
				std::string hash;
				CryptoPP::StringSource ss(content + *childIterator, true, new CryptoPP::HashFilter(ripemd, new CryptoPP::HexEncoder(new CryptoPP::StringSink(hash))));
				parents.push_back(hash);
				pair = false;
			}
			else {
				content = *childIterator;
				pair = true;
			}
		}

		//create parents from leaf nodes
		for (listIterator; i < *parentsIterator && listIterator != list.end(); ++listIterator, i++) {
			if (pair) {
				std::string hash;
				CryptoPP::StringSource ss(content + *listIterator, true, new CryptoPP::HashFilter(ripemd, new CryptoPP::HexEncoder(new CryptoPP::StringSink(hash))));
				parents.push_back(hash);
				pair = false;
			}
			else {
				content = *listIterator;
				pair = true;
			}
		}

		//odd number of items, create a new parent node with the last item and the zero-length hash
		if(pair) {
			std::string hash;
			CryptoPP::StringSource ss(content + RIPEMD160_NULL_HASH, true, new CryptoPP::HashFilter(ripemd, new CryptoPP::HexEncoder(new CryptoPP::StringSink(hash))));
			parents.push_back(hash);
			pair = false;
		}

		//move up a tree level
		children = parents;
		parents.clear();
	}

	//create the root node of the merkle tree
	std::string root;
	CryptoPP::StringSource ss(children.front()+children.back(), true, new CryptoPP::HashFilter(ripemd, new CryptoPP::HexEncoder(new CryptoPP::StringSink(root))));
	return root;
}
