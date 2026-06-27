#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <time.h>
#include <stdint.h>
#include <vector>

#include "common/message.h"

class client
{
    char nick[16];
	std::string ip;
	int port;
public:
	client(std::string server_ip, int server_port, char *nick);
	void send_message(message msg);
	std::vector<message> messages_since (time_t timestamp, uint32_t ms);
};

#endif

