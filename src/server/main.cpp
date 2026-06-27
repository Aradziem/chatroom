#include <stdlib.h>

#include "server/server.h"

server *ps;

void cleanup(void) {
	ps->~server();
}

int main() {
	server s(6666);
	ps = &s;
	atexit(cleanup);
	while(true)
	{
		s.handle();
	}
}

