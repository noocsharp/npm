#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
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

	xwrite(sock, argv[1], strlen(argv[1]) + 1); // include terminator

	read_answer(sock);

	puts(answer);

closesock:
	close(sock);
end:
	return 0;
}
