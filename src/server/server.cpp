#include "server.h"

void server::handle_s ()
{
	message msg;

	conn.read(sizeof(msg),&msg);
	msg.id = msgs.msgs.size() + 1;
	msgs.register_msg(msg);
	conn.write(sizeof(msg_id), &msg.id);
}

void server::handle_r()
{
	msg_id first_id;
	std::map<msg_id, struct message>::iterator it;
	unsigned int i, num;

	conn.read(sizeof(msg_id), &first_id);

	it = msgs.msgs.upper_bound(first_id);
	num = std::distance(it, msgs.msgs.end());
	conn.write(sizeof(unsigned int), &num);

	for(i = 0; i< num; ++i, ++it)
	{
		conn.write(sizeof(struct message), &it->second);
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

