/* $Id: udppump.cc,v 1.1 2008-09-24 19:56:21 grahn Exp $
 *
 * udppump.cc -- udp load generator
 *
 * Copyright (c) 2008 Jörgen Grahn
 * All rights reserved.
 *
 */
#include <string>
#include <iostream>
#include <ostream>

#include <getopt.h>


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

    return 0;
}
