/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GHook: Callback maintenance functions
 * Copyright (C) 1998 Tim Janik
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

#include	"glib.h"


/* --- defines --- */
#define	G_HOOKS_PREALLOC	(16)


/* --- functions --- */
void
g_hook_list_init (GHookList *hook_list,
		  guint	     hook_size)
{
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_size >= sizeof (GHook));
  
  hook_list->seq_id = 1;
  hook_list->hook_size = hook_size;
  hook_list->is_setup = TRUE;
  hook_list->hooks = NULL;
  hook_list->hook_memchunk = g_mem_chunk_new ("GHook Memchunk",
					      hook_size,
					      hook_size * G_HOOKS_PREALLOC,
					      G_ALLOC_AND_FREE);
  hook_list->hook_free = NULL;
  hook_list->hook_destroy = NULL;
}

void
g_hook_list_clear (GHookList *hook_list)
{
  g_return_if_fail (hook_list != NULL);
  
  if (hook_list->is_setup)
    {
      GHook *hook;
      
      hook_list->is_setup = FALSE;
      
      hook = hook_list->hooks;
      if (!hook)
	{
	  g_mem_chunk_destroy (hook_list->hook_memchunk);
	  hook_list->hook_memchunk = NULL;
	}
      else
	do
	  {
	    GHook *tmp;
	    
	    g_hook_ref (hook_list, hook);
	    g_hook_destroy_link (hook_list, hook);
	    tmp = hook->next;
	    g_hook_unref (hook_list, hook);
	    hook = tmp;
	  }
	while (hook);
    }
}

GHook*
g_hook_alloc (GHookList *hook_list)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  g_return_val_if_fail (hook_list->is_setup, NULL);
  
  hook = g_chunk_new0 (GHook, hook_list->hook_memchunk);
  hook->data = NULL;
  hook->next = NULL;
  hook->prev = NULL;
  hook->flags = G_HOOK_FLAG_ACTIVE;
  hook->ref_count = 0;
  hook->hook_id = 0;
  hook->func = NULL;
  hook->destroy = NULL;
  
  return hook;
}

void
g_hook_free (GHookList *hook_list,
	     GHook     *hook)
{
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  g_return_if_fail (hook != NULL);
  g_return_if_fail (G_HOOK_IS_UNLINKED (hook));

  if (hook_list->hook_free)
    hook_list->hook_free (hook_list, hook);
  
  g_chunk_free (hook, hook_list->hook_memchunk);
}

void
g_hook_destroy_link (GHookList *hook_list,
		     GHook     *hook)
{
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook != NULL);
  
  if (hook->hook_id)
    {
      hook->hook_id = 0;
      hook->flags &= ~G_HOOK_FLAG_ACTIVE;
      if (hook_list->hook_destroy)
	{
	  if (hook_list->hook_destroy != G_HOOK_DEFERRED_DESTROY)
	    hook_list->hook_destroy (hook_list, hook);
	}
      else if (hook->destroy)
	{
	  hook->destroy (hook->data);
	  hook->data = NULL;
	  hook->func = NULL;
	  hook->destroy = NULL;
	}
      g_hook_unref (hook_list, hook); /* counterpart to g_hook_insert_before */
    }
}

gboolean
g_hook_destroy (GHookList   *hook_list,
		guint	     hook_id)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, FALSE);
  g_return_val_if_fail (hook_id > 0, FALSE);
  
  hook = g_hook_get (hook_list, hook_id);
  if (hook)
    {
      g_hook_destroy_link (hook_list, hook);
      return TRUE;
    }
  
  return FALSE;
}

void
g_hook_unref (GHookList *hook_list,
	      GHook	*hook)
{
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->hook_memchunk != NULL);
  g_return_if_fail (hook != NULL);
  g_return_if_fail (hook->ref_count > 0);
  
  hook->ref_count--;
  if (!hook->ref_count)
    {
      g_return_if_fail (hook->hook_id == 0);
      g_return_if_fail (!G_HOOK_IN_CALL (hook));
      
      if (hook->prev)
	hook->prev->next = hook->next;
      else
	hook_list->hooks = hook->next;
      if (hook->next)
	{
	  hook->next->prev = hook->prev;
	  hook->next = NULL;
	}
      hook->prev = NULL;

      if (!hook_list->is_setup)
	{
	  hook_list->is_setup = TRUE;
	  g_hook_free (hook_list, hook);
	  hook_list->is_setup = FALSE;
      
	  if (!hook_list->hooks)
	    {
	      g_mem_chunk_destroy (hook_list->hook_memchunk);
	      hook_list->hook_memchunk = NULL;
	    }
	}
      else
	g_hook_free (hook_list, hook);
    }
}

void
g_hook_ref (GHookList *hook_list,
	    GHook     *hook)
{
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook != NULL);
  g_return_if_fail (hook->ref_count > 0);
  
  hook->ref_count++;
}

