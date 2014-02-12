/* 2014 by Marek Behun <kabel@blackhole.sk>
   This file is in public domain */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <endian.h>

#include "time2posix.h"

#define RIGHT_TZ "right/UTC"
#define LEAP_TZFILE ("/usr/share/zoneinfo/" RIGHT_TZ)

struct leapsecond *t2p_leapsecs = NULL;
size_t t2p_leapsecs_num = 0;
time_t t2p_last_read = 0;

/* read the leap seconds table */
int
t2p_leaps_read (void)
{
  int fd, prev_change;
  const char *buf;
  const u_int32_t *lbuf;
  u_int32_t n, skip, i;
  struct leapsecond *ptr, *leapsecs;
  time_t now;

  /* we don't need to re-read every time
     (keep the same table at least for 30 days) */
  now = t2p_orig_time (NULL);
  if (now < (t2p_last_read + 86400*30))
    return 0;

  /* does O_NONBLOCK need to be here if we are mmapping? */
  fd = open (LEAP_TZFILE, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0)
    {
      fprintf (stderr, "time2posix error: Cannot open %s !\n", LEAP_TZFILE);
      return -1;
    }

  /* right/UTC shouldn't be bigger than 4096B */
  buf = mmap (NULL, 4096, PROT_READ, MAP_PRIVATE, fd, 0);
  close (fd);
  if (buf == MAP_FAILED)
    {
      fprintf (stderr, "time2posix error: Cannot mmap %s !\n", LEAP_TZFILE);
      return (-1);
    }

  /* check the header */
  if (memcmp (buf, (const void *) "TZif", 4))
    {
      fprintf (stderr, "time2posix error: Invalid %s header !\n", LEAP_TZFILE);
      goto err;
    }

  lbuf = (const u_int32_t *) buf;

  /* number of leap seconds in database */
  n = be32toh (lbuf[7]);
  if (n == 0)
    {
      fprintf (stderr, "time2posix warning: No leap seconds in %s !%s\n", LEAP_TZFILE,
               t2p_leapsecs != NULL ? " Using old table." : "");
      goto end;
    }

  leapsecs = malloc (n * sizeof (struct leapsecond));
  if (leapsecs == NULL)
    {
      fprintf (stderr, "time2posix error: Cannot allocate space for leap seconds table!\n");
      goto err;
    }

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

  /* this should be protected by some rw lock */
  if (t2p_leapsecs != NULL)
    free (t2p_leapsecs);
  t2p_leapsecs = leapsecs;
  t2p_leapsecs_num = n;
  t2p_last_read = now;

end:
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
time_t
t2p_time2posix (time_t t, int *state)
{
  time_t res = t;
  size_t i = t2p_leapsecs_num;
  struct leapsecond *ptr;

  /* reread leap seconds table sometimes */
  t2p_leaps_read ();

  do
    if (i-- == 0)
      return res;
  while (t < t2p_leapsecs[i].transition);

  ptr = t2p_leapsecs + i;

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
time_t
t2p_posix2time (time_t t, int *state)
{
  time_t res = t;
  size_t i = t2p_leapsecs_num;
  struct leapsecond *ptr;

  /* reread leap seconds table sometimes */
  t2p_leaps_read ();

  do
    if (i-- == 0)
      return;
  while (t < t2p_leapsecs[i].posix_transition);

  ptr = t2p_leapsecs + i;

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
inline struct timeval *
t2p_normalize_timeval (struct timeval *tv)
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
struct timeval *
t2p_time2posix_timeval (struct timeval *tv)
{
  int state;
  tv->tv_sec = t2p_time2posix (tv->tv_sec, &state);
  if (state == 1)
    tv->tv_usec >>= 1;
  else if (state == 2)
    tv->tv_usec = (tv->tv_usec + 1000000) >> 1;
  else if (state == -1)
    {
      tv->tv_usec <<= 1;
      t2p_normalize_timeval (tv);
    }
  return tv;
}

/* Inverse to the previous function. */
struct timeval *
t2p_posix2time_timeval (struct timeval *tv)
{
  int state;
  tv->tv_sec = t2p_posix2time (tv->tv_sec, &state);
  if (state == 1)
    {
      tv->tv_usec <<= 1;
      t2p_normalize_timeval (tv);
    }
  else if (state == -1)
    tv->tv_usec >>= 1;
  else if (state == -2)
    tv->tv_usec = (tv->tv_usec + 1000000) >> 1;
  return tv;
}

/* normalize struct timespec */
inline struct timespec *
t2p_normalize_timespec (struct timespec *ts)
{
  while (ts->tv_nsec >= 1000000000)
    {
      ts->tv_nsec -= 1000000000;
      ts->tv_sec ++;
    }
  return ts;
}

/* struct timespec version of t2p_time2posix_timeval */
struct timespec *
t2p_time2posix_timespec (struct timespec *ts)
{
  int state;
  ts->tv_sec = t2p_time2posix (ts->tv_sec, &state);
  if (state == 1)
    ts->tv_nsec >>= 1;
  else if (state == 2)
    ts->tv_nsec = (ts->tv_nsec + 1000000000) >> 1;
  else if (state == -1)
    {
      ts->tv_nsec <<= 1;
      t2p_normalize_timespec (ts);
    }
  return ts;
}

/* struct timespec version of t2p_posix2time_timeval */
struct timespec *
t2p_posix2time_timespec (struct timespec *ts)
{
  int state;
  ts->tv_sec = t2p_posix2time (ts->tv_sec, &state);
  if (state == 1)
    {
      ts->tv_nsec <<= 1;
      t2p_normalize_timespec (ts);
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
int
t2p_timestatus (time_t t)
{
  struct leapsecond *ptr;

  for (ptr = t2p_leapsecs + t2p_leapsecs_num - 1; ptr >= t2p_leapsecs; ptr--)
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

static void *t2p_libc_handle = NULL;

static void t2p_init (void) __attribute__((constructor));
static void t2p_fini (void) __attribute__((destructor));

/* initialize leap seconds table and original function pointers */
static void
t2p_init (void)
{
  t2p_libc_handle = dlopen ("libc.so.6", RTLD_LAZY);
  if (!t2p_libc_handle)
    exit (255);

#define fetchsymbol(name) \
  t2p_orig_##name = dlsym (t2p_libc_handle, #name); \
  if (t2p_orig_##name == NULL) \
    fprintf (stderr, "time2posix error: Cannot load symbol " #name " from libc.so.6!\n")

  fetchsymbol(getutent);
  fetchsymbol(getutid);
  fetchsymbol(getutline);
  fetchsymbol(pututline);
  fetchsymbol(updwtmp);

  fetchsymbol(getutent_r);
  fetchsymbol(getutid_r);
  fetchsymbol(getutline_r);

  fetchsymbol(getutxent);
  fetchsymbol(getutxid);
  fetchsymbol(getutxline);
  fetchsymbol(pututxline);
  fetchsymbol(updwtmpx);

  fetchsymbol(time);
  fetchsymbol(stime);
  fetchsymbol(clock_gettime);
  fetchsymbol(clock_settime);
  fetchsymbol(clock_adjtime);
  fetchsymbol(gettimeofday);
  fetchsymbol(settimeofday);
  fetchsymbol(adjtimex);
  fetchsymbol(ntp_adjtime);
  fetchsymbol(ntp_gettime);

  if (t2p_leaps_read ())
    exit (255);
}

static void
t2p_fini (void)
{
  if (t2p_libc_handle != NULL)
    dlclose (t2p_libc_handle);
}
