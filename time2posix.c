/* 2014 by Marek Behun <kabel@blackhole.sk>
   This file is in public domain */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <utmp.h>
#include <utmpx.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <sys/mman.h>
#include <endian.h>

#define RIGHT_TZ "right/UTC"
#define LEAP_TZFILE ("/usr/share/zoneinfo/" RIGHT_TZ)

struct leapsecond
{
  /* 0 for leap second deletion, 1 for insertion */
  int type;

  /* time of the transition */
  time_t transition;

  /* posix version of the previous */
  time_t posix_transition;

  /* start of the day when transition will occur */
  time_t daystart;

  /* posix version of the previous */
  time_t posix_daystart;

  /* difference between right and posix time just after this leap second */
  int change;

  /* difference between right and posix time just before this leap second */
  int prev_change;
};

static struct leapsecond *leapsecs = NULL;
static size_t leapsecs_num = 0;

/* read the leap seconds table */
static int
leaps_read (void)
{
  int fd, prev_change;
  const char *buf;
  const u_int32_t *lbuf;
  u_int32_t n, skip, i;
  struct leapsecond *ptr;

  /* does O_NONBLOCK need to be here if we are mmapping? */
  fd = open (LEAP_TZFILE, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0)
    return -1;

  /* right/UTC shouldn't be bigger than 4096B */
  buf = mmap (NULL, 4096, PROT_READ, MAP_PRIVATE, fd, 0);
  close (fd);
  if (buf == MAP_FAILED)
    return (-1);

  /* check the header */
  if (memcmp (buf, (const void *) "TZif", 4))
    goto err;

  lbuf = (const u_int32_t *) buf;

  /* number of leap seconds in database */
  n = be32toh (lbuf[7]);
  if (n == 0)
    goto err;

  leapsecs = malloc (n * sizeof (struct leapsecond));
  if (leapsecs == NULL)
    goto err;
  leapsecs_num = n;

  /* skip unimportant stuff */
  skip = 5*be32toh (lbuf[8]) + 6*be32toh (lbuf[9]) + be32toh (lbuf[10]);
  lbuf = (const u_int32_t *) (buf + 44 + skip);

  prev_change = 0;
  for (i = 0, ptr = leapsecs; i < n; i++, ptr++)
    {
      time_t transition, posix_transition, daystart, posix_daystart;
      int change, type;

      transition = (time_t) be32toh (*lbuf++);
      change = (int) be32toh (*lbuf++);
      type = change > prev_change;

      /* inserted is always 86400th second of the day,
         but deleted should be 86399th */
      daystart = transition - 86400 + !type;
      posix_daystart = daystart - prev_change;

      ptr->type = type;

      /* and we want to prolong the 23:59:59, not 00:00:00, if inserting
         (therefore subtracting one second if type == 1) */
      ptr->transition = transition - type;
      ptr->posix_transition = ptr->transition - prev_change;
      ptr->daystart = daystart;
      ptr->posix_daystart = posix_daystart;
      ptr->prev_change = prev_change;
      ptr->change = change;

      prev_change = change;
    }

  munmap ((void *) buf, 4096);
  return 0;
err:
  munmap ((void *) buf, 4096);
  return -1;
}

/* Convert right timestamp to a posix timestamp.
   If a leap second is being inserted at t+1, *state is set to 1.
   If a leap second is being inesrted at t, *state is set to 2.
   If a leap second is being deleted at t+1, *state is set to -1.
   Otherwise, *state is set to 0.

   Example:

   time       time2posix state
   1341100821 1341100797 0
   1341100822 1341100798 0
   1341100823 1341100799 1
   1341100824 1341100799 2
   1341100825 1341100800 0
   1341100826 1341100801 0
   */
static time_t
time2posix (time_t t, int *state)
{
  time_t res = t;
  size_t i = leapsecs_num;
  struct leapsecond *ptr;

  do
    if (i-- == 0)
      return res;
  while (t < leapsecs[i].transition);

  ptr = leapsecs + i;

  res = t - ptr->change;

  if (ptr->type && t-1 == ptr->transition)
    *state = 2;
  else if (ptr->type && t == ptr->transition)
    {
      *state = 1;
      res++;
    }
  else if (!ptr->type && t == ptr->transition)
    {
      *state = -1;
      res--;
    }
  else
    *state = 0;

  return res;
}

