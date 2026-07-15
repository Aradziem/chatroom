#include "cmds.h"

#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <fstream>
#include <stdarg.h>

struct username nick = un_from_str("Anonymous");
std::string ip = "127.0.0.1";
int port = 6666;
struct style highlight_msg_id = {.type=COLOR_DEFAULT,.styles=0};
struct style highlight_msg_time = {.type=COLOR_DEFAULT,.styles=0};
struct style highlight_msg_time_ms = {.type=COLOR_DEFAULT,.styles=0};
struct style highlight_msg_author = {.type=COLOR_DEFAULT,.styles=0};
struct style highlight_msg_text = {.type=COLOR_DEFAULT,.styles=0};
struct style highlight_command = {.type=COLOR_DEFAULT,.styles=0};
struct style highlight_command_failure = {.type=COLOR_RGB,.rgb={255,0,0},.styles=0};
long int fetch_timeout_ms = 100;

#define FAIL(MSG) \
	do { \
		strncpy(failure_reason, MSG, failure_len-1); \
		failure_reason[failure_len - 1] = 0; \
		return { .type=COMMAND_FAILED }; \
	} while(0);

struct command_result set(std::vector<char *> argv, char *failure_reason, unsigned int failure_len);
struct command_result highlight(std::vector<char *> argv, char *failure_reason, unsigned int failure_len);
struct command_result source(std::vector<char *> argv, char *failure_reason, unsigned int failure_len);

void print_style(struct style c);
void reset_styles();

struct command {
	char const *name;
	int argc;
	struct command_result (*cmd)(std::vector<char *> argv, char *failure_reason, unsigned int failure_len);
};
struct command commands[] = {
/*    name          argc cmd */
	{ "set",        2,   set },
	{ "se",         2,   set },
	{ "highlight",  -1,  highlight },
	{ "hi",         -1,  highlight },
	{ "source",     1,   source },
	{ "so",         1,   source },
};

struct command_result exec_command(char *cmd, char *failure_reason, unsigned int failure_len)
{
	char *token;
	std::vector<char *> argv;
	unsigned int i;
	struct command command;

	token = strtok(cmd, " \t");
	if(! token) FAIL("command empty");

	command.name = NULL;
	for(i = 0; i < sizeof(commands)/sizeof(*commands); ++i) {
		if(strcmp(token, commands[i].name) == 0) {
			command = commands[i];
			break;
		}
	}
	if(command.name == NULL) FAIL("unknown command");

	if(command.argc >= 0) {
		argv.reserve(command.argc);
		for(i = 0; i < (unsigned int)command.argc; ++i) {
			token = strtok(NULL, " \t");
			if(! token) FAIL("expected more arguments");

			argv.push_back(token);
		}
	} else {
		for(token = strtok(NULL, " \t"); token; token = strtok(NULL, " \t")) {
			argv.push_back(token);
		}
	}

	return command.cmd(argv, failure_reason, failure_len);
}

void eval_config_update(struct update_config config)
{
	switch(config.type) {
	case CONFIG_SET:
		switch(config.set.name) {
		case CONFIG_SET_NICK:
			nick = un_from_str(config.set.value);
			break;
		}
		break;
	case CONFIG_HIGHLIGHT:
		switch(config.hi.name) {
		case CONFIG_HI_MSG_ID:
			highlight_msg_id = config.hi.value;
			break;
		case CONFIG_HI_MSG_TIME:
			highlight_msg_time = config.hi.value;
			break;
		case CONFIG_HI_MSG_TIME_MS:
			highlight_msg_time_ms = config.hi.value;
			break;
		case CONFIG_HI_MSG_AUTHOR:
			highlight_msg_author = config.hi.value;
			break;
		case CONFIG_HI_MSG_TEXT:
			highlight_msg_text = config.hi.value;
			break;
		case CONFIG_HI_COMMAND:
			highlight_command = config.hi.value;
			break;
		case CONFIG_HI_COMMAND_FAILURE:
			highlight_command_failure = config.hi.value;
			break;
		}
	}
}

void printf_styled(struct style s, char const *fmt, ...) {
	va_list args;

	if(s.styles & STYLE_HIDE) return;

	print_style(s);

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);

	reset_styles();
}

struct command_result set(std::vector<char *> argv, char *failure_reason, unsigned int failure_len)
{
	struct command_result res;

	res.type = COMMAND_OK;
	res.config.resize(1);
	res.config[0].type = CONFIG_SET;

	if(strcmp(argv[0], "nick") == 0) {
		nick = un_from_str(argv[1]);
		res.config[0].set.name = CONFIG_SET_NICK;
	} else if(strcmp(argv[0], "ip") == 0) {
		ip = argv[1];
		res.config.resize(0);
		return res;
	} else if(strcmp(argv[0], "port") == 0) {
		port = atoi(argv[1]);
		res.config.resize(0);
		return res;
	} else if(strcmp(argv[0], "fetch_timeout") == 0) {
		fetch_timeout_ms = atoll(argv[1]);
		res.config.resize(0);
		return res;
	} else FAIL("unknown set option");

