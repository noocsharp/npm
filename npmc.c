#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

#include "common.h"

#ifndef SOCKPATH
#define SOCKPATH "/tmp/npm-agent"
#endif

char answer[PASSWORD_MAX_LEN + 1];
int answerlen = 0;

char abspath[PATH_MAX];

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

int
read_answer(int fd)
{
	fprintf(stderr, "%s\n", __func__);
	ssize_t ret;
	char *ptr = answer;
	while (answerlen <= PASSWORD_MAX_LEN && !memchr(answer, '\n', answerlen)) {
		ret = read(fd, ptr, PASSWORD_MAX_LEN - answerlen);
		fprintf(stderr, "ret = %d\n", ret);
		if (ret == -1)
			return ret;

		if (ret == 0)
			break;

		answerlen += ret;
		ptr += ret;
	}

	return answerlen;
}

int
main(int argc, char *argv[])
{
	const struct sockaddr_un sockaddr = {
		.sun_family = AF_UNIX,
		.sun_path = SOCKPATH
	};
	if (argc != 2)
		return 1;

	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1) {
		fprintf(stderr, "failed to create socket: %s\n", strerror(errno));
		goto end;
	}

	if (connect(sock, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) == -1) {
		fprintf(stderr, "failed to connect to socket: %s\n", strerror(errno));
		goto closesock;
	}

	if (realpath(argv[1], abspath) == NULL) {
		fprintf(stderr, "failed to get absolute path of %s\n", argv[1]);
		goto closesock;
	}

	xwrite(sock, abspath, strlen(abspath) + 1); // include terminator

	read_answer(sock);

	if (*answer)
		printf("%s", answer);

closesock:
	close(sock);
end:
	return !(*answer);
}
