#ifndef SERVER_H
#define SERVER_H

#include <vector>

#include "server/server-connection.h"
#include "common/message.h"
#include "common/msg-storage.h"

class server
{
	server_connection conn;
	msg_storage msgs;

	void handle_write_message();
	void handle_query_messages();
public:

	server(int port, std::string svpth) : conn(port), msgs(svpth) { };
	~server() = default;

	void handle();
};

#endif

