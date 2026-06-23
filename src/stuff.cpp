#include <iostream>
#include <cstring>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>

#include "stuff.h"

using namespace std;

void server_connection::accept()
{
	if( client_fd < 0)
	{
		socklen_t sizeof_addr = sizeof(address);
		if((client_fd = ::accept(fd, (struct sockaddr*)& address, &sizeof_addr)) < 0)
		{
			cerr<<"accept error";
			exit(1);
		}
	}
}
server_connection::server_connection(int port)
{
	if((fd = socket(AF_INET,SOCK_STREAM, 0))<0)
		{
			cerr<<"socket error";
			exit(1);
		}
	client_fd = -1;

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if(bind(fd, (struct sockaddr*)& address, sizeof(address))<0)
		{
			cerr<<"bind error";
			exit(1);
		}
	if(listen(fd,64)<0)
		{
			cerr<<"listen error";
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

client_connection::client_connection(string ip,int port)
{
	if((fd = socket(AF_INET,SOCK_STREAM, 0)) < 0)
		{
			cerr<<"socket error";
			exit(1);
		}

	address.sin_family = AF_INET;
	address.sin_port = htons(port);

	if(inet_pton(AF_INET, ip.c_str(), &address.sin_addr) <= 0)
		{
			cerr<<"inet_pton error";
			exit(1);
		}

	if(connect(fd, (struct sockaddr*)& address, sizeof(address)) < 0)
		{
			cerr<<"connect error";
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

client::client(string server_ip, int server_port)
{
	ip = server_ip;
	port = server_port;
}

void client::send_message(message msg)
{
	client_connection conn(ip,port);
	char message_type = 's';

	conn.write(1,&message_type);
	conn.write(sizeof(msg),&msg);
}

vector<message> client::messages_since (time_t timestamp)
{
	client_connection conn(ip,port);
	char message_type = 'r';

	conn.write(1,&message_type);
	conn.write(sizeof(timestamp),&timestamp);

	int message_number;
	conn.read(sizeof(message_number),&message_number);
	vector<message> messages;
	messages.resize(message_number);
	for(int i=0;i<message_number;i++)
	{
		conn.read(sizeof(message),&messages[i]);
	}
	return messages;
}

void server::handle_s ()
{
	message msg;

	conn.read(sizeof(msg),&msg);
	messages.push_back(msg);
}

void server::handle_r()
{
	time_t timestamp;
	int first_to_send =-1;

	conn.read(sizeof(time_t),&timestamp);

	for(int i = 0 ; i < messages.size(); i++)
	{
		if(messages[i].send_time >= timestamp)
		{
			first_to_send = i;
			break;
		}
	}
	int num_messages = messages.size() - first_to_send;
	if(first_to_send == -1)
	{
		num_messages = 0;
	}

	conn.write(sizeof(num_messages), &num_messages);
	for(int i = 0; i< num_messages;i++)
	{
		conn.write(sizeof(message),&messages[first_to_send+i]);
	}
}

void server::handle()
{
	char message_type;
	conn.read(1,&message_type);
	switch (message_type)
	{
		case 's':
			handle_s();
			break;
		case 'r':
			handle_r();
			break;
	}
	conn.drop();
}

