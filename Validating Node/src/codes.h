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
 * @file codes.h
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#ifndef CODES_H
#define CODES_H

#include <string>

//Transaction Types
#define TRANSACTION_BASIC					0x00
#define TRANSACTION_DELAYED					0x01
#define TRANSACTION_FUTURE					0x02
#define TRANSACTION_DAO						0x4F
#define TRANSACTION_DAS						0x53
static const std::string TRANSACTION_TYPE_BASIC		= "00";
static const std::string TRANSACTION_TYPE_DELAYED	= "01";
static const std::string TRANSACTION_TYPE_FUTURE	= "02";
static const std::string TRANSACTION_TYPE_DAO		= "4F";
static const std::string TRANSACTION_TYPE_DAS		= "53";

//Event Types
static const std::string TX_DELAYED_REQUEST		= "request";
static const std::string TX_DELAYED_RELEASE		= "release";
static const std::string TX_FUTURE_AUTHORIZE	= "authorize";
static const std::string TX_FUTURE_EXECUTE		= "request";


//Error Codes
//Transaction
#define ERROR_TRANSACTION_INVALID			1000
#define ERROR_TRANSACTION_DUPLICATE			1001
#define ERROR_TRANSACTION_REJECTED			1002
#define ERROR_UNSUPPORTED_TYPE				1003
#define ERROR_UNSUPPORTED_EVENT				1004
#define ERROR_UNSUPPORTED_SERVICE			1005
#define ERROR_HASH							1006
#define ERROR_TIMESTAMP						2000
#define ERROR_AMOUNT_SENT					2001
#define ERROR_FEE_OUTBOUND					2002
#define ERROR_REMAIN_OUTBOUND				2003
#define ERROR_FEE_INBOUND					2004
#define ERROR_REMAIN_INBOUND				2005
#define ERROR_INSUFICIENT_FUNDS				2006
#define ERROR_ACCOUNT						3000
#define ERROR_ACCOUNT_SENDER				3001
#define ERROR_ACCOUNT_RECEIVER				3002
#define ERROR_ACCOUNT_OUTBOUND				3003
#define ERROR_ACCOUNT_INBOUND				3004
#define ERROR_SLOT							3005
#define ERROR_SIGNATURE						4000
#define ERROR_SIGNATURE_SENDER				4001
#define ERROR_SIGNATURE_OUTBOUND			4002
#define ERROR_SIGNATURE_INBOUND				4003
#define ERROR_CONTENT						9000
//Network Management Entry
#define ERROR_ENTRY_INVALID					1000
#define ERROR_ENTRY_DUPLICATE				1001
#define ERROR_ENTRY_REJECTED				1002
#define ERROR_UNAUTHORIZED					1003
#define ERROR_UNSUPPORTED					1004
//#define ERROR_HASH							1006 //shared with transaction errors
#define ERROR_RESOURCE						2000
#define ERROR_DATA_STRUCTURE				2001
#define ERROR_DATA_CONTENT					2002
#define ERROR_TYPE							2003
#define ERROR_DATE							2004
#define ERROR_SIGNER						2005
//#define ERROR_SIGNATURE						4000 //shared with transaction errors
//Other
#define VALID								   0
#define ERROR_NODE_OFFLINE					3001


