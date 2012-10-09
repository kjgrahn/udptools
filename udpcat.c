/* $Id: udpcat.c,v 1.6 2011-11-30 01:37:44 grahn Exp $
 *
 * udpcat.cc -- kind of like 'netcat -u host port', but with hexdump input
 *              so it can be binary
 *
 * Copyright (c) 2008, 2011, 2012 Jörgen Grahn
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

#include "hexread.h"


struct Client {
    int fd;
    int connected;
    struct addrinfo * suggestions;
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
    this->fd = socket(first.ai_family,
		      first.ai_socktype,
		      first.ai_protocol);
    if(this->fd==-1) {
	fprintf(stderr, "error: %s\n", strerror(errno));
	return;
    }

    this->connected = 0;
    this->multiplier = multiplier;
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
 * Read hex from 'in' and write to UDP socket until
 * EOF. Will log parse errors and I/O errors meanwhile.
 * Returns an exit code.
 */
static int udpcat(FILE* in, const struct Client* const cli)
{
    int lineno = 0;
    int s;
    uint8_t buf[10000];
    unsigned acc = 0;
    unsigned eacc = 0;

    while((s = hexline(in, ++lineno, buf)) != -1) {

	unsigned m = cli->multiplier;

	while(m--) {
	    ssize_t n = cli_send(cli, buf, s);
	    int complained = 0;
	    if(n!=s) {
		if(!complained) {
		    fprintf(stderr, "warning: line %d: sending caused %s\n",
			    lineno, strerror(errno));
		    complained = 1;
		}
		eacc++;
	    }
	    acc++;
	}
    }

    if(eacc) {
	fprintf(stdout, "send(2) got %u packets to send; "
		"%u complaint(s)\n",
		acc, eacc);
    }
    else {
	fprintf(stdout, "%u datagrams sent\n", acc);
    }
    return eacc!=0;
}


int main(int argc, char ** argv)
{
    const char* const prog = argv[0];
    char usage[500];
    sprintf(usage,
	    "usage: %s [-d N] [--connect] [--ip-option] host port",
	    prog);
    const char optstring[] = "d:";
    struct option long_options[] = {
	{"connect", 0, 0, 'c'},
	{"ip-option", 0, 0, 'o'},
	{"version", 0, 0, 'v'},
	{"help", 0, 0, 'h'},
	{0, 0, 0, 0}
    };

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

    if(connect && !cli_connect(&cli)) {
	return 1;
    }

    if(use_ipoptions) silly_options(cli.fd);

    int rc = udpcat(stdin, &cli);

    cli_destroy(&cli);
    return rc;
}
