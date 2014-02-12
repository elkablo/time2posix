/* 2014 by Marek Behun <kabel@blackhole.sk>
   This file is in public domain */

#include "time2posix.h"

struct utmp *(*t2p_orig_getutent) (void);
struct utmp *(*t2p_orig_getutid) (const struct utmp *);
struct utmp *(*t2p_orig_getutline) (const struct utmp *);
struct utmp *(*t2p_orig_pututline) (const struct utmp *);
void (*t2p_orig_updwtmp) (const char *, const struct utmp *);

int (*t2p_orig_getutent_r) (struct utmp *, struct utmp **);
int (*t2p_orig_getutid_r) (const struct utmp *, struct utmp *, struct utmp **);
int (*t2p_orig_getutline_r) (const struct utmp *, struct utmp *, struct utmp **);

struct utmpx *(*t2p_orig_getutxent) (void);
struct utmpx *(*t2p_orig_getutxid) (const struct utmpx *);
struct utmpx *(*t2p_orig_getutxline) (const struct utmpx *);
struct utmpx *(*t2p_orig_pututxline) (const struct utmpx *);
void (*t2p_orig_updwtmpx) (const char *, const struct utmpx *);

#define def_conv(conv, type)		\
static inline struct type *		\
conv##_##type (struct type *ut)		\
{					\
  if (ut == NULL)			\
    return NULL;			\
  struct timeval tv;			\
  tv.tv_sec = ut->ut_tv.tv_sec;		\
  tv.tv_usec = ut->ut_tv.tv_usec;	\
  t2p_##conv##_timeval (&tv);		\
  ut->ut_tv.tv_sec = tv.tv_sec;		\
  ut->ut_tv.tv_usec = tv.tv_usec;	\
  return ut;				\
}

def_conv(time2posix, utmp)
def_conv(posix2time, utmp)
def_conv(time2posix, utmpx)
def_conv(posix2time, utmpx)

#define def_getent(type,name)				\
struct type *						\
name (void)						\
{							\
  return time2posix_##type (t2p_orig_##name ());	\
}

#define def_getput(type,name)				\
struct type *						\
name (const struct type *p)				\
{							\
  struct type ut = *p;					\
  posix2time_##type (&ut);				\
  return time2posix_##type (t2p_orig_##name (&ut));	\
}

#define def_upd(type,name)			\
void						\
name (const char *file, const struct type *p)	\
{						\
  struct type ut = *p;				\
  posix2time_##type (&ut);			\
  t2p_orig_##name (file, &ut);			\
}

def_getent(utmp,getutent)
def_getput(utmp,getutid)
def_getput(utmp,getutline)
def_getput(utmp,pututline)
def_upd(utmp,updwtmp)

def_getent(utmpx,getutxent)
def_getput(utmpx,getutxid)
def_getput(utmpx,getutxline)
def_getput(utmpx,pututxline)
def_upd(utmpx,updwtmpx)

int
getutent_r (struct utmp *ubuf, struct utmp **ubufp)
{
  int res = t2p_orig_getutent_r (ubuf, ubufp);
  if (!res)
    time2posix_utmp (ubuf);
  return res;
}

#define def_get_r(name)							\
int									\
name (const struct utmp *p, struct utmp *ubuf, struct utmp **ubufp)	\
{									\
  struct utmp ut = *p;							\
  int res;								\
  posix2time_utmp (&ut);						\
  res = t2p_orig_##name (&ut, ubuf, ubufp);				\
  if (!res)								\
    time2posix_utmp (ubuf);						\
  return res;								\
}

def_get_r(getutid_r)
def_get_r(getutline_r)
