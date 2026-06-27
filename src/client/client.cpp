#include <cstring>

#include "client.h"
#include "client/client-connection.h"

client::client(std::string server_ip, int server_port, char *nick)
{
	strcpy(this->nick, nick);
	ip = server_ip;
	port = server_port;
}

void client::send_message(message msg)
{
	strcpy(msg.nick, nick);
	client_connection conn(ip,port);
	char message_type = 's';

	conn.write(1,&message_type);
	conn.write(sizeof(msg),&msg);
}

std::vector<message> client::messages_since (time_t timestamp, uint32_t ms)
{
	client_connection conn(ip,port);
	char message_type = 'r';

	conn.write(1,&message_type);
	conn.write(sizeof(timestamp),&timestamp);
	conn.write(4,&ms);

	int message_number;
	conn.read(sizeof(message_number),&message_number);
	std::vector<message> messages;
	messages.resize(message_number);
	for(int i=0;i<message_number;i++)
	{
		conn.read(sizeof(message),&messages[i]);
	}
	return messages;
}

