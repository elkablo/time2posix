/* 2014 by Marek Behun <kabel@blackhole.sk>
   This file is in public domain */

#ifndef HAVE_TIME2POSIX_H
#define HAVE_TIME2POSIX_H

#include <utmp.h>
#include <utmpx.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <sys/time.h>
#include <sys/timex.h>

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

extern struct leapsecond *t2p_leapsecs;
extern size_t t2p_leapsecs_num;

int t2p_leaps_read (void);
time_t t2p_time2posix (time_t, int *);
time_t t2p_posix2time (time_t, int *);
struct timeval *t2p_normalize_timeval (struct timeval *);
struct timeval *t2p_time2posix_timeval (struct timeval *);
struct timeval *t2p_posix2time_timeval (struct timeval *);
struct timespec *t2p_normalize_timespec (struct timespec *);
struct timespec *t2p_time2posix_timespec (struct timespec *);
struct timespec *t2p_posix2time_timespec (struct timespec *);
int t2p_timestatus (time_t);

/*
extern struct utmp *(*t2p_orig_pututline) (const struct utmp *);
extern void (*t2p_orig_updwtmp) (const char *, const struct utmp *);
extern struct utmpx *(*t2p_orig_pututxline) (const struct utmpx *);
extern void (*t2p_orig_updwtmpx) (const char *, const struct utmpx *);
*/
extern time_t (*t2p_orig_time) (time_t *);
extern int (*t2p_orig_stime) (const time_t *);
extern int (*t2p_orig_clock_gettime) (clockid_t, struct timespec *);
extern int (*t2p_orig_clock_settime) (clockid_t, const struct timespec *);
extern int (*t2p_orig_clock_adjtime) (clockid_t, struct timex *);
extern int (*t2p_orig_gettimeofday) (struct timeval *, struct timezone *);
extern int (*t2p_orig_settimeofday) (const struct timeval *, const struct timezone *);
extern int (*t2p_orig_adjtimex) (struct timex *);
extern int (*t2p_orig_ntp_adjtime) (struct timex *);
extern int (*t2p_orig_ntp_gettime) (struct ntptimeval *);

#endif /* !HAVE_TIME2POSIX_H */
