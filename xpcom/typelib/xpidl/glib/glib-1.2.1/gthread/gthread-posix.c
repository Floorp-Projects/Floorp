/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gthread.c: posix thread system implementation
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

#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#define posix_print_error( name, num )                          \
  g_error( "file %s: line %d (%s): error %s during %s",         \
           __FILE__, __LINE__, G_GNUC_PRETTY_FUNCTION,          \
           g_strerror((num)), #name )

#define posix_check_for_error( what ) G_STMT_START{             \
  int error = (what);                                           \
  if( error ) { posix_print_error( what, error ); }             \
  }G_STMT_END

static GMutex *
g_mutex_new_posix_impl (void)
{
  GMutex *result = (GMutex *) g_new (pthread_mutex_t, 1);
  posix_check_for_error (pthread_mutex_init ((pthread_mutex_t *) result, NULL));
  return result;
}

static void
g_mutex_free_posix_impl (GMutex * mutex)
{
  posix_check_for_error (pthread_mutex_destroy ((pthread_mutex_t *) mutex));
  g_free (mutex);
}

/* NOTE: the functions g_mutex_lock and g_mutex_unlock may not use
   functions from gmem.c and gmessages.c; */

/* pthread_mutex_lock, pthread_mutex_unlock can be taken directly, as
   signature and semantic are right, but without error check then!!!!,
   we might want to change this therefore. */

static gboolean
g_mutex_trylock_posix_impl (GMutex * mutex)
{
  int result;

  result = pthread_mutex_trylock ((pthread_mutex_t *) mutex);
#ifdef HAVE_PTHREAD_MUTEX_TRYLOCK_POSIX
  if (result == EBUSY)
    return FALSE;
  posix_check_for_error (result);
#else
  if (result == 0)
    return FALSE;
#endif
  return TRUE;
}

static GCond *
g_cond_new_posix_impl (void)
{
  GCond *result = (GCond *) g_new (pthread_cond_t, 1);
  posix_check_for_error (pthread_cond_init ((pthread_cond_t *) result, NULL));
  return result;
}

/* pthread_cond_signal, pthread_cond_broadcast and pthread_cond_wait
   can be taken directly, as signature and semantic are right, but
   without error check then!!!!, we might want to change this
   therfore. */

#define G_MICROSEC 1000000
#define G_NANOSEC 1000000000

static gboolean
g_cond_timed_wait_posix_impl (GCond * cond,
			      GMutex * entered_mutex,
			      GTimeVal * abs_time)
{
  int result;
  struct timespec end_time;
  gboolean timed_out;

  g_return_val_if_fail (cond != NULL, FALSE);
  g_return_val_if_fail (entered_mutex != NULL, FALSE);

  if (!abs_time)
    {
      g_cond_wait (cond, entered_mutex);
      return TRUE;
    }

  end_time.tv_sec = abs_time->tv_sec;
  end_time.tv_nsec = abs_time->tv_usec * (G_NANOSEC / G_MICROSEC);
  g_assert (end_time.tv_nsec < G_NANOSEC);
  result = pthread_cond_timedwait ((pthread_cond_t *) cond,
				   (pthread_mutex_t *) entered_mutex,
				   &end_time);
#ifdef HAVE_PTHREAD_COND_TIMEDWAIT_POSIX
  timed_out = (result == ETIMEDOUT);
#else
  timed_out = (result == -1 && errno == EAGAIN);
#endif
  if (!timed_out)
    posix_check_for_error (result);
  return !timed_out;
}

static void
g_cond_free_posix_impl (GCond * cond)
{
  posix_check_for_error (pthread_cond_destroy ((pthread_cond_t *) cond));
  g_free (cond);
}

static GPrivate *
g_private_new_posix_impl (GDestroyNotify destructor)
{
  GPrivate *result = (GPrivate *) g_new (pthread_key_t, 1);
  posix_check_for_error (pthread_key_create ((pthread_key_t *) result,
					     destructor));
  return result;
}

/* NOTE: the functions g_private_get and g_private_set may not use
   functions from gmem.c and gmessages.c */

static void
g_private_set_posix_impl (GPrivate * private_key, gpointer value)
{
  if (!private_key)
    return;

  pthread_setspecific (*(pthread_key_t *) private_key, value);
}

static gpointer
g_private_get_posix_impl (GPrivate * private_key)
{
  if (!private_key)
    return NULL;
#ifdef HAVE_PTHREAD_GETSPECIFIC_POSIX
  return pthread_getspecific (*(pthread_key_t *) private_key);
#else /* HAVE_PTHREAD_GETSPECIFIC_POSIX */
  {
    void* data;
    pthread_getspecific (*(pthread_key_t *) private_key, &data);
    return data;
  }
#endif /* HAVE_PTHREAD_GETSPECIFIC_POSIX */
}

static GThreadFunctions g_thread_functions_for_glib_use_default =
{
  g_mutex_new_posix_impl,
  (void (*)(GMutex *)) pthread_mutex_lock,
  g_mutex_trylock_posix_impl,
  (void (*)(GMutex *)) pthread_mutex_unlock,
  g_mutex_free_posix_impl,
  g_cond_new_posix_impl,
  (void (*)(GCond *)) pthread_cond_signal,
  (void (*)(GCond *)) pthread_cond_broadcast,
  (void (*)(GCond *, GMutex *)) pthread_cond_wait,
  g_cond_timed_wait_posix_impl,
  g_cond_free_posix_impl,
  g_private_new_posix_impl,
  g_private_get_posix_impl,
  g_private_set_posix_impl
};
