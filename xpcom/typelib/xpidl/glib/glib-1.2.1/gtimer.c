/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "glib.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifndef NATIVE_WIN32
#include <sys/time.h>
#endif /* NATIVE_WIN32 */

#ifdef NATIVE_WIN32
#include <windows.h>
#endif /* NATIVE_WIN32 */

typedef struct _GRealTimer GRealTimer;

struct _GRealTimer
{
#ifdef NATIVE_WIN32
  DWORD start;
  DWORD end;
#else /* !NATIVE_WIN32 */
  struct timeval start;
  struct timeval end;
#endif /* !NATIVE_WIN32 */

  guint active : 1;
};

GTimer*
g_timer_new (void)
{
  GRealTimer *timer;

  timer = g_new (GRealTimer, 1);
  timer->active = TRUE;

#ifdef NATIVE_WIN32
  timer->start = GetTickCount ();
#else /* !NATIVE_WIN32 */
  gettimeofday (&timer->start, NULL);
#endif /* !NATIVE_WIN32 */

  return ((GTimer*) timer);
}

void
g_timer_destroy (GTimer *timer)
{
  g_return_if_fail (timer != NULL);

  g_free (timer);
}

void
g_timer_start (GTimer *timer)
{
  GRealTimer *rtimer;

  g_return_if_fail (timer != NULL);

  rtimer = (GRealTimer*) timer;
  rtimer->active = TRUE;

#ifdef NATIVE_WIN32
  rtimer->start = GetTickCount ();
#else /* !NATIVE_WIN32 */
  gettimeofday (&rtimer->start, NULL);
#endif /* !NATIVE_WIN32 */
}

void
g_timer_stop (GTimer *timer)
{
  GRealTimer *rtimer;

  g_return_if_fail (timer != NULL);

  rtimer = (GRealTimer*) timer;
  rtimer->active = FALSE;

#ifdef NATIVE_WIN32
  rtimer->end = GetTickCount ();
#else /* !NATIVE_WIN32 */
  gettimeofday (&rtimer->end, NULL);
#endif /* !NATIVE_WIN32 */
}

void
g_timer_reset (GTimer *timer)
{
  GRealTimer *rtimer;

  g_return_if_fail (timer != NULL);

  rtimer = (GRealTimer*) timer;

#ifdef NATIVE_WIN32
   rtimer->start = GetTickCount ();
#else /* !NATIVE_WIN32 */
  gettimeofday (&rtimer->start, NULL);
#endif /* !NATIVE_WIN32 */
}

gdouble
g_timer_elapsed (GTimer *timer,
		 gulong *microseconds)
{
  GRealTimer *rtimer;
  gdouble total;
#ifndef NATIVE_WIN32
  struct timeval elapsed;
#endif /* NATIVE_WIN32 */

  g_return_val_if_fail (timer != NULL, 0);

  rtimer = (GRealTimer*) timer;

#ifdef NATIVE_WIN32
  if (rtimer->active)
    rtimer->end = GetTickCount ();

  /* Check for wraparound, which happens every 49.7 days.
   * No, Win95 machines probably are never running for that long,
   * but NT machines are.
   */
  if (rtimer->end < rtimer->start)
    total = (UINT_MAX - (rtimer->start - rtimer->end)) / 1000.0;
  else
    total = (rtimer->end - rtimer->start) / 1000.0;

  if (microseconds)
    {
      if (rtimer->end < rtimer->start)
	*microseconds =
	  ((UINT_MAX - (rtimer->start - rtimer->end)) % 1000) * 1000;
      else
	*microseconds =
	  ((rtimer->end - rtimer->start) % 1000) * 1000;
    }
#else /* !NATIVE_WIN32 */
  if (rtimer->active)
    gettimeofday (&rtimer->end, NULL);

  if (rtimer->start.tv_usec > rtimer->end.tv_usec)
    {
      rtimer->end.tv_usec += 1000000;
      rtimer->end.tv_sec--;
    }

  elapsed.tv_usec = rtimer->end.tv_usec - rtimer->start.tv_usec;
  elapsed.tv_sec = rtimer->end.tv_sec - rtimer->start.tv_sec;

  total = elapsed.tv_sec + ((gdouble) elapsed.tv_usec / 1e6);

  if (microseconds)
    *microseconds = elapsed.tv_usec;
#endif /* !NATIVE_WIN32 */

  return total;
}
