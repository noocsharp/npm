#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/random.h>

#include "chacha20.h"
#include "common.h"
#include "argon2/argon2.h"
#include "util.h"

char *argv0;

char data[SALT_LEN + PASSWORD_MAX_LEN + 1];
char encryptee[PASSWORD_MAX_LEN];
char encryptor[PASSWORD_MAX_LEN+1];
char key[KEY_LEN];
char nonce[NONCE_LEN];
char salt[SALT_LEN];

void
clear()
{
	memset(data, 0, sizeof(data));
	memset(encryptee, 0, sizeof(encryptee));
	memset(encryptor, 0, sizeof(encryptor));
	memset(key, 0, sizeof(key));
	memset(nonce, 0, sizeof(nonce));
	memset(salt, 0, sizeof(salt));
}

ssize_t
get_password(char *buf)
{
	fgets(buf, PASSWORD_MAX_LEN+1, stdin);
	// XXX: is strlen problematic because it isn't constant time and acts on a secret?
	size_t len = strlen(buf);

	/* the last character of the line should be '\n', which should not be
	 * included in the password */
	if (len == PASSWORD_MAX_LEN && buf[PASSWORD_MAX_LEN-1] != '\n')
		return -1;

	buf[len-1] = '\0';
	return len - 1;
}

void
error(const char *s)
{
	fprintf(stderr, "%s: %s", argv0, s);
}

void
usage()
{
	fprintf(stderr, "%s: [-d file] | [-e]\n", argv0);
	exit(1);
}

int main(int argc, char *argv[]) {
	char *c;
	size_t len;
	FILE *file = NULL;

	argv0 = argv[0];

	if (argc == 1)
		usage();

	if (strcmp(argv[1], "-e") && strcmp(argv[1], "-d"))
		usage();

	if (!strcmp(argv[1], "-e") && argc != 2) {
		error("option -e does not take an argument");
		usage();
	}

	if (!strcmp(argv[1], "-d") && argc != 3) {
		error("option -d takes exactly 1 argument");
		usage();
	}

	/* we want to prevent secret data from being swapped to disk */
	if (mlock(data, sizeof(data)) < 0) {
		fprintf(stderr, "mlock failed: %s", strerror(errno));
	}

	if (mlock(encryptor, sizeof(encryptor)) < 0) {
		fprintf(stderr, "mlock failed: %s", strerror(errno));
	}

	if (mlock(encryptee, sizeof(encryptee)) < 0) {
		fprintf(stderr, "mlock failed: %s", strerror(errno));
	}

	if (mlock(key, sizeof(key)) < 0) {
		fprintf(stderr, "mlock failed: %s", strerror(errno));
	}

	if (mlock(nonce, sizeof(nonce)) < 0) {
		fprintf(stderr, "mlock failed: %s", strerror(errno));
	}

	if (mlock(salt, sizeof(salt)) < 0) {
		fprintf(stderr, "mlock failed: %s", strerror(errno));
	}

	if (strcmp(argv[1], "-e") == 0) {
		if (getrandom(salt, SALT_LEN, 0) < SALT_LEN) {
			error("failed to generate salt");
			goto fail;
		}

		len = get_password(encryptor);
		if (len < 0) {
			error("master password too long");
			goto fail;
		}

		if (argon2id_hash_raw(T_COST, M_COST, PARALLELISM, encryptor, len, salt, SALT_LEN, key, KEY_LEN) < 0) {
			error("key derivation failed");
			goto fail;
		}

		if (getrandom(nonce, NONCE_LEN, 0) < NONCE_LEN) {
			error("failed to generate nonce");
			goto fail;
		}

		memcpy(data, salt, SALT_LEN);

		len = get_password(encryptee);
		if (len < 0) {
			error("password to encrypt is too long");
			goto fail;
		}

		memset(data, 0, SALT_LEN + PASSWORD_MAX_LEN);
		memcpy(data, salt, SALT_LEN);
		memcpy(data + SALT_LEN, encryptee, PASSWORD_MAX_LEN);

		br_chacha20_ct_run(key, nonce, 0, data, SALT_LEN + PASSWORD_MAX_LEN);

		fwrite(nonce, sizeof(char), NONCE_LEN, stdout);
		fwrite(salt, sizeof(char), SALT_LEN, stdout);
		fwrite(data, sizeof(char), SALT_LEN + PASSWORD_MAX_LEN, stdout);
	} else if (strcmp(argv[1], "-d") == 0) {
		file = fopen(argv[2], "r");
		if (file == NULL) {
			error("failed to open file");
			goto fail;
		}
			
		len = fread(nonce, sizeof(char), NONCE_LEN, file);
		if (len < NONCE_LEN) {
			error("failed to read nonce");
			goto fail;
		}

		len = fread(salt, sizeof(char), SALT_LEN, file);
		if (len < SALT_LEN) {
			error("failed to read salt");
			goto fail;
		}

		len = fread(data, sizeof(char), SALT_LEN + PASSWORD_MAX_LEN, file);
		if (len < SALT_LEN + PASSWORD_MAX_LEN) {
			error("failed to read encrypted data");
			goto fail;
		}

		len = get_password(encryptor);

		if (argon2id_hash_raw(T_COST, M_COST, PARALLELISM, encryptor, len, salt, SALT_LEN, key, KEY_LEN) < 0) {
			error("key derivation failed");
			goto fail;
		}

		br_chacha20_ct_run(key, nonce, 0, data, SALT_LEN + PASSWORD_MAX_LEN);

		if (memcmp(data, salt, SALT_LEN) != 0) {
			error("incorrect master password");
			goto fail;
		}

		puts(data + SALT_LEN);
		fclose(file);
	}

	clear();
	return 0;

fail:
	if (file)
		fclose(file);
	clear();
	return 1;
}