OBJS	=	\
	time2posix.o

CC ?= gcc
CFLAGS = -O2 -fPIC -pipe
LDFLAGS = -ldl

all: time2posix.so

time2posix.so: $(OBJS)
	$(CC) $(LDFLAGS) -shared -o time2posix.so $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) time2posix.so

test: time2posix.so
	LD_PRELOAD=$$(pwd)/time2posix.so date
