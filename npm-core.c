#define _BSD_SOURCE

#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "common.h"
#include "monocypher.h"

char *argv0;

uint8_t plain[PASSWORD_MAX_LEN + 1];
uint8_t cipher[PASSWORD_MAX_LEN + 1];
uint8_t master[PASSWORD_MAX_LEN + 1];
uint8_t key[KEY_LEN];
uint8_t mac[MAC_LEN];
uint8_t data[SALT_LEN + NONCE_LEN];
uint8_t *salt = data;
uint8_t *nonce = data + SALT_LEN;

char *work;

void
clear()
{
	explicit_bzero(plain, sizeof(plain));
	explicit_bzero(cipher, sizeof(cipher));
	explicit_bzero(master, sizeof(master));
	explicit_bzero(key, sizeof(key));
	explicit_bzero(nonce, sizeof(nonce));
	explicit_bzero(data, sizeof(data));
}

ssize_t
get_password(uint8_t *buf)
{
	int ret;
	uint8_t *ptr = buf;
	while (ptr - buf < PASSWORD_MAX_LEN) {
		ret = fgetc(stdin);
		if (ret == EOF) {
			return -1;
		}

		if (ret == '\n')
			return ptr - buf;

		*(ptr++) = ret;
	}

	return -2;
}

void
error(const char *s)
{
	fprintf(stderr, "%s: %s\n", argv0, s);
}

void
usage()
{
	fprintf(stderr, "%s: [-d file] | [-e]\n", argv0);
	exit(1);
}

int main(int argc, char *argv[]) {
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
	if (mlock(plain, sizeof(plain)) < 0) {
		fprintf(stderr, "mlock failed: %s", strerror(errno));
	}

	if (mlock(cipher, sizeof(cipher)) < 0) {
		fprintf(stderr, "mlock failed: %s", strerror(errno));
	}

	if (mlock(master, sizeof(master)) < 0) {
		fprintf(stderr, "mlock failed: %s", strerror(errno));
	}

	if (mlock(key, sizeof(key)) < 0) {
		fprintf(stderr, "mlock failed: %s", strerror(errno));
	}

	if (mlock(data, sizeof(data)) < 0) {
		fprintf(stderr, "mlock failed: %s", strerror(errno));
	}

	if (strcmp(argv[1], "-e") == 0) {
		if (getentropy(salt, SALT_LEN) < SALT_LEN) {
			error("failed to generate salt");
			goto fail;
		}

		switch (len = get_password(master)) {
		case -1:
			error("encountered EOF when reading master password");
			goto fail;
		case -2:
			error("entered master password is too long");
			goto fail;
		}

		work = malloc(M_COST * 1024);
		if (!work) {
			error("failed to allocate work buffer for argon2");
			goto fail;
		}

		crypto_argon2i(key, KEY_LEN, work, M_COST, T_COST, master, len, salt, SALT_LEN);

		if (getentropy(nonce, NONCE_LEN) < NONCE_LEN) {
			error("failed to generate nonce");
			goto fail;
		}

		switch (len = get_password(plain)) {
		case -1:
			error("encountered EOF when reading password");
			goto fail;
		case -2:
			error("entered password is too long");
			goto fail;
		}

		crypto_lock_aead(mac, cipher, key, nonce, data, sizeof(data), plain, PASSWORD_MAX_LEN);

		fwrite(salt, sizeof(char), SALT_LEN, stdout);
		fwrite(nonce, sizeof(char), NONCE_LEN, stdout);
		fwrite(mac, sizeof(char), MAC_LEN, stdout);
		fwrite(cipher, sizeof(char), PASSWORD_MAX_LEN, stdout);
	} else if (strcmp(argv[1], "-d") == 0) {
		file = fopen(argv[2], "r");
		if (file == NULL) {
			error("failed to open file");
			goto fail;
		}
			
		len = fread(salt, sizeof(char), SALT_LEN, file);
		if (len < SALT_LEN) {
			error("failed to read salt");
			goto fail;
		}

		len = fread(nonce, sizeof(char), NONCE_LEN, file);
		if (len < NONCE_LEN) {
			error("failed to read nonce");
			goto fail;
		}

		len = fread(mac, sizeof(char), MAC_LEN, file);
		if (len < MAC_LEN) {
			error("failed to read MAC");
			goto fail;
		}

		len = fread(cipher, sizeof(char), PASSWORD_MAX_LEN, file);
		if (len < PASSWORD_MAX_LEN) {
			error("failed to read encrypted data");
			goto fail;
		}

		switch (len = get_password(master)) {
		case -1:
			error("encountered EOF when reading master password");
			goto fail;
		case -2:
			error("entered master password is too long");
			goto fail;
		}

		work = malloc(M_COST * 1024);
		if (!work) {
			error("failed to allocate argon2 work buffer");
			goto fail;
		}

		crypto_argon2i(key, KEY_LEN, work, M_COST, T_COST, master, len, salt, SALT_LEN);

		if (crypto_unlock_aead(plain, key, nonce, mac, data, sizeof(data), cipher, PASSWORD_MAX_LEN) != 0) {
			error("incorrect master password");
			goto fail;
		}

		puts((char *)plain);
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
