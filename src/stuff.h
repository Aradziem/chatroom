#include <iostream>
#include <cstring>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>

using namespace std;

class connection
{
	public:
		void read(int bytes, void* buffer);
		void write(int bytes, void* buffer);
		void drop();
};

class server_connection : public connection
{
	int fd;
	int client_fd;
	struct sockaddr_in address;

	void accept();
	public:
		server_connection(int port);
		~server_connection();
		void read(int bytes, void* buffer);
		void write(int bytes, void* buffer);
		void drop();
};

class client_connection : public connection
{
	int fd;
	struct sockaddr_in address;

	public:
		client_connection(string ip,int port);
		~client_connection();
		void read(int bytes, void* buffer);
		void write(int bytes, void* buffer);
};

struct message
{
	time_t send_time;
	uint32_t ms;
	char str[64];
    char nick[16];
};

class client
{
    char nick[16];
	string ip;
	int port;
	public:
		client(string server_ip, int server_port, char *nick);
		void send_message(message msg);
		vector<message> messages_since (time_t timestamp, uint32_t ms);
};

class server
{
	server_connection conn;
	vector<message> messages;

	void handle_s ();
	void handle_r();
	public:

		server(int port) : conn(port) { };

		void handle();
};

