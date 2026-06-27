#ifndef MESSAGE_H
#define MESSAGE_H

#include <time.h>
#include <stdint.h>

struct message
{
	time_t send_time;
	uint32_t ms;
	char str[64];
    char nick[16];
};

#endif

