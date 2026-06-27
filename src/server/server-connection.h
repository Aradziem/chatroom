#ifndef SERVER_CONNECTION_H
#define SERVER_CONNECTION_H

#include <arpa/inet.h>

#include "common/connection.h"

class server_connection : public connection
{
	int fd;
	int client_fd;
	struct sockaddr_in address;

	void accept();

public:
	server_connection(int port);
	~server_connection();
	void read(int bytes, void* buffer);
	void write(int bytes, void* buffer);
	void drop();
};

#endif

