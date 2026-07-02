#include <cstring>

#include "client.h"
#include "client/client-connection.h"

client::client(std::string server_ip, int server_port, struct username nick, std::string svpth) : msgs(svpth)
{
	un = nick;
	ip = server_ip;
	port = server_port;
}

void client::send_message(message msg)
{
	msg.un = un;
	client_connection conn(ip,port);
	char message_type = 's';

	conn.write(1,&message_type);
	conn.write(sizeof(msg),&msg);

	conn.read(sizeof(msg_id), &msg.id);
	msgs.register_msg(msg);
}

void client::recv_messages (msg_id last)
{
	client_connection conn(ip,port);
	char message_type = 'r';

	conn.write(1,&message_type);
	conn.write(sizeof(msg_id),&last);

	unsigned int message_number;
	conn.read(sizeof(unsigned int),&message_number);
	for(unsigned int i=0;i<message_number;i++)
	{
		struct message m;
		conn.read(sizeof(struct message),&m);
		msgs.register_msg(m);
	}
}

