/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gthread.c: thread related functions
 * Copyright 1998 Sebastian Wilhelmi; University of Karlsruhe
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

static gboolean thread_system_already_initialized = FALSE;

#include G_THREAD_SOURCE

void g_mutex_init (void);
void g_mem_init (void);
void g_messages_init (void);

void
g_thread_init (GThreadFunctions* init)
{
  gboolean supported;

#ifndef	G_THREADS_ENABLED
  g_error ("GLib thread support is disabled.");
#endif	/* !G_THREADS_ENABLED */

  if (thread_system_already_initialized)
    g_error ("GThread system may only be initialized once.");
    
  thread_system_already_initialized = TRUE;

  if (init == NULL)
    init = &g_thread_functions_for_glib_use_default;
  else
    g_thread_use_default_impl = FALSE;

  g_thread_functions_for_glib_use = *init;

  /* It is important, that g_threads_got_initialized is not set before the
   * thread initialization functions of the different modules are called
   */

  supported = (init->mutex_new &&  
	       init->mutex_lock && 
	       init->mutex_trylock && 
	       init->mutex_unlock && 
	       init->mutex_free && 
	       init->cond_new && 
	       init->cond_signal && 
	       init->cond_broadcast && 
	       init->cond_wait && 
	       init->cond_timed_wait &&
	       init->cond_free &&
	       init->private_new &&
	       init->private_get &&
	       init->private_get);

  /* if somebody is calling g_thread_init (), it means that he wants to
   * have thread support, so check this
   */
  if (!supported)
    {
      if (g_thread_use_default_impl)
	g_error ("Threads are not supported on this platform.");
      else
	g_error ("The supplied thread function vector is invalid.");
    }

  /* now call the thread initialization functions of the different
   * glib modules. order does matter, g_mutex_init MUST come first.
   */
  g_mutex_init ();
  g_mem_init ();
  g_messages_init ();

  /* now we can set g_threads_got_initialized and thus enable
   * all the thread functions
   */
  g_threads_got_initialized = TRUE;
}
