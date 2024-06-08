/*
 * Copyright (c) 2012 Jörgen Grahn
 * All rights reserved.
 *
 */
#include <hexdump.h>

#include <orchis.h>
#include <string>
#include <cassert>


namespace {

    const char data[] =
	"Foo bar baz bat\n"
	"FOO BAR BAZ BAT\n"
	"foo bar baz bat\n"
	"barney";
    const size_t datalen = sizeof data - 1;

    /**
     * Wrapped version of the C function under test.
     */
    const void* hexdump(std::string& buf, size_t count,
			const void* begin, const void* end)
    {
	/* to please Valgrind */
	char* cbuf = new char[count];
	const void* p = ::hexdump(cbuf, count, begin, end);
	buf = std::string(cbuf);
	delete[] cbuf;
	return p;
    }
}


namespace hex {

    using orchis::assert_eq;

    void test_nil()
    {
	std::string s;
	assert_eq(hexdump(s, 1, data, data), data);
	assert_eq(s, "");
    }

    void test_simple()
    {
	std::string s;
	const void * p = data;
	p = hexdump(s, 3*8, p, data+datalen);
	assert_eq(s, "46 6f 6f 20 62 61 72 20");
	assert_eq(p, data+8);
    }

    void test_almost_enough()
    {
	std::string s;
	const void * p = data;
	p = hexdump(s, 3*8-1, p, data+datalen);
	assert_eq(s, "46 6f 6f 20 62 61 72");
	assert_eq(p, data+7);
    }

    void test_lines()
    {
	std::string s;
	const void * p = data;
	p = hexdump(s, 3*16, p, data+datalen);
	assert_eq(s,
		  "46 6f 6f 20 62 61 72 20 "
		  "62 61 7a 20 62 61 74 0a");
	p = hexdump(s, 3*16, p, data+datalen);
	assert_eq(s,
		  "46 4f 4f 20 42 41 52 20 "
		  "42 41 5a 20 42 41 54 0a");
	p = hexdump(s, 3*16, p, data+datalen);
	assert_eq(s,
		  "66 6f 6f 20 62 61 72 20 "
		  "62 61 7a 20 62 61 74 0a");
	p = hexdump(s, 3*16, p, data+datalen);
	assert_eq(s, "62 61 72 6e 65 79");
	assert_eq(p, data+datalen);

	p = hexdump(s, 3*16, p, data+datalen);
	assert_eq(s, "");
	assert_eq(p, data+datalen);
    }
}