void
g_hook_prepend (GHookList *hook_list,
		GHook	  *hook)
{
  g_return_if_fail (hook_list != NULL);
  
  g_hook_insert_before (hook_list, hook_list->hooks, hook);
}

void
g_hook_insert_before (GHookList *hook_list,
		      GHook	*sibling,
		      GHook	*hook)
{
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  g_return_if_fail (hook != NULL);
  g_return_if_fail (G_HOOK_IS_UNLINKED (hook));
  g_return_if_fail (hook->func != NULL);
  
  hook->hook_id = hook_list->seq_id++;
  hook->ref_count = 1; /* counterpart to g_hook_destroy_link */
  
  if (sibling)
    {
      if (sibling->prev)
	{
	  hook->prev = sibling->prev;
	  hook->prev->next = hook;
	  hook->next = sibling;
	  sibling->prev = hook;
	}
      else
	{
	  hook_list->hooks = hook;
	  hook->next = sibling;
	  sibling->prev = hook;
	}
    }
  else
    {
      if (hook_list->hooks)
	{
	  sibling = hook_list->hooks;
	  while (sibling->next)
	    sibling = sibling->next;
	  hook->prev = sibling;
	  sibling->next = hook;
	}
      else
	hook_list->hooks = hook;
    }
}

void
g_hook_list_invoke (GHookList *hook_list,
		    gboolean   may_recurse)
{
  GHook *hook;
  
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);

  hook = g_hook_first_valid (hook_list, may_recurse);
  while (hook)
    {
      GHookFunc func;
      gboolean was_in_call;
      
      func = (GHookFunc) hook->func;
      
      was_in_call = G_HOOK_IN_CALL (hook);
      hook->flags |= G_HOOK_FLAG_IN_CALL;
      func (hook->data);
      if (!was_in_call)
	hook->flags &= ~G_HOOK_FLAG_IN_CALL;
      
      hook = g_hook_next_valid (hook_list, hook, may_recurse);
    }
}

void
g_hook_list_invoke_check (GHookList *hook_list,
			  gboolean   may_recurse)
{
  GHook *hook;
  
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  
  hook = g_hook_first_valid (hook_list, may_recurse);
  while (hook)
    {
      GHookCheckFunc func;
      gboolean was_in_call;
      gboolean need_destroy;
      
      func = (GHookCheckFunc) hook->func;
      
      was_in_call = G_HOOK_IN_CALL (hook);
      hook->flags |= G_HOOK_FLAG_IN_CALL;
      need_destroy = !func (hook->data);
      if (!was_in_call)
	hook->flags &= ~G_HOOK_FLAG_IN_CALL;
      if (need_destroy)
	g_hook_destroy_link (hook_list, hook);
      
      hook = g_hook_next_valid (hook_list, hook, may_recurse);
    }
}

void
g_hook_list_marshal_check (GHookList	       *hook_list,
			   gboolean		may_recurse,
			   GHookCheckMarshaller marshaller,
			   gpointer		data)
{
  GHook *hook;
  
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  g_return_if_fail (marshaller != NULL);
  
  hook = g_hook_first_valid (hook_list, may_recurse);
  while (hook)
    {
      gboolean was_in_call;
      gboolean need_destroy;
      
      was_in_call = G_HOOK_IN_CALL (hook);
      hook->flags |= G_HOOK_FLAG_IN_CALL;
      need_destroy = !marshaller (hook, data);
      if (!was_in_call)
	hook->flags &= ~G_HOOK_FLAG_IN_CALL;
      if (need_destroy)
	g_hook_destroy_link (hook_list, hook);
      
      hook = g_hook_next_valid (hook_list, hook, may_recurse);
    }
}

void
g_hook_list_marshal (GHookList		     *hook_list,
		     gboolean		      may_recurse,
		     GHookMarshaller	      marshaller,
		     gpointer		      data)
{
  GHook *hook;
  
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  g_return_if_fail (marshaller != NULL);
  
  hook = g_hook_first_valid (hook_list, may_recurse);
  while (hook)
    {
      gboolean was_in_call;
      
      was_in_call = G_HOOK_IN_CALL (hook);
      hook->flags |= G_HOOK_FLAG_IN_CALL;
      marshaller (hook, data);
      if (!was_in_call)
	hook->flags &= ~G_HOOK_FLAG_IN_CALL;
      
      hook = g_hook_next_valid (hook_list, hook, may_recurse);
    }
}

GHook*
g_hook_first_valid (GHookList *hook_list,
		    gboolean   may_be_in_call)
{
  g_return_val_if_fail (hook_list != NULL, NULL);
  
  if (hook_list->is_setup)
    {
      GHook *hook;
      
      hook = hook_list->hooks;
      if (hook)
	{
	  g_hook_ref (hook_list, hook);
	  if (G_HOOK_IS_VALID (hook) && (may_be_in_call || !G_HOOK_IN_CALL (hook)))
	    return hook;
	  else
	    return g_hook_next_valid (hook_list, hook, may_be_in_call);
	}
    }
  
  return NULL;
}

