/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gmutex.c: MT safety related functions
 * Copyright 1998 Sebastian Wilhelmi; University of Karlsruhe
 *                Owen Taylor
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

#include "glib.h"

typedef struct _GStaticPrivateNode GStaticPrivateNode;

struct _GStaticPrivateNode
{
  gpointer       data;
  GDestroyNotify destroy;
};

static void g_static_private_free_data (gpointer data);
static void g_thread_fail (void);

/* Global variables */

gboolean g_thread_use_default_impl = TRUE;
gboolean g_threads_got_initialized = FALSE;

GThreadFunctions g_thread_functions_for_glib_use = {
  (GMutex*(*)())g_thread_fail,                 /* mutex_new */
  NULL,                                        /* mutex_lock */
  NULL,                                        /* mutex_trylock */
  NULL,                                        /* mutex_unlock */
  NULL,                                        /* mutex_free */
  (GCond*(*)())g_thread_fail,                  /* cond_new */
  NULL,                                        /* cond_signal */
  NULL,                                        /* cond_broadcast */
  NULL,                                        /* cond_wait */
  NULL,                                        /* cond_timed_wait  */
  NULL,                                        /* cond_free */
  (GPrivate*(*)(GDestroyNotify))g_thread_fail, /* private_new */
  NULL,                                        /* private_get */
  NULL,                                        /* private_set */
}; 

/* Local data */

static GMutex   *g_mutex_protect_static_mutex_allocation = NULL;
static GMutex   *g_thread_specific_mutex = NULL;
static GPrivate *g_thread_specific_private = NULL;

/* This must be called only once, before any threads are created.
 * It will only be called from g_thread_init() in -lgthread.
 */
void
g_mutex_init (void)
{
  /* We let the main thread (the one that calls g_thread_init) inherit
   * the data, that it set before calling g_thread_init
   */
  gpointer private_old = g_thread_specific_private;

  g_thread_specific_private = g_private_new (g_static_private_free_data);

  /* we can not use g_private_set here, as g_threads_got_initialized is not
   * yet set TRUE, whereas the private_set function is already set.
   */
  g_thread_functions_for_glib_use.private_set (g_thread_specific_private, 
					       private_old);

  g_mutex_protect_static_mutex_allocation = g_mutex_new();
  g_thread_specific_mutex = g_mutex_new();
}

GMutex *
g_static_mutex_get_mutex_impl (GMutex** mutex)
{
  if (!g_thread_supported ())
    return NULL;

  g_assert (g_mutex_protect_static_mutex_allocation);

  g_mutex_lock (g_mutex_protect_static_mutex_allocation);

  if (!(*mutex)) 
    *mutex = g_mutex_new(); 

  g_mutex_unlock (g_mutex_protect_static_mutex_allocation);
  
  return *mutex;
}

gpointer
g_static_private_get (GStaticPrivate *private_key)
{
  GArray *array;

  array = g_private_get (g_thread_specific_private);
  if (!array)
    return NULL;

  if (!private_key->index)
    return NULL;
  else if (private_key->index <= array->len)
    return g_array_index (array, GStaticPrivateNode, private_key->index - 1).data;
  else
    return NULL;
}

void
g_static_private_set (GStaticPrivate *private_key, 
		      gpointer        data,
		      GDestroyNotify  notify)
{
  GArray *array;
  static guint next_index = 0;
  GStaticPrivateNode *node;
  
  array = g_private_get (g_thread_specific_private);
  if (!array)
    {
      array = g_array_new (FALSE, TRUE, sizeof (GStaticPrivateNode));
      g_private_set (g_thread_specific_private, array);
    }

  if (!private_key->index)
    {
      g_mutex_lock (g_thread_specific_mutex);

      if (!private_key->index)
	private_key->index = ++next_index;

      g_mutex_unlock (g_thread_specific_mutex);
    }

  if (private_key->index > array->len)
    g_array_set_size (array, private_key->index);

  node = &g_array_index (array, GStaticPrivateNode, private_key->index - 1);
  if (node->destroy)
    {
      gpointer ddata = node->data;
      GDestroyNotify ddestroy = node->destroy;

      node->data = data;
      node->destroy = notify;

      ddestroy (ddata);
    }
  else
    {
      node->data = data;
      node->destroy = notify;
    }
}

static void
g_static_private_free_data (gpointer data)
{
  if (data)
    {
      GArray* array = data;
      guint i;
      
      for (i = 0; i < array->len; i++ )
	{
	  GStaticPrivateNode *node = &g_array_index (array, GStaticPrivateNode, i);
	  if (node->destroy)
	    node->destroy (node->data);
	}
    }
}

static void
g_thread_fail (void)
{
  g_error ("The thread system is not yet initialized.");
}
