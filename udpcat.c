/* $Id: udpcat.c,v 1.2 2011-11-30 00:35:22 grahn Exp $
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
 * Read hex from 'in' and write to UDP socket 'fd' until
 * EOF. Will log parse errors and I/O errors meanwhile.
 */
static int udpcat(FILE* in, int fd);

#if 0
namespace {

    int udppump(const std::string& host,
		const std::string& port,
		const unsigned npackets)
    {
	int fd = udpclient(host, port);
	if(fd == -1) {
	    return 1;
	}

	const char payload = 'x';
	unsigned acc = 0;

	for(unsigned i=0; i<npackets; ++i) {

	    ssize_t n = send(fd, &payload, sizeof payload, 0);
	    if(n==-1) {
		std::cerr << "error: " << strerror(errno) << '\n';
		break;
	    }
	    else {
		acc += n/(sizeof payload);
	    }
	}

	std::cout << "send(2) says we got away " << acc << " packets\n";

	return close(fd);
    }
}
#endif

int main(int argc, char ** argv)
{
    const char* const prog = argv[0];
    char usage[500];
    sprintf(usage, "usage: %s host port", prog);
    const char optstring[] = "+";
    struct option long_options[] = {
	{"version", 0, 0, 'v'},
	{"help", 0, 0, 'h'},
	{0, 0, 0, 0}
    };

    int ch;
    while((ch = getopt_long(argc, argv,
			    optstring, &long_options[0], 0)) != -1) {
	switch(ch) {
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

    return udpcat(stdin, fd);
}
