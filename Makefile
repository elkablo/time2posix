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

all: time2posix.so ntpd ntpdate

install: all
	install -d $(DESTDIR)$(PREFIX)/$(LIBDIR)
	install -t $(DESTDIR)$(PREFIX)/$(LIBDIR) time2posix.so
	install -d $(DESTDIR)$(PREFIX)/sbin
	install -t $(DESTDIR)$(PREFIX)/sbin ntpd ntpdate

ntpd: ntpd.in
	sed -e 's|@prefix@|$(PREFIX)|' -e 's|@libdir@|$(LIBDIR)|' ntpd.in >ntpd
	chmod +x ntpd

ntpdate: ntpdate.in
	sed -e 's|@prefix@|$(PREFIX)|' -e 's|@libdir@|$(LIBDIR)|' ntpdate.in >ntpdate
	chmod +x ntpdate

time2posix.so: $(OBJS)
	$(CC) $(LDFLAGS) -shared -o time2posix.so $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) time2posix.so ntpd ntpdate t2p_test

t2p_test: t2p_test.c
	$(CC) $(CFLAGS) -Wl,-rpath,$$(pwd) -L$$(pwd) time2posix.so -o t2p_test t2p_test.c

test: time2posix.so t2p_test
	./t2p_test
