# $Id: Makefile,v 1.2 2011-11-30 00:35:22 grahn Exp $
#
# Makefile
#
# Copyright (c) 2011 Jörgen Grahn.
# All rights reserved.

SHELL=/bin/bash
INSTALLBASE=/usr/local

.PHONY: all
all: udpdiscard
all: udppump
all: udpcat

# .PHONY: install
# install: leakyqueue.cc
# install: leakyqueue.h
# 	install -m644 $^ $(INSTALLBASE)/lib

.PHONY: clean
clean:
	$(RM) udpdiscard udppump udpcat
	$(RM) *.o Makefile.bak core TAGS

.PHONY: check checkv
check: tests
	./tests
checkv: tests
	valgrind -q ./tests -v

CXXFLAGS=-Wall -Wextra -pedantic -Wold-style-cast -std=c++98 -g -Os
CFLAGS=-Wall -Wextra -pedantic -std=c99 -g -Os

all: udpdiscard
all: udppump
all: udpcat

udpdiscard: udpdiscard.o
	$(CXX) $(CXXFLAGS) -o $@ $^
udppump: udppump.o
	$(CXX) $(CXXFLAGS) -o $@ $^
udpcat: udpcat.o
	$(CXX) $(CXXFLAGS) -o $@ $^

udpcat.o: CPPFLAGS=-D_POSIX_SOURCE

libudptools.a: hexdump.o
libudptools.a: hexread.o
	$(AR) -r $@ $^


test.cc: libtest.a
	testicle -o$@ $^

tests: test.o libtest.a libudptools.a
	$(CXX) $(CXXFLAGS) -o $@ test.o -L. -ltest -ludptools

libtest.a: test/hexread.o
libtest.a: test/hexdump.o
	$(AR) -r $@ $^

test/%.o : CPPFLAGS+=-I.


.PHONY: tags
tags: TAGS
TAGS:
	etags *.cc *.h

depend:
	makedepend -- -I. $(CFLAGS) -- -Y *.cc *.c test/*.cc

love:
	@echo "not war?"

# DO NOT DELETE

hexdump.o: hexdump.h
hexread.o: hexread.h
test/hexdump.o: hexdump.h
test/hexread.o: hexread.h
