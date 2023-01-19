
BIN:=iso-chooser-menu

URL:=http://cdimage.ubuntu.com/streams/v1/com.ubuntu.cdimage.daily:ubuntu-server.json

# CFLAGS+=-std=c11 -Wall -Werror -Wfatal-errors -Wformat -Werror=format-security
CFLAGS+=-std=c11 -Wall -Werror -Wfatal-errors
CFLAGS+=-Wpedantic

# CFLAGS+=-g

CFLAGS+=$(shell pkg-config --cflags ncursesw)
LDFLAGS+=$(shell pkg-config --libs ncursesw)

CFLAGS+=$(shell pkg-config --cflags json-c)
LDFLAGS+=$(shell pkg-config --libs json-c)

SRCS:=$(wildcard *.c)
OBJS:=$(SRCS:.c=.o)

default: new

new: clean build

ubuntu-server.json:
	wget "$(URL)" -O $@

clean:
	rm -f $(BIN) $(OBJS) out.vars

distclean: clean
	rm -f ubuntu-server.json

build: $(OBJS)
	$(CC) -o $(BIN) $^ $(CFLAGS) $(LDFLAGS)

run: ubuntu-server.json
	./$(BIN) --input "$^" --output "out.vars"
