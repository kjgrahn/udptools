/*
 * ethercat -- Ethernet frame generator
 *
 * Copyright (c) 2012 Jörgen Grahn
 * All rights reserved.
 *
 */
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>

#include <getopt.h>
#include <pcap/pcap.h>
#include <string.h>
#include <errno.h>

#include "hexread.h"

/*

 ----------
  6  h_dest
  6  h_source
  2 h_proto
 ----------
 14

 ----------
  1  version+ihl
  1  tos
  2  total_len
  2  id
  2  frag_off
  1  ttl
  1  proto
  2  checksum
  4  source
  4  dest
 ----------
 20

 */


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
    const char* a = fbuf;
    const char* b = a + strlen(a);
    const size_t n = hexread(buf, &a, b);

    if(a!=b) {
	fprintf(stderr, "error: line %d: unexpected character '%c'\n",
		lineno, *a);
	return 0;
    }

    return n;
}


/**
 * Read hex from 'in' and pcap_inject() until EOF. Will log parse
 * errors and I/O errors meanwhile.  Returns an exit code.
 */
static int ethercat(FILE* in, pcap_t* pcap)
{
    int lineno = 0;
    int s;
    uint8_t buf[10000];
    unsigned acc = 0;
    unsigned eacc = 0;

    while((s = hexline(in, ++lineno, buf)) != -1) {

	int n = pcap_inject(pcap, buf, s);
	if(n < s) {
	    fprintf(stderr, "warning: line %d: sending caused %s\n",
		    lineno, pcap_geterr(pcap));
	    eacc++;
	}
	acc++;
    }
    fprintf(stdout, "got %u packets to pcap-inject; %u whined about errors\n",
	    acc, eacc);
    return eacc!=0;
}


int main(int argc, char ** argv)
{
    const char* const prog = argv[0];
    char usage[500];
    sprintf(usage, "usage: %s -i interface", prog);
    const char optstring[] = "+i";
    struct option long_options[] = {
	{"version", 0, 0, 'v'},
	{"help", 0, 0, 'h'},
	{0, 0, 0, 0}
    };

    const char* iface = 0;

    int ch;
    while((ch = getopt_long(argc, argv,
			    optstring, &long_options[0], 0)) != -1) {
	switch(ch) {
	case 'i':
	    iface = optarg;
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

    if(!iface) {
	fprintf(stderr,
		"error: required argument missing.\n"
		"%s\n", usage);
	return 1;
    }

    if(argc - optind != 0) {
	fprintf(stderr, "%s\n", usage);
	return 1;
    }

    char err[PCAP_ERRBUF_SIZE];
    strcpy(err, "");
    pcap_t* pcap = pcap_open_live(iface, 65, 0, 0, err);
    if(strlen(err)) {
	fprintf(stderr, "%s: %s\n", prog, err);
    }
    if(!pcap) {
	return 1;
    }

    return ethercat(stdin, pcap);
}
