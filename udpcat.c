/* $Id: udpcat.c,v 1.6 2011-11-30 01:37:44 grahn Exp $
 *
 * udpcat.cc -- kind of like 'netcat -u host port', but with hexdump input
 *              so it can be binary
 *
 * Copyright (c) 2008, 2011, 2012, 2014 Jörgen Grahn
 * All rights reserved.
 *
 */
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>

#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>

#include "hexread.h"


struct Client {
    int fd;
    int connected;
    struct addrinfo * suggestions;
    int efd;
    unsigned multiplier;
};


static void cli_create(struct Client* const this,
		       const char* host, const char* port,
		       unsigned multiplier)
{
    this->fd = -1;
    static const struct addrinfo hints = { AI_ADDRCONFIG | AI_CANONNAME,
					   AF_UNSPEC,
					   SOCK_DGRAM,
					   0,
					   0, 0, 0, 0 };
    int rc = getaddrinfo(host,
			 port,
			 &hints,
			 &this->suggestions);
    if(rc) {
	fprintf(stderr, "error: %s\n", gai_strerror(rc));
	return;
    }

    const struct addrinfo first = *this->suggestions;
    const int fd = socket(first.ai_family,
			  first.ai_socktype,
			  first.ai_protocol);
    if(fd==-1) {
	fprintf(stderr, "error: %s\n", strerror(errno));
	return;
    }

    this->fd = fd;

    const int efd = epoll_create(1);
    assert(efd > 0);
    this->efd = efd;
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = 0;
    rc = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev);
    assert(!rc);

    this->connected = 0;
    this->multiplier = multiplier;
}


static int cli_bind(struct Client* const this,
		    const char* source)
{
    static const struct addrinfo hints = { AI_ADDRCONFIG | AI_PASSIVE,
					   AF_UNSPEC,
					   SOCK_DGRAM,
					   0,
					   0, 0, 0, 0 };
    struct addrinfo * suggestions;
    int rc = getaddrinfo(source,
			 NULL,
			 &hints,
			 &suggestions);
    if(rc) {
	fprintf(stderr, "error: can't bind: %s\n", gai_strerror(rc));
	return 0;
    }

    const struct addrinfo first = *suggestions;
    rc = bind(this->fd,
	      first.ai_addr,
	      first.ai_addrlen);
    freeaddrinfo(suggestions);
    if(rc) {
	fprintf(stderr, "error: can't bind: %s\n", strerror(errno));
	return 0;
    }

    return 1;
}


static int cli_connect(struct Client* const this)
{
    const struct addrinfo first = *this->suggestions;

    fprintf(stdout, "connecting to: %s\n", first.ai_canonname);

    int rc = connect(this->fd, first.ai_addr, first.ai_addrlen);
    if(rc) {
	fprintf(stderr, "error: %s\n", strerror(errno));
	return 0;
    }

    this->connected = 1;
    return 1;
}


static ssize_t cli_send(const struct Client* const this,
			const void *buf, size_t len)
{
    if(this->connected) {
	return send(this->fd, buf, len, 0);
    }
    else {
	const struct addrinfo first = *this->suggestions;
	return sendto(this->fd,
		      buf, len, 0,
		      first.ai_addr, first.ai_addrlen);
    }
}


static void cli_destroy(struct Client* const this)
{
    freeaddrinfo(this->suggestions);
    close(this->fd);
    close(this->efd);
}


/**
 * Ask socket 'fd' to carry around do-nothing IP options,
 * assuming it's an IPv4 socket.
 */
static void silly_options(int fd)
{
    uint8_t nopnop[] = {0, 0};
    int rc = setsockopt(fd, IPPROTO_IP, IP_OPTIONS, nopnop, sizeof nopnop);
    if(rc) {
	fprintf(stderr, "warning: failed to set IP options: %s\n",
		strerror(errno));
    }
}


/**
 * Read a line of text from 'in' and (assuming it's all hex digits)
 * encode it into 'buf' (assumed to be big enough).
 * Returns the number of octets encoded, or -1.
 * Will log parse errors to stderr.
 */
static int hexline(FILE* const in, const int lineno,
		   uint8_t* const buf)
{
    char fbuf[20000];
    if(!fgets(fbuf, sizeof fbuf, in)) {
	return -1;
    }
    const char* a = fbuf;
    const char* b = a + strlen(a);
    const size_t n = hexread(buf, &a, b);

    if(a!=b) {
	fprintf(stderr, "error: line %d: unexpected character '%c'\n",
		lineno, *a);
	return 0;
    }

    return n;
}


/**
 * Is the rxbuf same as the txbuf, keeping in mind that rxbuf might
 * be truncated?  Print an error otherwise, and mention 'lineno'.
 */
static int equal(const int lineno,
		 const void* txbuf, size_t txsize,
		 const void* rxbuf, size_t rxsize,
		 ssize_t received)
{
    if((size_t)received!=txsize) {
	fprintf(stderr, "warning: line %d: sent %zd octets but got %zu\n",
		lineno, txsize, received);
	return 0;
    }

    if(memcmp(txbuf, rxbuf, (txsize>rxsize)? rxsize: txsize)) {
	fprintf(stderr, "warning: line %d: rx data differs\n",
		lineno);
	return 0;
    }

    return 1;
}


