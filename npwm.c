#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/random.h>
#include <termios.h>
#include <unistd.h>

#include "chacha20.h"
#include "common.h"
#include "pkcs5_pbkdf2.h"
#include "util.h"

int main(int argc, char *argv[]) {
    char encrypted[SALT_LEN + PASSWORD_MAX_LEN];
    char key[KEY_LEN];
    char nonce[NONCE_LEN];
    char salt[SALT_LEN];
    char *c;
    size_t len;

    /* TODO add usage */
    if (argc != 3)
        die("invalid args");

    if (strcmp(argv[1], "-e") == 0) {
        if (getrandom(salt, SALT_LEN, 0) < SALT_LEN)
            die("failed to generate salt");

        if (pkcs5_pbkdf2(argv[2], strlen(argv[2]), salt, SALT_LEN, key,
                    KEY_LEN, ROUNDS) == -1)
            die("key derivation failed");

        if (getrandom(nonce, NONCE_LEN, 0) < NONCE_LEN)
            die("failed to generate nonce");

        errno = 0;

        memcpy(encrypted, salt, SALT_LEN);
        memset(encrypted + SALT_LEN, 0, PASSWORD_MAX_LEN);

        fgets(encrypted + SALT_LEN, PASSWORD_MAX_LEN, stdin);
        if ((c = strchr(encrypted + SALT_LEN, '\n')) == NULL)
            die("password is too long");

        *c = 0;
        len = c - encrypted - SALT_LEN + 1;

        br_chacha20_ct_run(key, nonce, 0, encrypted, SALT_LEN + len);

        fwrite(nonce, sizeof(char), NONCE_LEN, stdout);
        fwrite(salt, sizeof(char), SALT_LEN, stdout);
        fwrite(encrypted, sizeof(char), SALT_LEN + len, stdout);
    } else if (strcmp(argv[1], "-d") == 0) {
        if (fread(nonce, sizeof(char), NONCE_LEN, stdin) < NONCE_LEN)
            die("failed to read nonce");

        if (fread(salt, sizeof(char), SALT_LEN, stdin) < SALT_LEN)
            die("failed to read salt");

        len = fread(encrypted, sizeof(char), SALT_LEN + PASSWORD_MAX_LEN,
                stdin) - SALT_LEN - 1;

        if (pkcs5_pbkdf2(argv[2], strlen(argv[2]), salt, SALT_LEN, key,
                    KEY_LEN, ROUNDS) == -1)
            die("key derivation failed");

        br_chacha20_ct_run(key, nonce, 0, encrypted, SALT_LEN + len);

        if (memcmp(encrypted, salt, SALT_LEN) != 0)
            die("invalid input!");

        fwrite(encrypted + SALT_LEN, sizeof(char), len, stdout);
    }
}
