#include <stdlib.h>
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include <string.h>

#define CONFIG_FILE "/.chatroomserverrc"
#define STATE_DIR "/.local/share"
#define SAVE_DIR "/chatroom-server"
#define SAVE_FILE "/msgs"

#include "server/server.h"

server *ps;

void cleanup(void) {
	ps->~server();
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
	auto conf = config();
	for(int i = 1; i < argc; ++i) {
		if(argv[i][0] == '-') {
			for(int j = 1; argv[i][j]; ++j) {
				switch(argv[i][j]) {
#define ARG_OPT(OPT, STR) \
					case OPT: \
						if(i + 1 >= argc) { \
							std::cerr << "expected -" << OPT << " <" << STR << ">\n"; \
							exit(1); \
						} \
						conf[STR] = argv[i+1]; \
						goto argument_end;
					ARG_OPT('p', "port");
					default:
						std::cerr << "unknown -" << argv[i][j] << '\n';
						exit(1);
				}
			}
		}
argument_end:
	}
	if(!conf.contains("port")) conf["port"] = "6666";

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
		save_path = "/tmp/chatroom-msgs-server";
	}

	server s(atoi(conf["port"].c_str()), save_path);
	ps = &s;
	atexit(cleanup);
	while(true)
	{
		s.handle();
	}
}

