#include "msg-storage.h"

#include <stdio.h>

#define FLUSH_INTERVAL 16

void msg_storage::flush()
{
	FILE *f = fopen(pth.c_str(), "wb");
	if(f) {
		std::map<msg_id, struct message>::size_type s = msgs.size();
		fwrite(&s, sizeof(std::map<msg_id, struct message>::size_type), 1, f);
		for(auto const& m : msgs) {
			fwrite(&m.second, sizeof(struct message), 1, f);
		}
		fclose(f);
	}
}

msg_storage::msg_storage(std::string const svpth) : unsaved(0), pth(svpth)
{
	FILE *f = fopen(pth.c_str(), "rb");
	if(f) {
		std::map<msg_id, struct message>::size_type s;
		fread(&s, sizeof(std::map<msg_id, struct message>::size_type), 1, f);
		for(std::map<msg_id, struct message>::size_type i = 0; i < s; ++i) {
			struct message m;
			fread(&m, sizeof(struct message), 1, f);
			msgs[m.id] = m;
		}
		fclose(f);
	}
}

msg_storage::~msg_storage()
{
	flush();
}

void msg_storage::register_msg(struct message const msg)
{
	msgs[msg.id] = msg;
	++unsaved;
	if(unsaved >= FLUSH_INTERVAL) {
		unsaved -= FLUSH_INTERVAL;
		flush();
	}
}

