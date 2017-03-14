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
 * @file transaction_das.h
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#ifndef TRANSACTION_DAS_H
#define TRANSACTION_DAS_H

#include <map>
#include <unordered_map>
#include <utility>
#include <string>
#include <vector>

#include "transaction.h"


class DASTransaction : public Transaction {
	public:
		DASTransaction(){}
		DASTransaction(const char rawTx[]) : Transaction(rawTx){}

		virtual bool IsTransaction();

		std::string GetDAS();
		uint64_t GetNetworkFees();

		virtual uint64_t GetFees() {return fees;}
		virtual bool SetFees(uint64_t fees) {this->fees=fees; return true;};

	protected:
		uint64_t fees = 0;
};

class FeeRedistributionDASTransaction : public DASTransaction {
	public:
		FeeRedistributionDASTransaction(uint64_t start, uint64_t end, uint64_t total, uint64_t reported, std::map<std::string, uint64_t> shares, std::unordered_map<std::string, std::string> accounts);
		FeeRedistributionDASTransaction(const char rawTx[]) : DASTransaction(rawTx){}

		bool IsTransaction();
		bool CheckTimestamp();

		void Execute(std::vector<std::pair<std::string, uint64_t>> &from, std::vector<std::pair<std::string, uint64_t>> &to);

		uint64_t GetTimestamp();
		uint64_t GetStart();
		uint64_t GetEnd();
		uint64_t GetTotal();
		uint64_t GetReported();
		std::map<std::string, std::pair<std::string, uint64_t>> GetShares();

};


#endif