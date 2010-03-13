/* $Id: udpdiscard.cc,v 1.5 2010-03-13 19:26:04 grahn Exp $
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
#include <cstdlib>

#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>


namespace {

    int udpserver(const std::string& host,
		  const std::string& port,
		  const bool nonblocking)
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

	if(nonblocking) {
	    const int flags = fcntl(fd, F_GETFL, 0);
	    rc = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	    assert(!rc);
	}

	return fd;
    }

    int udpdiscard(const std::string& host,
		   const std::string& port,
		   const unsigned maxpackets,
		   const bool nonblocking)
    {
	const int fd = udpserver(host, port, nonblocking);
	if(fd == -1) {
	    return 1;
	}

	fd_set fds;
	FD_ZERO(&fds);

	unsigned npackets = 0;
	unsigned nselects = 0;
	unsigned nreads = 0;

	while(maxpackets && npackets < maxpackets) {

	    FD_SET(fd, &fds);

	    int rc = select(fd+1, &fds, 0, 0, 0);
	    ++nselects;
	    if(rc==-1 && errno==EINTR) continue;
	    assert(rc!=-1);
	    assert(rc==1);
	    assert(FD_ISSET(fd, &fds));

	    do {
		/* just read the first octet and ditch the rest  */
		char buf[1];
		ssize_t nb = recv(fd, buf, sizeof buf, 0);
		++nreads;
		if(nb==-1) {
		    assert(errno==EWOULDBLOCK);
		    break;
		}
		assert(nb==1);
		++npackets;
	    } while(nonblocking);
	}

	std::cerr << npackets << " datagrams found via "
		  << nreads << " read(2) calls, "
		  << nselects << " select(2) calls\n";

	return close(fd);
    }
}


int main(int argc, char ** argv)
{
    using std::string;

    const string prog = argv[0];
    const string usage = string("usage: ")
	+ prog
	+ " [-n packets] [-N] host port";
    const char optstring[] = "+n:N";
    struct option long_options[] = {
	{"packets", 0, 0, 'p'},
	{"nonblocking", 0, 0, 'N'},
	{"version", 0, 0, 'v'},
	{"help", 0, 0, 'h'},
	{0, 0, 0, 0}
    };

    unsigned npackets = 1000000;
    bool nonblocking = false;

    int ch;
    while((ch = getopt_long(argc, argv,
			    optstring, &long_options[0], 0)) != -1) {
	switch(ch) {
	case 'n':
	    npackets = std::atol(optarg);
	    break;
	case 'N':
	    nonblocking = true;
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

    return udpdiscard(host, port, npackets, nonblocking);
}
