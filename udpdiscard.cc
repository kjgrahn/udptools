/* $Id: udpdiscard.cc,v 1.2 2008-10-07 18:32:41 grahn Exp $
 *
 * udpdiscard.cc -- federal udp pound-me-in-the-ass prison
 *
 * Copyright (c) 2008 Jörgen Grahn
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

    int udpserver(const std::string& host,
		  const std::string& port)
    {
	static const struct addrinfo hints = { AI_PASSIVE,
					       AF_UNSPEC,
					       SOCK_DGRAM,
					       0,
					       0, 0, 0, 0 };
	struct addrinfo * suggestions;
	int rc = getaddrinfo(host.empty()? 0: host.c_str(),
			     port.c_str(),
			     &hints,
			     &suggestions);
	if(rc) {
	    std::cerr << "error: " << gai_strerror(rc) << '\n';
	    return -1;
	}

	const struct addrinfo& first = *suggestions;

	int fd = socket(first.ai_family,
			first.ai_socktype,
			first.ai_protocol);
	if(fd==-1) {
	    std::cerr << "error: " << strerror(errno) << '\n';
	    return -1;	    
	}

	const char * const canon = first.ai_canonname;
	std::cout << "binding to: "
		  << (canon? canon : "*") << ':' << port << '\n';

	rc = bind(fd, first.ai_addr, first.ai_addrlen);
	if(rc) {
	    std::cerr << "error: " << strerror(errno) << '\n';
	    return -1;	    
	}

	freeaddrinfo(suggestions);

	return fd;
    }

    int udpdiscard(const std::string& host,
		   const std::string& port,
		   const unsigned npackets)
    {
	int fd = udpserver(host, port);
	if(fd == -1) {
	    return 1;
	}

	sleep(10);

	return close(fd);
    }
}


int main(int argc, char ** argv)
{
    using std::string;

    const string prog = argv[0];
    const string usage = string("usage: ")
	+ prog
	+ " [-n packets] host port";
    const char optstring[] = "+n:";
    struct option long_options[] = {
	{"packets", 0, 0, 'p'},
	{"version", 0, 0, 'v'},
	{"help", 0, 0, 'h'},
	{0, 0, 0, 0}
    };

    unsigned npackets = 1000000;

    int ch;
    while((ch = getopt_long(argc, argv,
			    optstring, &long_options[0], 0)) != -1) {
	switch(ch) {
	case 'n':
	    npackets = std::atol(optarg);
	    break;
	case 'h':
	    std::cout << usage << '\n';
	    return 0;
	    break;
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

    return udpdiscard(host, port, npackets);
}
