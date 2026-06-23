CFLAGS = -g -O0 -Isrc -std=gnu++2b

all: client server

build:
	mkdir -p build

clean:
	rm -rf build server client

build/%.o : src/%.cpp | build
	g++ -o $@ -c $^ $(CFLAGS)

client: build/client.o build/stuff.o
	g++ -o client $^ $(CFLAGS)

server: build/server.o build/stuff.o
	g++ -o server $^ $(CFLAGS)

.PHONY: all clean

