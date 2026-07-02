#include <string.h>
#include <iostream>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <stdio.h>
#include <map>
#include <fstream>
#include <sys/stat.h>

#include "client/client.h"
#include "common/username.h"

using namespace std;

#define CONFIG_FILE "/.chatroomrc"
#define STATE_DIR "/.local/share"
#define SAVE_DIR "/chatroom-client"
#define SAVE_FILE "/msgs"

struct termios term, orig_term;
struct username nick;

client *pc;

void cleanup(void) {
	tcsetattr(STDIN_FILENO, 0, &orig_term);
	pc->~client();
}

uint32_t get_ms() {
	static struct timespec ts = {};
	clock_gettime(CLOCK_REALTIME, &ts);
	return ts.tv_nsec / 1e6;
}

void display(client* c)
{
	static msg_id last_msg = 0;

	while(1)
	{
		c->recv_messages(last_msg);
		for(auto m = c->msgs.msgs.upper_bound(last_msg); m != c->msgs.msgs.end(); ++m)
		{
			printf("\r");
			std::cout << username_pretty(m->second.un) << ": " << m->second.str << '\n'; 
			if(m->second.id > last_msg) last_msg = m->second.id;
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

	printf("\r");
	cout << username_pretty(nick) << ": ";

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
		printf("\r");
		cout << username_pretty(nick) << ": ";
		printf("%.*s\033[0K", idx, msg.str);
		fflush(stdout);
	}

send:
	msg.str[idx] = 0;
	msg.send_time = time(0);
	msg.ms = get_ms();
	return msg;
}

std::map<std::string, std::string> config()
{
	std::map<std::string, std::string> m;
	char *home = getenv("HOME");
	if(home) {
		char *config_path = new char[strlen(home) + sizeof(CONFIG_FILE)];
		if(config_path) {
			strcpy(config_path, home);
			strcat(config_path, CONFIG_FILE);
			std::ifstream config(config_path);
			if(config.is_open()) {
				std::string ln;
				while(std::getline(config, ln)) {
					if(ln[0] == '#') continue;
					auto d = ln.find('=');
					if(d != std::string::npos) {
						m[ln.substr(0, d)] = ln.substr(d+1);
					}
				}
			}
		}
		delete[] config_path;
	}

	return m;
}

int main(int argc, char **argv) {
	std::ios::sync_with_stdio(true);

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

	std::string raw_nick = "Anonymous";
	auto conf = config();
	if(conf.contains("nick")) raw_nick = conf["nick"];
	nick = un_from_str(raw_nick);

	char *home = getenv("HOME");
	std::string save_path;
	if(home) {
		save_path = home;
		save_path += STATE_DIR;
		mkdir(save_path.c_str(), 0775);
		save_path += SAVE_DIR;
		mkdir(save_path.c_str(), 0775);
		save_path += SAVE_FILE;
	} else {
		save_path = "/tmp/chatroom-msgs-client";
	}

	client c(ip, 6666, nick, save_path);
	pc = &c;
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