/**
 * Send 'n' copies of 'buf' and wait for 'n' identical responses
 * for at most 0.5s.
 *
 * If 'n' is sufficiently small and the reflector on the other side is
 * never delayed that much, this should be no problem.
 *
 * Complain about I/O errors, missing responses, corrupt responses,
 * and return the number of high-level failures (0--n).
 */
static unsigned udpping(const uint8_t* const buf, const size_t size,
			const int lineno,
			const struct Client* const cli,
			const unsigned n)
{
    unsigned expected = 0;

    for(unsigned i=0; i<n; i++) {
	if(cli_send(cli, buf, size) >= 0) {
	    expected++;
	}
    }

    /* This is not terribly efficient (two syscalls per packet) but we
     * don't aim for efficiency.  Nonblocking I/O or recvmmsg(2) would
     * be better though.
     */
    int got = 0;

    for(unsigned i=0; i<expected; i++) {
	struct epoll_event ev;
	/* ok, so we can wait for much more than 0.5s
	 * in degenerate cases, but I don't want to
	 * do clock arithmetics right now
	 */
	const int ew = epoll_wait(cli->efd, &ev, 1, 500);
	if(ew==-1) {
	    if(errno!=EINTR) {
		fprintf(stderr, "warning: line %d: %s failed: %s\n",
			lineno, "epoll", strerror(errno));
		break;
	    }
	}
	else if(ew==0) {
	    break;
	}
	else {
	    uint8_t rxbuf[10000];
	    ssize_t n = recv(cli->fd, rxbuf, sizeof rxbuf, MSG_TRUNC);
	    if(n==-1) {
		fprintf(stderr, "warning: line %d: %s failed: %s\n",
			lineno, "recv", strerror(errno));
		break;
	    }

	    if(equal(lineno, buf, size, rxbuf, sizeof rxbuf, n)) {
		got++;
	    }
	}
    }

    return n - got;
}



/**
 * Read hex from 'in' and write to UDP socket until
 * EOF. Will log parse errors and I/O errors meanwhile.
 * Returns an exit code.
 */
static int udpcat(FILE* in, const struct Client* const cli)
{
    int lineno = 0;
    int s;
    uint8_t buf[10000];
    unsigned totalfailure = 0;

    while((s = hexline(in, ++lineno, buf)) != -1) {

	unsigned failures = 0;
	static const unsigned BATCH = 100;
	unsigned m = cli->multiplier;

	while(m) {
	    const unsigned batch = (m>BATCH)? BATCH: m;

	    failures += udpping(buf, s, lineno, cli, batch);
	    m -= batch;
	}

	if(failures) {
	    fprintf(stderr, "warning: line %d: %u packets lost\n",
		    lineno, failures);
	    totalfailure += failures;
	}
    }

    return totalfailure!=0;
}


int main(int argc, char ** argv)
{
    const char* const prog = argv[0];
    char usage[500];
    sprintf(usage,
	    "usage: %s [-d N] [--connect] [--ip-option] "
	    "[-s source] host port",
	    prog);
    const char optstring[] = "d:s:";
    struct option long_options[] = {
	{"connect", 0, 0, 'c'},
	{"ip-option", 0, 0, 'o'},
	{"version", 0, 0, 'v'},
	{"help", 0, 0, 'h'},
	{0, 0, 0, 0}
    };

    char source[300];
    strcpy(source, "");
    int use_ipoptions = 0;
    int connect = 0;
    unsigned multiplier = 1;

    int ch;
    while((ch = getopt_long(argc, argv,
			    optstring, &long_options[0], 0)) != -1) {
	switch(ch) {
	case 'd':
	    multiplier = strtoul(optarg, 0, 0);
	    break;
	case 's':
	    strcpy(source, optarg);
	    break;
	case 'c':
	    connect = 1;
	    break;
	case 'o':
	    use_ipoptions = 1;
	    break;
	case 'h':
	    fprintf(stdout, "%s\n", usage);
	    return 0;
	    break;
	case 'v':
	    fprintf(stdout, "%s, the only version\n", prog);
	    return 0;
	    break;
	case ':':
	case '?':
	    fprintf(stderr, "%s\n", usage);
	    return 1;
	    break;
	default:
	    break;
	}
    }

    if(argc - optind != 2 || !multiplier) {
	fprintf(stderr, "%s\n", usage);
	return 1;
    }

    const char* const host = argv[optind++];
    const char* const port = argv[optind++];
    struct Client cli;
    cli_create(&cli, host, port, multiplier);
    if(cli.fd == -1) {
	return 1;
    }

    if(*source && !cli_bind(&cli, source)) {
	return 1;
    }

    if(connect && !cli_connect(&cli)) {
	return 1;
    }

    if(use_ipoptions) silly_options(cli.fd);

    int rc = udpcat(stdin, &cli);

    cli_destroy(&cli);
    return rc;
}
