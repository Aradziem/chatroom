#ifndef MESSAGE_H
#define MESSAGE_H

#include <time.h>
#include <stdint.h>

#include "common/username.h"

struct message
{
	time_t send_time;
	uint32_t ms;
	char str[64];
	struct username un;
};

#endif

