#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <time.h>
#include <stdint.h>
#include <vector>

#include "common/message.h"
#include "common/username.h"
#include "common/msg-storage.h"

class client
{
	std::string ip;
	int port;
public:
	msg_storage msgs;

	client(std::string server_ip, int server_port, std::string svpth);
	~client() = default;

	void send_message(message msg);
	void recv_messages(msg_id last);
};

#endif

