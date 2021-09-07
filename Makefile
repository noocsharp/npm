
.PHONY: all clean install

PREFIX = /usr/local
LIBS = argon2/argon2.a
SRC = chacha20.c npm.c util.c
OBJ = $(SRC:%.c=%.o)
NPM_CORE = "npm-core"

CFLAGS = '-DNPM_CORE=$(NPM_CORE)'

all: npm-core npm-agent npmc

npm-core: $(LIBS) chacha20.o npm.o util.o
	$(CC) -static chacha20.o npm.o util.o $(LIBS) -o $@

npm-agent: npm-agent.o
	$(CC) -static npm-agent.o -o $@

npmc: npmc.o
	$(CC) -static npmc.o -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

install:
	install -Dm755 -t $(DESTDIR)$(PREFIX)/bin npm npm-core npmc npm-agent

include argon2/Makefile

clean: argon2-clean
	rm -f $(OBJ) $(EXE)
