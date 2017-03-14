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
 * @file globals.h
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "includes/boost/asio.hpp"
static boost::asio::io_service NETWORK_SOCKET_SERVICE;

// static const std::pair<std::string,std::string> WORLD_BANK("entity.udc.world","4000");
static const std::pair<std::string,std::string> WORLD_BANK("localhost","4000");

static std::vector<std::pair<std::string,std::string>> TRACKERS = {
	std::pair<std::string,std::string>("localhost","5000")//,
	//std::pair<std::string,std::string>("tracker.udc.world","5000")
};

//Metadata
static std::string CURRENT_VERSION 						= "";
static std::string CURRENT_NMB_VERSION 					= "";

static const std::string WORLD_BANK_ACCOUNT 			= "UDC0000000";
static const std::string WORLD_BANK_ENTITY 				= "45FFFFFF";
static const std::string WORLD_BANK_NODE 				= "4EFFFFFF";
static const std::string WORLD_BANK_NODE_ACCOUNT 		= "UDC0000002";


#define SYNCHRONIZATION_MARGIN							60 //60secs to allow connecting to all peers

#define FIRST_ENTITY_ID									"45000000"
#define FIRST_NODE_ID									"4E000000"

#define DAO_BASE_ID										"4FFFFFFF"
#define DAO_BASE_ACCOUNT								"DAO0000000"
#define DAO_NETWORK_RATE								0.1
#define DAS_BASE_ID										"53FFFFFF"
#define DAS_BASE_ACCOUNT								"DAS0000000"
#define DAS_NETWORK_RATE								0.1

#define ITERATOR_ENTITY_FIRST							"ENTITY45000000" //first Managing Entity ID
#define ITERATOR_ENTITY_END								"ENTITY46000000" //ID following the last Managing Entity ID
#define ITERATOR_NODE_FIRST								"NODE4E000000" //first Validating Node ID
#define ITERATOR_NODE_END								"NODE4F000000" //ID following the last Validating Node ID

#define STANDARD_HASH_LENGTH							40
#define ACCOUNTS_HASH_LENGTH							STANDARD_HASH_LENGTH
#define MERKLE_HASH_LENGTH								STANDARD_HASH_LENGTH
#define LEDGER_HASH_LENGTH								STANDARD_HASH_LENGTH
#define ENTRY_HASH_LENGTH								STANDARD_HASH_LENGTH
#define BLOCK_HASH_LENGTH								STANDARD_HASH_LENGTH

#define NETWORK_CODE_LENGTH								2
#define NETWORK_DATA_LENGTH								2
#define NETWORK_DATA_LENGTH_BIG							4
#define NETWORK_FILE_LENGTH								4
#define NETWORK_SIGNATURE_LENGTH						1
#define NETWORK_TRANSACTION_LENGTH						2
#define NETWORK_ENTRY_LENGTH							2
#define NETWORK_PUBLIC_KEY_LENGTH						1
#define NETWORK_TIMESTAMP_LENGTH						10
#define NETWORK_IP_LENGTH								15
#define NETWORK_PORT_LENGTH								2
#define NETWORK_SUBSCRIPTION_LENGTH						1
#define NETWORK_REGISTRATION_DELAY						86400 //1day
#define NETWORK_IDENTIFICATION_DELAY					60 //1min
#define ERROR_CODE_LENGTH								2
#define NETWORK_FILE_BLOCK_SIZE							16384	//16kB chunks for file transfers


#define TRANSACTION_DELAY_NEW							40000000000000000 //now 15s but should be 1s or 5s
// #define TRANSACTION_DELAY_NEW							15000000000 //now 15s but should be 1s or 5s
#define TRANSACTION_DELAY_CONFIRMATION					15000000000 //now 15s but should be 5s or 10s
#define TRANSACTION_DELAY_REGISTRATION					5000000000 //now 5s but should be 1s or 3s

#define DAO_FEE_REDISTRIBUTION_INTERVAL					3600000000000 //1h in nanos
#define DAS_FEE_REDISTRIBUTION_INTERVAL					3600000000000 //1h in nanos

#define TRANSACTION_HASH_LENGTH							STANDARD_HASH_LENGTH
#define TRANSACTION_MAX_LENGTH							64000 // 64kB
#define TRANSACTION_MIN_LENGTH							13 // {"type":"XX"}

#define NETWORK_QUORUM_RATIO							0.4
#define TRANSACTION_BROADCAST_RATIO						0.8
#define TRANSACTION_CONFIRMATION_RATIO					0.6
#define LEDGER_CONFIRMATION_RATIO						0.6
#define BLOCK_CONFIRMATION_RATIO						0.6

