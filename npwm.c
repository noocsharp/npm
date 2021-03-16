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
#include "pkcs5_pbkdf2.h"
#include "util.h"

#define PASSWORD_MAX_LEN 512
#define PASSPHRASE_MAX_LEN 512
#define KEY_LEN 32
#define NONCE_LEN 12
#define SALT_LEN 8
#define ROUNDS 2000

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

int
getpassphrase(char *buf)
{
    struct termios old, new;
    char *c;
    fputs("Passphrase: ", stderr);

    if (isatty(STDIN_FILENO)) {
        if (tcgetattr(STDIN_FILENO, &old) == -1)
            return -1;
        new = old;
        new.c_lflag &= ~(ICANON | ECHO);
        if (tcsetattr(STDIN_FILENO, TCSANOW, &new) == -1)
            return -1;
    }

    if (fgets(buf, PASSPHRASE_MAX_LEN, stdin) == NULL)
        return -1;

    if (isatty(STDIN_FILENO)) {
        if (tcsetattr(STDIN_FILENO, TCSANOW, &old) == -1)
            return -1;

        putchar('\n');
    }
    if ((c = strchr(buf, '\n')) == NULL)
        die("passphrase too long");

    c = '\0';
}

int main(int argc, char *argv[]) {
    char password[PASSWORD_MAX_LEN];
    char passphrase[PASSPHRASE_MAX_LEN];
    char key[KEY_LEN];
    char nonce[NONCE_LEN];
    char salt[SALT_LEN];
    int vlen;

    if (argc == 2 && strcmp(argv[1], "-g") == 0) {
        if (getpassphrase(passphrase) == -1)
            die("failed to read password");

        if (getrandom(salt, SALT_LEN, 0) < SALT_LEN)
            die("failed to generate salt");

        if (pkcs5_pbkdf2(passphrase, strlen(passphrase), salt, SALT_LEN, key,
                    KEY_LEN, ROUNDS) == -1)
            die("key derivation failed");

        if (getrandom(nonce, NONCE_LEN, 0) < NONCE_LEN)
            die("failed to generate nonce");

        errno = 0;
        len = strtol(getenv("NPWM_LENGTH"), NULL, 10);
        if (errno || len <= 0 || len > PASSWORD_MAX_LEN)
            die("invalid value for NPWM_LENGTH:");

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

        memset(password, 0, PASSWORD_MAX_LEN);
        gen(password);

        br_chacha20_ct_run(key, nonce, 0, password, len);
    }
}
