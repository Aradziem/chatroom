#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <time.h>
#include <stdint.h>
#include <vector>

#include "common/message.h"
#include "common/username.h"

class client
{
	struct username un;
	std::string ip;
	int port;
public:
	client(std::string server_ip, int server_port, struct username nick);
	void send_message(message msg);
	std::vector<message> messages_since (time_t timestamp, uint32_t ms);
};

#endif