#define ACCOUNT_LENGTH									10
#define SLOT_LENGTH										4
#define NUMBER_SLOTS									26*26*26*10 //175760

#define DISPATCHER_NODE									0
#define DISPATCHER_ENTITY								1
#define DISPATCHER_DAO									2
#define DISPATCHER_DAS									3

#define NODE_REPUTATION_DEFAULT							0
#define NODE_REPUTATION_TRESHOLD						0
#define NODE_REPUTATION_INCREMENT						1
#define NODE_REPUTATION_DECREMENT						1
#define NODE_REPUTATION_LEDGER_MUX						100

#define DEFAULT_NODE_PORT								4200
#define NODE_MAX_RETRIES								5
#define NODE_KEEP_ALIVE_INTERVAL						60

#define STANDARD_ID_LENGTH								8
#define NODE_ID_LENGTH									STANDARD_ID_LENGTH
#define ENTITY_ID_LENGTH 								STANDARD_ID_LENGTH

//public key size requirements in bytes (uncompressed and without prefix)
#define ACCOUNT_MIN_KEY_SIZE							64
#define NETWORK_MIN_KEY_SIZE							96
#define NODE_MIN_KEY_SIZE								NETWORK_MIN_KEY_SIZE
#define ENTITY_MIN_KEY_SIZE								NETWORK_MIN_KEY_SIZE

static const std::string LEDGER_EXTENSION				= ".ldgr";
static const std::string LEDGER_ACCOUNTS_EXTENSION		= ".accs";
static const std::string LEDGER_TRANSACTIONS_EXTENSION	= ".txs";
static const std::string LEDGER_OPERATIONS_EXTENSION	= ".ops";

//#define LEDGER_DURATION								86400000000000 //if 1 per day
#define LEDGER_DURATION									3600000000000 //if 1 per hour
#define LEDGER_CLOSING_INTERVAL							60000000000 //1min
#define LEDGER_SKIP_METADATA							400

#define MAX_DATA_REQUESTS								3

static const std::string GENESIS_LEDGER 				= "{\"ledgerId\":0,\"ledgerHash\":\"E22BA98B0FB3472B7F0EAF23D60A059AFCD86C5F\",\"previousLedgerHash\":\"9C1185A5C5E9FC54612808977EE8F548B2258D31\",\"accountsHash\":\"9C1185A5C5E9FC54612808977EE8F548B2258D31\",\"transactionsRoot\":\"9C1185A5C5E9FC54612808977EE8F548B2258D31\",opening\":0,\"closing\":1451606399000000000,\"numberOfAccounts\":0,\"numberOfTransactions\":0,\"amountInCirculation\":0,\"amountTraded\":0,\"feesCollected\":0,\"accounts\":[],\"transactions\":[]}";
static const std::string GENESIS_LEDGER_HASH 			= "E22BA98B0FB3472B7F0EAF23D60A059AFCD86C5F";
#define GENESIS_LEDGER_ID								0
#define GENESIS_LEDGER_END								1451606399000000000 //2015/12/31 23h59s59


static const std::string BLOCK_EXTENSION 				= ".nmb";
static const std::string BLOCK_ENTRIES_EXTENSION 		= ".ents";

#define PUBLISHER_SUBSCRIPTIONS_LENGTH 					40	

#define KEYS_BACKUP_INTERVAL							5400 //1h30
#define NETWORK_BACKUP_INTERVAL							7200 //2h
#define PUBLISHER_BACKUP_INTERVAL						3600 //1h
#define SLOTS_BACKUP_INTERVAL							3600 //1h

static const std::string RIPEMD160_NULL_HASH = "9C1185A5C5E9FC54612808977EE8F548B2258D31";

