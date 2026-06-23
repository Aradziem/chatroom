#include "stuff.h"

server *ps;

void cleanup(void) {
	ps->~server();
}

int main(int argc, char **argv) {
	server s(6666);
	ps = &s;
	atexit(cleanup);
	while(true)
	{
		s.handle();
	}
}

