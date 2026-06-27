#include <string.h>
#include <iostream>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <stdio.h>

#include "client/client.h"

using namespace std;

#define CONFIG_FILE "/.chatroomrc"

struct termios term, orig_term;
char nick[16] = "Anonymous";

void cleanup(void) {
	tcsetattr(STDIN_FILENO, 0, &orig_term);
}

uint32_t get_ms() {
	static struct timespec ts = {};
	clock_gettime(CLOCK_REALTIME, &ts);
	return ts.tv_nsec / 1e6;
}

void display(client* c)
{
	static time_t timestamp =0;
	static uint32_t ms = 0;

	while(1)
	{
		vector<message> messages = c->messages_since(timestamp, ms);
		timestamp = time(0);
		ms = get_ms();
		for(long unsigned int i = 0; i< messages.size();i++)
		{
			printf("\r");
			cout<< messages[i].nick << ": " << messages[i].str<<'\n'; 
		}
		usleep(100000);
	}
}

message read_message()
{
	message msg;
	int idx = 0;
	int ret;
	char c;
	struct pollfd pfd;

	printf("\r%s: ", msg.nick);

	pfd.fd = STDIN_FILENO;
	pfd.events = POLLIN;
	
	for(idx = 0; idx < 63;) {
		ret = poll(&pfd, 1, 10);
		if(ret > 0 && pfd.revents & POLLIN) {
			ignore = read(STDIN_FILENO, &c, 1);

			switch(c) {
			case '\n':
			case '\r':
				goto send;
			case 127:
				--idx;
				break;
			case 21:
				idx = 0;
				break;
			default:
				msg.str[idx++] = c;
				break;
			}
		}
		printf("\r%s: %.*s\033[0K", nick, idx, msg.str);
		fflush(stdout);
	}

send:
	msg.str[idx] = 0;
	msg.send_time = time(0);
	msg.ms = get_ms();
	return msg;
}

int main(int argc, char **argv) {
	char const *ip = "127.0.0.1";
	for(int i = 1; i < argc; ++i) {
		if(argv[i][0] == '-') {
			for(int j = 1; argv[i][j]; ++j) {
				switch(argv[i][j]) {
					case 'i':
						if(i + 1 >= argc) {
							cerr << "expected -i <ip>\n";
							exit(1);
						}
						ip = argv[i+1];
						goto argument_end;
					default:
						cerr << "unknown -" << argv[i][j] << '\n';
						exit(1);
				}
			}
		}
argument_end:
	}

	char *home = getenv("HOME");
	if(home) {
		char *config_path = new char[strlen(home) + sizeof(CONFIG_FILE)];
		if(config_path) {
			strcpy(config_path, home);
			strcat(config_path, CONFIG_FILE);
			FILE *config = fopen(config_path, "r");
			if(config) {
				if(!fscanf(config, "%16[^\n]", nick))
				{
					strcpy(nick, "Anonymous");
				}
				char newline;
				ignore = fscanf(config, "%c", &newline);//return isn't important here
			}
		}
		delete[] config_path;
	}

	client c(ip, 6666,nick);
	pid_t pid = fork();
	if(pid < 0) {
		cerr << "fork error\n";
		exit(1);
	} else if(pid == 0) {
		display(&c);
		return 0;
	}

	tcgetattr(STDIN_FILENO, &term);
	orig_term = term;
	term.c_lflag &= ~ECHO & ~ICANON & ~IEXTEN & ~ICRNL;
	atexit(cleanup);
	tcsetattr(STDIN_FILENO, 0, &term);

	while(1) {
		c.send_message(read_message());
	}
}

