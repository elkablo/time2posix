OBJS	=		\
	time2posix.o	\
	time.o		\
	utmp.o

DESTDIR ?=
PREFIX ?= /usr/local
LIBDIR ?= lib

CC ?= gcc
CFLAGS = -O2 -fPIC -pipe
LDFLAGS = -ldl

all: time2posix.so ntpd ntpdate time2posix

install: all
	install -d $(DESTDIR)$(PREFIX)/$(LIBDIR)
	install -t $(DESTDIR)$(PREFIX)/$(LIBDIR) time2posix.so
	install -d $(DESTDIR)$(PREFIX)/bin
	install -t $(DESTDIR)$(PREFIX)/bin time2posix
	install -d $(DESTDIR)$(PREFIX)/sbin
	install -t $(DESTDIR)$(PREFIX)/sbin ntpd ntpdate

time2posix: time2posix.in
	sed -e 's|@prefix@|$(PREFIX)|' -e 's|@libdir@|$(LIBDIR)|' time2posix.in >time2posix
	chmod +x time2posix

ntpd:
	echo -e '#!/bin/sh\n\nexec $(PREFIX)/bin/time2posix /usr/sbin/ntpd "$$@"' >ntpd
	chmod +x ntpd

ntpdate:
	echo -e '#!/bin/sh\n\nexec $(PREFIX)/bin/time2posix /usr/sbin/ntpdate "$$@"' >ntpdate
	chmod +x ntpdate

time2posix.so: $(OBJS)
	$(CC) $(LDFLAGS) -shared -o time2posix.so $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) time2posix.so ntpd ntpdate time2posix t2p_test

t2p_test: t2p_test.c
	$(CC) $(CFLAGS) -Wl,-rpath,$$(pwd) -L$$(pwd) time2posix.so -o t2p_test t2p_test.c

test: time2posix.so t2p_test
	./t2p_test
