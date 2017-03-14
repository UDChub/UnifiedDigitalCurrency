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
 * @file communication.h
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#ifndef NODE_COMMUNICATION_H
#define NODE_COMMUNICATION_H

#include "includes/boost/asio.hpp"

class DAOManager;
class DASManager;
class Entities;
class Keys;
class Ledger;
class NetworkManager;
class Nodes;
class Publisher;
class TransactionsManager;

struct NodeStruct;
struct EntityStruct;
struct SubscriberStruct;


namespace Communication{

	void ListenToNode(bool *IS_OPERATING, bool *IS_SYNCHRONIZED, DAOManager *managerDAO, DASManager *managerDAS, Ledger *ledger, NetworkManager *networkManager, Nodes *nodes, TransactionsManager *txManager, NodeStruct *node, boost::asio::ip::tcp::socket *socket);
	void ListenToEntity(bool *IS_OPERATING, bool *IS_SYNCHRONIZED, DAOManager *managerDAO, DASManager *managerDAS, Keys *keysDB, NetworkManager *networkManager, TransactionsManager *txManager, EntityStruct *entity);
	void ListenToSubscriber(bool *IS_OPERATING, Publisher *publisher, SubscriberStruct *subscriber);
	void ListenToAnyone(bool *IS_OPERATING, Entities *entities, Nodes *nodes, Publisher *publisher);

}

#endif