#
# Makefile
#
# Copyright (c) 2011, 2024 Jörgen Grahn.
# All rights reserved.

SHELL=/bin/bash
INSTALLBASE=/usr/local
CXXFLAGS=-Wall -Wextra -pedantic -std=c++17 -g -Os -Wold-style-cast
CFLAGS=-Wall -Wextra -pedantic -std=c99 -g -Os
CPPFLAGS=
ARFLAGS=rTP

.PHONY: all
all: udpdiscard
all: udpecho
all: udppump
all: udpcat
all: ipcat
all: ethercat
all: tests

.PHONY: install
install: udpcat udpcat.1
install:  ipcat  ipcat.1
	install -d $(INSTALLBASE)/{bin,man/man1}
	install -m755 {udp,ip}cat   $(INSTALLBASE)/bin/
	install -m644 {udp,ip}cat.1 $(INSTALLBASE)/man/man1/

.PHONY: clean
clean:
	$(RM) udpdiscard udpecho udppump udpcat
	$(RM) ethercat
	$(RM) {,test/}*.o
	$(RM) lib*.a
	$(RM) test.cc tests
	$(RM) TAGS
	$(RM) -r dep/

.PHONY: check checkv
check: tests
	./tests
checkv: tests
	valgrind -q ./tests -v

udpdiscard: udpdiscard.o libudptools.a
	$(CXX) $(CXXFLAGS) -L. -o $@ $< -l udptools
udpecho: udpecho.o libudptools.a
	$(CXX) $(CXXFLAGS) -L. -o $@ $< -l udptools
udppump: udppump.o libudptools.a
	$(CXX) $(CXXFLAGS) -L. -o $@ $< -l udptools
udpcat: udpcat.o libudptools.a
	$(CC) $(CFLAGS) -L. -o $@ $< -l udptools
ipcat: ipcat.o libudptools.a
	$(CC) $(CFLAGS) -L. -o $@ $< -l udptools
ethercat: ethercat.o libudptools.a
	$(CC) $(CFLAGS) -L. -o $@ $< -l udptools -lpcap

udpcat.o: CFLAGS+=-std=gnu99
ipcat.o: CFLAGS+=-std=gnu99
ethercat.o: CFLAGS+=-std=gnu99
udpdiscard.o: CXXFLAGS+=-Wno-old-style-cast
udpecho.o: CXXFLAGS+=-Wno-old-style-cast

libudptools.a: hexdump.o
libudptools.a: hexread.o
	$(AR) $(ARFLAGS) $@ $^

test.cc: libtest.a
	orchis -o$@ $^

tests: test.o libtest.a libudptools.a
	$(CXX) $(CXXFLAGS) -o $@ $< -L. -ltest -ludptools

libtest.a: test/hexread.o
libtest.a: test/hexdump.o
	$(AR) $(ARFLAGS) $@ $^

test/%.o : CPPFLAGS+=-I.

.PHONY: tags
tags: TAGS
TAGS:
	ctags --exclude='test' -eR . --extra=q

love:
	@echo "not war?"

# DO NOT DELETE

$(shell mkdir -p dep/test)
DEPFLAGS=-MT $@ -MMD -MP -MF dep/$*.Td
COMPILE.cc=$(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
COMPILE.c=$(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c

%.o: %.cc
	$(COMPILE.cc) $(OUTPUT_OPTION) $<
	@mv dep/$*.{Td,d}

dep/%.d: ;
dep/test/%.d: ;
-include dep/*.d
-include dep/test/*.d
