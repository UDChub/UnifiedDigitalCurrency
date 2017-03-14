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
 * @file processing.h
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#ifndef PROCESSING_H
#define PROCESSING_H

#include <sstream>
#include <string>

#include "includes/boost/regex.hpp"

#include "ecdsa.h"
#include "modules_interface.h"

class DAOManager;
class DASManager;
class Entry;
//class ModulesInterface;
class Transaction;


namespace Processing{

	Transaction* CreateTransaction(DAOManager *managerDAO, DASManager *managerDAS, std::string &rawTx, int &errorCode);
	Transaction* PrepareTransaction(DAOManager *managerDAO, DASManager *managerDAS, std::string &rawTx, int &errorCode);

	template <typename T>
	bool CheckAmounts(T *Tx, int &errorCode) {
		uint64_t amount = Tx->GetAmount();
		uint64_t feeO = Tx->GetOutboundFee();
		uint64_t feeI = Tx->GetInboundFee();

		if(!amount) errorCode = ERROR_AMOUNT_SENT;
		else if(amount < feeO) errorCode = ERROR_FEE_OUTBOUND;
		else if((amount-feeO) < feeI) errorCode = ERROR_FEE_INBOUND;
		return (errorCode == VALID);
	}
	
	template <typename T>
	bool CheckAccounts(T *Tx, int &errorCode) {
		std::string account;
		boost::regex accRegex(PATTERN_ACCOUNT);

		account = Tx->GetSender();
		if(!boost::regex_match(account, accRegex)) {
			errorCode = ERROR_ACCOUNT_SENDER;
			return false;
		}

		account = Tx->GetReceiver();
		if(!boost::regex_match(account, accRegex)) {
			errorCode = ERROR_ACCOUNT_RECEIVER;
			return false;
		}

		account = Tx->GetOutboundAccount();
		if(!boost::regex_match(account, accRegex)) {
			errorCode = ERROR_ACCOUNT_OUTBOUND;
			return false;
		}

		account = Tx->GetInboundAccount();
		if(!boost::regex_match(account, accRegex)) {
			errorCode = ERROR_ACCOUNT_INBOUND;
			return false;
		}

		return true;
	}
	
	bool CheckBalance(ModulesInterface *interface, std::string from, uint64_t amount, int &errorCode);
	
	template <typename T>
	bool CheckSignatures(ModulesInterface *interface, T *Tx, int &errorCode) {
		std::stringstream ss;
		std::string message, publicKey;

		//Verify sender's signature
		if(!interface->GetPublicKey(Tx->GetSender(), publicKey) || !Crypto::Verify(Tx->GetCore(), Tx->GetSignature(), publicKey)) {
			errorCode = ERROR_SIGNATURE_SENDER;
			return false;
		}

		//Verify Outbound signature
		ss << Tx->GetSignature() << Tx->GetOutboundAccount() << Tx->GetOutboundFee();
		message = ss.str();
		if(!interface->GetManagingEntityKey(Tx->GetOutboundAccount(), publicKey) || !Crypto::Verify(message, Tx->GetOutboundSignature(), publicKey)) {
			errorCode = ERROR_SIGNATURE_OUTBOUND;
			return false;
		}

		//Verify Inbound signature
		ss.str("");
		ss << Tx->GetOutboundSignature() << Tx->GetInboundAccount() << Tx->GetInboundFee();
		message = ss.str();
		if(!interface->GetManagingEntityKey(Tx->GetInboundAccount(), publicKey) || !Crypto::Verify(message, Tx->GetInboundSignature(), publicKey)) {
			errorCode = ERROR_SIGNATURE_INBOUND;
			return false;
		}

		return true;
	}

	Entry* CreateEntry(std::string &content, int &errorCode);
	int GetIdType(std::string id);

}

#endif