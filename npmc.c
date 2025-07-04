#define _DEFAULT_SOURCE

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
#include "config.h"
#include "util.h"

char answer[PASSWORD_MAX_LEN + 1];
int answerlen = 0;

char abspath[PATH_MAX];

int
main(int argc, char *argv[])
{
	struct sockaddr_un sockaddr = {
		.sun_family = AF_UNIX,
	};

	snprintf(sockaddr.sun_path, sizeof(sockaddr.sun_path) / sizeof(char), SOCKPATH "-%d", getuid());

	if (argc != 2)
		return 1;

	if (realpath(argv[1], abspath) == NULL) {
		fprintf(stderr, "failed to get absolute path of %s\n", argv[1]);
		goto end;
	}

	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1) {
		fprintf(stderr, "failed to create socket: %s\n", strerror(errno));
		goto end;
	}

	if (connect(sock, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) == -1) {
		fprintf(stderr, "failed to connect to socket: %s\n", strerror(errno));
		close(sock);
		goto end;
	}

	xwrite(sock, abspath, strlen(abspath) + 1); // include terminator

	FILE *sockfile = fdopen(sock, "rw");
	if (!sockfile) {
		perror("failed to open socket as FILE");
		close(sock);
		goto end;
	}

	get_password(sockfile, answer);
	if (*answer)
		printf("%s", answer);

closesock:
	fclose(sockfile);
end:
	return !(*answer);
}
