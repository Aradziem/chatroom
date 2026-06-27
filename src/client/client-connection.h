#ifndef CLIENT_CONNECTION_H
#define CLIENT_CONNECTION_H

#include <string>
#include <arpa/inet.h>

#include "common/connection.h"

class client_connection : public connection
{
	int fd;
	struct sockaddr_in address;

public:
	client_connection(std::string ip,int port);
	~client_connection();
	void read(int bytes, void* buffer);
	void write(int bytes, void* buffer);
};

#endif

