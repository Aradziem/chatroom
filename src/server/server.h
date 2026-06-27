#ifndef SERVER_H
#define SERVER_H

#include <vector>

#include "server/server-connection.h"
#include "common/message.h"

class server
{
	server_connection conn;
	std::vector<message> messages;

	void handle_s ();
	void handle_r();
public:

	server(int port) : conn(port) { };

	void handle();
};

#endif

