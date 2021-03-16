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

char *valid;
int len;

int
isvalid(char c)
{
    int len;
    if (strcmp(valid, "print") == 0)
        return isprint(c);
    else if (strcmp(valid, "alnum") == 0)
        return isalnum(c);
    else {
        /* TODO validate valid */
        for (int i = 0; i < strlen(valid); i++) {
            if (c == valid[i])
                return 1;
        }
        
        return 0;
    }
}

int
gen(char *buf)
{
    int i = 0;
    char c;
    while (i < len) {
        getrandom(&c, 1, 0);

        if (isvalid(c)) {
            buf[i] = c;
            i++;
        }
    }
}

int main(int argc, char *argv[]) {
    char encrypted[SALT_LEN + PASSWORD_MAX_LEN];
    char key[KEY_LEN];
    char nonce[NONCE_LEN];
    char salt[SALT_LEN];
    int vlen;

    /* TODO add usage */
    if (argc != 3)
        die("invalid args");

    if (strcmp(argv[1], "-g") == 0) {
        if (getrandom(salt, SALT_LEN, 0) < SALT_LEN)
            die("failed to generate salt");

        if (pkcs5_pbkdf2(argv[2], strlen(argv[2]), salt, SALT_LEN, key,
                    KEY_LEN, ROUNDS) == -1)
            die("key derivation failed");

        if (getrandom(nonce, NONCE_LEN, 0) < NONCE_LEN)
            die("failed to generate nonce");

        errno = 0;
        if (!getenv("NPWM_LENGTH"))
            len = DEFAULT_LEN;
        else {
            len = strtol(getenv("NPWM_LENGTH"), NULL, 10);
            if (errno || len <= 0 || len > PASSWORD_MAX_LEN)
                die("invalid value for NPWM_LENGTH:");
        }

        if ((valid = getenv("NPWM_VALID")) == NULL)
            valid = "print";

        if ((vlen = strlen(valid)) > 0x7F - 0x20)
            die("NPWM_VALID should not contain duplicate or non-printable characters");

        for (int i = 0; i < vlen; i++) {
            if (!isprint(valid[i]))
                die("NPWM_VALID may not contain non-printable characters");

            for (int j = 0; j < i; j++)
                if (valid[i] == valid[j])
                    die("NPWM_VALID may not contain duplicate characters");
        }

        memcpy(encrypted, salt, SALT_LEN);
        memset(encrypted + SALT_LEN, 0, PASSWORD_MAX_LEN);
        gen(encrypted + SALT_LEN);

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
