time2posix
==========

POSIX timestamps compatibility library for glibc/linux system which uses *right timestamp* (with leap seconds)

# WTF?

time2posix is a compatibility library for systems which use *right timestamp* on system clock
instead of Unix (POSIX) timestamp.

I primarily wrote it to hack ntpdate/ntpd to think that system clock is in Unix timestamp, so when
I'm synchronizing time from a classic NTP server, my clock is synchronized to right timestamp.

In future, it could also be used on programs that expect Unix timestamp (for example PostgreSQL cannot handle
timezones with leap seconds). For that, more system calls which takes time_t arguments must be implemented
(for example utime(2), stat(2) and so on).

## How to use?

With the LD_PRELOAD environment variable one can override glibc functions with get or set system time by
the ones provided in time2posix.

There are two scripts for ntpd and ntpdate which call original ntpd/ntpdate with preloaded time2posix.so library.

First, install time2posix into /usr/local (so it won't override ntpd/ntpdate which should be in /usr):

    git clone https://github.com/elkablo/time2posix.git
    cd time2posix
    make
    make install

Then set your system timezone to a right timezone, for example Europe/Prague:

    TZ=right/Europe/Prague
    echo $TZ >/etc/timezone
    rm /etc/localtime
    cp /usr/share/zoneinfo/$TZ /etc/localtime

Finally, synchronize with preloaded ntpdate:

    /usr/local/sbin/ntpdate -b 0.pool.ntp.org

# Why?

## Unix timestamp is not continuous

When a leap second occurs, there is a discontinuity in Unix timestamp to keep it in line with UTC.
This discontinuity can reflect on your system clock in several ways:

- stepping one second back when a leap second insertion occurs
- stopping for one second when a leap second insertion occurs
- slowing down for a while when a leap second insertion occurs
- skipping one second when a leap second deletion occurs
- speeding up for a while when a leap second deletion occurs

(Although leap second deletion didn't occur in the 40 years of leap seconds usage).

Because of this there are points in time with no proper representation in Unix timestamp.
For example, the leap second inserted between 23:59:59 June 30, 2012 and 00:00:00 July 1, 2012
-- written as 23:59:60 June 30, 2012 -- has no representation. The first one is 1341100799, the
second one is 1341100800.

Modern NTP synchronized kernels do not step/stop/skip when a leap second occurs, instead
they speed up or slow down for a while and the leap second accumulates slowly. The clock
is always increasing and so it shouldn't pose a problem for majority of uses.

## What is "right time"?

IANA timezone database, which is used virtually by every operating system for converting
time from/to human readable form, contains "right" timezones. These timezones work with
a slightly different definition of Unix timestamp (breaking compatibility with Unix),
they expect time_t to represent **right timestamp**, that is the number of seconds that have
elapsed since 00:00:00 UTC, Thursday, 1 January 1970, **couting leap seconds** (instead of
not counting leap seconds).

As of February 2014, right timestamp is 25 seconds larger than Unix timestamp.

See http://www.ucolick.org/~sla/leapsecs/right+gps.html for more information about why
this definition brekas Unix expectations.
