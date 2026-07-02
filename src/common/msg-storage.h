#ifndef MSG_STORAGE_H
#define MSG_STORAGE_H

#include <map>
#include <string>

#include "common/message.h"

class msg_storage {
	unsigned int unsaved;
	std::string pth;
	void flush();
public:
	std::map<msg_id, struct message> msgs;

	msg_storage() = delete;
	msg_storage(std::string const svpth);
	~msg_storage();

	void register_msg(struct message const msg);
};

#endif

