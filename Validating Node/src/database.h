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
 * @file database.h
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#ifndef DATABASE_H
#define DATABASE_H

//#include <string>
//#include <cstdint>

#include "includes/rocksdb/db.h"
//#include "includes/rocksdb/merge_operator.h"

namespace Database {

rocksdb::DB* LoadNetworkManagementDB();
rocksdb::DB* LoadBalancesDB();
rocksdb::DB* LoadKeysDB();
rocksdb::DB* LoadSlotsDB();


/*
class AdditionMergeOperator: public AssociativeMergeOperator {
	public:
		virtual bool Merge(const Slice &key, const Slice* existing_value, const Slice& value, string* new_value, Logger* logger) const override {
			uint64_t existing = 0;
	        uint64_t oper;

	        if (existing_value) {
	          if (!Deserialize(*existing_value, &existing)) existing = 0;
	        }

	        if (!Deserialize(value, &oper)) oper = 0;

	        auto now = existing + oper;
	        *new_value = Serialize(now);

	        return true;
		}
};
*/
}

#endif