OBJS	=	\
	time2posix.o

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
	rm -f $(OBJS) time2posix.so ntpd ntpdate

test: time2posix.so
	LD_PRELOAD=$$(pwd)/time2posix.so date
