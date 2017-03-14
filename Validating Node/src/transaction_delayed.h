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
 * @file transaction_delayed.h
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#ifndef TRANSACTION_DELAYED_H
#define TRANSACTION_DELAYED_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "transaction.h"

class ModulesInterface;


class DelayedTransaction : public Transaction
{
	public:
		DelayedTransaction(const char rawTx[]) : Transaction(rawTx){}

		virtual bool IsTransaction();
		virtual bool CheckTimestamp();

		std::string GetEvent();
		uint64_t GetTimestamp();
		std::string GetSender();
		std::string GetReceiver();
		uint64_t GetAmount();
};

class RequestDelayedTransaction : public DelayedTransaction
{
	public:
		RequestDelayedTransaction(const char rawTx[]) : DelayedTransaction(rawTx){}

		bool IsTransaction();
		bool CheckTimestamp();
		
		bool Process(ModulesInterface *interface, int &errorCode);
		void Execute(std::vector<std::pair<std::string, uint64_t>> &from, std::vector<std::pair<std::string, uint64_t>> &to);

		uint64_t GetExecution();
		std::string GetSignature();
		std::string GetCore();

		std::string GetOutboundAccount();
		uint64_t GetOutboundFee();
		std::string GetOutboundSignature();

		std::string GetInboundAccount();
		uint64_t GetInboundFee();
		std::string GetInboundSignature();

		uint64_t GetFees();
		uint64_t GetNetAmount();
		
};

class ReleaseDelayedTransaction : public DelayedTransaction
{
	public:
		ReleaseDelayedTransaction(const char rawTx[]) : DelayedTransaction(rawTx){}

		bool IsTransaction();
		
		bool Process(ModulesInterface *interface, int &errorCode);
		void Execute(std::vector<std::pair<std::string, uint64_t>> &from, std::vector<std::pair<std::string, uint64_t>> &to);

		std::string GetRequest();
};


#endif