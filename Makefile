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
	$(RM) udpdiscard udpecho udppump udpcat
	$(RM) ethercat
	$(RM) *.o *.a Makefile.bak core TAGS

.PHONY: check checkv
check: tests
	./tests
checkv: tests
	valgrind -q ./tests -v

CXXFLAGS=-Wall -Wextra -pedantic -Wold-style-cast -std=c++98 -g -Os
CFLAGS=-Wall -Wextra -pedantic -std=c99 -g -Os

all: udpdiscard
all: udpecho
all: udppump
all: udpcat
all: ethercat

udpdiscard: udpdiscard.o libudptools.a
	$(CXX) $(CXXFLAGS) -L. -o $@ $< -l udptools
udpecho: udpecho.o libudptools.a
	$(CXX) $(CXXFLAGS) -L. -o $@ $< -l udptools
udppump: udppump.o libudptools.a
	$(CXX) $(CXXFLAGS) -L. -o $@ $< -l udptools
udpcat: udpcat.o libudptools.a
	$(CXX) $(CXXFLAGS) -L. -o $@ $< -l udptools
ethercat: ethercat.o libudptools.a
	$(CXX) $(CXXFLAGS) -L. -o $@ $< -l udptools -lpcap

udpcat.o: CPPFLAGS=-D_POSIX_SOURCE
ethercat.o: CPPFLAGS=-D_BSD_SOURCE
udpdiscard.o: CXXFLAGS+=-Wno-old-style-cast
udpecho.o: CXXFLAGS+=-Wno-old-style-cast

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

ethercat.o: hexread.h
hexdump.o: hexdump.h
hexread.o: hexread.h
udpcat.o: hexread.h
test/hexdump.o: hexdump.h
test/hexread.o: hexread.h
