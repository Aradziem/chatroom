BUILDDIR = build
SRCDIR = src
SRCS = $(shell find $(SRCDIR) -name "*.cpp")
OBJS = $(patsubst $(SRCDIR)/%.cpp, $(BUILDDIR)/%.o, $(SRCS))
SERVER_OBJS = $(filter-out $(BUILDDIR)/client/%, $(OBJS))
CLIENT_OBJS = $(filter-out $(BUILDDIR)/server/%, $(OBJS))
CFLAGS = -g -O2 -Isrc -std=gnu++2b -Wall -Wextra -Werror -Wno-unused-result -Wno-missing-field-initializers
LDFLAGS =

all: client server

build:
	mkdir -p $(patsubst $(SRCDIR)%, $(BUILDDIR)%, $(shell find $(SRCDIR) -type d))

clean:
	rm -rf build server client

build/%.o : src/%.cpp | build
	g++ $(CFLAGS) -o $@ -c $^

client: $(CLIENT_OBJS)
	g++ $(LDFLAGS) -o $@ $^

server: $(SERVER_OBJS)
	g++ $(LDFLAGS) -o $@ $^

.PHONY: all clean