//data validation patterns
static const std::string PATTERN_ACCOUNT				= "[A-Z]{3}[0-9]{7}";
static const std::string PATTERN_SLOT					= "[A-Z]{3}[0-9]";
static const std::string PATTERN_PASSPORT_ID			= "50[A-F0-9]{6}";
static const std::string PATTERN_ENTITY_ID				= "45[A-F0-9]{6}";
static const std::string PATTERN_NODE_ID				= "4E[A-F0-9]{6}";
static const std::string PATTERN_DAO_ID					= "4F[A-F0-9]{6}";
static const std::string PATTERN_DAS_ID					= "53[A-F0-9]{6}";
static const std::string PATTERN_HOSTNAME				= "([a-zA-Z0-9/.-]+)";
static const std::string PATTERN_IPV4					= "(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9][0-9]|[0-9])([.](25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9][0-9]|[0-9])){3}";
static const std::string PATTERN_PORT					= "(6553[0-5]|655[0-2][0-9]|65[0-4][0-9]{2}|6[0-4][0-9]{3}|[1-5][0-9]{4}|[1-9][0-9]{1,3}|[0-9])";
static const std::string PATTERN_HOST					= "("+PATTERN_HOSTNAME+"|"+PATTERN_IPV4+")"+PATTERN_PORT;
static const std::string PATTERN_HEX_STRING				= "[A-F0-9]+";
static const std::string PATTERN_BASE64					= "[a-zA-Z0-9/+]+={0,2}";
static const std::string PATTERN_HASH					= "[A-F0-9]{40}";
static const std::string PATTERN_DATA_VERSION			= "(201[5-9]|20[2-9][0-9])((01|03|05|07|08|10|12)([0-2][0-9]|3[0-1])|(04|06|09|11)([0-2][0-9]|30)|02[0-2][0-9])v([1-9][0-9]*)";
static const std::string PATTERN_PATH					= "[/]?(.+[/])*.+";
static const std::string PATTERN_NETWORK_VERSION		= "*";
// static const std::string PATTERN_PUBLIC_KEY			= "(\x04(.{64}|.{96}|.{132})|(\x03|\x02)(.{32}|.{48}|.{64}))";
// static const std::string PATTERN_PUBLIC_KEY_HEX		= "(04([A-F0-9]{124}|[A-F0-9]{126}|[A-F0-9]{128}|[A-F0-9]{188}|[A-F0-9]{190}|[A-F0-9]{192}|[A-F0-9]{260}|[A-F0-9]{262}|[A-F0-9]{264})|(03|02)([A-F0-9]{62}|[A-F0-9]{64}|[A-F0-9]{94}|[A-F0-9]{96}|[A-F0-9]{130}|[A-F0-9]{132}))";
///Optimized regex
static const std::string PATTERN_PUBLIC_KEY				= "(\x04.{64}(.{32}(.{36})?)?)|((\x03|\x02).{31}(.{15}(.{17})?)?)";
static const std::string PATTERN_PUBLIC_KEY_HEX			= "(04[A-F0-9]{128}([A-F0-9]{64}([A-F0-9]{72})?)?)|((02|03)[A-F0-9]{64}([A-F0-9]{32}([A-F0-9]{36})?)?)";
///Optimized regex with support for different coordinate sizes: e.g. (x,y) coordinates size in Bytes for SECP256r1: ((31|32),(31|32))
// static const std::string PATTERN_PUBLIC_KEY			= "(\x04.{62}(.(.(.{30}(.(.(.{34}(.(.)?)?)?)?)?)?)?)?)|((\x03|\x02).{31}(.(.{15}(.(.{17}(.)?)?)?)?)?)";
// static const std::string PATTERN_PUBLIC_KEY_HEX		= "(04[A-F0-9]{124}([A-F0-9]{2}([A-F0-9]{2}([A-F0-9]{60}([A-F0-9]{2}([A-F0-9]{2}([A-F0-9]{68}([A-F0-9]{2}([A-F0-9]{2})?)?)?)?)?)?)?)?)|((02|03)[A-F0-9]{62}([A-F0-9]{2}([A-F0-9]{30}([A-F0-9]{2}([A-F0-9]{34}([A-F0-9]{2})?)?)?)?)?)";


//local data locations
static const std::string LOCAL_DATA						= "data/";
static const std::string LOCAL_DATA_SLOTS				= LOCAL_DATA+"slots.dat";
static const std::string LOCAL_DATA_NODES				= LOCAL_DATA+"nodes.dat";
static const std::string LOCAL_DATA_NODES_INDEX			= LOCAL_DATA+"nodes.idx";
static const std::string LOCAL_DATA_ENTITIES			= LOCAL_DATA+"entities.dat";
static const std::string LOCAL_DATA_ENTITIES_INDEX		= LOCAL_DATA+"entities.idx";
static const std::string LOCAL_DATA_SUBSCRIBERS			= LOCAL_DATA+"subscribers.dat";
static const std::string LOCAL_DATA_UPDATES				= LOCAL_DATA+"updates.dat";
static const std::string LOCAL_DATA_LEDGERS_INDEX		= LOCAL_DATA+"ldgr.idx";
static const std::string LOCAL_DATA_BLOCKS_INDEX		= LOCAL_DATA+"nmb.idx";
static const std::string LOCAL_DATA_DATABASES			= LOCAL_DATA+"dbs/";
static const std::string LOCAL_DATA_LEDGERS				= LOCAL_DATA+"ledgers/";
static const std::string LOCAL_DATA_BLOCKS				= LOCAL_DATA+"nmb/";
static const std::string LOCAL_DATA_KEYS				= LOCAL_DATA+"keys/";
static const std::string LOCAL_TEMP_DATA				= LOCAL_DATA+"temp/";
// static const std::string LOCAL_DATA_DAO				= LOCAL_DATA+"dao/";
// static const std::string LOCAL_DATA_DAS				= LOCAL_DATA+"das/";
static const std::string LOCAL_DATA_MODULES				= LOCAL_DATA+"modules/";

