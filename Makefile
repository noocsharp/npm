LIBS = argon2/argon2.a
SRC = chacha20.c npwm.c util.c
OBJ = $(SRC:%.c=%.o)
EXE = npwm-core

$(EXE): $(LIBS) $(OBJ)
	$(CC) $(OBJ) $(LIBS) -o $@

.c.o:
	$(CC) -c $< -o $@

include argon2/Makefile

clean: argon2-clean
	rm -f $(OBJ) $(EXE)
