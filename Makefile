.POSIX:
.PHONY: all clean install

PREFIX = /usr/local
SRC = npm-core.c npm-agent.c npmc.c monocypher.c util.c
OBJ = $(SRC:%.c=%.o)
EXE = npm-agent npm-core npmc
NPM_CORE = "npm-core"

all: npm-core npm-agent npmc

$(OBJ): config.h

config.h: config.def.h
	cp config.def.h $@

npm-core: npm-core.o monocypher.o util.o
	$(CC) $(LDFLAGS) npm-core.o monocypher.o util.o -o $@

npm-agent: npm-agent.o util.o
	$(CC) $(LDFLAGS) npm-agent.o util.o -o $@

npmc: npmc.o util.o
	$(CC) $(LDFLAGS) npmc.o util.o -o $@

npm-agent.o: npm-agent.c
	$(CC) '-DNPM_CORE=$(NPM_CORE)' $(CFLAGS) -c npm-agent.c -o $@

install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -Dm755 npm npm-core npmc npm-agent $(DESTDIR)$(PREFIX)/bin

clean:
	rm -f $(OBJ) $(EXE)
