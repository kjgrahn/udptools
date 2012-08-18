/*
 * Copyright (c) 2012 Jörgen Grahn.
 * All rights reserved.
 *
 * The ultimate hexdump implementation, independent from
 * I/O mechanisms and long buffers.
 */
#ifndef UDPTOOLS_HEXDUMP_H
#define UDPTOOLS_HEXDUMP_H
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

const void* hexdump(char* buf, size_t count,
		    const void* begin, const void* end);

#ifdef __cplusplus
}
#endif
#endif
