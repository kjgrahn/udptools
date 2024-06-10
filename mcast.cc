/*
 * Copyright (c) 2024 Jörgen Grahn
 * All rights reserved.
 *
 */
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

#include <getopt.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "hexread.h"

namespace {

    /* Resolve an UDPv4 host:port destination. Returns only the first
     * suggestion, and leaks memory (cannot freeaddrinfo because a
     * single addrinfo contains pointers into that memory).
     */
    addrinfo resolve(std::ostream& err,
		     const std::string& host,
		     const std::string& port)
    {
	addrinfo hints = {};
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	addrinfo* res;
	int rc = getaddrinfo(host.c_str(), port.c_str(),
			     &hints, &res);
	if (rc) {
	    err << "error: " << host << ':' << port
		<< ": " << gai_strerror(rc) << '\n';
	    return {};
	}

	return res[0];
    }

    /* Resolve an UDPv4 host source (the port will be dynamically
     * allocated in the end). Returns only the first suggestion, and
     * leaks memory.
     */
    addrinfo resolve(std::ostream& err,
		     const std::string& host)
    {
	addrinfo hints = {};
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	addrinfo* res;
	int rc = getaddrinfo(host.c_str(), nullptr,
			     &hints, &res);
	if (rc) {
	    err << "error: " << host
		<< ": " << gai_strerror(rc) << '\n';
	    return {};
	}

	return res[0];
    }

    bool mcast_ttl(int fd, const std::string& s)
    {
	unsigned char uttl = std::strtoul(s.c_str(), nullptr, 10);
	int ttl = std::strtol(s.c_str(), nullptr, 10);
	int err;
	/* OpenBSD has u_char as parameter, while Linux has int. A
	 * sign, perhaps, that nobody takes multicast seriously enough
	 * to maintain a portable API. Try both.
	 */
	err = setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL,
			 &uttl, sizeof uttl);
	if (!err || errno!=EINVAL) return !err;
	err = setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL,
			 &ttl, sizeof ttl);
	return !err;
    }

    in_addr in4_of(const sockaddr& sa)
    {
	const auto p = reinterpret_cast<const sockaddr_in*>(&sa);
	return p->sin_addr;
    }

    bool add_membership(int fd, const sockaddr& addr)
    {
	const ip_mreqn req {in4_of(addr), {}, 0};
	int err = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			     &req, sizeof req);
	return !err;
    }

    unsigned atoi(const std::string& s, unsigned def)
    {
	if (s.empty()) return def;
	return std::strtoul(s.c_str(), nullptr, 10);
    }

    /* Wrapper around the other hexread().
     */
    std::vector<uint8_t> hexread(const std::string& s)
    {
	std::vector<uint8_t> v(s.size()/2);
	auto a = s.data();
	auto b = a + s.size();
	size_t n = ::hexread(&v[0], &a, b);
	v.resize(n);
	return v;
    }

    /* Wrapper around send/sendto, with support for connected sockets
     * and duplication.  Doesn't check for write errors.
     */
    struct Tx {
	Tx(int fd, const addrinfo dst, bool connected, const std::string& dup)
	    : fd {fd},
	      dst {dst},
	      connected {connected},
	      dup {atoi(dup, 1)}
	{}

	void operator() (const uint8_t* buf, size_t len) const;

	const int fd;
	const addrinfo dst;
	const bool connected;
	const unsigned dup;
    };

    void Tx::operator() (const uint8_t* buf, size_t len) const
    {
	if (connected) {
	    for (unsigned i=0; i < dup; i++) {
		send(fd, buf, len, 0);
	    }
	}
	else {
	    for (unsigned i=0; i < dup; i++) {
		sendto(fd, buf, len, 0,
		       dst.ai_addr, dst.ai_addrlen);
	    }
	}
    }

    /* The I/O loop for the case where the user supplies datagrams in
     * hex.
     */
    bool transmit_hex(const Tx& tx, std::istream& is)
    {
	std::string s;
	while(std::getline(is, s)) {
	    std::vector<uint8_t> v = hexread(s);
	    tx(v.data(), v.size());
	}
	return true;
    }

    /* The I/O loop for the case where the user supplies datagrams
     * which are lines of text (without \n).
     */
    bool transmit(const Tx& tx, std::istream& is)
    {
	std::string s;
	while(std::getline(is, s)) {
	    const auto p = reinterpret_cast<const uint8_t*>(s.data());
	    tx(p, s.size());
	}
	return true;
    }

    /* The core of the program, minus command-line parsing.
     * Leaks resources, which is fine since we'll exit afterwards.
     */
    template <class Arg>
    bool work(std::istream& is,
	      std::ostream& err,
	      const Arg arg)
    {
	auto error = [&err] (const char* s) {
	    err << "error: " << s << ": " << std::strerror(errno) << '\n';
	    return false;
	};

	const addrinfo ai = resolve(err, arg.dst.host, arg.dst.port);
	if (!ai.ai_addr) return false;

	addrinfo si = {};
	if (arg.bind.size()) {
	    si = resolve(err, arg.bind);
	    if (!si.ai_addr) return false;
	}

	const int fd = socket(ai.ai_family, ai.ai_socktype, ai.ai_protocol);
	if (fd==-1) return error("cannot open socket");

	if (si.ai_addr && bind(fd, si.ai_addr, si.ai_addrlen) == -1) {
	    return error("cannot bind");
	}

	if (arg.ttl.size() && !mcast_ttl(fd, arg.ttl)) {
	    return error("cannot set IP_MULTICAST_TTL");
	}

	if (arg.dst.connect && connect(fd, ai.ai_addr, ai.ai_addrlen) == -1) {
	    return error("cannot connect");
	}

	if (arg.join && !add_membership(fd, *ai.ai_addr)) {
	    return error("IP_ADD_MEMBERSHIP");
	}

	const Tx tx {fd, ai, arg.dst.connect, arg.dup};

	if (arg.hex) {
	    return transmit_hex(tx, is);
	}
	else {
	    return transmit(tx, is);
	}
    }
}