	strncpy(res.config[0].set.value, argv[1], sizeof(res.config[0].set.value)-1);
	res.config[0].set.value[sizeof(res.config[0].set.value)-1] = 0;
	return res;
}

struct command_result highlight(std::vector<char *> argv, char *failure_reason, unsigned int failure_len)
{
	struct command_result res;
	struct style *tgt_hi;
	unsigned int i;
	char *it;

	if(argv.size() < 1) FAIL("expected a highlight group");

	res.type = COMMAND_OK;
	res.config.resize(1);
	res.config[0].type = CONFIG_HIGHLIGHT;

	if(strcmp(argv[0], "msg_id") == 0) {
		tgt_hi = &highlight_msg_id;
		res.config[0].hi.name = CONFIG_HI_MSG_ID;
	} else if(strcmp(argv[0], "msg_time") == 0) {
		tgt_hi = &highlight_msg_time;
		res.config[0].hi.name = CONFIG_HI_MSG_TIME;
	} else if(strcmp(argv[0], "msg_time_ms") == 0) {
		tgt_hi = &highlight_msg_time_ms;
		res.config[0].hi.name = CONFIG_HI_MSG_TIME_MS;
	} else if(strcmp(argv[0], "msg_author") == 0) {
		tgt_hi = &highlight_msg_author;
		res.config[0].hi.name = CONFIG_HI_MSG_AUTHOR;
	} else if(strcmp(argv[0], "msg_text") == 0) {
		tgt_hi = &highlight_msg_text;
		res.config[0].hi.name = CONFIG_HI_MSG_TEXT;
	} else if(strcmp(argv[0], "command") == 0) {
		tgt_hi = &highlight_command;
		res.config[0].hi.name = CONFIG_HI_COMMAND;
	} else if(strcmp(argv[0], "command_failure") == 0) {
		tgt_hi = &highlight_command_failure;
		res.config[0].hi.name = CONFIG_HI_COMMAND_FAILURE;
	} else FAIL("unknown highlight group");

	for(i = 1; i < argv.size(); ++i) {
		if(strcmp(argv[i], "default") == 0) {
			tgt_hi->type = COLOR_DEFAULT;
		} else if(strncmp(argv[i], "c256=", 5) == 0) {
			tgt_hi->type = COLOR_256;
			if(sscanf(argv[i]+5, "%hh" PRIu8, &tgt_hi->c256) < 1) FAIL("expected a number for c256");
		} else if(strncmp(argv[i], "rgb=", 4) == 0) {
			tgt_hi->type = COLOR_RGB;
			if(sscanf(argv[i]+4, "%hh" PRIu8 ",%hh" PRIu8 ",%hh" PRIu8, &tgt_hi->rgb.r, &tgt_hi->rgb.g, &tgt_hi->rgb.b) < 3) FAIL("expected 3 numbers for rgb");
		} else if(strncmp(argv[i], "style=", 6) == 0) {
			tgt_hi->styles = 0;
			for(it = argv[i]+6; *it; ++it) {
				switch(*it) {
				case 'b':
					tgt_hi->styles |= STYLE_BOLD;
					break;
				case 'u':
					tgt_hi->styles |= STYLE_UNDERLINE;
					break;
				case 'h':
					tgt_hi->styles |= STYLE_HIDE;
					break;
				case 'H':
					tgt_hi->styles &= ~STYLE_HIDE;
					break;
				default:
					FAIL("unknown style");
				}
			}
		}
	}

	res.config[0].hi.value = *tgt_hi;
	return res;
}

struct command_result source(std::vector<char *> argv, char *failure_reason, unsigned int failure_len)
{
	std::string ln;
	std::ifstream f;
	struct command_result partial_res, full_res;

	f.open(argv[0]);
	if(! f.is_open()) FAIL("couldn't open file");

	*failure_reason = 0;
	full_res.type = COMMAND_OK;

	while(std::getline(f, ln)) {
		if(ln[0] != '#' && ln.size()) {
			partial_res = exec_command(ln.data(), failure_reason, failure_len);
			if(*failure_reason) {
				failure_len -= strlen(failure_reason);
				failure_reason += strlen(failure_reason);
				if(failure_len > 1) {
					--failure_len;
					*(failure_reason++) = ',';
				}
			}
			if(partial_res.type == COMMAND_FAILED) full_res.type = COMMAND_FAILED;
			full_res.config.insert(full_res.config.end(), partial_res.config.begin(), partial_res.config.end());
		}
	}

	return full_res;
}

void print_style(struct style c)
{
	switch(c.type) {
	case COLOR_DEFAULT:
		printf("\033[39m");
		break;
	case COLOR_256:
		printf("\033[38;5;%dm", c.c256);
		break;
	case COLOR_RGB:
		printf("\033[38;2;%d;%d;%dm", c.rgb.r, c.rgb.g, c.rgb.b);
		break;
	}

	if(c.styles & STYLE_BOLD) printf("\033[1m");
	if(c.styles & STYLE_UNDERLINE) printf("\033[4m");
}

void reset_styles()
{
	printf("\033[0m");
}

