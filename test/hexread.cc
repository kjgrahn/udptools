/*
 * Copyright (c) 2012 Jörgen Grahn
 * All rights reserved.
 *
 */
#include <hexread.h>

#include <testicle.h>
#include <vector>
#include <cstring>


namespace {

    typedef std::vector<uint8_t> octets;


    /* For convenience.
     */
    octets hexread(const char** begin, const char* end)
    {
	uint8_t buf[1000];
	size_t n = ::hexread(buf, begin, end);
	return octets(buf, buf+n);
    }
}


namespace hexr {

    using testicle::assert_eq;

    void test_nil()
    {
	const char s[] = "";
	const char* a = s;
	const char* b = a;
	octets cc = hexread(&a, b);
	assert_eq(cc.size(), 0);
	assert_eq(a, b);
    }

    void test_simple()
    {
	const char s[] = "feedc0edbabe";
	const char* a = s;
	const char* b = a+2;
	octets cc;

	cc = hexread(&a, b);
	assert_eq(cc.size(), 1);
	assert_eq(cc[0], 0xfe);
	assert_eq(a, b);

	a = s;
	b = a+6;
	cc = hexread(&a, b);
	assert_eq(cc.size(), 3);
	assert_eq(cc[0], 0xfe);
	assert_eq(cc[1], 0xed);
	assert_eq(cc[2], 0xc0);
	assert_eq(a, b);

	a = s;
	b = a + std::strlen(a);
	cc = hexread(&a, b);
	assert_eq(cc.size(), 6);
	assert_eq(cc[5], 0xbe);
	assert_eq(a, b);
    }

    void test_whitespace()
    {
	const char s[] = "  01 0f 0e   z";
	const char* a = s;
	const char* b = a + std::strlen(a);
	octets cc = hexread(&a, b);
	assert_eq(cc.size(), 3);
	assert_eq(*a, 'z');
    }

    void test_whitespace2()
    {
	const char s[] = "01ff 0e3 6777";
	const char* a = s;
	const char* b = a + std::strlen(a);
	octets cc = hexread(&a, b);
	assert_eq(cc.size(), 3);
	assert_eq(cc[0], 0x01);
	assert_eq(cc[1], 0xff);
	assert_eq(cc[2], 0x0e);
	assert_eq(*a, '3');
    }
}
