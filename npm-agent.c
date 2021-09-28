#define _BSD_SOURCE

/* npm-agent uses a simple socket protocol to receive and respond to requests:
 * a client sends a null-terminated path over a socket, with a maximum length
 * PATH_MAX.
 * npm-agent responds with a null-terminated password with a max length of
 * PASSWORD_MAX_LEN characters.
 */

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#include "common.h"

#ifndef SOCKPATH
#define SOCKPATH "/tmp/npm-agent"
#endif

#ifndef TIMEOUT
#define TIMEOUT 5000
#endif

#define LISTENER 0
#define TIMER 1
#define CLIENT 2

char *corecmd[] = { NPM_CORE, "-d", NULL, NULL };
//char *const getpasscmd[] = { "dmenu", "-P", "-p", "Password:", NULL };
char *const getpasscmd[] = { "bemenu", "-x", "-p", "Password:", NULL };

bool cached = false;

char inbuf[PATH_MAX];
char encryptor[PASSWORD_MAX_LEN+1];
size_t encryptorlen;
char *inptr = inbuf;
size_t inlen;
struct pollfd fds[3];

int cstdin, cstdout; // stdin, out from the core

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
	while (ret && len <= PASSWORD_MAX_LEN && !memchr(buf, '\n', len)) {
		ret = read(fd, ptr, PASSWORD_MAX_LEN - len);
		if (ret == -1)
			return ret;

		len += ret;
		ptr += ret;
	}

	return len;
}

void
clear_encryptor()
{
	cached = false;
	explicit_bzero(encryptor, sizeof(encryptor));
	encryptorlen = 0;
}

int
get_password()
{
	int stdoutpipe[2], stdinpipe[2], status;

	if (pipe(stdoutpipe) == -1) {
		fprintf(stderr, "failed to create stdout pipe: %s\n", strerror(errno));
	}

	if (pipe(stdinpipe) == -1) {
		fprintf(stderr, "failed to create stdin pipe: %s\n", strerror(errno));
	}

	pid_t pid = fork();
	switch (pid) {
	case -1:
		fprintf(stderr, "fork failed\n");
		return -1;
	case 0:
		close(stdoutpipe[0]);
		dup2(stdoutpipe[1], 1);
		close(stdinpipe[1]);
		dup2(stdinpipe[0], 0);
		
		if (execvp(getpasscmd[0], getpasscmd) == -1)
			fprintf(stderr, "exec failed: %s\n", strerror(errno));

		break;
	default:
		close(stdoutpipe[1]);
		close(stdinpipe[0]);
		close(stdinpipe[1]);

		if ((encryptorlen = read_to_nl(stdoutpipe[0], encryptor)) == -1) {
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
		fprintf(stderr, "failed to create stdin pipe: %s\n", strerror(errno));
	}

	pid_t pid = fork();
	switch (pid) {
	case -1:
		fprintf(stderr, "fork failed\n");
		return -1;
	case 0:
		close(stdinpipe[1]);

		dup2(stdinpipe[0], 0);
		dup2(fds[CLIENT].fd, 1);

		corecmd[2] = inbuf;
		if (execvp(corecmd[0], corecmd) == -1)
			fprintf(stderr, "exec failed: %s\n", strerror(errno));

		break;
	default:
		close(stdinpipe[0]);

		if (xwrite(stdinpipe[1], encryptor, encryptorlen) == -1) {
			fprintf(stderr, "failed to write password to pipe\n");
			return -1;
		}

		close(stdinpipe[1]);

		waitpid(pid, &status, 0);
	}

	return status;
}

const struct itimerspec timerspec = {
	.it_interval = {0},
	.it_value = { TIMEOUT/1000, (TIMEOUT % 1000) * 1000 * 1000 },
};

void
set_timer()
{
	if (timerfd_settime(fds[TIMER].fd, 0, &timerspec, NULL) == -1) {
		fprintf(stderr, "failed to set cache timeout: %s\n", strerror(errno));
	}
}

int
agent()
{
	int status;

	if (!cached) {
		get_password();
		set_timer();
	}

	// if the password is wrong, we don't cache it
	if (status = run_core()) {
		clear_encryptor();
	}
}

bool running = true;

void
handler(int sig)
{
	running = false;
}

int
main(int argc, char *argv[])
{
	struct sockaddr_un sockaddr = {
		.sun_family = AF_UNIX,
		.sun_path = SOCKPATH
	};
	int ret;

	struct sigaction sa = {
		.sa_handler = handler,
	};

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1) {
		fprintf(stderr, "failed to create socket: %s\n", strerror(errno));
		goto error_socket;
	}

    if (bind(sock, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) == -1) {
        fprintf(stderr, "failed to bind to socket: %s\n", strerror(errno));
        goto error;
    }

    if (listen(sock, 50) == -1) {
        fprintf(stderr, "failed to set socket to listening: %s\n", strerror(errno));
        goto error;
    }

    int timer = timerfd_create(CLOCK_MONOTONIC, 0);
    if (!timer) {
    	fprintf(stderr, "failed to create timerfd: %s\n", strerror(errno));
    	goto error;
    }

    fds[LISTENER].fd = sock;
    fds[LISTENER].events = POLLIN;
    fds[TIMER].fd = timer;
    fds[TIMER].events = POLLIN;
    fds[CLIENT].fd = -1;

    while (running) {
    	if (poll(fds, sizeof(fds) / sizeof(fds[0]), -1) == -1) {
    		fprintf(stderr, "poll failed: %s", strerror(errno));
    		goto error;
    	}

		// new connection
    	if (fds[LISTENER].revents & POLLIN) {
    		if (fds[CLIENT].fd < 0) {
    			fds[CLIENT].fd = accept(fds[LISTENER].fd, NULL, NULL);
    			if (fds[CLIENT].fd == -1)
		    		fprintf(stderr, "accept failed: %s", strerror(errno));

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
			clear_encryptor();
    	}

		// incoming data
    	if (fds[CLIENT].revents & POLLIN) {
    		ret = read(fds[CLIENT].fd, inptr, sizeof(inbuf) - inlen);
    		if (ret == -1) {
    			fprintf(stderr, "failed to read from client: %s\n", strerror(errno));
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
