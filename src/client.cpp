#include "stuff.h"

void display(client* c)
{
	static time_t timestamp =0;
	vector<message> messages = c->messages_since(timestamp);
	timestamp = time(0);
	for(int i = 0; i< messages.size();i++)
	{
		cout<< messages[i].send_time << ": " << messages[i].str<<endl; 
	}
}

message read_message()
{
	message msg;
	char newline;
	cout << "you: ";
	scanf("%63[^\n]",msg.str);
	scanf("%c", &newline);
	msg.str[63]=0;
	msg.send_time=time(0);
	return msg;
}

int main(int argc, char **argv) {
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
	client c(ip, 6666);
	while(1) {
		display(&c);
		c.send_message(read_message());
	}
}