GHook*
g_hook_next_valid (GHookList *hook_list,
		   GHook     *hook,
		   gboolean   may_be_in_call)
{
  GHook *ohook = hook;

  g_return_val_if_fail (hook_list != NULL, NULL);

  if (!hook)
    return NULL;
  
  hook = hook->next;
  while (hook)
    {
      if (G_HOOK_IS_VALID (hook) && (may_be_in_call || !G_HOOK_IN_CALL (hook)))
	{
	  g_hook_ref (hook_list, hook);
	  g_hook_unref (hook_list, ohook);
	  
	  return hook;
	}
      hook = hook->next;
    }
  g_hook_unref (hook_list, ohook);

  return NULL;
}

GHook*
g_hook_get (GHookList *hook_list,
	    guint      hook_id)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  g_return_val_if_fail (hook_id > 0, NULL);
  
  hook = hook_list->hooks;
  while (hook)
    {
      if (hook->hook_id == hook_id)
	return hook;
      hook = hook->next;
    }
  
  return NULL;
}

GHook*
g_hook_find (GHookList	  *hook_list,
	     gboolean	   need_valids,
	     GHookFindFunc func,
	     gpointer	   data)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  g_return_val_if_fail (func != NULL, NULL);
  
  hook = hook_list->hooks;
  while (hook)
    {
      GHook *tmp;

      /* test only non-destroyed hooks */
      if (!hook->hook_id)
	{
	  hook = hook->next;
	  continue;
	}
      
      g_hook_ref (hook_list, hook);
      
      if (func (hook, data) && hook->hook_id && (!need_valids || G_HOOK_ACTIVE (hook)))
	{
	  g_hook_unref (hook_list, hook);
	  
	  return hook;
	}

      tmp = hook->next;
      g_hook_unref (hook_list, hook);
      hook = tmp;
    }
  
  return NULL;
}

GHook*
g_hook_find_data (GHookList *hook_list,
		  gboolean   need_valids,
		  gpointer   data)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  
  hook = hook_list->hooks;
  while (hook)
    {
      /* test only non-destroyed hooks */
      if (hook->data == data &&
	  hook->hook_id &&
	  (!need_valids || G_HOOK_ACTIVE (hook)))
	return hook;

      hook = hook->next;
    }
  
  return NULL;
}

GHook*
g_hook_find_func (GHookList *hook_list,
		  gboolean   need_valids,
		  gpointer   func)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  g_return_val_if_fail (func != NULL, NULL);
  
  hook = hook_list->hooks;
  while (hook)
    {
      /* test only non-destroyed hooks */
      if (hook->func == func &&
	  hook->hook_id &&
	  (!need_valids || G_HOOK_ACTIVE (hook)))
	return hook;

      hook = hook->next;
    }
  
  return NULL;
}

GHook*
g_hook_find_func_data (GHookList *hook_list,
		       gboolean	  need_valids,
		       gpointer	  func,
		       gpointer	  data)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  g_return_val_if_fail (func != NULL, NULL);
  
  hook = hook_list->hooks;
  while (hook)
    {
      /* test only non-destroyed hooks */
      if (hook->data == data &&
	  hook->func == func &&
	  hook->hook_id &&
	  (!need_valids || G_HOOK_ACTIVE (hook)))
	return hook;

      hook = hook->next;
    }
  
  return NULL;
}

void
g_hook_insert_sorted (GHookList	      *hook_list,
		      GHook	      *hook,
		      GHookCompareFunc func)
{
  GHook *sibling;
  
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  g_return_if_fail (hook != NULL);
  g_return_if_fail (G_HOOK_IS_UNLINKED (hook));
  g_return_if_fail (hook->func != NULL);
  g_return_if_fail (func != NULL);

  /* first non-destroyed hook */
  sibling = hook_list->hooks;
  while (sibling && !sibling->hook_id)
    sibling = sibling->next;
  
  while (sibling)
    {
      GHook *tmp;
      
      g_hook_ref (hook_list, sibling);
      if (func (hook, sibling) <= 0 && sibling->hook_id)
	{
	  g_hook_unref (hook_list, sibling);
	  break;
	}

      /* next non-destroyed hook */
      tmp = sibling->next;
      while (tmp && !tmp->hook_id)
	tmp = tmp->next;

      g_hook_unref (hook_list, sibling);
      sibling = tmp;
    }
  
  g_hook_insert_before (hook_list, sibling, hook);
}

gint
g_hook_compare_ids (GHook *new_hook,
		    GHook *sibling)
{
  return ((glong) new_hook->hook_id) - ((glong) sibling->hook_id);
}
