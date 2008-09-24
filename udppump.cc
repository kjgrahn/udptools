/* $Id: udppump.cc,v 1.3 2008-09-24 21:22:33 grahn Exp $
 *
 * udppump.cc -- udp load generator
 *
 * Copyright (c) 2008 J�rgen Grahn
 * All rights reserved.
 *
 */
#include <string>
#include <iostream>
#include <ostream>
#include <cassert>

#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>


namespace {

    int udpclient(const std::string& host,
		  const std::string& port)
    {
	static const struct addrinfo hints = { AI_ADDRCONFIG | AI_CANONNAME,
					       AF_UNSPEC,
					       SOCK_DGRAM,
					       0,
					       0, 0, 0, 0 };
	struct addrinfo * suggestions;
	int rc = getaddrinfo(host.c_str(),
			     port.c_str(),
			     &hints,
			     &suggestions);
	if(rc) {
	    std::cerr << "error: " << gai_strerror(rc) << '\n';
	    return -1;
	}

	const struct addrinfo& first = *suggestions;

	std::cout << first.ai_canonname << '\n';

	int fd = socket(first.ai_family,
			first.ai_socktype,
			first.ai_protocol);
	if(fd==-1) {
	    std::cerr << "error: " << strerror(errno) << '\n';
	    return -1;	    
	}

	rc = connect(fd, first.ai_addr, first.ai_addrlen);
	if(rc) {
	    std::cerr << "error: " << strerror(errno) << '\n';
	    return -1;	    
	}

	freeaddrinfo(suggestions);

	return fd;
    }

    int udppump(const std::string& host,
		const std::string& port)
    {
	int fd = udpclient(host, port);
	if(fd == -1) {
	    return 1;
	}

	const char payload = 'x';
	ssize_t n = send(fd, &payload, sizeof payload, 0);
	if(n!=sizeof payload) {
	    std::cerr << "error: " << strerror(errno) << '\n';
	}

	return close(fd);
    }
}


int main(int argc, char ** argv)
{
    using std::string;

    const string prog = argv[0];
    const string usage = string("usage: ")
	+ prog
	+ " host port";
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
	    std::cout << usage << '\n';
	    return 0;
	case 'v':
	    std::cout << prog << ", the only version\n";
	    return 0;
	    break;
	case ':':
	case '?':
	    std::cerr << usage << '\n';
	    return 1;
	    break;
	default:
	    break;
	}
    }

    if(argc - optind != 2) {
	std::cerr << usage << '\n';
	return 1;
    }

    const string host = argv[optind++];
    const string port = argv[optind++];

    return udppump(host, port);
}
