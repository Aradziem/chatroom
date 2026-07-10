#ifndef CONNECTION_H
#define CONNECTION_H

#include <stdint.h>

#include "common/message.h"

#define NETWORK_DATA_WRITE_MESSAGE 0
#define NETWORK_DATA_QUERY_MESSAGES 1

namespace network_data {
	typedef uint16_t type;

	struct write_message {
		struct message msg;
	} __attribute__((packed));

	struct message_recvd {
		msg_id id;
	} __attribute__((packed));

	struct query_messages {
		msg_id first_id;
	} __attribute__((packed));

	struct messages_diff {
		unsigned int num;
		/* struct message msgs[num]; */
	} __attribute__((packed));

	class connection
	{
	public:
		virtual void read(int bytes, void* buffer) = 0;
		virtual void write(int bytes, void* buffer) = 0;
		virtual void drop() = 0;

		void write_read(int w_sz, void *w_buf, int r_sz, void *r_buf);
	};
}

#endif

