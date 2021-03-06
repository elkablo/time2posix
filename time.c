/* 2014 by Marek Behun <kabel@blackhole.sk>
   This file is in public domain */

#include <string.h>

#include "time2posix.h"

time_t (*t2p_orig_time) (time_t *);
int (*t2p_orig_stime) (const time_t *);
int (*t2p_orig_clock_gettime) (clockid_t, struct timespec *);
int (*t2p_orig_clock_settime) (clockid_t, const struct timespec *);
int (*t2p_orig_clock_adjtime) (clockid_t, struct timex *);
int (*t2p_orig_gettimeofday) (struct timeval *, struct timezone *);
int (*t2p_orig_settimeofday) (const struct timeval *, const struct timezone *);
int (*t2p_orig_adjtimex) (struct timex *);
int (*t2p_orig_ntp_adjtime) (struct timex *);
int (*t2p_orig_ntp_gettime) (struct ntptimeval *);
ssize_t (*t2p_orig_recvmsg) (int, struct msghdr *, int);

time_t time (time_t *t)
{
  int state;
  time_t res = t2p_orig_time (t);
  if (res == (time_t) -1)
    return res;

  res = t2p_time2posix (res, &state);
  if (t != NULL)
    *t = res;
  return res;
}

int stime (const time_t *t)
{
  int state;
  if (t == NULL)
    return t2p_orig_stime (t);

  time_t right = t2p_posix2time (*t, &state);
  return t2p_orig_stime (&right);
}

int
clock_gettime (clockid_t clkid, struct timespec *ts)
{
  int res = t2p_orig_clock_gettime (clkid, ts);
  if (clkid == CLOCK_REALTIME || clkid == CLOCK_REALTIME_COARSE)
    t2p_time2posix_timespec (ts);
  return res;
}

int
clock_settime (clockid_t clkid, const struct timespec *ts)
{
  if (clkid == CLOCK_REALTIME)
    {
      struct timespec myts;
      memcpy (&myts, ts, sizeof (struct timespec));
      t2p_posix2time_timespec (&myts);
      return t2p_orig_clock_settime (clkid, &myts);
    }
  else
    return t2p_orig_clock_settime (clkid, ts);
}

int
gettimeofday (struct timeval *tv, struct timezone *tz)
{
  int res = t2p_orig_gettimeofday (tv, tz);
  if (tv != NULL)
    t2p_time2posix_timeval (tv);
  return res;
}

int
settimeofday (const struct timeval *tv, const struct timezone *tz)
{
  if (tv != NULL)
    {
      struct timeval mytv;
      memcpy (&mytv, tv, sizeof (struct timeval));
      t2p_posix2time_timeval (&mytv);
      return t2p_orig_settimeofday (&mytv, tz);
    }
  else
    return t2p_orig_settimeofday (tv, tz);
}

int
adjtimex (struct timex *buf)
{
  int res, status;

  if (buf == NULL)
    return t2p_orig_adjtimex (buf);

  /* we don't want to insert or delete leap seconds in the kernel */
  buf->status &= ~(STA_INS|STA_DEL);
  res = t2p_orig_adjtimex (buf);

  status = t2p_timestatus (buf->time.tv_sec);

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

  t2p_time2posix_timeval (&buf->time);

  return res == TIME_OK ? status : res;
}

/* clock_adjtime with CLOCK_REALTIME calls do_adjtimex in kernel */
int
clock_adjtime (clockid_t clkid, struct timex *buf)
{
  if (clkid == CLOCK_REALTIME)
    return adjtimex (buf);
  else
    return t2p_orig_clock_adjtime (clkid, buf);
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
  int res = t2p_orig_ntp_gettime (buf);
  if (buf != NULL)
    t2p_time2posix_timeval (&buf->time);
  return res;
}

ssize_t
recvmsg (int fd, struct msghdr *msg, int flags)
{
  struct cmsghdr *cmsg;
  ssize_t res = t2p_orig_recvmsg (fd, msg, flags);

  if (res < 0 || !msg->msg_control || !msg->msg_controllen)
    return res;

  for (cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg))
    {
      switch (cmsg->cmsg_type)
        {
        case SCM_TIMESTAMPNS:
          t2p_time2posix_timespec ((struct timespec *) CMSG_DATA(cmsg));
          break;
        case SCM_TIMESTAMP:
          t2p_time2posix_timeval ((struct timeval *) CMSG_DATA(cmsg));
          break;
        default:
          /* TODO: SCM_TIMESTAMPING */
          break;
        }
    }

  return res;
}
