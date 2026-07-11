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
volatile sig_atomic_t signal_raised = 0;

enum io_comm_type { IO_COMM_DISP_MSG, IO_COMM_DISP_STATUS, IO_COMM_CONFIG, IO_COMM_QUIT };
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
			struct update_config new_conf;
		} config;
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

void signal_handler(int sig)
{
	signal_raised = 1;

	signal(sig, signal_handler);
}

void cleanup(void) {
	tcsetattr(STDIN_FILENO, 0, &orig_term);
	printf("\033[?25h\n");
}

uint32_t get_ms() {
	static struct timespec ts = {};
	clock_gettime(CLOCK_REALTIME, &ts);
	return ts.tv_nsec / 1e6;
}

void display_command(char *cmd, unsigned int len, unsigned int win_h, char preceding_char, int display_cursor, struct style s)
{
	print_style(s);
	printf("\0337\033[%d;0H%c%.*s", win_h, preceding_char, len, cmd);
	if(display_cursor) printf(CURSOR);
	printf("\033[0K\0338");
	reset_styles();
	fflush(stdout);
}

void io_proc(int fd_in, int fd_out)
{
	int ret;
	struct pollfd poll_fds[2];
	struct io_comm incoming;
	struct logic_comm outgoing;
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
	printf("\r(writing) ");
	print_style(highlight_msg_author);
	std::cout << username_pretty(nick);
	reset_styles();
	printf(": " CURSOR "\033[0K");
	fflush(stdout);
	while(1) {
		ret = poll(poll_fds, sizeof(poll_fds)/sizeof(*poll_fds), -1);
		if(ret > 0 && poll_fds[0].revents & POLLIN) {
			/* pipe */
			read(fd_in, &incoming, sizeof(struct io_comm));
			switch(incoming.type) {
			case IO_COMM_DISP_MSG:
				printf("\r");
				print_style(highlight_msg_author);
				std::cout << username_pretty(incoming.disp_msg.inc_msg.un);
				reset_styles();
				printf(": ");
				print_style(highlight_msg_text);
				printf("%s", incoming.disp_msg.inc_msg.str);
				reset_styles();
				printf("\033[0K");
				fflush(stdout);
				break;
			case IO_COMM_DISP_STATUS:
				if(incoming.disp_status.failure) {
					display_command(incoming.disp_status.status, strlen(incoming.disp_status.status), ws.ws_row, '!', 0, highlight_command_failure);
				} else {
					display_command(incoming.disp_status.status, strlen(incoming.disp_status.status), ws.ws_row, ' ', 0, highlight_command);
				}
				break;
			case IO_COMM_CONFIG:
				eval_config_update(incoming.config.new_conf);
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
					display_command(read_buffer, 0, ws.ws_row, ' ', 0, highlight_command);
					break;
				}
				break;
			case ':':
				if(state == IO_WRITING_MSG && read_length == 0) {
					printf("\r(writing) ");
					print_style(highlight_msg_author);
					std::cout << username_pretty(nick);
					reset_styles();
					printf(": \033[0K");
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
				read_buffer[read_length] = 0;
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
			printf("\r(writing) ");
			print_style(highlight_msg_author);
			std::cout << username_pretty(nick);
			reset_styles();
			printf(": ");
			print_style(highlight_msg_text);
			printf("%.*s", read_length, read_buffer);
			reset_styles();
			printf(CURSOR "\033[0K");
			fflush(stdout);
			break;
		case IO_WRITING_COMMAND:
			display_command(read_buffer, read_length, ws.ws_row, ':', 1, highlight_command);
			break;
		}

		if(signal_raised) {
			outgoing.type = LOGIC_COMM_QUIT;
			outgoing.quit.ret_val = 1;
			write(fd_out, &outgoing, sizeof(struct logic_comm));
			exit(1);
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
	struct command_result cmd_res;
	unsigned int i;

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
				cmd_res = exec_command(incoming.cmd.raw_cmd, outgoing.disp_status.status, sizeof(outgoing.disp_status.status));
				outgoing.type = IO_COMM_DISP_STATUS;
				outgoing.disp_status.failure = cmd_res.type == COMMAND_FAILED;
				if(! outgoing.disp_status.failure) strncpy(outgoing.disp_status.status, "ok", sizeof(outgoing.disp_status.status));
				write(fd_out, &outgoing, sizeof(struct io_comm));
				for(i = 0; i < cmd_res.config.size(); ++i ) {
					outgoing.type = IO_COMM_CONFIG;
					outgoing.config.new_conf = cmd_res.config[i];
					write(fd_out, &outgoing, sizeof(struct io_comm));
				}
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

		if(signal_raised) {
			outgoing.type = IO_COMM_QUIT;
			outgoing.quit.ret_val = 1;
			write(fd_out, &outgoing, sizeof(struct logic_comm));
			exit(1);
		}
	}
}

int main(int argc, char **argv) {
	std::ios::sync_with_stdio(true);

	signal(SIGINT, signal_handler);
	signal(SIGPIPE, signal_handler);

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
					default:
						cerr << "unknown -" << argv[i][j] << '\n';
						exit(1);
				}
			}
		}
	}

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

		client c("127.0.0.1", 6666, save_path);

		if(home) {
			char *config_cmd = new char[strlen("source ") + strlen(home) + sizeof(CONFIG_FILE)];
			if(config_cmd) {
				strcpy(config_cmd, "source ");
				strcat(config_cmd, home);
				strcat(config_cmd, CONFIG_FILE);
				char failure[2048];
				auto res = exec_command(config_cmd, failure, sizeof(failure));
				if(res.type == COMMAND_FAILED) {
					/* TODO: move this to the IO proc */
					printf("\n\n\n%s\n\n\n", failure);
				}
				for(auto uc : res.config) {
					struct io_comm c;
					c.type = IO_COMM_CONFIG;
					c.config.new_conf = uc;
					write(io_pipe[1], &c, sizeof(struct io_comm));
				}
				delete[] config_cmd;
			}
		}

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