/* Convert a posix timestamp to a right timestamp.
   If a leap second is being inserted at t+1, *state is set to 1.
   If a leap second is being deleted at t+1, *state is set to -1.
   If a leap second is being deleted at t, *state is set to -2.
   Otherwise, *state is set to 0.

   Example:

   time       posix2time state
   1341100797 1341100821 0
   1341100798 1341100822 0
   1341100799 1341100823 1
   1341100800 1341100825 0
   1341100801 1341100826 0
   */
static time_t
posix2time (time_t t, int *state)
{
  time_t res = t;
  size_t i = leapsecs_num;
  struct leapsecond *ptr;

  do
    if (i-- == 0)
      return;
  while (t < leapsecs[i].posix_transition);

  ptr = leapsecs + i;

  res = t + ptr->change;

  if (ptr->type && t == ptr->posix_transition)
    {
      *state = 1;
      res--;
    }
  else if (!ptr->type && t == ptr->posix_transition)
    {
      *state = -1;
      res++;
    }
  else if (!ptr->type && t-1 == ptr->posix_transition)
    *state = -2;
  else
    *state = 0;

  return res;
}

/* normalize struct timeval */
static inline struct timeval *
normalize_tv (struct timeval *tv)
{
  while (tv->tv_usec >= 1000000)
    {
      tv->tv_usec -= 1000000;
      tv->tv_sec ++;
    }
  return tv;
}

/* Convert a right timeval to a posix timeval.
   If leap second is being inserted, simulates slowdown of the 23:59:59 second.
   (That means that 2 second will pass from 23:59:59 to 00:00:00).
   If a leap second is being deleted, simulates speedup of the 23:59:58 second.
   (That means that 1 second will pass from 23:59:58 to 00:00:00).  */
static struct timeval *
time2posix_tv (struct timeval *tv)
{
  int state;
  tv->tv_sec = time2posix (tv->tv_sec, &state);
  if (state == 1)
    tv->tv_usec >>= 1;
  else if (state == 2)
    tv->tv_usec = (tv->tv_usec + 1000000) >> 1;
  else if (state == -1)
    {
      tv->tv_usec <<= 1;
      normalize_tv (tv);
    }
  return tv;
}

/* Inverse to the previous function. */
static struct timeval *
posix2time_tv (struct timeval *tv)
{
  int state;
  tv->tv_sec = posix2time (tv->tv_sec, &state);
  if (state == 1)
    {
      tv->tv_usec <<= 1;
      normalize_tv (tv);
    }
  else if (state == -1)
    tv->tv_usec >>= 1;
  else if (state == -2)
    tv->tv_usec = (tv->tv_usec + 1000000) >> 1;
  return tv;
}

/* normalize struct timespec */
static inline struct timespec *
normalize_ts (struct timespec *ts)
{
  while (ts->tv_nsec >= 1000000000)
    {
      ts->tv_nsec -= 1000000000;
      ts->tv_sec ++;
    }
  return ts;
}

/* struct timespec version of time2posix_tv */
static struct timespec *
time2posix_ts (struct timespec *ts)
{
  int state;
  ts->tv_sec = time2posix (ts->tv_sec, &state);
  if (state == 1)
    ts->tv_nsec >>= 1;
  else if (state == 2)
    ts->tv_nsec = (ts->tv_nsec + 1000000000) >> 1;
  else if (state == -1)
    {
      ts->tv_nsec <<= 1;
      normalize_ts (ts);
    }
  return ts;
}

/* struct timespec version of posix2time_tv */
static struct timespec *
posix2time_ts (struct timespec *ts)
{
  int state;
  ts->tv_sec = posix2time (ts->tv_sec, &state);
  if (state == 1)
    {
      ts->tv_nsec <<= 1;
      normalize_ts (ts);
    }
  else if (state == -1)
    ts->tv_nsec >>= 1;
  else if (state == -2)
    ts->tv_nsec = (ts->tv_nsec + 1000000000) >> 1;
  return ts;
}

/* simulates adjtimex() return value when inserting or deleting a leap second.

   If a leap second will be inserted at the end of the day, return TIME_INS.
   If a leap second is being inserted now, return TIME_OOP.
   If a leap second was inserted in the previous second, return TIME_WAIT.

   If a leap second will be deleted at the end of the day, return TIME_DEL.
   If a leap second was deleted in the previous second, return TIME_WAIT.

   Otherwise return TIME_OK.  */
