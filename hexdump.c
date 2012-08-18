/*
 * The ultimate hexdump implementation, independent from
 * I/O mechanisms and long buffers.
 *
 * Copyright (c) 2012 Jörgen Grahn
 * All rights reserved.
 *
 */
#include "hexdump.h"

#include <stdint.h>


static char nybble(uint8_t n)
{
    if(n<10) return '0' + n;
    return 'a' + (n-10);
}


static const uint8_t* dump(char* buf, size_t size,
			   const uint8_t* const begin,
			   const uint8_t* const end)
{
    const uint8_t* p = begin;

    while(size>2 && p!=end) {
	if(p!=begin) {
	    *buf++ = ' ';
	    size--;
	}
	*buf++ = nybble((*p) >> 4);
	*buf++ = nybble((*p) & 0x0f);
	size -= 2;
	p++;
    }
    /* buggy */
    *buf++ = '\0';
    return p;
}


/**
 * Format [begin .. end) as hex octets, as much of it
 * which fits in 'buf', of size 'size'.  The NUL character is
 * included in the calculations.
 *
 * Returns the first octet not formatted.
 *
 * This way, you can e.g. specify a buffer of 3*8 characters,
 * loop until hexdump() returns 'end', printing the buffer + '\n'
 * at every iteration -- and you have a hexdump with 8 octets
 * per line.
 *
 */
const void* hexdump(char* buf, size_t size,
		    const void* begin, const void* end)
{
    return dump(buf, size, begin, end);
}
