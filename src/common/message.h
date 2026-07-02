#ifndef MESSAGE_H
#define MESSAGE_H

#include <time.h>
#include <stdint.h>

#include "common/username.h"

typedef uint64_t msg_id;

struct message
{
	msg_id id;
	time_t send_time;
	uint32_t ms;
	char str[64];
	struct username un;
};

#endif