static inline int
time_status (time_t t)
{
  struct leapsecond *ptr;

  for (ptr = leapsecs + leapsecs_num - 1; ptr >= leapsecs; ptr--)
    {
      if (ptr->type)
        {
          if (t > ptr->transition + 2)
            return TIME_OK;
          else if (t > ptr->transition + 1)
            return TIME_WAIT;
          else if (t >= ptr->transition)
            return TIME_OOP;
          else if (t >= ptr->daystart)
            return TIME_INS;
        }
      else
        {
          if (t > ptr->transition + 1)
            return TIME_OK;
          else if (t > ptr->transition)
            return TIME_WAIT;
          else if (t >= ptr->daystart)
            return TIME_DEL;
        }
    }

  return TIME_OK;
}

/* a little test procedure */
static void
testthis (void)
{
  time_t t, e, pt2p = 0;
  struct timeval tv;
  int i;

  t = leapsecs[leapsecs_num-1].transition-2;
  e = t + 6;
  printf ("time       time2posix st posix2time st status\n");
  for (; t < e; t++)
    {
      time_t t2p, p2t;
      int st1, st2, status = time_status (t);

      t2p = time2posix (t, &st1);

      if (t2p == pt2p + 2)
        {
          p2t = posix2time (pt2p + 1, &st2);
          printf ("           %li    %li %+i\n", pt2p+1, p2t, st2);
        }
      pt2p = t2p;

      p2t = posix2time (t2p, &st2);
      printf ("%li %li %+i %li %+i %s\n", t, t2p, st1, p2t, st2,
              status == TIME_OK ? "OK" :
              status == TIME_WAIT ? "WAIT" :
              status == TIME_OOP ? "OOP" :
              status == TIME_INS ? "INS" :
              status == TIME_DEL ? "DEL" : "unknown");
    }

  printf ("\ntime         time2posix   posix2time\n");
  tv.tv_sec = leapsecs[leapsecs_num-1].transition;
  tv.tv_usec = 0;
  for (i = 0; i < 20; i++)
    {
      struct timeval t2p, p2t;
      t2p = tv;
      time2posix_tv (&t2p);
      p2t = t2p;
      posix2time_tv (&p2t);
      printf ("%li.%li %li.%li %li.%li\n", tv.tv_sec, tv.tv_usec/100000, t2p.tv_sec, t2p.tv_usec/100000, p2t.tv_sec, p2t.tv_usec/100000);
      tv.tv_usec += 200000;
      normalize_tv (&tv);
    }
}

/*
static struct utmp *(*pututline_orig) (const struct utmp *);
static void (*updwtmp_orig) (const char *, const struct utmp *);
static struct utmpx *(*pututxline_orig) (const struct utmpx *);
static void (*updwtmpx_orig) (const char *, const struct utmpx *);
*/
static time_t (*time_orig) (time_t *);
static int (*stime_orig) (const time_t *);
static int (*clock_gettime_orig) (clockid_t, struct timespec *);
static int (*clock_settime_orig) (clockid_t, const struct timespec *);
static int (*clock_adjtime_orig) (clockid_t, struct timex *);
static int (*gettimeofday_orig) (struct timeval *, struct timezone *);
static int (*settimeofday_orig) (const struct timeval *, const struct timezone *);
static int (*adjtimex_orig) (struct timex *);
static int (*ntp_adjtime_orig) (struct timex *);
static int (*ntp_gettime_orig) (struct ntptimeval *);

static void *handle = NULL;

static void init (void) __attribute__((constructor));
static void fini (void) __attribute__((destructor));

