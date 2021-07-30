dir := argon2

ARGON2_SRC = argon2/argon2.c \
             argon2/core.c   \
             argon2/ref.c    \
             argon2/thread.c \
             argon2/blake2/blake2b.c

ARGON2_OBJ = $(ARGON2_SRC:%.c=%.o)
ARGON2_LIB = argon2/argon2.a

$(ARGON2_LIB): $(ARGON2_OBJ)
	$(AR) crs $@ $(ARGON2_OBJ)

$(dir)-clean:
	rm -f $(ARGON2_OBJ) $(ARGON2_LIB)
