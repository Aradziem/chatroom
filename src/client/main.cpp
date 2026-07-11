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
#include <sys/timerfd.h>

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

enum io_comm_type { IO_COMM_DISP_MSG, IO_COMM_DISP_STATUS, IO_COMM_QUIT };
struct io_comm {
	enum io_comm_type type;
	union {
		struct {
			struct message inc_msg;
		} disp_msg;
		struct {
			int failure;
			char status[256];
		} disp_status;
		struct {
			int ret_val;
		} quit;
	};
};

enum logic_comm_type { LOGIC_COMM_MSG_WRITTEN, LOGIC_COMM_CMD, LOGIC_COMM_QUIT };
struct logic_comm {
	enum logic_comm_type type;
	union {
		struct {
			struct message new_msg;
		} msg_written;
		struct {
			char raw_cmd[256];
		} cmd;
		struct {
			int ret_val;
		} quit;
	};
};

void cleanup(void) {
	tcsetattr(STDIN_FILENO, 0, &orig_term);
	printf("\033[?25h\n");
}

uint32_t get_ms() {
	static struct timespec ts = {};
	clock_gettime(CLOCK_REALTIME, &ts);
	return ts.tv_nsec / 1e6;
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

void io_proc(int fd_in, int fd_out)
{
	int ret;
	struct pollfd poll_fds[2];
	struct io_comm incoming;
	struct logic_comm outgoing;
	std::string pretty_un;
	struct winsize ws;
	enum { IO_WRITING_MSG, IO_WRITING_COMMAND } state;
	char read_buffer[256];
	unsigned int read_length;
	char c;

	ws.ws_row = 24; /* fallback term height */
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

	poll_fds[0].fd = fd_in;
	poll_fds[0].events = POLLIN;

	poll_fds[1].fd = STDIN_FILENO;
	poll_fds[1].events = POLLIN;

	state = IO_WRITING_MSG;
	read_length = 0;
	pretty_un = username_pretty(nick);
	printf("\r(writing) %s: " CURSOR "\033[0K", pretty_un.c_str());
	fflush(stdout);
	while(1) {
		ret = poll(poll_fds, sizeof(poll_fds)/sizeof(*poll_fds), -1);
		if(ret > 0 && poll_fds[0].revents & POLLIN) {
			/* pipe */
			read(fd_in, &incoming, sizeof(struct io_comm));
			switch(incoming.type) {
			case IO_COMM_DISP_MSG:
				pretty_un = username_pretty(incoming.disp_msg.inc_msg.un);
				printf("\r%s: %s\033[0K\n", pretty_un.c_str(), incoming.disp_msg.inc_msg.str);
				fflush(stdout);
				break;
			case IO_COMM_DISP_STATUS:
				display_command(incoming.disp_status.status, strlen(incoming.disp_status.status), ws.ws_row, incoming.disp_status.failure ? '!' : ' ', 0);
				break;
			case IO_COMM_QUIT:
				exit(incoming.quit.ret_val);
			}
		}
		if(ret > 0 && poll_fds[1].revents & POLLIN) {
			/* stdin */
			read(STDIN_FILENO, &c, 1);
			switch(c) {
			case '\n':
			case '\r':
				goto send;
			case 127:
				if(read_length) --read_length;
				break;
			case 21:
				read_length = 0;
				break;
			case 3:
			case 27:
				switch(state) {
				case IO_WRITING_MSG:
					outgoing.type = LOGIC_COMM_QUIT;
					outgoing.quit.ret_val = 0;
					write(fd_out, &outgoing, sizeof(struct logic_comm));
					exit(0);
					break;
				case IO_WRITING_COMMAND:
					state = IO_WRITING_MSG;
					display_command(NULL, 0, ws.ws_row, ' ', 0);
					break;
				}
				break;
			case ':':
				if(state == IO_WRITING_MSG && read_length == 0) {
					printf("\r(writing) %s:\033[0K", pretty_un.c_str());
					fflush(stdout);
					state = IO_WRITING_COMMAND;
					break;
				}
				/* FALLTHROUGH */
			default:
				read_buffer[read_length++] = c;
				break;
			}

			if((state == IO_WRITING_MSG     && read_length >= sizeof(outgoing.msg_written.new_msg.str)-1) ||
			   (state == IO_WRITING_COMMAND && read_length >= sizeof(outgoing.cmd.raw_cmd)-1)) {
			send:
				switch(state) {
				case IO_WRITING_MSG:
					outgoing.type = LOGIC_COMM_MSG_WRITTEN;
					outgoing.msg_written.new_msg.send_time = time(0);
					outgoing.msg_written.new_msg.ms = get_ms();
					strcpy(outgoing.msg_written.new_msg.str, read_buffer);
					outgoing.msg_written.new_msg.un = nick;
					write(fd_out, &outgoing, sizeof(struct logic_comm));
					read_length = 0;
					break;
				case IO_WRITING_COMMAND:
					outgoing.type = LOGIC_COMM_CMD;
					strcpy(outgoing.cmd.raw_cmd, read_buffer);
					write(fd_out, &outgoing, sizeof(struct logic_comm));
					read_length = 0;
					state = IO_WRITING_MSG;
					break;
				}
			}
		}

		switch(state) {
		case IO_WRITING_MSG:
			pretty_un = username_pretty(nick);
			printf("\r(writing) %s: %.*s" CURSOR "\033[0K", pretty_un.c_str(), read_length, read_buffer);
			fflush(stdout);
			break;
		case IO_WRITING_COMMAND:
			display_command(read_buffer, read_length, ws.ws_row, ':', 1);
			break;
		}
	}
}

void logic_proc(client c, int fd_in, int fd_out)
{
	int ret;
	struct pollfd poll_fds[2];
	struct itimerspec its;
	uint64_t exp;
	msg_id last_msg;
	struct logic_comm incoming;
	struct io_comm outgoing;

	poll_fds[0].fd = fd_in;
	poll_fds[0].events = POLLIN;

	poll_fds[1].fd = timerfd_create(CLOCK_MONOTONIC, 0);
	its = {.it_interval = {.tv_sec = 0, .tv_nsec = 100 * 1000000}, .it_value = {.tv_sec = 0, .tv_nsec = 100 * 1000000}};
	timerfd_settime(poll_fds[1].fd, 0, &its, NULL);
	poll_fds[1].events = POLLIN;

	last_msg = 0;
	while(1) {
		ret = poll(poll_fds, sizeof(poll_fds)/sizeof(*poll_fds), -1);
		if(ret > 0 && poll_fds[0].revents & POLLIN) {
			/* pipe */
			read(fd_in, &incoming, sizeof(struct logic_comm));
			switch(incoming.type) {
			case LOGIC_COMM_MSG_WRITTEN:
				c.send_message(incoming.msg_written.new_msg);
				break;
			case LOGIC_COMM_CMD:
				outgoing.type = IO_COMM_DISP_STATUS;
				outgoing.disp_status.failure = exec_command(incoming.cmd.raw_cmd, outgoing.disp_status.status, sizeof(outgoing.disp_status.status));
				if(! outgoing.disp_status.failure) strncpy(outgoing.disp_status.status, "ok", sizeof(outgoing.disp_status.status));
				write(fd_out, &outgoing, sizeof(struct io_comm));
				break;
			case LOGIC_COMM_QUIT:
				exit(incoming.quit.ret_val);
			}
		}
		if(ret > 0 && poll_fds[1].revents & POLLIN) {
			read(poll_fds[1].fd, &exp, sizeof(uint64_t));
			/* fetch new messages */
			c.recv_messages(last_msg);
			for(auto m = c.msgs.msgs.upper_bound(last_msg); m != c.msgs.msgs.end(); ++m)
			{
				outgoing.type = IO_COMM_DISP_MSG;
				outgoing.disp_msg.inc_msg = m->second;
				write(fd_out, &outgoing, sizeof(struct io_comm));
				if(m->second.id > last_msg) last_msg = m->second.id;
			}
		}
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

	int io_pipe[2];
	int logic_pipe[2];
	if(pipe(io_pipe) < 0) {
		perror("pipe (io)");
		return 1;
	}
	if(pipe(logic_pipe) < 0) {
		perror("pipe (logic)");
		return 1;
	}
	pid_t pid = fork();
	if(pid < 0) {
		cerr << "fork error\n";
		exit(1);
	} else if(pid == 0) {
		/* child -> logic */
		close(io_pipe[0]);
		close(logic_pipe[1]);

		client c(conf["ip"], 6666, save_path);

		logic_proc(c, logic_pipe[0], io_pipe[1]);
		return 0;
	}

	/* parent -> io */
	close(io_pipe[1]);
	close(logic_pipe[0]);

	tcgetattr(STDIN_FILENO, &term);
	orig_term = term;
	term.c_lflag &= ~ECHO & ~ICANON & ~ISIG & ~IEXTEN & ~ICRNL;
	term.c_cc[VMIN] = 1;
	term.c_cc[VTIME] = 0;
	atexit(cleanup);
	tcsetattr(STDIN_FILENO, 0, &term);
	printf("\033[?25l");

	io_proc(io_pipe[0], logic_pipe[1]);
	kill(pid, SIGTERM);
	waitpid(pid, NULL, 0);
}

