#include <string.h>
#include <iostream>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <stdio.h>
#include <map>
#include <fstream>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/wait.h>

#include "client/client.h"
#include "common/username.h"
#include "client/cmds.h"

using namespace std;

#define CONFIG_FILE "/.chatroomrc"
#define STATE_DIR "/.local/share"
#define SAVE_DIR "/chatroom-client"
#define SAVE_FILE "/msgs"

#define CURSOR "\033[7m \033[27m"

struct termios term, orig_term;

void cleanup(void) {
	tcsetattr(STDIN_FILENO, 0, &orig_term);
	printf("\033[?25h\n");
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
			std::cout << username_pretty(m->second.un) << ": " << m->second.str;
			printf("\033[0K\n");
			fflush(stdout);
			if(m->second.id > last_msg) last_msg = m->second.id;
		}
		usleep(100000);
	}
}

void display_command(char *cmd, unsigned int len, unsigned int win_h, char preceding_char, int display_cursor)
{
	unsigned int cursor_x, cursor_y;
	char resp_buf[64];
	unsigned int resp_len;
	int ret;
	char c;
	struct pollfd pfd;

	pfd.fd = STDIN_FILENO;
	pfd.events = POLLIN;

	printf("\033[6n");
	fflush(stdout);
	cursor_x = cursor_y = 0; /* fallback cur pos */
	resp_len = 0;
	for(resp_len = 0; resp_len < sizeof(resp_buf)-1;) {
		ret = poll(&pfd, 1, 10);
		if(ret > 0 && pfd.revents & POLLIN) {
			read(STDIN_FILENO, &c, 1);
			resp_buf[resp_len++] = c;
			if(c == 'R') break;
		}
	}
	resp_buf[resp_len] = 0;
	sscanf(resp_buf, "\033[%d;%dR", &cursor_y, &cursor_x);

	if(cursor_y == win_h) {
		printf("\n");
		--cursor_y;
	}
	
	printf("\033[%d;0H%c%.*s", win_h, preceding_char, len, cmd);
	if(display_cursor) printf(CURSOR);
	printf("\033[0K\033[%d;%dH", cursor_y, cursor_x);
	fflush(stdout);
}

void command_mode()
{
	struct winsize ws;
	char cmd_buf[2048];
	struct pollfd pfd;
	int ret;
	unsigned int cmd_len;
	char c;
	char failure_reason[64];

	ws.ws_row = 24; /* fallback term height */
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

	pfd.fd = STDIN_FILENO;
	pfd.events = POLLIN;

	display_command(cmd_buf, 0, ws.ws_row, ':', 1);
	for(cmd_len = 0; cmd_len < sizeof(cmd_buf)-1;) {
		ret = poll(&pfd, 1, 10);
		if(ret > 0 && pfd.revents & POLLIN) {
			read(STDIN_FILENO, &c, 1);

			switch(c) {
			case '\n':
			case '\r':
			case 4:
				goto exec;
			case 127:
				if(cmd_len) --cmd_len;
				break;
			case 21:
				cmd_len = 0;
				break;
			case 3:
			case 27:
				display_command(cmd_buf, 0, ws.ws_row, ' ', 0);
				return;
			default:
				cmd_buf[cmd_len++] = c;
				break;
			}
			display_command(cmd_buf, cmd_len, ws.ws_row, ':', 1);
		}
	}

exec:
	cmd_buf[cmd_len] = 0;
	if(exec_command(cmd_buf, failure_reason, sizeof(failure_reason))) {
		display_command(failure_reason, strlen(failure_reason), ws.ws_row, '!', 0);
	} else {
		display_command(cmd_buf, 0, ws.ws_row, ' ', 0);
	}
}

void read_message(client *clnt)
{
	message msg;
	int idx = 0;
	int ret;
	char c;
	struct pollfd pfd;

	pfd.fd = STDIN_FILENO;
	pfd.events = POLLIN;

	while(1) {
		printf("\r");
		cout << username_pretty(nick) << ": ";
		printf(CURSOR);
		
		for(idx = 0; idx < 63;) {
			ret = poll(&pfd, 1, 10);
			if(ret > 0 && pfd.revents & POLLIN) {
				read(STDIN_FILENO, &c, 1);

				switch(c) {
				case '\n':
				case '\r':
					goto send;
				case 127:
					if(idx) --idx;
					break;
				case 21:
					idx = 0;
					break;
				case 3:
				case 4:
				case 27:
					return;
				case ':':
					if(idx == 0) {
						printf("\r");
						cout << username_pretty(nick) << ": ";
						printf("%.*s\033[0K", idx, msg.str);
						fflush(stdout);
						command_mode();
						break;
					}
					/* FALLTHROUGH */
				default:
					msg.str[idx++] = c;
					break;
				}
			}
			printf("\r");
			cout << username_pretty(nick) << ": ";
			printf("%.*s\033[0K" CURSOR, idx, msg.str);
			fflush(stdout);
		}

	send:
		msg.str[idx] = 0;
		msg.send_time = time(0);
		msg.ms = get_ms();
		msg.un = nick;
		clnt->send_message(msg);
	}
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

	auto conf = config();
	for(int i = 1; i < argc; ++i) {
		if(argv[i][0] == '-') {
			for(int j = 1; argv[i][j]; ++j) {
				switch(argv[i][j]) {
#define ARG_OPT(OPT, STR) \
					case OPT: \
						if(i + 1 >= argc) { \
							cerr << "expected -" << OPT << " <" << STR << ">\n"; \
							exit(1); \
						} \
						conf[STR] = argv[i+1]; \
						goto argument_end;
					ARG_OPT('i', "ip");
					ARG_OPT('u', "nick");
					default:
						cerr << "unknown -" << argv[i][j] << '\n';
						exit(1);
				}
			}
		}
argument_end:
	}
	if(!conf.contains("nick")) conf["nick"] = "Anonymous";
	if(!conf.contains("ip")) conf["ip"] = "127.0.0.1";

	std::string raw_nick = "Anonymous";
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

	client c(conf["ip"], 6666, save_path);
	pid_t pid = fork();
	if(pid < 0) {
		cerr << "fork error\n";
		exit(1);
	} else if(pid == 0) {
		display(&c);
		return 0;
	}

	setbuf(stdin, NULL);
	tcgetattr(STDIN_FILENO, &term);
	orig_term = term;
	term.c_lflag &= ~ECHO & ~ICANON & ~ISIG & ~IEXTEN & ~ICRNL;
	term.c_cc[VMIN] = 1;
	term.c_cc[VTIME] = 0;
	atexit(cleanup);
	tcsetattr(STDIN_FILENO, 0, &term);
	printf("\033[?25l");

	read_message(&c);
	kill(pid, SIGTERM);
	waitpid(pid, NULL, 0);
}

