/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gmain.c: Main loop abstraction, timeouts, and idle functions
 * Copyright 1998 Owen Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include "config.h"

/* uncomment the next line to get poll() debugging info */
/* #define G_MAIN_POLL_DEBUG */



#include "glib.h"
#include <sys/types.h>
#include <time.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif /* HAVE_SYS_TIME_H */
#ifdef GLIB_HAVE_SYS_POLL_H
#  include <sys/poll.h>
#  undef events	 /* AIX 4.1.5 & 4.3.2 define this for SVR3,4 compatibility */
#  undef revents /* AIX 4.1.5 & 4.3.2 define this for SVR3,4 compatibility */
#endif /* GLIB_HAVE_SYS_POLL_H */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <errno.h>

#ifdef NATIVE_WIN32
#define STRICT
#include <windows.h>
#endif /* NATIVE_WIN32 */

#ifdef _MSC_VER
#include <fcntl.h>
#include <io.h>
#endif /* _MSC_VER */

/* Types */

typedef struct _GTimeoutData GTimeoutData;
typedef struct _GSource GSource;
typedef struct _GPollRec GPollRec;

typedef enum
{
  G_SOURCE_READY = 1 << G_HOOK_FLAG_USER_SHIFT,
  G_SOURCE_CAN_RECURSE = 1 << (G_HOOK_FLAG_USER_SHIFT + 1)
} GSourceFlags;

struct _GSource
{
  GHook hook;
  gint priority;
  gpointer source_data;
};

struct _GMainLoop
{
  gboolean is_running;
};

struct _GTimeoutData
{
  GTimeVal    expiration;
  gint        interval;
  GSourceFunc callback;
};

struct _GPollRec
{
  gint priority;
  GPollFD *fd;
  GPollRec *next;
};

/* Forward declarations */

static gint     g_source_compare          (GHook      *a,
					   GHook      *b);
static void     g_source_destroy_func     (GHookList  *hook_list,
					   GHook      *hook);
static void     g_main_poll               (gint      timeout,
					   gboolean  use_priority, 
					   gint      priority);
static void     g_main_add_poll_unlocked  (gint      priority,
					   GPollFD  *fd);

static gboolean g_timeout_prepare      (gpointer  source_data, 
					GTimeVal *current_time,
					gint     *timeout,
					gpointer  user_data);
static gboolean g_timeout_check        (gpointer  source_data,
					GTimeVal *current_time,
					gpointer  user_data);
static gboolean g_timeout_dispatch     (gpointer  source_data,
					GTimeVal *current_time,
					gpointer  user_data);
static gboolean g_idle_prepare         (gpointer  source_data, 
					GTimeVal *current_time,
					gint     *timeout,
					gpointer  user_data);
static gboolean g_idle_check           (gpointer  source_data,
					GTimeVal *current_time,
					gpointer  user_data);
static gboolean g_idle_dispatch        (gpointer  source_data,
					GTimeVal *current_time,
					gpointer  user_data);

/* Data */

static GSList *pending_dispatches = NULL;
static GHookList source_list = { 0 };
static gint in_check_or_prepare = 0;

/* The following lock is used for both the list of sources
 * and the list of poll records
 */
G_LOCK_DEFINE_STATIC (main_loop);

static GSourceFuncs timeout_funcs =
{
  g_timeout_prepare,
  g_timeout_check,
  g_timeout_dispatch,
  g_free,
};

static GSourceFuncs idle_funcs =
{
  g_idle_prepare,
  g_idle_check,
  g_idle_dispatch,
  NULL,
};

static GPollRec *poll_records = NULL;
static GPollRec *poll_free_list = NULL;
static GMemChunk *poll_chunk;
static guint n_poll_records = 0;

#ifdef G_THREADS_ENABLED
#ifndef NATIVE_WIN32
/* this pipe is used to wake up the main loop when a source is added.
 */
static gint wake_up_pipe[2] = { -1, -1 };
#else /* NATIVE_WIN32 */
static HANDLE wake_up_semaphore = NULL;
#endif /* NATIVE_WIN32 */
static GPollFD wake_up_rec;
static gboolean poll_waiting = FALSE;
#endif /* G_THREADS_ENABLED */

#ifdef HAVE_POLL
static GPollFunc poll_func = (GPollFunc) poll;
#else	/* !HAVE_POLL */
#ifdef NATIVE_WIN32

