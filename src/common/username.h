#ifndef USERNAME_H
#define USERNAME_H

#include <string>
#include <stdint.h>

struct username /* is sent over the network, so it can't be a class */
{
	uint8_t packed[8][6];
	/* maybe pronouns? idk */
};

struct username un_from_str(std::string const str);
std::string username_pretty(struct username const un);

#endif

