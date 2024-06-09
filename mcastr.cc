/*
 * Copyright (c) 2024 J�rgen Grahn
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
#include <arpa/inet.h>

#include "hexread.h"

namespace {

    bool operator! (const in_addr& addr)
    {
	return !addr.s_addr;
    }

    /* Resolve an UDPv4 "server" at a fixed port. Returns only the
     * first suggestion, and leaks memory.
     */
    addrinfo resolve(std::ostream& err,
		     const std::string& port)
    {
	addrinfo hints = {};
	hints.ai_flags = AI_ADDRCONFIG | AI_PASSIVE;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	addrinfo* res;
	int rc = getaddrinfo(nullptr, port.c_str(),
			     &hints, &res);
	if (rc) {
	    err << "error: " << port
		<< ": " << gai_strerror(rc) << '\n';
	    return {};
	}

	return res[0];
    }

    in_addr pton(const std::string& s)
    {
	in_addr addr {};
	inet_pton(AF_INET, s.c_str(), &addr);
	return addr;
    }

    bool add_membership(int fd, const in_addr& addr)
    {
	const ip_mreqn req {addr, {}, 0};
	int err = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			     &req, sizeof req);
	return !err;
    }

    /* The core of the program, minus command-line parsing.
     * Leaks resources, which is fine since we'll exit afterwards.
     */
    template <class Arg>
    bool work(std::ostream& out,
	      std::ostream& err,
	      const Arg arg)
    {
	auto error = [&err] (const char* s) {
	    err << "error: " << s << ": " << std::strerror(errno) << '\n';
	    return false;
	};

	const addrinfo ai = resolve(err, arg.port);
	if (!ai.ai_addr) return false;

	const in_addr group = pton(arg.group);
	if (!group) {
	    err << "error: bad group " << arg.group << '\n';
	    return false;
	}

	const int fd = socket(ai.ai_family, ai.ai_socktype, ai.ai_protocol);
	if (fd==-1) return error("cannot open socket");

	if (ai.ai_addr && bind(fd, ai.ai_addr, ai.ai_addrlen) == -1) {
	    return error("cannot bind");
	}

	if (!add_membership(fd, group)) return error("IP_ADD_MEMBERSHIP");

	while (1) {
	    char buf[1];
	    auto rc = recv(fd, buf, sizeof buf, 0);
	    char ch = (rc==-1) ? 'e' : '.';
	    out.put(ch).flush();
	}
    }
}

int main(int argc, char ** argv)
{
    const std::string prog = argv[0] ? argv[0] : "mcastr";
    const std::string usage = "usage: "
	+ prog +
	" group port\n"
	"       "
	+ prog + " --help\n" +
	"       "
	+ prog + " --version";
    const char optstring[] = "";
    const struct option long_options[] = {
	{"help",	 0, 0, 'h'},
	{"version",	 0, 0, 'v'},
	{0, 0, 0, 0}
    };

    struct {
	std::string group;
	std::string port;
    } arg;

    int ch;
    while ((ch = getopt_long(argc, argv,
			     optstring, &long_options[0], 0)) != -1) {
	switch(ch) {
	case 'h':
	    std::cout << usage << '\n';
	    return 0;
	case 'v':
	    std::cout << prog << " 1.0\n"
		      << "Copyright (c) 2024 J�rgen Grahn\n";
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

    arg.group = argv[optind];
    arg.port = argv[optind+1];

    if (!work(std::cout, std::cerr, arg)) return 1;

    return 0;
}
