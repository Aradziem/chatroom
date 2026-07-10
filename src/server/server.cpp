#include "server.h"

void server::handle_write_message()
{
	network_data::write_message in;
	network_data::message_recvd out;

	conn.read(sizeof(network_data::write_message), &in);
	in.msg.id = msgs.msgs.size() + 1;
	msgs.register_msg(in.msg);
	out.id = in.msg.id;
	conn.write(sizeof(network_data::message_recvd), &out);
}

void server::handle_query_messages()
{
	network_data::query_messages in;
	network_data::messages_diff out;
	std::map<msg_id, struct message>::iterator it;
	unsigned int i;

	conn.read(sizeof(network_data::query_messages), &in);

	it = msgs.msgs.upper_bound(in.first_id);
	out.num = std::distance(it, msgs.msgs.end());
	conn.write(sizeof(network_data::messages_diff), &out);

	for(i = 0; i< out.num; ++i, ++it)
	{
		conn.write(sizeof(struct message), &it->second);
	}
}

void server::handle()
{
	network_data::type t;

	conn.read(sizeof(network_data::type), &t);

	switch (t)
	{
		case NETWORK_DATA_WRITE_MESSAGE:
			handle_write_message();
			break;
		case NETWORK_DATA_QUERY_MESSAGES:
			handle_query_messages();
			break;
	}
	conn.drop();
}

