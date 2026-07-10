#include <cstring>

#include "client.h"
#include "client/client-connection.h"

client::client(std::string server_ip, int server_port, std::string svpth) : msgs(svpth)
{
	ip = server_ip;
	port = server_port;
}

void client::send_message(message msg)
{
	client_connection conn(ip,port);
	network_data::type t;
	network_data::write_message out;
	network_data::message_recvd in;

	t = NETWORK_DATA_WRITE_MESSAGE;
	conn.write(sizeof(network_data::type), &t);
	out.msg = msg;
	conn.write_read(sizeof(network_data::write_message), &out, sizeof(network_data::message_recvd), &in);
	msg.id = in.id;
	msgs.register_msg(msg);
}

void client::recv_messages (msg_id last)
{
	client_connection conn(ip,port);
	network_data::type t;
	network_data::query_messages out;
	network_data::messages_diff in;
	unsigned int i;
	struct message m;

	t = NETWORK_DATA_QUERY_MESSAGES;
	conn.write(sizeof(network_data::type), &t);
	out.first_id = last;
	conn.write_read(sizeof(network_data::query_messages), &out, sizeof(network_data::messages_diff), &in);

	for(i=0;i<in.num;i++)
	{
		conn.read(sizeof(struct message),&m);
		msgs.register_msg(m);
	}
}

