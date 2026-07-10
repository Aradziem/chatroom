#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>

#include "client-connection.h"

client_connection::client_connection(std::string ip,int port)
{
	if((fd = socket(AF_INET,SOCK_STREAM, 0)) < 0)
		{
			std::cerr<<"socket error";
			exit(1);
		}

	address.sin_family = AF_INET;
	address.sin_port = htons(port);

	if(inet_pton(AF_INET, ip.c_str(), &address.sin_addr) <= 0)
		{
			std::cerr<<"inet_pton error";
			exit(1);
		}

	if(connect(fd, (struct sockaddr*)& address, sizeof(address)) < 0)
		{
			std::cerr<<"connect error";
			exit(1);
		}
}
	
client_connection::~client_connection()
{
	close(fd);
}

void client_connection::read(int bytes, void* buffer)
{
	::read(fd, buffer, bytes);
}

void client_connection::write(int bytes, void* buffer)
{
	::write(fd, buffer, bytes);
}

void client_connection::drop()
{
	close(fd);
	fd = -1;
}