int main(int argc, char ** argv)
{
    const std::string prog = argv[0] ? argv[0] : "mcast";
    const std::string usage = "usage: "
	+ prog +
	" [-a] [-d N] [--ttl N] [--connect] [--join] [-s source]"
	" addr port\n"
	"       "
	+ prog + " --help\n" +
	"       "
	+ prog + " --version";
    const char optstring[] = "ad:s:";
    const struct option long_options[] = {
	{"ttl",		 1, 0, 'T'},
	{"connect",	 0, 0, 'C'},
	{"join",	 0, 0, 'J'},
	{"help",	 0, 0, 'h'},
	{"version",	 0, 0, 'v'},
	{0, 0, 0, 0}
    };

    struct {
	bool hex = true;
	std::string dup;
	std::string bind;
	std::string ttl;
	bool join = false;
	struct {
	    bool connect = false;
	    std::string host;
	    std::string port;
	} dst;
    } arg;

    int ch;
    while ((ch = getopt_long(argc, argv,
			     optstring, &long_options[0], 0)) != -1) {
	switch(ch) {
	case 'a': arg.hex = false; break;
	case 'd': arg.dup = optarg; break;
	case 'T': arg.ttl = optarg; break;
	case 'C': arg.dst.connect = true; break;
	case 'J': arg.join = true; break;
	case 's': arg.bind = optarg; break;
	case 'h':
	    std::cout << usage << '\n';
	    return 0;
	case 'v':
	    std::cout << prog << " 1.0\n"
		      << "Copyright (c) 2024 Jörgen Grahn\n";
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

    if (optind + 2 != argc) {
	std::cerr << "error: required argument missing\n"
		  << usage << '\n';
	return 1;
    }

    arg.dst.host = argv[optind];
    arg.dst.port = argv[optind+1];

    if (!work(std::cin, std::cerr, arg)) return 1;

    return 0;
}
