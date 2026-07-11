#ifndef CMDS_H
#define CMDS_H

#include "common/username.h"

extern struct username nick;

enum config_name { CONFIG_NICK };
struct update_config {
	enum config_name name;
	char value[256];
};
enum command_res_type { COMMAND_OK, COMMAND_FAILED, COMMAND_UPDATE_CONFIG };
struct command_result {
	enum command_res_type type;
	struct update_config config;
};

struct command_result exec_command(char *cmd, char *failure_reason, unsigned int failure_len);
void eval_config_update(struct update_config config);

#endif