//databases locations
static const std::string DATABASE_PUBLIC_KEYS				= LOCAL_DATA_DATABASES+"public_keys";
static const std::string DATABASE_PUBLIC_KEYS_BACKUP		= LOCAL_DATA_DATABASES+"public_keys_backup";
static const std::string DATABASE_SLOTS						= LOCAL_DATA_DATABASES+"account_slots";
static const std::string DATABASE_BALANCES					= LOCAL_DATA_DATABASES+"account_balances";
static const std::string DATABASE_NETWORK_MANAGEMENT		= LOCAL_DATA_DATABASES+"network_management";
static const std::string DATABASE_NETWORK_MANAGEMENT_BACKUP	= LOCAL_DATA_DATABASES+"network_management_backup";



//Signatures Encoding
#define ENCODING_NONE									0
#define ENCODING_HEX									1
#define ENCODING_BASE64									2


#define CONFIRMATION_TRANSACTION						0
#define CONFIRMATION_LEDGER								1
#define CONFIRMATION_BLOCK								2

//NMB
static const std::string GENESIS_BLOCK_HASH 			= "E22BA98B0FB3472B7F0EAF23D60A059AFCD86C5F";
#define GENESIS_BLOCK_ID								0
#define GENESIS_BLOCK_END								1451606399 //2015/12/31 23h59s59


#define ENTRY_MIN_LENGTH								100
#define ENTRY_DELAY_NEW									30 //seconds

#define BLOCK_DURATION									900	//15mins
#define BLOCK_DATE_LENGTH								10
#define BLOCK_CLOSING_INTERVAL							60000000000 //1min

static const std::string RESOURCE_PROTECTION_PRIVATE	= "private";
static const std::string RESOURCE_PROTECTION_RESTRICTED	= "restricted";
static const std::string RESOURCE_PROTECTION_PUBLIC		= "public";
static const std::string DEFAULT_RESOURCE_PROTECTION	= RESOURCE_PROTECTION_PRIVATE;

static const std::string NMDB_MASK_DELIMITER			= ".";
static const std::string NMDB_MASK_ENTRY_TYPE			= NMDB_MASK_DELIMITER+"op"+NMDB_MASK_DELIMITER;
static const std::string NMDB_MASK_PROTECTION			= NMDB_MASK_DELIMITER+"protection";
static const std::string NMDB_MASK_SUPERVISOR			= NMDB_MASK_DELIMITER+"supervisor";
static const std::string NMDB_MASK_SUPERVISING			= NMDB_MASK_DELIMITER+"supervising";
static const std::string NMDB_MASK_MANAGER				= NMDB_MASK_DELIMITER+"manager";
static const std::string NMDB_MASK_START				= NMDB_MASK_DELIMITER+"start";
static const std::string NMDB_MASK_VALIDITY				= NMDB_MASK_DELIMITER+"validity";
static const std::string NMDB_MASK_IDENTIFIER			= NMDB_MASK_DELIMITER+"identifier";
static const std::string NMDB_MASK_PASSPORT				= NMDB_MASK_DELIMITER+"passport";
static const std::string NMDB_MASK_HOST					= NMDB_MASK_DELIMITER+"host";
static const std::string NMDB_MASK_VERSION				= NMDB_MASK_DELIMITER+"version";
static const std::string NMDB_MASK_ACCOUNT				= NMDB_MASK_DELIMITER+"account";
static const std::string NMDB_MASK_REPUTATION			= NMDB_MASK_DELIMITER+"reputation";
static const std::string NMDB_MASK_ID					= NMDB_MASK_DELIMITER+"id";
static const std::string NMDB_MASK_BLOCK				= "block"+NMDB_MASK_DELIMITER;
static const std::string NMDB_MASK_LEDGER				= "ledger"+NMDB_MASK_DELIMITER;
static const std::string NMDB_MASK_NEXT					= NMDB_MASK_DELIMITER+"next";
static const std::string NMDB_MASK_PREVIOUS				= NMDB_MASK_DELIMITER+"previous";
static const std::string NMDB_MASK_OPEN					= NMDB_MASK_DELIMITER+"open";
static const std::string NMDB_MASK_CLOSE				= NMDB_MASK_DELIMITER+"close";


#endif