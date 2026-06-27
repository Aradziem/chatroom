#ifndef CONNECTION_H
#define CONNECTION_H

class connection
{
public:
	void read(int bytes, void* buffer);
	void write(int bytes, void* buffer);
	void drop();
};

#endif

