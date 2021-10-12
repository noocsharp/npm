#define _DEFAULT_SOURCE

/* npm-agent uses a simple socket protocol to receive and respond to requests:
 * a client sends a null-terminated path over a socket, with a maximum length
 * PATH_MAX.
 * npm-agent responds with a null-terminated password with a max length of
 * PASSWORD_MAX_LEN characters.
 */

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"

#ifndef TIMEOUT
#define TIMEOUT 10000
#endif

#define LISTENER 0
#define TIMER 1
#define CLIENT 2

char *corecmd[] = { NPM_CORE, "-d", NULL, NULL };
//char *const getpasscmd[] = { "dmenu", "-P", "-p", "Password:", NULL };
char *const getpasscmd[] = { "bemenu", "-x", "-p", "Password:", NULL };

bool cached = false;

char inbuf[PATH_MAX];
char master[PASSWORD_MAX_LEN + 1];
ssize_t masterlen;
char *inptr = inbuf;
size_t inlen;
struct pollfd fds[3];
int timerpipe[2];

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
read_to_nl(int fd, char *buf)
{
	ssize_t ret;
	size_t len = 0;
	char *ptr = buf;

	do {
		ret = read(fd, ptr, PASSWORD_MAX_LEN - len);
		if (ret == -1)
			return ret;

		len += ret;
		ptr += ret;
	} while (ret && len <= PASSWORD_MAX_LEN && !memchr(ptr, '\n', ret));

	return len;
}

void
clear_master()
{
	cached = false;
	explicit_bzero(master, sizeof(master));
	masterlen = 0;
}

int
get_master()
{
	int stdoutpipe[2], stdinpipe[2], status = 1;

	if (pipe(stdoutpipe) == -1) {
		perror("failed to create stdout pipe");
		return status;
	}

	if (pipe(stdinpipe) == -1) {
		perror("failed to create stdin pipe");
		return status;
	}

	pid_t pid = fork();
	switch (pid) {
	case -1:
		fprintf(stderr, "fork failed\n");
		return status;
	case 0:
		close(stdoutpipe[0]);
		dup2(stdoutpipe[1], 1);
		close(stdinpipe[1]);
		dup2(stdinpipe[0], 0);
		
		if (execvp(getpasscmd[0], getpasscmd) == -1) {
			perror("failed to start password retrieval program");
			exit(1);
		}
	default:
		close(stdoutpipe[1]);
		close(stdinpipe[0]);
		close(stdinpipe[1]);

		if ((masterlen = read_to_nl(stdoutpipe[0], master)) == -1) {
			fprintf(stderr, "failed to read password from pipe\n");
			return -1;
		}

		close(stdoutpipe[0]);
		waitpid(pid, &status, 0);
	}

	cached = true;

	return status;
}

int
run_core()
{
	int stdinpipe[2], status;
	if (pipe(stdinpipe) == -1) {
		perror("failed to create stdin pipe");
		return 1;
	}

	pid_t pid = fork();
	switch (pid) {
	case -1:
		perror("fork failed");
		return 1;
	case 0:
		close(stdinpipe[1]);

		dup2(stdinpipe[0], 0);
		dup2(fds[CLIENT].fd, 1);

		corecmd[2] = inbuf;
		if (execvp(corecmd[0], corecmd) == -1) {
			perror("failed to run core");
			exit(1);
		}

		break;
	default:
		close(stdinpipe[0]);

		if (xwrite(stdinpipe[1], master, masterlen) == -1) {
			perror("failed to write master password to pipe");
			return 1;
		}

		close(stdinpipe[1]);

		waitpid(pid, &status, 0);
	}

	return status;
}

struct itimerval timerspec = {
	.it_interval = {0},
	.it_value = { TIMEOUT/1000, (TIMEOUT % 1000) * 1000 * 1000 },
};

void
agent()
{
	int status;

	if (!cached) {
		if (get_master() != 0)
			return;

		if (setitimer(ITIMER_REAL, &timerspec, NULL) == -1) {
			perror("failed to set cache timeout");
			clear_master();
			return;
		}
	}

	// if the password is wrong, we don't cache it
	if ((status = run_core()) != 0)
		clear_master();
}

bool running = true;

void
handler()
{
	running = false;
}

void
alarm_handler()
{
	int ret;
	while (ret = write(timerpipe[1], "a", 1) < 1) {
		if (ret == -1) {
			perror("failed to write to timerpipe");
			break;
		}
	}
}

int
main()
{
	struct sockaddr_un sockaddr = {
		.sun_family = AF_UNIX,
		.sun_path = SOCKPATH
	};
	int ret;

	struct sigaction sa_term = {
		.sa_handler = handler,
	};

	struct sigaction sa_alarm = {
		.sa_handler = alarm_handler,
	};

	sigaction(SIGINT, &sa_term, NULL);
	sigaction(SIGTERM, &sa_term, NULL);
	sigaction(SIGALRM, &sa_alarm, NULL);

	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("failed to create socket");
		goto error_socket;
	}

	if (bind(sock, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) == -1) {
		perror("failed to bind to socket");
		goto error;
	}

	if (listen(sock, 50) == -1) {
		perror("failed to set socket to listening");
		goto error;
	}

	if (pipe(timerpipe) == -1) {
		perror("failed to create timer pipe");
		goto error;
	}

	fds[LISTENER].fd = sock;
	fds[LISTENER].events = POLLIN;
	fds[TIMER].fd = timerpipe[0];
	fds[TIMER].events = POLLIN;
	fds[CLIENT].fd = -1;

	while (running) {
		if (poll(fds, sizeof(fds) / sizeof(fds[0]), -1) == -1) {
			if (errno == EINTR)
				continue;

			perror("poll failed");
			goto error;
		}

		// new connection
		if (fds[LISTENER].revents & POLLIN) {
			if (fds[CLIENT].fd < 0) {
				fds[CLIENT].fd = accept(fds[LISTENER].fd, NULL, NULL);
				if (fds[CLIENT].fd == -1)
					perror("accept failed");

				inptr = inbuf;
				inlen = 0;
				memset(inbuf, 0, sizeof(inbuf));
				fds[CLIENT].events = POLLIN;
			}
		}

		// timer expired
		if (fds[TIMER].revents & POLLIN) {
			uint64_t val;
			read(fds[TIMER].fd, &val, sizeof(val));
			clear_master();
		}

		// incoming data
		if (fds[CLIENT].revents & POLLIN) {
			ret = read(fds[CLIENT].fd, inptr, sizeof(inbuf) - inlen);
			if (ret == -1) {
				perror("failed to read from client");
				goto error;
			}

			inlen += ret;
			inptr += ret;
			// if there is a null, the path is complete
			if (memchr(inbuf, 0, inlen)) {
				fds[CLIENT].revents &= ~POLLIN;

				agent();
				close(fds[CLIENT].fd);
				fds[CLIENT].fd = -1;
				continue;
			}
			// if the buffer is full without a null, the client is misbehaving,
			// so close the connection and clear inbuf
			if (inlen == sizeof(inbuf)) {
				close(fds[CLIENT].fd);
				fds[CLIENT].fd = -1;
			}
		}
	}

error:
	close(sock);
error_socket:
	unlink(SOCKPATH);
	return 1;
}
