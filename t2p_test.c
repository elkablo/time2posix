/* 2014 by Marek Behun <kabel@blackhole.sk>
   This file is in public domain */

#include <stdio.h>
#include <stdlib.h>

#include "time2posix.h"

int
main (int argc, char **argv)
{
  time_t t, e, pt2p = 0;
  struct timeval tv;
  int i;

  t = t2p_leapsecs[t2p_leapsecs_num-1].transition-2;
  e = t + 6;
  printf ("time       time2posix st posix2time st status\n");
  for (; t < e; t++)
    {
      time_t t2p, p2t;
      int st1, st2, status = t2p_timestatus (t);

      t2p = t2p_time2posix (t, &st1);

      if (t2p == pt2p + 2)
        {
          p2t = t2p_posix2time (pt2p + 1, &st2);
          printf ("           %li    %li %+i\n", pt2p+1, p2t, st2);
        }
      pt2p = t2p;

      p2t = t2p_posix2time (t2p, &st2);
      printf ("%li %li %+i %li %+i %s\n", t, t2p, st1, p2t, st2,
              status == TIME_OK ? "OK" :
              status == TIME_WAIT ? "WAIT" :
              status == TIME_OOP ? "OOP" :
              status == TIME_INS ? "INS" :
              status == TIME_DEL ? "DEL" : "unknown");
    }

  printf ("\ntime         time2posix   posix2time\n");
  tv.tv_sec = t2p_leapsecs[t2p_leapsecs_num-1].transition;
  tv.tv_usec = 0;
  for (i = 0; i < 20; i++)
    {
      struct timeval t2p, p2t;
      t2p = tv;
      t2p_time2posix_timeval (&t2p);
      p2t = t2p;
      t2p_posix2time_timeval (&p2t);
      printf ("%li.%li %li.%li %li.%li\n", tv.tv_sec, tv.tv_usec/100000, t2p.tv_sec, t2p.tv_usec/100000, p2t.tv_sec, p2t.tv_usec/100000);
      tv.tv_usec += 200000;
      t2p_normalize_timeval (&tv);
    }

  exit (0);
}
