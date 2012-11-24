/*
 * udpecho.cc -- udp echo client
 *
 * Copyright (c) 2008, 2012 Jörgen Grahn
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
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>


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


    /**
     * Progress indicator similar to 'ping -f'.
     */
    class Feedback {
    public:
	Feedback(std::ostream& os, bool verbose)
	    : os(os),
	      verbose(verbose)
	{}

	void rx(char ch);
	void tx(char ch);

    private:
	std::ostream& os;
	bool verbose;
    };

    void Feedback::rx(char ch)
    {
	if(verbose) {
	    os.put(ch) << std::flush;
	}
    }

    void Feedback::tx(char ch)
    {
	if(verbose) {
	    os.put('\b').put(ch) << std::flush;
	}
    }


    int udpdiscard(const std::string& host,
		   const std::string& port,
		   const bool verbose)
    {
	const int fd = udpserver(host, port, true);
	if(fd == -1) {
	    return 1;
	}

	Feedback fb(std::cout, verbose);

	const int efd = epoll_create(1);
	assert(efd > 0);
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.ptr = 0;
	int err = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev);
	assert(!err);

	char buf[9000];
	struct iovec iov = { buf, sizeof buf };
	struct sockaddr_storage sa;
	msghdr msg;
	msg.msg_name = reinterpret_cast<void*>(&sa);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = 0;
	msg.msg_controllen = 0;

	while(1) {
	    int n = epoll_wait(efd, &ev, 1, -1);
	    if(n==-1 && errno==EINTR) continue;
	    if(n!=1) break;

	    /* recvmmsg(2) would have been nice here, but it's not in
	     * my current libc. It would have made blocking I/O
	     * without epoll/select efficient.
	     */
	    while(1) {
		msg.msg_namelen = sizeof sa;
		iov.iov_len = sizeof buf;
		const ssize_t err = recvmsg(fd, &msg, MSG_TRUNC);
		if(err==-1) {
		    if(errno==EAGAIN) break;
		    fb.rx('?');
		}
		else if(msg.msg_flags & MSG_TRUNC) {
		    fb.rx('/');
		}
		else {
		    const size_t n = err;
		    iov.iov_len = n;
		    fb.rx('.');
		    int err = sendmsg(fd, &msg, 0);
		    fb.tx(err==-1? '-' : '+');
		}
	    }
	}

	return close(fd);
	/* too lazy for the rest of the cleanup */
    }
}


int main(int argc, char ** argv)
{
    using std::string;

    const string prog = argv[0];
    const string usage = string("usage: ")
	+ prog
	+ " [-v] host port";
    const char optstring[] = "+v";
    struct option long_options[] = {
	{"version", 0, 0, 'V'},
	{"help", 0, 0, 'h'},
	{0, 0, 0, 0}
    };

    bool verbose = false;

    int ch;
    while((ch = getopt_long(argc, argv,
			    optstring, &long_options[0], 0)) != -1) {
	switch(ch) {
	case 'v':
	    verbose = true;
	    break;
	case 'h':
	    std::cout << usage << '\n';
	    return 0;
	    break;
	case 'V':
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

    return udpdiscard(host, port, verbose);
}
