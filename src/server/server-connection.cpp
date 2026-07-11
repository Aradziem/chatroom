#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "server-connection.h"

void server_connection::accept()
{
	if( client_fd < 0)
	{
		socklen_t sizeof_addr = sizeof(address);
		if((client_fd = ::accept(fd, (struct sockaddr*)& address, &sizeof_addr)) < 0)
		{
			std::cerr<<"accept error";
			exit(1);
		}
	}
}
server_connection::server_connection(int port)
{
	int one;

	if((fd = socket(AF_INET,SOCK_STREAM, 0))<0)
		{
			perror("socket");
			exit(1);
		}
	client_fd = -1;

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	one = 1;
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) < 0){
		perror("setsockopt");
	}

	if(bind(fd, (struct sockaddr*)& address, sizeof(address))<0)
		{
			perror("bind");
			exit(1);
		}
	if(listen(fd,64)<0)
		{
			perror("listen");
			exit(1);
		}

}
		
server_connection::~server_connection()
{
	drop();
	close(fd);
}

void server_connection::read(int bytes, void* buffer)
{
	accept();
	::read(client_fd, buffer, bytes);
}

void server_connection::write(int bytes, void* buffer)
{
	accept();
	::write(client_fd, buffer, bytes);
}
		
void server_connection::drop()
{
	close(client_fd);
	client_fd = -1;
}

