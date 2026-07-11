#ifndef CMDS_H
#define CMDS_H

#include <stdint.h>
#include <vector>

#include "common/username.h"

enum color_type { COLOR_DEFAULT, COLOR_256, COLOR_RGB };
#define STYLE_BOLD 1
#define STYLE_UNDERLINE 2
struct style {
	enum color_type type;
	union {
		uint8_t c256;
		struct {
			uint8_t r, g, b;
		} rgb;
	};
	uint8_t styles;
};

extern struct username nick;
extern struct style highlight_msg_author;
extern struct style highlight_msg_text;
extern struct style highlight_command;
extern struct style highlight_command_failure;

enum config_type { CONFIG_SET, CONFIG_HIGHLIGHT };
enum config_set_name { CONFIG_SET_NICK };
enum config_hi_name { CONFIG_HI_MSG_AUTHOR, CONFIG_HI_MSG_TEXT, CONFIG_HI_COMMAND, CONFIG_HI_COMMAND_FAILURE };
struct update_config {
	enum config_type type;
	union {
		struct {
			enum config_set_name name;
			char value[256];
		} set;
		struct {
			enum config_hi_name name;
			struct style value;
		} hi;
	};
};
enum command_res_type { COMMAND_OK, COMMAND_FAILED };
struct command_result {
	enum command_res_type type;
	std::vector<struct update_config> config;
};

struct command_result exec_command(char *cmd, char *failure_reason, unsigned int failure_len);
void eval_config_update(struct update_config config);
void print_style(struct style c);
void reset_styles();

#endif

