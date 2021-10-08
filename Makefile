.POSIX:
.PHONY: all clean install

PREFIX = /usr/local
SRC = npm-core.c npm-agent.c npmc.c monocypher.c
OBJ = $(SRC:%.c=%.o)
EXE = npm-agent npm-core npmc
NPM_CORE = "npm-core"

all: npm-core npm-agent npmc

npm-core: $(LIBS) npm-core.o monocypher.o
	$(CC) $(LDFLAGS) npm-core.o monocypher.o -o $@

npm-agent: npm-agent.o
	$(CC) $(LDFLAGS) npm-agent.o -o $@

npmc: npmc.o
	$(CC) $(LDFLAGS) npmc.o -o $@

npm-agent.o: npm-agent.c
	$(CC) '-DNPM_CORE=$(NPM_CORE)' $(CFLAGS) -c $< -o $@

install:
	install -Dm755 -t $(DESTDIR)$(PREFIX)/bin npm npm-core npmc npm-agent

clean:
	rm -f $(OBJ) $(EXE)
