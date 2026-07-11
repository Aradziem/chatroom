#include "cmds.h"

#include <string.h>
#include <vector>

struct username nick = un_from_str("Anonymous");

#define FAIL(MSG) \
	do { \
		strncpy(failure_reason, MSG, failure_len-1); \
		failure_reason[failure_len - 1] = 0; \
		return { .type=COMMAND_FAILED }; \
	} while(0);

struct command_result set(char **argv, char *failure_reason, unsigned int failure_len);

struct command {
	char const *name;
	unsigned int argc;
	struct command_result (*cmd)(char **argv, char *failure_reason, unsigned int failure_len);
};
struct command commands[] = {
/*    name   argc cmd */
	{ "set", 2,   set },
	{ "se",  2,   set },
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

	argv.reserve(command.argc);
	for(i = 0; i < command.argc; ++i) {
		token = strtok(NULL, " \t");
		if(! token) FAIL("expected more arguments");

		argv.push_back(token);
	}

	return command.cmd(argv.data(), failure_reason, failure_len);
}

void eval_config_update(struct update_config config)
{
	switch(config.name) {
	case CONFIG_NICK:
		nick = un_from_str(config.value);
		break;
	}
}

struct command_result set(char **argv, char *failure_reason, unsigned int failure_len)
{
	struct command_result res;

	if(strcmp(argv[0], "nick") == 0) {
		nick = un_from_str(argv[1]);
	} else FAIL("unknown set option");

	res.type = COMMAND_UPDATE_CONFIG;
	res.config.name = CONFIG_NICK;
	strncpy(res.config.value, argv[1], sizeof(res.config.value)-1);
	res.config.value[sizeof(res.config.value)] = 0;
	return res;
}

