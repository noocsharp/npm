
.PHONY: all clean install

PREFIX = /usr/local
SRC = npm-core.c npm-agent.c npmc.c util.c monocypher.c
OBJ = $(SRC:%.c=%.o)
EXE = npm-agent npm-core npmc
NPM_CORE = "npm-core"

CFLAGS = '-DNPM_CORE=$(NPM_CORE)'

all: npm-core npm-agent npmc

npm-core: $(LIBS) npm-core.o util.o monocypher.o
	$(CC) -static npm-core.o util.o monocypher.o -o $@

npm-agent: npm-agent.o
	$(CC) -static npm-agent.o -o $@

npmc: npmc.o
	$(CC) -static npmc.o -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

install:
	install -Dm755 -t $(DESTDIR)$(PREFIX)/bin npm npm-core npmc npm-agent

clean:
	rm -f $(OBJ) $(EXE)
