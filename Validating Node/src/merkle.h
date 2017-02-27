#ifndef MERKLE_H
#define MERKLE_H

#include <set>
#include <string>


namespace Crypto {

	std::string MerkleRoot(std::set<std::string> &transactionsList);

}

#endif
