/*
 * udpecho.cc -- udp echo server
 *
 * Copyright (c) 2008, 2012, 2013 Jörgen Grahn
 * All rights reserved.
 *
 */
#include <string>
#include <vector>
#include <iostream>
#include <ostream>
#include <cassert>
#include <cstdlib>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>

#ifdef MSG_WAITFORONE
/* recvmmsg(2); Linux-specific and recent */
#define HAS_MMSG
#endif

namespace {

    /**
     * Parse a local address specification. The format goes:
     * - host:port
     * - port
     * It turns out that this is enough to support even numerical IPv6
     * addresses and host/service names.
     */
    void host_and_port(const char* s,
		       std::string& host, std::string& port)
    {
	const char* p = std::strrchr(s, ':');
	if(p) {
	    host = std::string(s, p);
	    p++;
	    port = p;
	}
	else {
	    host = "";
	    port = s;
	}
    }
}


/**
 * Wrapper for getaddrinfo() for the UDP server case.
 */
struct Addrinfo {
    explicit Addrinfo(const std::string& name);
    ~Addrinfo();

    const std::string name;
    int err;
    const char* strerror() const { return gai_strerror(err); }

    int socket() const;

private:
    addrinfo* ai;

    Addrinfo(const Addrinfo&);
    Addrinfo& operator= (const Addrinfo&);
};


Addrinfo::Addrinfo(const std::string& name)
    : name(name),
      err(0),
      ai(0)
{
    static const struct addrinfo hints = {
	AI_PASSIVE,
	AF_UNSPEC,
	SOCK_DGRAM,
	0,
	0, 0, 0, 0 };

    std::string host;
    std::string port;
    host_and_port(name.c_str(), host, port);

    err = getaddrinfo(host.empty()? 0: host.c_str(),
		      port.c_str(),
		      &hints,
		      &ai);
}


Addrinfo::~Addrinfo()
{
    freeaddrinfo(ai);
}


/**
 * Create a suitable socket (bound, UDP) from this info, or return -1.
 * and set errno.  Undefined results if err is already set.
 *
 */
int Addrinfo::socket() const
{
    assert(!err);

    int fd = -1;
    const addrinfo* i = ai;
    while(fd==-1 && i) {
	fd = ::socket(i->ai_family,
		      i->ai_socktype,
		      i->ai_protocol);
	if(fd!=-1) {
	    int err = bind(fd, i->ai_addr, i->ai_addrlen);
	    if(err) {
		close(fd);
		fd = -1;
	    }
	}
	i = i->ai_next;
    }

    return fd;
}


/**
 * Wrapper for simple counters.
 */
template<class T>
class Accumulator {
public:
    Accumulator() : val(0) {}
    Accumulator operator++ () { val++; return *this; }
    Accumulator operator+= (unsigned n) { val+=n; return *this; }

private:
    T val;
};


/**
 * The state for a certain echo socket.
 */
struct Endpoint {
    Endpoint(const std::string& name, int fd)
	: name(name),
	  fd(fd)
    {}

    std::string name;
    int fd;
    Accumulator<unsigned> rx;
    Accumulator<unsigned> tx;
    Accumulator<unsigned> err;
    Accumulator<unsigned long> rxb;
};


namespace {

    void epoll_add(int efd, int fd, unsigned index)
    {
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.u32 = index;
	int err = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev);
	assert(!err);
    }


    /**
     * Just something to keep track of the bits & pieces
     * needed for our struct msghdr.
     */
    struct Msg {
	Msg() {
	    iov.iov_base = buf;
	    iov.iov_len = sizeof buf;
	}

	msghdr hdr_of() {
	    msghdr h;
	    h.msg_name = reinterpret_cast<void*>(&sa);
	    h.msg_namelen = sizeof sa;
	    h.msg_iov = &iov;
	    h.msg_iovlen = 1;
	    h.msg_control = 0;
	    h.msg_controllen = 0;
	    return h;
	}

	sockaddr_storage sa;
	iovec iov;
	char buf[10000];

    private:
	Msg(const Msg&);
	Msg& operator= (const Msg&);
    };


    bool controlmsg(int, const std::vector<Endpoint>&)
    {
	return true;
    }


    void reflect(Endpoint& ep,
		 const msghdr& h,
		 const unsigned len)
    {
	++ep.rx;
	ep.rxb += len;

	if(h.msg_flags & MSG_TRUNC) {
	    /* truncation; treat as an error */
	    ++ep.err;
	}
	else {
	    ssize_t n = sendmsg(ep.fd,
				&h,
				MSG_DONTWAIT);
	    if(n==-1) {
		++ep.err;
	    }
	    else {
		++ep.tx;
	    }
	}
    }

