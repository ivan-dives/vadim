/* -*- c-default-style: "linux"; -*- */

/* compile me with 'make udp CFLAGS='-lm -g3' */

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <error.h>
#include <limits.h>
#include <math.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define CMD_MOTD "motd"
#define CMD_SQRT "sqrt"

#define SERVER_MOTD "Hello. I am a simple calc server. I can sqrt things."

#define UDP_PORT 10000

static char *buf, *str;
static int fd, len, ret;
static ssize_t bytes;
static struct pollfd fds;
static struct sockaddr_in sa;

static long next_number(void)
{
	static long last_base;

	long max_base = sqrt(LONG_MAX);

	if (last_base == max_base || last_base == 0) {
		last_base = 1;
	} else {
		last_base++;
	}

	return pow(last_base, 2);
}

static int client_main(void)
{
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	assert_perror(fd == -1);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(UDP_PORT);
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	bytes = sendto(fd, CMD_MOTD, sizeof(CMD_MOTD), 0, (const struct sockaddr *)&sa, sizeof(struct sockaddr_in));
	assert_perror(bytes == -1);
	printf("client sent '%s'\n", CMD_MOTD);

	fds.fd = fd;
	fds.events = 0 | POLLIN;
	for (;;) {
		ret = poll(&fds, 1, INT_MAX);
		assert_perror(ret == -1);
		ret = ioctl(fd, FIONREAD, &len);
		assert_perror(ret == -1);
		if (len == 0) {
			continue;
		}
		buf = realloc(buf, len);
		assert_perror(buf == NULL);
		bytes = recv(fd, buf, len, 0);
		assert_perror(bytes == -1);
		assert(bytes == len);
		assert(buf[bytes - 1] == '\0');
		printf("client received '%s'\n", buf);

		ret = asprintf(&str, "%s %ld", CMD_SQRT, next_number());
		assert(str != NULL); /* OOM */
		bytes = sendto(fd, str, ret + 1, 0, (const struct sockaddr *)&sa, sizeof(struct sockaddr_in));
		assert_perror(bytes == -1);
		printf("client sent '%s'\n", str);
		free(str);

		printf("client is going to sleep for 1 second\n");
		sleep(1);
		printf("client woke up\n");
	}

	return 0;
}

static int server_main(void)
{
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	assert_perror(fd == -1);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(UDP_PORT);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	ret = bind(fd, (const struct sockaddr *)&sa, sizeof(sa));
	assert_perror(ret == -1);
	printf("server ready\n");

	fds.fd = fd;
	fds.events = 0 | POLLIN;
	for (;;) {
		ret = poll(&fds, 1, INT_MAX);
		assert_perror(ret == -1);
		for (;;) {
			socklen_t addrlen;
			struct sockaddr src_addr;

			ret = ioctl(fd, FIONREAD, &len);
			assert_perror(ret == -1);
			if (len == 0) {
				break;
			}
			buf = realloc(buf, len);
			assert_perror(buf == NULL);
			/* memset(&src_addr, 0, sizeof(struct sockaddr)); */
			addrlen = sizeof(struct sockaddr);
			bytes = recvfrom(fd, buf, len, 0, &src_addr, &addrlen);
			assert_perror(bytes == -1);
			assert(bytes == len);
			assert(buf[bytes - 1] == '\0');
			printf("server received '%s'\n", buf);

			if (!strncmp(buf, CMD_MOTD, strlen(CMD_MOTD))) {
				bytes = sendto(fd, SERVER_MOTD, sizeof(SERVER_MOTD), 0, &src_addr, addrlen);
				assert_perror(bytes == -1);
				printf("server sent '%s'\n", SERVER_MOTD);
				continue;
			}
			/* example message: "sqrt 16" */
			if (!strncmp(buf, CMD_SQRT, strlen(CMD_SQRT))) {
				char *s;
				double d;
				long l;

				s = &buf[strlen(CMD_SQRT) + 1];
				l = atol(s);
				d = sqrt(l);

				ret = asprintf(&str, "%f", d);
				assert(str != NULL); /* OOM */

				bytes = sendto(fd, str, ret + 1, 0, &src_addr, addrlen);
				assert_perror(bytes == -1);
				printf("server sent '%s'\n", str);
				free(str);
				continue;
			}
			printf("dunno what to do with message '%s'\n", buf);
		}
	}

	return 0;
}

static void usage(const char *filename)
{
	error(0, 0, "usage: %s client|server", filename);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		usage(argv[0]);
	}

	if (!strcmp(argv[1], "client")) {
		return client_main();
	}
	if (!strcmp(argv[1], "server")) {
		return server_main();
	}

	usage(argv[0]);

	return 0;
}

