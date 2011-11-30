/* $Id: udpcat.c,v 1.6 2011-11-30 01:37:44 grahn Exp $
 *
 * udpcat.cc -- kind of like 'netcat -u host port', but with hexdump input
 *              so it can be binary
 *
 * Copyright (c) 2008, 2011 Jörgen Grahn
 * All rights reserved.
 *
 */
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>


static int udpclient(const char* host, const char* port)
{
    static const struct addrinfo hints = { AI_ADDRCONFIG | AI_CANONNAME,
					   AF_UNSPEC,
					   SOCK_DGRAM,
					   0,
					   0, 0, 0, 0 };
    struct addrinfo * suggestions;
    int rc = getaddrinfo(host,
			 port,
			 &hints,
			 &suggestions);
    if(rc) {
	fprintf(stderr, "error: %s\n", gai_strerror(rc));
	return -1;
    }

    const struct addrinfo first = *suggestions;

    fprintf(stdout, "connecting to: %s\n", first.ai_canonname);

    int fd = socket(first.ai_family,
		    first.ai_socktype,
		    first.ai_protocol);
    if(fd==-1) {
	fprintf(stderr, "error: %s\n", strerror(errno));
	return -1;	    
    }

    rc = connect(fd, first.ai_addr, first.ai_addrlen);
    if(rc) {
	fprintf(stderr, "error: %s\n", strerror(errno));
	return -1;	    
    }

    freeaddrinfo(suggestions);

    return fd;
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
	fprintf(stderr, "warning: failed to set IP options: %s\n", strerror(errno));
    }
}


static unsigned nybble(const char ch)
{
    /* sure, I assume a sane character set */
    assert(isxdigit(ch));
    if(isdigit(ch)) return ch - '0';
    if('a' <= ch && ch <= 'f') return 10 + (ch - 'a');
    assert('A' <= ch && ch <= 'F');
    return 10 + (ch - 'A');
}


static void octet(uint8_t* const dst, const char* const src)
{
    *dst = nybble(src[0]) << 4;
    *dst |= nybble(src[1]);
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
    char* a = fbuf;
    const char* b = fbuf;
    /* now do the compaction, and whitespace removal, from b to a */
    while(*b) {
	const char ch = *b;
	if(isspace(ch)) {
	    b++;
	    continue;
	}
	if(ch=='#') {
	    break;
	}
	if(isxdigit(ch)) {
	    *a++ = *b++;
	}
	else {
	    fprintf(stderr, "error: line %d: unexpected character '%c'\n", lineno, ch);
	    return 0;
	}
    }
    const char* const end = a;
    if((end-fbuf) % 2) {
	fprintf(stderr, "error: line %d: odd number of hex digits (%u)\n", lineno, (unsigned)(end-fbuf));
	return 0;
    }
    /* now hex-encode from fbuf..end into 'buf' */
    uint8_t* p = buf;
    a = fbuf;
    while(a!=end) {
	octet(p++, a);
	a+=2;
    }
    return p-buf;
}


/**
 * Read hex from 'in' and write to UDP socket 'fd' until
 * EOF. Will log parse errors and I/O errors meanwhile.
 * Returns an exit code.
 */
static int udpcat(FILE* in, int fd)
{
    int lineno = 0;
    int s;
    uint8_t buf[10000];
    unsigned acc = 0;
    unsigned eacc = 0;

    while((s = hexline(in, ++lineno, buf)) != -1) {

	ssize_t n = send(fd, buf, s, 0);
	if(n!=s) {
	    fprintf(stderr, "warning: line %d: sending caused %s\n", lineno, strerror(errno));
	    eacc++;
	}
	acc++;
    }
    fprintf(stdout, "send(2) got %u packets to send; %u whined about errors\n", acc, eacc);
    return eacc!=0;
}


int main(int argc, char ** argv)
{
    const char* const prog = argv[0];
    char usage[500];
    sprintf(usage, "usage: %s [--ip-option] host port", prog);
    const char optstring[] = "+";
    struct option long_options[] = {
	{"ip-option", 0, 0, 'o'},
	{"version", 0, 0, 'v'},
	{"help", 0, 0, 'h'},
	{0, 0, 0, 0}
    };

    int use_ipoptions = 0;

    int ch;
    while((ch = getopt_long(argc, argv,
			    optstring, &long_options[0], 0)) != -1) {
	switch(ch) {
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

    if(argc - optind != 2) {
	fprintf(stderr, "%s\n", usage);
	return 1;
    }

    const char* const host = argv[optind++];
    const char* const port = argv[optind++];
    int fd = udpclient(host, port);
    if(fd == -1) {
	return 1;
    }

    if(use_ipoptions) silly_options(fd);

    return udpcat(stdin, fd);
}