static gint
g_poll (GPollFD *fds, guint nfds, gint timeout)
{
  HANDLE handles[MAXIMUM_WAIT_OBJECTS];
  GPollFD *f;
  DWORD ready;
  MSG msg;
  UINT timer;
  LONG prevcnt;
  gint poll_msgs = -1;
  gint nhandles = 0;

  for (f = fds; f < &fds[nfds]; ++f)
    if (f->fd >= 0)
      {
	if (f->events & G_IO_IN)
	  if (f->fd == G_WIN32_MSG_HANDLE)
	    poll_msgs = f - fds;
	  else
	    {
	      /* g_print ("g_poll: waiting for handle %#x\n", f->fd); */
	      handles[nhandles++] = (HANDLE) f->fd;
	    }
      }

  if (timeout == -1)
    timeout = INFINITE;

  if (poll_msgs >= 0)
    {
      /* Waiting for messages, and maybe events */
      if (nhandles == 0)
	{
	  if (timeout == INFINITE)
	    {
	      /* Waiting just for messages, infinite timeout
	       * -> Use PeekMessage, then WaitMessage
	       */
	      /* g_print ("WaitMessage, PeekMessage\n"); */
	      if (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
		ready = WAIT_OBJECT_0;
	      else if (!WaitMessage ())
		g_warning ("g_poll: WaitMessage failed");
	      ready = WAIT_OBJECT_0;
	    }
	  else if (timeout == 0)
	    {
	      /* Waiting just for messages, zero timeout
	       * -> Use PeekMessage
	       */
	      /* g_print ("PeekMessage\n"); */
	      if (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
		ready = WAIT_OBJECT_0;
	      else
		ready = WAIT_TIMEOUT;
	    }
	  else
	    {
	      /* Waiting just for messages, some timeout
	       * -> First try PeekMessage, then set a timer, wait for message,
	       * kill timer, use PeekMessage
	       */
	      /* g_print ("PeekMessage\n"); */
	      if (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
		ready = WAIT_OBJECT_0;
	      else if ((timer = SetTimer (NULL, 0, timeout, NULL)) == 0)
		g_warning ("g_poll: SetTimer failed");
	      else
		{
		  /* g_print ("WaitMessage\n"); */
		  WaitMessage ();
		  KillTimer (NULL, timer);
		  if (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
		    ready = WAIT_OBJECT_0;
		  else
		    ready = WAIT_TIMEOUT;
		}
	    }
	}
      else
	{
	  /* Wait for either message or event
	   * -> Use MsgWaitForMultipleObjects
	   */
	  /* g_print ("MsgWaitForMultipleObjects(%d, %d)\n", nhandles, timeout); */
	  ready = MsgWaitForMultipleObjects (nhandles, handles, FALSE,
					     timeout, QS_ALLINPUT);
	  /* g_print("=%d\n", ready); */
	  if (ready == WAIT_FAILED)
	    g_warning ("g_poll: MsgWaitForMultipleObjects failed");
	}
    }
  else if (nhandles == 0)
    {
      /* Wait for nothing (huh?) */
      return 0;
    }
  else
    {
      /* Wait for just events
       * -> Use WaitForMultipleObjects
       */
      /* g_print ("WaitForMultipleObjects(%d, %d)\n", nhandles, timeout); */
      ready = WaitForMultipleObjects (nhandles, handles, FALSE, timeout);
      /* g_print("=%d\n", ready); */
      if (ready == WAIT_FAILED)
	g_warning ("g_poll: WaitForMultipleObjects failed");
    }

  for (f = fds; f < &fds[nfds]; ++f)
    f->revents = 0;

  if (ready == WAIT_FAILED)
    return -1;
  else if (poll_msgs >= 0 && ready == WAIT_OBJECT_0 + nhandles)
    {
      fds[poll_msgs].revents |= G_IO_IN;
    }
  else if (ready >= WAIT_OBJECT_0 && ready < WAIT_OBJECT_0 + nhandles)
    for (f = fds; f < &fds[nfds]; ++f)
      {
	if ((f->events & G_IO_IN)
	    && f->fd == (gint) handles[ready - WAIT_OBJECT_0])
	  {
	    f->revents |= G_IO_IN;
	    /* g_print ("event %#x\n", f->fd); */
	    ResetEvent ((HANDLE) f->fd);
	  }
      }
    
  if (ready == WAIT_TIMEOUT)
    return 0;
  else
    return 1;
}

#else  /* !NATIVE_WIN32 */

/* The following implementation of poll() comes from the GNU C Library.
 * Copyright (C) 1994, 1996, 1997 Free Software Foundation, Inc.
 */

#include <string.h> /* for bzero on BSD systems */

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H_ */

#ifndef NO_FD_SET
#  define SELECT_MASK fd_set
#else /* !NO_FD_SET */
#  ifndef _AIX
typedef long fd_mask;
#  endif /* _AIX */
#  ifdef _IBMR2
#    define SELECT_MASK void
#  else /* !_IBMR2 */
#    define SELECT_MASK int
#  endif /* !_IBMR2 */
#endif /* !NO_FD_SET */

static gint 
g_poll (GPollFD *fds,
	guint    nfds,
	gint     timeout)
{
  struct timeval tv;
  SELECT_MASK rset, wset, xset;
  GPollFD *f;
  int ready;
  int maxfd = 0;

  FD_ZERO (&rset);
  FD_ZERO (&wset);
  FD_ZERO (&xset);

  for (f = fds; f < &fds[nfds]; ++f)
    if (f->fd >= 0)
      {
	if (f->events & G_IO_IN)
	  FD_SET (f->fd, &rset);
	if (f->events & G_IO_OUT)
	  FD_SET (f->fd, &wset);
	if (f->events & G_IO_PRI)
	  FD_SET (f->fd, &xset);
	if (f->fd > maxfd && (f->events & (G_IO_IN|G_IO_OUT|G_IO_PRI)))
	  maxfd = f->fd;
      }

  tv.tv_sec = timeout / 1000;
  tv.tv_usec = (timeout % 1000) * 1000;

  ready = select (maxfd + 1, &rset, &wset, &xset,
		  timeout == -1 ? NULL : &tv);
  if (ready > 0)
    for (f = fds; f < &fds[nfds]; ++f)
      {
	f->revents = 0;
	if (f->fd >= 0)
	  {
	    if (FD_ISSET (f->fd, &rset))
	      f->revents |= G_IO_IN;
	    if (FD_ISSET (f->fd, &wset))
	      f->revents |= G_IO_OUT;
	    if (FD_ISSET (f->fd, &xset))
	      f->revents |= G_IO_PRI;
	  }
      }

  return ready;
}

#endif /* !NATIVE_WIN32 */

static GPollFunc poll_func = g_poll;
#endif	/* !HAVE_POLL */

/* Hooks for adding to the main loop */

/* Use knowledge of insert_sorted algorithm here to make
 * sure we insert at the end of equal priority items
 */
static gint
g_source_compare (GHook *a,
		  GHook *b)
{
  GSource *source_a = (GSource *)a;
  GSource *source_b = (GSource *)b;

  return (source_a->priority < source_b->priority) ? -1 : 1;
}

/* HOLDS: main_loop lock */
static void
g_source_destroy_func (GHookList *hook_list,
		       GHook     *hook)
{
  GSource *source = (GSource*) hook;
  GDestroyNotify destroy;

  G_UNLOCK (main_loop);

  destroy = hook->destroy;
  if (destroy)
    destroy (hook->data);

  destroy = ((GSourceFuncs*) hook->func)->destroy;
  if (destroy)
    destroy (source->source_data);

  G_LOCK (main_loop);
}

guint 
g_source_add (gint           priority,
	      gboolean       can_recurse,
	      GSourceFuncs  *funcs,
	      gpointer       source_data, 
	      gpointer       user_data,
	      GDestroyNotify notify)
{
  guint return_val;
  GSource *source;

  G_LOCK (main_loop);

  if (!source_list.is_setup)
    {
      g_hook_list_init (&source_list, sizeof (GSource));

      source_list.hook_destroy = G_HOOK_DEFERRED_DESTROY;
      source_list.hook_free = g_source_destroy_func;
    }

  source = (GSource*) g_hook_alloc (&source_list);
  source->priority = priority;
  source->source_data = source_data;
  source->hook.func = funcs;
  source->hook.data = user_data;
  source->hook.destroy = notify;
  
  g_hook_insert_sorted (&source_list, 
			(GHook *)source, 
			g_source_compare);

  if (can_recurse)
    source->hook.flags |= G_SOURCE_CAN_RECURSE;

  return_val = source->hook.hook_id;

#ifdef G_THREADS_ENABLED
  /* Now wake up the main loop if it is waiting in the poll() */

  if (poll_waiting)
    {
      poll_waiting = FALSE;
#ifndef NATIVE_WIN32
      write (wake_up_pipe[1], "A", 1);
#else
      ReleaseSemaphore (wake_up_semaphore, 1, NULL);
#endif
    }
#endif
  G_UNLOCK (main_loop);

  return return_val;
}

gboolean
g_source_remove (guint tag)
{
  GHook *hook;

  g_return_val_if_fail (tag > 0, FALSE);

  G_LOCK (main_loop);

  hook = g_hook_get (&source_list, tag);
  if (hook)
    g_hook_destroy_link (&source_list, hook);

  G_UNLOCK (main_loop);

  return hook != NULL;
}

gboolean
g_source_remove_by_user_data (gpointer user_data)
{
  GHook *hook;
  
  G_LOCK (main_loop);
  
  hook = g_hook_find_data (&source_list, TRUE, user_data);
  if (hook)
    g_hook_destroy_link (&source_list, hook);

  G_UNLOCK (main_loop);

  return hook != NULL;
}

static gboolean
g_source_find_source_data (GHook	*hook,
			   gpointer	 data)
{
  GSource *source = (GSource *)hook;

  return (source->source_data == data);
}

gboolean
g_source_remove_by_source_data (gpointer source_data)
{
  GHook *hook;

  G_LOCK (main_loop);

  hook = g_hook_find (&source_list, TRUE, 
		      g_source_find_source_data, source_data);
  if (hook)
    g_hook_destroy_link (&source_list, hook);

  G_UNLOCK (main_loop);

  return hook != NULL;
}

static gboolean
g_source_find_funcs_user_data (GHook   *hook,
			       gpointer data)
{
  gpointer *d = data;

  return hook->func == d[0] && hook->data == d[1];
}

gboolean
g_source_remove_by_funcs_user_data (GSourceFuncs *funcs,
				    gpointer      user_data)
{
  gpointer d[2];
  GHook *hook;

  g_return_val_if_fail (funcs != NULL, FALSE);

  G_LOCK (main_loop);

  d[0] = funcs;
  d[1] = user_data;

  hook = g_hook_find (&source_list, TRUE,
		      g_source_find_funcs_user_data, d);
  if (hook)
    g_hook_destroy_link (&source_list, hook);

  G_UNLOCK (main_loop);

  return hook != NULL;
}

void
g_get_current_time (GTimeVal *result)
{
#ifndef _MSC_VER
  struct timeval r;
  g_return_if_fail (result != NULL);

  /*this is required on alpha, there the timeval structs are int's
    not longs and a cast only would fail horribly*/
  gettimeofday (&r, NULL);
  result->tv_sec = r.tv_sec;
  result->tv_usec = r.tv_usec;
#else
  /* Avoid calling time() except for the first time.
   * GetTickCount() should be pretty fast and low-level?
   * I could also use ftime() but it seems unnecessarily overheady.
   */
  static DWORD start_tick = 0;
  static time_t start_time;
  DWORD tick;
  time_t t;

  g_return_if_fail (result != NULL);
 
  if (start_tick == 0)
    {
      start_tick = GetTickCount ();
      time (&start_time);
    }

  tick = GetTickCount ();

  result->tv_sec = (tick - start_tick) / 1000 + start_time;
  result->tv_usec = ((tick - start_tick) % 1000) * 1000;
#endif
}

/* Running the main loop */

/* HOLDS: main_loop_lock */
static void
g_main_dispatch (GTimeVal *current_time)
{
  while (pending_dispatches != NULL)
    {
      gboolean need_destroy;
      GSource *source = pending_dispatches->data;
      GSList *tmp_list;

      tmp_list = pending_dispatches;
      pending_dispatches = g_slist_remove_link (pending_dispatches, pending_dispatches);
      g_slist_free_1 (tmp_list);

      if (G_HOOK_IS_VALID (source))
	{
	  gboolean was_in_call;
	  gpointer hook_data = source->hook.data;
	  gpointer source_data = source->source_data;
	  gboolean (*dispatch) (gpointer,
				GTimeVal *,
				gpointer);

	  dispatch = ((GSourceFuncs *) source->hook.func)->dispatch;
	  
	  was_in_call = G_HOOK_IN_CALL (source);
	  source->hook.flags |= G_HOOK_FLAG_IN_CALL;

	  G_UNLOCK (main_loop);
	  need_destroy = ! dispatch (source_data,
				     current_time,
				     hook_data);
	  G_LOCK (main_loop);

	  if (!was_in_call)
	    source->hook.flags &= ~G_HOOK_FLAG_IN_CALL;
	  
	  if (need_destroy && G_HOOK_IS_VALID (source))
	    g_hook_destroy_link (&source_list, (GHook *) source);
	}

      g_hook_unref (&source_list, (GHook*) source);
    }
}

/* g_main_iterate () runs a single iteration of the mainloop, or,
 * if !dispatch checks to see if any sources need dispatching.
 * basic algorithm for dispatch=TRUE:
 *
 * 1) while the list of currently pending sources is non-empty,
 *    we call (*dispatch) on those that are !IN_CALL or can_recurse,
 *    removing sources from the list after each returns.
 *    the return value of (*dispatch) determines whether the source
 *    itself is kept alive.
 *
 * 2) call (*prepare) for sources that are not yet SOURCE_READY and
 *    are !IN_CALL or can_recurse. a return value of TRUE determines
 *    that the source would like to be dispatched immediatedly, it
 *    is then flagged as SOURCE_READY.
 *
 * 3) poll with the pollfds from all sources at the priority of the
 *    first source flagged as SOURCE_READY. if there are any sources
 *    flagged as SOURCE_READY, we use a timeout of 0 or the minimum
 *    of all timouts otherwise.
 *
 * 4) for each source !IN_CALL or can_recurse, if SOURCE_READY or
 *    (*check) returns true, add the source to the pending list.
 *    once one source returns true, stop after checking all sources
 *    at that priority.
 *
 * 5) while the list of currently pending sources is non-empty,
 *    call (*dispatch) on each source, removing the source
 *    after the call.
 *
 */
static gboolean
g_main_iterate (gboolean block,
		gboolean dispatch)
{
  GHook *hook;
  GTimeVal current_time  ={ 0, 0 };
  gint n_ready = 0;
  gint current_priority = 0;
  gint timeout;
  gboolean retval = FALSE;

  g_return_val_if_fail (!block || dispatch, FALSE);

  g_get_current_time (&current_time);

  G_LOCK (main_loop);
  
  /* If recursing, finish up current dispatch, before starting over */
  if (pending_dispatches)
    {
      if (dispatch)
	g_main_dispatch (&current_time);
      
      G_UNLOCK (main_loop);

      return TRUE;
    }

  /* Prepare all sources */

  timeout = block ? -1 : 0;
  
  hook = g_hook_first_valid (&source_list, TRUE);
  while (hook)
    {
      GSource *source = (GSource*) hook;
      gint source_timeout = -1;

      if ((n_ready > 0) && (source->priority > current_priority))
	{
	  g_hook_unref (&source_list, hook);
	  break;
	}
      if (G_HOOK_IN_CALL (hook) && !(hook->flags & G_SOURCE_CAN_RECURSE))
	{
	  hook = g_hook_next_valid (&source_list, hook, TRUE);
	  continue;
	}

      if (!(hook->flags & G_SOURCE_READY))
	{
	  gboolean (*prepare)  (gpointer  source_data, 
				GTimeVal *current_time,
				gint     *timeout,
				gpointer  user_data);

	  prepare = ((GSourceFuncs *) hook->func)->prepare;
	  in_check_or_prepare++;
	  G_UNLOCK (main_loop);

	  if ((*prepare) (source->source_data, &current_time, &source_timeout, source->hook.data))
	    hook->flags |= G_SOURCE_READY;
	  
	  G_LOCK (main_loop);
	  in_check_or_prepare--;
	}

      if (hook->flags & G_SOURCE_READY)
	{
	  if (!dispatch)
	    {
	      g_hook_unref (&source_list, hook);
	      G_UNLOCK (main_loop);

	      return TRUE;
	    }
	  else
	    {
	      n_ready++;
	      current_priority = source->priority;
	      timeout = 0;
	    }
	}
      
      if (source_timeout >= 0)
	{
	  if (timeout < 0)
	    timeout = source_timeout;
	  else
	    timeout = MIN (timeout, source_timeout);
	}

      hook = g_hook_next_valid (&source_list, hook, TRUE);
    }

  /* poll(), if necessary */

  g_main_poll (timeout, n_ready > 0, current_priority);

  /* Check to see what sources need to be dispatched */

  n_ready = 0;
  
  hook = g_hook_first_valid (&source_list, TRUE);
  while (hook)
    {
      GSource *source = (GSource *)hook;

      if ((n_ready > 0) && (source->priority > current_priority))
	{
	  g_hook_unref (&source_list, hook);
	  break;
	}
      if (G_HOOK_IN_CALL (hook) && !(hook->flags & G_SOURCE_CAN_RECURSE))
	{
	  hook = g_hook_next_valid (&source_list, hook, TRUE);
	  continue;
	}

      if (!(hook->flags & G_SOURCE_READY))
	{
	  gboolean (*check) (gpointer  source_data,
			     GTimeVal *current_time,
			     gpointer  user_data);

	  check = ((GSourceFuncs *) hook->func)->check;
	  in_check_or_prepare++;
	  G_UNLOCK (main_loop);
	  
	  if ((*check) (source->source_data, &current_time, source->hook.data))
	    hook->flags |= G_SOURCE_READY;

	  G_LOCK (main_loop);
	  in_check_or_prepare--;
	}

      if (hook->flags & G_SOURCE_READY)
	{
	  if (dispatch)
	    {
	      hook->flags &= ~G_SOURCE_READY;
	      g_hook_ref (&source_list, hook);
	      pending_dispatches = g_slist_prepend (pending_dispatches, source);
	      current_priority = source->priority;
	      n_ready++;
	    }
	  else
	    {
	      g_hook_unref (&source_list, hook);
	      G_UNLOCK (main_loop);

	      return TRUE;
	    }
	}
      
      hook = g_hook_next_valid (&source_list, hook, TRUE);
    }

  /* Now invoke the callbacks */

  if (pending_dispatches)
    {
      pending_dispatches = g_slist_reverse (pending_dispatches);
      g_main_dispatch (&current_time);
      retval = TRUE;
    }

  G_UNLOCK (main_loop);

  return retval;
}

/* See if any events are pending
 */
gboolean 
g_main_pending (void)
{
  return in_check_or_prepare ? FALSE : g_main_iterate (FALSE, FALSE);
}

/* Run a single iteration of the mainloop. If block is FALSE,
 * will never block
 */
gboolean
g_main_iteration (gboolean block)
{
  if (in_check_or_prepare)
    {
      g_warning ("g_main_iteration(): called recursively from within a source's check() or "
		 "prepare() member or from a second thread, iteration not possible");
      return FALSE;
    }
  else
    return g_main_iterate (block, TRUE);
}

GMainLoop*
g_main_new (gboolean is_running)
{
  GMainLoop *loop;

  loop = g_new0 (GMainLoop, 1);
  loop->is_running = is_running != FALSE;

  return loop;
}

void 
g_main_run (GMainLoop *loop)
{
  g_return_if_fail (loop != NULL);

  if (in_check_or_prepare)
    {
      g_warning ("g_main_run(): called recursively from within a source's check() or "
		 "prepare() member or from a second thread, iteration not possible");
      return;
    }

  loop->is_running = TRUE;
  while (loop->is_running)
    g_main_iterate (TRUE, TRUE);
}

void 
g_main_quit (GMainLoop *loop)
{
  g_return_if_fail (loop != NULL);

  loop->is_running = FALSE;
}

void 
g_main_destroy (GMainLoop *loop)
{
  g_return_if_fail (loop != NULL);

  g_free (loop);
}

gboolean
g_main_is_running (GMainLoop *loop)
{
  g_return_val_if_fail (loop != NULL, FALSE);

  return loop->is_running;
}

/* HOLDS: main_loop_lock */
static void
g_main_poll (gint     timeout,
	     gboolean use_priority,
	     gint     priority)
{
#ifdef  G_MAIN_POLL_DEBUG
  GTimer *poll_timer;
#endif
  GPollFD *fd_array;
  GPollRec *pollrec;
  gint i;
  gint npoll;

#ifdef G_THREADS_ENABLED
#ifndef NATIVE_WIN32
  if (wake_up_pipe[0] < 0)
    {
      if (pipe (wake_up_pipe) < 0)
	g_error ("Cannot create pipe main loop wake-up: %s\n",
		 g_strerror (errno));

      wake_up_rec.fd = wake_up_pipe[0];
      wake_up_rec.events = G_IO_IN;
      g_main_add_poll_unlocked (0, &wake_up_rec);
    }
#else
  if (wake_up_semaphore == NULL)
    {
      if ((wake_up_semaphore = CreateSemaphore (NULL, 0, 100, NULL)) == NULL)
	g_error ("Cannot create wake-up semaphore: %d", GetLastError ());
      wake_up_rec.fd = (gint) wake_up_semaphore;
      wake_up_rec.events = G_IO_IN;
      g_main_add_poll_unlocked (0, &wake_up_rec);
    }
#endif
#endif
  fd_array = g_new (GPollFD, n_poll_records);
 
  pollrec = poll_records;
  i = 0;
  while (pollrec && (!use_priority || priority >= pollrec->priority))
    {
      if (pollrec->fd->events)
	{
	  fd_array[i].fd = pollrec->fd->fd;
	  fd_array[i].events = pollrec->fd->events;
	  fd_array[i].revents = 0;
	  i++;
	}
      
      pollrec = pollrec->next;
    }
#ifdef G_THREADS_ENABLED
  poll_waiting = TRUE;
#endif
  
  npoll = i;
  if (npoll || timeout != 0)
    {
#ifdef	G_MAIN_POLL_DEBUG
      g_print ("g_main_poll(%d) timeout: %d\r", npoll, timeout);
      poll_timer = g_timer_new ();
#endif
      
      G_UNLOCK (main_loop);
      (*poll_func) (fd_array, npoll, timeout);
      G_LOCK (main_loop);
      
#ifdef	G_MAIN_POLL_DEBUG
      g_print ("g_main_poll(%d) timeout: %d - elapsed %12.10f seconds",
	       npoll,
	       timeout,
	       g_timer_elapsed (poll_timer, NULL));
      g_timer_destroy (poll_timer);
      pollrec = poll_records;
      i = 0;
      while (i < npoll)
	{
	  if (pollrec->fd->events)
	    {
	      if (fd_array[i].revents)
		{
		  g_print (" [%d:", fd_array[i].fd);
		  if (fd_array[i].revents & G_IO_IN)
		    g_print ("i");
		  if (fd_array[i].revents & G_IO_OUT)
		    g_print ("o");
		  if (fd_array[i].revents & G_IO_PRI)
		    g_print ("p");
		  if (fd_array[i].revents & G_IO_ERR)
		    g_print ("e");
		  if (fd_array[i].revents & G_IO_HUP)
		    g_print ("h");
		  if (fd_array[i].revents & G_IO_NVAL)
		    g_print ("n");
		  g_print ("]");
		}
	      i++;
	    }
	  pollrec = pollrec->next;
	}
      g_print ("\n");
#endif
    } /* if (npoll || timeout != 0) */
  
#ifdef G_THREADS_ENABLED
  if (!poll_waiting)
    {
#ifndef NATIVE_WIN32
      gchar c;
      read (wake_up_pipe[0], &c, 1);
#endif
    }
  else
    poll_waiting = FALSE;
#endif

  pollrec = poll_records;
  i = 0;
  while (i < npoll)
    {
      if (pollrec->fd->events)
	{
	  pollrec->fd->revents = fd_array[i].revents;
	  i++;
	}
      pollrec = pollrec->next;
    }

  g_free (fd_array);
}

void 
g_main_add_poll (GPollFD *fd,
		 gint     priority)
{
  G_LOCK (main_loop);
  g_main_add_poll_unlocked (priority, fd);
  G_UNLOCK (main_loop);
}

/* HOLDS: main_loop_lock */
static void 
g_main_add_poll_unlocked (gint     priority,
			  GPollFD *fd)
{
  GPollRec *lastrec, *pollrec, *newrec;

  if (!poll_chunk)
    poll_chunk = g_mem_chunk_create (GPollRec, 32, G_ALLOC_ONLY);

  if (poll_free_list)
    {
      newrec = poll_free_list;
      poll_free_list = newrec->next;
    }
  else
    newrec = g_chunk_new (GPollRec, poll_chunk);

  newrec->fd = fd;
  newrec->priority = priority;

  lastrec = NULL;
  pollrec = poll_records;
  while (pollrec && priority >= pollrec->priority)
    {
      lastrec = pollrec;
      pollrec = pollrec->next;
    }
  
  if (lastrec)
    lastrec->next = newrec;
  else
    poll_records = newrec;

  newrec->next = pollrec;

  n_poll_records++;
}

void 
g_main_remove_poll (GPollFD *fd)
{
  GPollRec *pollrec, *lastrec;

  G_LOCK (main_loop);
  
  lastrec = NULL;
  pollrec = poll_records;

  while (pollrec)
    {
      if (pollrec->fd == fd)
	{
	  if (lastrec != NULL)
	    lastrec->next = pollrec->next;
	  else
	    poll_records = pollrec->next;

	  pollrec->next = poll_free_list;
	  poll_free_list = pollrec;

	  n_poll_records--;
	  break;
	}
      lastrec = pollrec;
      pollrec = pollrec->next;
    }

  G_UNLOCK (main_loop);
}

void 
g_main_set_poll_func (GPollFunc func)
{
  if (func)
    poll_func = func;
  else
#ifdef HAVE_POLL
    poll_func = (GPollFunc) poll;
#else
    poll_func = (GPollFunc) g_poll;
#endif
}

/* Timeouts */

static gboolean 
g_timeout_prepare  (gpointer  source_data, 
		    GTimeVal *current_time,
		    gint     *timeout,
		    gpointer  user_data)
{
  glong msec;
  GTimeoutData *data = source_data;

  msec = (data->expiration.tv_sec  - current_time->tv_sec) * 1000 +
         (data->expiration.tv_usec - current_time->tv_usec) / 1000;

  *timeout = (msec <= 0) ? 0 : msec;

  return (msec <= 0);
}

static gboolean 
g_timeout_check (gpointer  source_data,
		 GTimeVal *current_time,
		 gpointer  user_data)
{
  GTimeoutData *data = source_data;

  return (data->expiration.tv_sec < current_time->tv_sec) ||
         ((data->expiration.tv_sec == current_time->tv_sec) &&
	  (data->expiration.tv_usec <= current_time->tv_usec));
}

static gboolean
g_timeout_dispatch (gpointer source_data, 
		    GTimeVal *current_time,
		    gpointer user_data)
{
  GTimeoutData *data = source_data;

  if (data->callback (user_data))
    {
      guint seconds = data->interval / 1000;
      guint msecs = data->interval - seconds * 1000;

      data->expiration.tv_sec = current_time->tv_sec + seconds;
      data->expiration.tv_usec = current_time->tv_usec + msecs * 1000;
      if (data->expiration.tv_usec >= 1000000)
	{
	  data->expiration.tv_usec -= 1000000;
	  data->expiration.tv_sec++;
	}
      return TRUE;
    }
  else
    return FALSE;
}

guint 
g_timeout_add_full (gint           priority,
		    guint          interval, 
		    GSourceFunc    function,
		    gpointer       data,
		    GDestroyNotify notify)
{
  guint seconds;
  guint msecs;
  GTimeoutData *timeout_data = g_new (GTimeoutData, 1);

  timeout_data->interval = interval;
  timeout_data->callback = function;
  g_get_current_time (&timeout_data->expiration);

  seconds = timeout_data->interval / 1000;
  msecs = timeout_data->interval - seconds * 1000;

  timeout_data->expiration.tv_sec += seconds;
  timeout_data->expiration.tv_usec += msecs * 1000;
  if (timeout_data->expiration.tv_usec >= 1000000)
    {
      timeout_data->expiration.tv_usec -= 1000000;
      timeout_data->expiration.tv_sec++;
    }

  return g_source_add (priority, FALSE, &timeout_funcs, timeout_data, data, notify);
}

guint 
g_timeout_add (guint32        interval,
	       GSourceFunc    function,
	       gpointer       data)
{
  return g_timeout_add_full (G_PRIORITY_DEFAULT, 
			     interval, function, data, NULL);
}

/* Idle functions */

static gboolean 
g_idle_prepare  (gpointer  source_data, 
		 GTimeVal *current_time,
		 gint     *timeout,
		 gpointer  user_data)
{
  timeout = 0;
  return TRUE;
}

static gboolean 
g_idle_check    (gpointer  source_data,
		 GTimeVal *current_time,
		 gpointer  user_data)
{
  return TRUE;
}

static gboolean
g_idle_dispatch (gpointer source_data, 
		 GTimeVal *current_time,
		 gpointer user_data)
{
  GSourceFunc func = source_data;

  return func (user_data);
}

guint 
g_idle_add_full (gint           priority,
		 GSourceFunc    function,
		 gpointer       data,
		 GDestroyNotify notify)
{
  g_return_val_if_fail (function != NULL, 0);

  return g_source_add (priority, FALSE, &idle_funcs, function, data, notify);
}

guint 
g_idle_add (GSourceFunc    function,
	    gpointer       data)
{
  return g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, function, data, NULL);
}

gboolean
g_idle_remove_by_data (gpointer data)
{
  return g_source_remove_by_funcs_user_data (&idle_funcs, data);
}
