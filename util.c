#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "common.h"

int
xwrite(int fd, char *buf, size_t count)
{
	ssize_t ret;
	char *ptr = buf;
	while (count) {
		ret = write(fd, ptr, count);
		if (ret == -1)
			return -1;

		count -= ret;
		ptr += ret;
	}

	return count;
}

ssize_t
get_password(FILE *f, char *buf)
{
	int ret;
	char *ptr = buf;
	while (ptr - buf < PASSWORD_MAX_LEN) {
		ret = fgetc(f);
		if (ret == EOF) {
			if (ptr > buf && *(ptr - 1) == '\n')
				return ptr - buf - 1;
			else
				return ptr - buf;
		}

		*(ptr++) = ret;
	}

	return -2;
}

