#include "server.h"

void server::handle_s ()
{
	message msg;

	conn.read(sizeof(msg),&msg);
	messages.push_back(msg);
}

void server::handle_r()
{
	time_t timestamp;
	uint32_t ms;
	int first_to_send =-1;

	conn.read(sizeof(time_t),&timestamp);
	conn.read(4,&ms);

	for(unsigned long int i = 0 ; i < messages.size(); i++)
	{
		if(messages[i].send_time > timestamp || (messages[i].send_time == timestamp && messages[i].ms >= ms))
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

