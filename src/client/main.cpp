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
#include <inttypes.h>

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
	printf_styled(s, "\0337\033[%d;0H%c%.*s", win_h, preceding_char, len, cmd);
	if(display_cursor) printf(CURSOR);
	printf("\033[0K\0338");
	fflush(stdout);
}

void display_msg(char const *prefix, msg_id mid, time_t send_time, uint32_t ms, struct username un, char *msg, unsigned int len, int cursor)
{
	std::string pretty_un;
	char time_buf[128];

	printf("\r");
	if(prefix) printf("%s", prefix);

	printf_styled(highlight_msg_id, "[%" PRIu64 "] ", (uint64_t)mid);

	pretty_un = username_pretty(un);
	printf_styled(highlight_msg_author, "%s", pretty_un.c_str());

	strftime(time_buf, sizeof(time_buf), "%a %b %d %Y %H:%M:%S", gmtime(&send_time));
	printf_styled(highlight_msg_time, " @ %s", time_buf);

	printf_styled(highlight_msg_time_ms, ":%03" PRIu32 " ", ms);

	printf(": ");

	printf_styled(highlight_msg_text, "%.*s", len, msg);

	if(cursor) printf(CURSOR);
	printf("\033[0K");
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
	enum { IO_INPUT_NORM, IO_INPUT_ESC, IO_INPUT_CSI, IO_INPUT_SS3 } control_sequence;
	unsigned int esc_timeout;
	char csi_buffer[32];
	unsigned int len_csi_buf;
	unsigned int i;

	ws.ws_row = 24; /* fallback term height */
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

	poll_fds[0].fd = fd_in;
	poll_fds[0].events = POLLIN;

	poll_fds[1].fd = STDIN_FILENO;
	poll_fds[1].events = POLLIN;

	state = IO_WRITING_MSG;
	read_length = len_csi_buf = 0;
	control_sequence = IO_INPUT_NORM;
	display_msg("(writing) ", 0, time(0), get_ms(), nick, read_buffer, read_length, 1);
	fflush(stdout);
	while(1) {
		ret = poll(poll_fds, sizeof(poll_fds)/sizeof(*poll_fds), 1);
		if(ret > 0 && poll_fds[0].revents & POLLIN) {
			/* pipe */
			read(fd_in, &incoming, sizeof(struct io_comm));
			switch(incoming.type) {
			case IO_COMM_DISP_MSG:
				display_msg(
					NULL,
					incoming.disp_msg.inc_msg.id,
					incoming.disp_msg.inc_msg.send_time,
					incoming.disp_msg.inc_msg.ms,
					incoming.disp_msg.inc_msg.un,
					incoming.disp_msg.inc_msg.str,
					sizeof(incoming.disp_msg.inc_msg.str), 0);
				printf("\n");
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
#define HANDLE_CHARACTER(CHAR) \
	switch(CHAR) { \
	case '\n': \
	case '\r': \
		goto send; \
	case 127: \
		if(read_length) --read_length; \
		break; \
	case 21: \
		read_length = 0; \
		break; \
	case 27: \
	case 3: \
		switch(state) { \
		case IO_WRITING_MSG: \
			outgoing.type = LOGIC_COMM_QUIT; \
			outgoing.quit.ret_val = 0; \
			write(fd_out, &outgoing, sizeof(struct logic_comm)); \
			exit(0); \
			break; \
		case IO_WRITING_COMMAND: \
			state = IO_WRITING_MSG; \
			display_command(read_buffer, 0, ws.ws_row, ' ', 0, highlight_command); \
			break; \
		} \
		break; \
	case ':': \
		if(state == IO_WRITING_MSG && read_length == 0) { \
			printf("\r\033[0K"); \
			fflush(stdout); \
			state = IO_WRITING_COMMAND; \
			break; \
		} \
		/* FALLTHROUGH */ \
	default: \
		read_buffer[read_length++] = CHAR; \
		break; \
	}
			switch(control_sequence) {
			case IO_INPUT_NORM:
				if(c != 27) {
					HANDLE_CHARACTER(c);
				} else {
					control_sequence = IO_INPUT_ESC;
					csi_buffer[0] = 27;
					len_csi_buf = 1;
				}
				break;
			case IO_INPUT_ESC:
				if(c == '[') {
					control_sequence = IO_INPUT_CSI;
					csi_buffer[1] = '[';
					len_csi_buf = 2;
				} else if(c == 'O') {
					control_sequence = IO_INPUT_SS3;
					csi_buffer[1] = 'O';
					len_csi_buf = 2;
				} else {
					HANDLE_CHARACTER(27);
					HANDLE_CHARACTER(c);
					control_sequence = IO_INPUT_NORM;
				}
				break;
			case IO_INPUT_CSI:
				csi_buffer[len_csi_buf++] = c;
				//3 char sequnces
				if(strncmp(csi_buffer, "\033[A", len_csi_buf) == 0) {
					/* UP ARROW */
					control_sequence = IO_INPUT_NORM;
				} else if(strncmp(csi_buffer, "\033[B", len_csi_buf) == 0) {
					/* DOWN ARROW */
					control_sequence = IO_INPUT_NORM;
				} else if(strncmp(csi_buffer, "\033[C", len_csi_buf) == 0) {
					/* RIGHT ARROW */
					control_sequence = IO_INPUT_NORM;
				} else if(strncmp(csi_buffer, "\033[D", len_csi_buf) == 0) {
					/* LEFT ARROW */
					control_sequence = IO_INPUT_NORM;
				//4 char sequences
				} else if(len_csi_buf < 4){
					break;
				} else if(strncmp(csi_buffer, "\033[5~", len_csi_buf) == 0) {
					/* PAGE UP */
					control_sequence = IO_INPUT_NORM;
				} else if(strncmp(csi_buffer, "\033[6~", len_csi_buf) == 0) {
					/* PAGE DOWN */
					control_sequence = IO_INPUT_NORM;
				//5 char sequences
				} else if(len_csi_buf < 5){
					break;
				} else if(strncmp(csi_buffer, "\033[15~", len_csi_buf) == 0) {
					/* F5 key*/
					control_sequence = IO_INPUT_NORM;
				} else if(strncmp(csi_buffer, "\033[17~", len_csi_buf) == 0) {
					/* F6 key (yes, It's 2 more)*/
					control_sequence = IO_INPUT_NORM;
				} else if(strncmp(csi_buffer, "\033[18~", len_csi_buf) == 0) {
					/* F7 key*/
					control_sequence = IO_INPUT_NORM;
				} else if(strncmp(csi_buffer, "\033[19~", len_csi_buf) == 0) {
					/* F8 key */
					control_sequence = IO_INPUT_NORM;
				} else if(strncmp(csi_buffer, "\033[20~", len_csi_buf) == 0) {
					/* F9 key */
					control_sequence = IO_INPUT_NORM;
				} else if(strncmp(csi_buffer, "\033[21~", len_csi_buf) == 0) {
					/* F10 key */
					control_sequence = IO_INPUT_NORM;
				} else if(strncmp(csi_buffer, "\033[23~", len_csi_buf) == 0) {
					/* F11 key */
					control_sequence = IO_INPUT_NORM;
				} else if(strncmp(csi_buffer, "\033[24~", len_csi_buf) == 0) {
					/* F12 key */
					control_sequence = IO_INPUT_NORM;
				} else if(len_csi_buf == sizeof(csi_buffer)) {
					/* prevent buffer overflow */
					control_sequence = IO_INPUT_NORM;
				}
				break;
			case IO_INPUT_SS3:
				csi_buffer[len_csi_buf++] = c;
				if(strncmp(csi_buffer, "\033OP", len_csi_buf) == 0) {
					/* f1 KEY */
					control_sequence = IO_INPUT_NORM;
				} else if(strncmp(csi_buffer, "\033OQ", len_csi_buf) == 0) {
					/* f2 KEY */
					control_sequence = IO_INPUT_NORM;
				} else if(strncmp(csi_buffer, "\033OR", len_csi_buf) == 0) {
					/* f3 KEY */
					control_sequence = IO_INPUT_NORM;
				} else if(strncmp(csi_buffer, "\033OS", len_csi_buf) == 0) {
					/* f4 KEY */
					control_sequence = IO_INPUT_NORM;
				} else if(len_csi_buf == sizeof(csi_buffer)) {
					/* prevent buffer overflow */
					control_sequence = IO_INPUT_NORM;
				}
				break;
			}

			if(control_sequence != IO_INPUT_NORM) {
				esc_timeout = key_arrival_timeout;
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
		} else {
			if(control_sequence != IO_INPUT_NORM) {
				--esc_timeout;
				if(esc_timeout == 0) {
					control_sequence = IO_INPUT_NORM;
					for(i = 0; i < len_csi_buf; ++i) {
						HANDLE_CHARACTER(csi_buffer[i]);
#undef HANDLE_CHARACTER
					}
				}
			}
		}

		switch(state) {
		case IO_WRITING_MSG:
			display_msg("(writing) ", 0, time(0), get_ms(), nick, read_buffer, read_length, 1);
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
	its = {.it_interval = {.tv_sec = fetch_timeout_ms/1000, .tv_nsec = (fetch_timeout_ms%1000) * 1000000}, .it_value = {.tv_sec = fetch_timeout_ms/1000, .tv_nsec = (fetch_timeout_ms%1000) * 1000000}};
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

int main(int argc, char **argv) 
{
	std::ios::sync_with_stdio(true);

	signal(SIGINT, signal_handler);
	signal(SIGPIPE, signal_handler);

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
		client c(ip, port, save_path);

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

		for(int i = 1; i < argc; ++i) {
			if(strcmp(argv[i], "-C") == 0) {
				char failure[256];
				if(i + 1 >= argc) {
					std::cerr << "expected command after -C\n";
					exit(1);
				}
				auto res = exec_command(argv[i+1], failure, sizeof(failure));
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

