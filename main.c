#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <arpa/inet.h>

#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>

/*
 * Called immediatly after function fails, if the function is a syscall and sets errno use ERROR instead.
 */
#define WARN(msg) \
	fprintf(stderr, "[%s:%d] " msg "\n", __FILE__, __LINE__), -1

/* 
 * Should be called immediately after a syscall fails and sets `errno` (ideally on the very next line),
 * this way we can use `errno` knowing it hasn't been overwritten by another function.
 */
#define ERROR(name) \
	fprintf(stderr, "[%s:%d] %s: %s" "\n", __FILE__, __LINE__, name, strerror(errno)), -1

struct sock_t {
	struct sockaddr_in addr;
	socklen_t addr_len;
	int fd;
};

int sock_init(struct sock_t *sock, const char *ip) {

	sock->addr = (struct sockaddr_in) {
		.sin_family = AF_INET,
	};

	sock->addr_len = sizeof(struct sockaddr);

	sock->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
	if (sock->fd == -1)
		return ERROR("socket");

	if (inet_aton(ip, &sock->addr.sin_addr) != 1)
		return WARN("inet_aton: failed");

	struct timeval t = (struct timeval) {
		.tv_sec = 3,
		.tv_usec = 0
	};

	if (setsockopt(sock->fd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t)) == -1)
		return ERROR("setsockopt");

	return 0;
}

/*
 * "The 16 bit one's complement of the one's complement sum of all 16
 * bit words in the header.  For computing the checksum, the checksum
 * field should be zero."
 * - RFC 792 
 */
u_short sock_cksum(struct icmp *hdr) {
	u_short byte;
	u_short a, b;
	u_short acc = 0;
	for (size_t i=0; i<sizeof(struct icmp); i+=2) {
		a = (((unsigned char *)hdr)[i]);
		b = (((unsigned char *)hdr)[i+1]);
		byte = (a << 8) + b;
		acc += (byte);
	}
	return htons(~(acc));
}

int sock_send(struct sock_t *sock) {
	ssize_t wrote;
	struct icmp hdr[1];
	bzero(hdr, sizeof(hdr));

	hdr->icmp_type = ICMP_ECHO;
	hdr->icmp_id = 1337;

	hdr->icmp_cksum = (sock_cksum(hdr));

	wrote = sendto(
		sock->fd,
		hdr,
		sizeof(hdr),
		0,
		(struct sockaddr *)&sock->addr,
		sock->addr_len);

	if (wrote == -1)
		return ERROR("sendto");
	if (wrote < sizeof(hdr))
		return WARN("sendto: sent less than n bytes");

	return 0;
}

int sock_recv(struct sock_t *sock) {
	ssize_t len;
	char buf[512];
	socklen_t slen = 0;
	size_t buf_len = sizeof(buf);

	len = recvfrom(
		sock->fd,
		buf,
		buf_len,
		0,
		NULL,
		&slen);

	if (len == -1)
		return ERROR("recvfrom");

	printf("reclen = %ld\n", len);

	return 0;
}

void print_usage(const char *name) {
	printf("usage: %s IP_ADDRESS\n", name);
}

int main(int argc, char *argv[])
{
	struct sock_t sock[1];
	bzero(sock, sizeof(sock));

	if (argc != 2) {
		print_usage(argv[0]);
		return 1;
	}

	const char *ip = argv[1];

	if (sock_init(sock, ip))
		return 1;
	if (sock_send(sock))
		return 1;
	if (sock_recv(sock))
		return 1;

	return 0;
}