/* initialize leap seconds table and original function pointers */
static void
init (void)
{
  static int doneinit = 0;

  if (doneinit)
    return;

  if (leaps_read ())
    exit (255);

  if (0)
    testthis ();

  handle = dlopen ("libc.so.6", RTLD_LAZY);
  if (!handle)
    exit (255);

/*
  pututline_orig = dlsym (handle, "pututline");
  pututxline_orig = dlsym (handle, "pututxline");
  updwtmp_orig = dlsym (handle, "updwtmp");
  updwtmpx_orig = dlsym (handle, "updwtmpx");
*/
  time_orig = dlsym (handle, "time");
  stime_orig = dlsym (handle, "stime");
  clock_gettime_orig = dlsym (handle, "clock_gettime");
  clock_settime_orig = dlsym (handle, "clock_settime");
  clock_adjtime_orig = dlsym (handle, "clock_adjtime");
  gettimeofday_orig = dlsym (handle, "gettimeofday");
  settimeofday_orig = dlsym (handle, "settimeofday");
  adjtimex_orig = dlsym (handle, "adjtimex");
  ntp_adjtime_orig = dlsym (handle, "ntp_adjtime");
  ntp_gettime_orig = dlsym (handle, "ntp_gettime");
}

static void
fini (void)
{
  if (handle != NULL)
    dlclose (handle);
}

/*
struct utmp *
pututline (const struct utmp *ut)
{
  return pututline_orig (ut);
}

void
updwtmp (const char *file, const struct utmp *ut)
{
  return updwtmp_orig (file, ut);
}

struct utmpx *
pututxline (const struct utmpx *ut)
{
  return pututxline_orig (ut);
}

void
updwtmpx (const char *file, const struct utmpx *utx)
{
  return updwtmpx_orig (file, utx);
}
*/

time_t time (time_t *t)
{
  int state;
  time_t res = time_orig (t);
  if (res == (time_t) -1)
    return res;

  res = time2posix (res, &state);
  if (t != NULL)
    *t = res;
  return res;
}

int stime (const time_t *t)
{
  int state;
  if (t == NULL)
    return stime_orig (t);

  time_t right = posix2time (*t, &state);
  return stime_orig (&right);
}

int
clock_gettime (clockid_t clkid, struct timespec *ts)
{
  int res = clock_gettime_orig (clkid, ts);
  if (clkid == CLOCK_REALTIME || clkid == CLOCK_REALTIME_COARSE)
    time2posix_ts (ts);
  return res;
}

int
clock_settime (clockid_t clkid, const struct timespec *ts)
{
  if (clkid == CLOCK_REALTIME)
    {
      struct timespec myts;
      memcpy (&myts, ts, sizeof (struct timespec));
      posix2time_ts (&myts);
      return clock_settime_orig (clkid, &myts);
    }
  else
    return clock_settime_orig (clkid, ts);
}

int
gettimeofday (struct timeval *tv, struct timezone *tz)
{
  int res = gettimeofday_orig (tv, tz);
  if (tv != NULL)
    time2posix_tv (tv);
  return res;
}

int
settimeofday (const struct timeval *tv, const struct timezone *tz)
{
  if (tv != NULL)
    {
      struct timeval mytv;
      memcpy (&mytv, tv, sizeof (struct timeval));
      posix2time_tv (&mytv);
      return settimeofday_orig (&mytv, tz);
    }
  else
    return settimeofday_orig (tv, tz);
}

int
adjtimex (struct timex *buf)
{
  int res, status;

  if (buf == NULL)
    return adjtimex_orig (buf);

  /* we don't want to insert or delete leap seconds in the kernel */
  buf->status &= ~(STA_INS|STA_DEL);
  res = adjtimex_orig (buf);

  status = time_status (buf->time.tv_sec);

  /* will/did something interesting happen today? */
  switch (status)
    {
      case TIME_INS:
      case TIME_OOP:
        buf->status |= STA_INS;
        break;
      case TIME_DEL:
        buf->status |= STA_DEL;
        break;
    }

  time2posix_tv (&buf->time);

  return res == TIME_OK ? status : res;
}

/* clock_adjtime with CLOCK_REALTIME calls do_adjtimex in kernel */
int
clock_adjtime (clockid_t clkid, struct timex *buf)
{
  if (clkid == CLOCK_REALTIME)
    return adjtimex (buf);
  else
    return clock_adjtime_orig (clkid, buf);
}

/* ntp_adjtime is an alias for adjtimex in glibc */
int
ntp_adjtime (struct timex *buf)
{
  return adjtimex (buf);
}

int
ntp_gettime (struct ntptimeval *buf)
{
  int res = ntp_gettime_orig (buf);
  if (buf != NULL)
    time2posix_tv (&buf->time);
  return res;
}