//Network Protocol Codes
//Management
#define NETWORK_KEEP_ALIVE					0x0000
#define NETWORK_ERROR						0x0001
#define NETWORK_EXIT						0x0002
#define NETWORK_NO_QUORUM					0x0003
//Registration
#define NETWORK_REGISTRATION				0x0100
#define NETWORK_REGISTRATION_SUCCESS		0x0101
#define NETWORK_REGISTRATION_FAILURE		0x0102
#define NETWORK_NEW_ENTITY					0x0103
#define NETWORK_NEW_NODE					0x0104
//Identification
#define NETWORK_IDENTIFICATION_ENTITY		0x2001
#define NETWORK_IDENTIFICATION_NODE			0x2002
#define NETWORK_IDENTIFICATION_SUCCESS		0x2003
#define NETWORK_IDENTIFICATION_FAILURE		0x2004
//Account
#define NETWORK_GET_PUBLIC_KEY				0x0300
#define NETWORK_PUBLIC_KEY					0x0301
#define NETWORK_KEY_NOT_FOUND				0x0302
//Transaction
#define NETWORK_SIGN						0xA000
#define NETWORK_SIGNED						0xA001
#define NETWORK_UNSYNCHRONIZED 				0xA002
#define NETWORK_NEW_TRANSACTION 			0xA100
#define NETWORK_TRANSACTION_ACCEPTED		0xA101
#define NETWORK_TRANSACTION_REJECTED		0xA102
#define NETWORK_BROADCAST_CONFIRMATION		0xA103
#define NETWORK_BROADCAST_TRANSACTION		0xA104
#define NETWORK_GET_TRANSACTION				0xA105
#define NETWORK_TRANSACTION					0xA106
//Ledger
#define NETWORK_LEDGER_CONSENSUS			0xA200
#define NETWORK_GET_LEDGER					0xA201
#define NETWORK_LEDGER						0xA202
#define NETWORK_LEDGER_NOT_FOUND			0xA203
//Network Management Entry
#define NETWORK_NEW_ENTRY		 			0xA300
#define NETWORK_ENTRY_ACCEPTED				0xA301
#define NETWORK_ENTRY_REJECTED				0xA302
#define NETWORK_GET_ENTRY					0xA303
#define NETWORK_ENTRY						0xA304
//Network Management Block
#define NETWORK_BLOCK_CONSENSUS				0xA400
#define NETWORK_GET_BLOCK					0xA401
#define NETWORK_BLOCK						0xA402
#define NETWORK_BLOCK_NOT_FOUND				0xA403
//Subscription
#define NETWORK_PUBLISH_TRANSACTION			0xB000
#define NETWORK_PUBLISH_LEDGER				0xB001
#define NETWORK_PUBLISH_BLOCK				0xB002
#define NETWORK_SUBSCRIBE					0xB100
#define NETWORK_SUBSCRIPTIONS				0xB101
#define NETWORK_UNSUBSCRIBE					0xB102
//Tracker
#define NETWORK_TRACKER_LATEST				0xB200
#define NETWORK_TRACKER_GET					0xB201

//Tracker Codes
#define TRACKER_CODE_TRANSACTION			1000
#define TRACKER_CODE_LEDGER					1001
#define TRACKER_CODE_NME					1002
#define TRACKER_CODE_NMB					1003
#define TRACKER_GET_BY_HASH					0x00
#define TRACKER_GET_BY_ID					0x01


//NMB
static const std::string ENTRY_TYPE_CREATE		= "create";
static const std::string ENTRY_TYPE_UPDATE		= "update";
static const std::string ENTRY_TYPE_CANCEL		= "cancel";
static const std::string ENTRY_TYPE_RENEW		= "renew";

#define ID_TYPE_INVALID						0
#define ID_TYPE_ACCOUNT						1
#define ID_TYPE_PASSPORT					2
#define ID_TYPE_ENTITY						3
#define ID_TYPE_NODE						4
#define ID_TYPE_DAO							5
#define ID_TYPE_DAS							6

static const std::string NMB_RESOURCE			= "RESOURCE";
static const std::string NMB_RESOURCE_PASSPORT	= "PASSPORT";
static const std::string NMB_RESOURCE_ENTITY	= "ENTITY";
static const std::string NMB_RESOURCE_NODE		= "NODE";
static const std::string NMB_RESOURCE_DAO		= "DAO";
static const std::string NMB_RESOURCE_DAS		= "DAS";
static const std::string NMB_RESOURCE_SLOT		= "SLOT";
static const std::string NMB_RESOURCE_ACCOUNT	= "ACCOUNT";

#endif