#ifdef HAS_MMSG
    /**
     * The (blocking) UDP socket has become readable, and we're
     * supposed to reflect at least /some/ of whatever is there, and
     * update the statistics.
     */
    void reflect(Endpoint& ep)
    {
	const int fd = ep.fd;
	static Msg msg[5];
	mmsghdr mm[5];
	for(int i=0; i<5; i++) {
	    mm[i] = msg[i].hdr_of();
	}

	const int n = recvmmsg(fd, mm, 5, MSG_WAITFORONE | MSG_TRUNC, 0);
	if(n==-1) {
	    ++ep.err;
	    return;
	}
	for(int i=0; i<n; i++) {
	    msghdr& h = mm[i].msg_hdr;
	    unsigned len = mm[i].msg_len;
	    h.msg_iov[0].iov_len = len;
	    reflect(ep, h, len);
	}
    }
#else
    void reflect(Endpoint& ep)
    {
	const int fd = ep.fd;
	static Msg msg;
	msghdr h = msg.hdr_of();

	const ssize_t n = recvmsg(fd, &h, MSG_TRUNC);
	if(n==-1) {
	    ++ep.err;
	    return;
	}

	unsigned len = n;
	h.msg_iov[0].iov_len = len;
	reflect(ep, h, len);
    }
#endif


    int udpserver(const std::string& control,
		  const std::vector<std::string>& sockets,
		  const bool verbose)
    {
	const int efd = epoll_create(1);
	assert(efd > 0);

	int cfd = -1;
	if(!control.empty()) {

	    const Addrinfo cai(control);
	    if(cai.err) {
		std::cerr << "error: cannot open control socket: "
			  << cai.strerror() << '\n';
		return 1;
	    }

	    cfd = cai.socket();
	    if(cfd==-1) {
		std::cerr << "error: cannot open control socket: "
			  << strerror(errno) << '\n';
		return 1;
	    }

	    epoll_add(efd, cfd, ~0u);
	}

	std::vector<Endpoint> ep;

	for(std::vector<std::string>::const_iterator i = sockets.begin();
	    i!= sockets.end(); i++) {
	    const Addrinfo ai(*i);
	    if(ai.err) {
		std::cerr << *i << ": error: cannot open socket: "
			  << ai.strerror() << '\n';
		return 1;
	    }

	    int fd = ai.socket();
	    if(fd==-1) {
		std::cerr << *i << ": error: cannot open socket: "
			  << strerror(errno) << '\n';
		return 1;
	    }

	    ep.push_back(Endpoint(*i, fd));
	    epoll_add(efd, fd, ep.size()-1);

	    if(verbose) {
		std::cout << *i << " on fd " << fd << '\n';
	    }
	}

	bool die = false;
	while(!die) {
	    struct epoll_event ev[10];
	    const int n = epoll_wait(efd, ev, 10, -1);

	    if(n==-1 && errno==EINTR) {
		continue;
	    }
	    if(n<1) {
		die = true;
		continue;
	    }

	    for(int i=0; i<n; i++) {
		const unsigned index = ev[i].data.u32;
		if(index==~0u) {
		    die = !controlmsg(cfd, ep);
		    if(die) {
			break;
		    }
		    else {
			continue;
		    }
		}
		Endpoint& e = ep[index];
		reflect(e);
	    }
	}

	/* should clean up, I suppose ... */

	return 0;
    }
}


int main(int argc, char ** argv)
{
    using std::string;

    const string prog = argv[0];
    const string usage = string("usage: ")
	+ prog
	+ " [-v] [--control port] [host:]port ...";
    const char optstring[] = "vhc:";
    struct option long_options[] = {
	{"version", 0, 0, 'V'},
	{"help", 0, 0, 'h'},
	{"control", 1, 0, 'c'},
	{0, 0, 0, 0}
    };

    bool verbose = false;
    string control;

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
	case 'c':
	    control = optarg;
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

    if(control.empty() && argc - optind == 0) {
	/* neither control port nor any traffic ports */
	std::cerr << usage << '\n';
	return 1;
    }

    const std::vector<string> sockets(argv + optind,
				      argv + argc);

    return udpserver(control, sockets, verbose);
}
