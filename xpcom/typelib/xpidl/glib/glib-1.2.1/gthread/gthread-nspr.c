/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gthread.c: nspr thread system implementation
 * Copyright 1998 Sebastian Wilhelmi; University of Karlsruhe
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

#include <prpdce.h>
#include <prthread.h>
#include <stdlib.h>

#ifdef G_DISABLE_ASSERT

#define STDERR_ASSERT(expr)

#else /* G_DISABLE_ASSERT */

#define STDERR_ASSERT(expr)                  G_STMT_START{      \
     if (!(expr))                                               \
       g_log (G_LOG_DOMAIN,                                     \
              G_LOG_LEVEL_ERROR,                                \
              "file %s: line %d: assertion failed: (%s)",       \
              __FILE__,                                         \
              __LINE__,                                         \
              #expr);                   }G_STMT_END

#endif /* G_DISABLE_ASSERT */

/* NOTE: the functions g_mutex_lock and g_mutex_unlock may not use
   functions from gmem.c and gmessages.c; */

static gboolean
g_mutex_trylock_nspr_impl (GMutex * mutex)
{
  PRStatus status = PRP_TryLock ((PRLock *) mutex);
  if (status == PR_SUCCESS)
    {
      return TRUE;
    }
  return FALSE;
}

static void
g_cond_wait_nspr_impl (GCond * cond,
		       GMutex * entered_mutex)
{
  PRStatus status = PRP_NakedWait ((PRCondVar *) cond, 
				   (PRLock *) entered_mutex,
				   PR_INTERVAL_NO_TIMEOUT);
  g_assert (status == PR_SUCCESS);
}

#define G_MICROSEC 1000000

static gboolean
g_cond_timed_wait_nspr_impl (GCond * cond,
			     GMutex * entered_mutex,
			     GTimeVal * abs_time)
{
  PRStatus status;
  PRIntervalTime interval;
  GTimeVal current_time;
  glong microsecs;

  g_return_val_if_fail (cond != NULL, FALSE);
  g_return_val_if_fail (entered_mutex != NULL, FALSE);

  g_get_current_time (&current_time);

  if (abs_time->tv_sec < current_time.tv_sec ||
      (abs_time->tv_sec == current_time.tv_sec &&
       abs_time->tv_usec < current_time.tv_usec))
    return FALSE;

  interval = PR_SecondsToInterval (abs_time->tv_sec - current_time.tv_sec);
  microsecs = abs_time->tv_usec - current_time.tv_usec;
  if (microsecs < 0)
    interval -= PR_MicrosecondsToInterval (-microsecs);
  else
    interval += PR_MicrosecondsToInterval (microsecs);

  status = PRP_NakedWait ((PRCondVar *) cond, (PRLock *) entered_mutex,
			  interval);

  g_assert (status == PR_SUCCESS);

  g_get_current_time (&current_time);

  if (abs_time->tv_sec < current_time.tv_sec ||
      (abs_time->tv_sec == current_time.tv_sec &&
       abs_time->tv_usec < current_time.tv_usec))
    return FALSE;
  return TRUE;
}

typedef struct _GPrivateNSPRData GPrivateNSPRData;
struct _GPrivateNSPRData
  {
    gpointer data;
    GDestroyNotify destructor;
  };

typedef struct _GPrivateNSPR GPrivateNSPR;
struct _GPrivateNSPR
  {
    PRUintn private_key;
    GDestroyNotify destructor;
  };

static GPrivateNSPRData *
g_private_nspr_data_constructor (GDestroyNotify destructor, gpointer data)
{
  /* we can not use g_new and friends, as they might use private data by
     themself */
  GPrivateNSPRData *private_key = malloc (sizeof (GPrivateNSPRData));
  g_assert (private_key);
  private_key->data = data;
  private_key->destructor = destructor;

  return private_key;
}

static void
g_private_nspr_data_destructor (gpointer data)
{
  GPrivateNSPRData *private_key = data;
  if (private_key->destructor && private_key->data)
    (*private_key->destructor) (private_key->data);
  free (private_key);
}

static GPrivate *
g_private_new_nspr_impl (GDestroyNotify destructor)
{
  GPrivateNSPR *result = g_new (GPrivateNSPR, 1);
  PRStatus status = PR_NewThreadPrivateIndex (&result->private_key,
					    g_private_nspr_data_destructor);
  g_assert (status == PR_SUCCESS);

  result->destructor = destructor;
  return (GPrivate *) result;
}

/* NOTE: the functions g_private_get and g_private_set may not use
   functions from gmem.c and gmessages.c */

static GPrivateNSPRData *
g_private_nspr_data_get (GPrivateNSPR * private_key)
{
  GPrivateNSPRData *data;

  STDERR_ASSERT (private_key);

  data = PR_GetThreadPrivate (private_key->private_key);
  if (!data)
    {
      data = g_private_nspr_data_constructor (private_key->destructor, NULL);
      STDERR_ASSERT (PR_SetThreadPrivate (private_key->private_key, data)
		     == PR_SUCCESS);
    }

  return data;
}

static void
g_private_set_nspr_impl (GPrivate * private_key, gpointer value)
{
  if (!private_key)
    return;

  g_private_nspr_data_get ((GPrivateNSPR *) private_key)->data = value;
}

static gpointer
g_private_get_nspr_impl (GPrivate * private_key)
{
  if (!private_key)
    return NULL;

  return g_private_nspr_data_get ((GPrivateNSPR *) private_key)->data;
}

static GThreadFunctions g_thread_functions_for_glib_use_default =
{
  (GMutex * (*)())PR_NewLock,
  (void (*)(GMutex *)) PR_Lock,
  g_mutex_trylock_nspr_impl,
  (void (*)(GMutex *)) PR_Unlock,
  (void (*)(GMutex *)) PR_DestroyLock,
  (GCond * (*)())PRP_NewNakedCondVar,
  (void (*)(GCond *)) PRP_NakedNotify,
  (void (*)(GCond *)) PRP_NakedBroadcast,
  g_cond_wait_nspr_impl,
  g_cond_timed_wait_nspr_impl,
  (void (*)(GCond *)) PRP_DestroyNakedCondVar,
  g_private_new_nspr_impl,
  g_private_get_nspr_impl,
  g_private_set_nspr_impl
};
