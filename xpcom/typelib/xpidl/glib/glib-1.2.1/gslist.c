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

#include "glib.h"


struct _GAllocator /* from gmem.c */
{
  gchar         *name;
  guint16        n_preallocs;
  guint          is_unused : 1;
  guint          type : 4;
  GAllocator    *last;
  GMemChunk     *mem_chunk;
  GSList        *free_lists; /* implementation specific */
};

G_LOCK_DEFINE_STATIC (current_allocator);
static GAllocator       *current_allocator = NULL;

/* HOLDS: current_allocator_lock */
static void
g_slist_validate_allocator (GAllocator *allocator)
{
  g_return_if_fail (allocator != NULL);
  g_return_if_fail (allocator->is_unused == TRUE);

  if (allocator->type != G_ALLOCATOR_SLIST)
    {
      allocator->type = G_ALLOCATOR_SLIST;
      if (allocator->mem_chunk)
	{
	  g_mem_chunk_destroy (allocator->mem_chunk);
	  allocator->mem_chunk = NULL;
	}
    }

  if (!allocator->mem_chunk)
    {
      allocator->mem_chunk = g_mem_chunk_new (allocator->name,
					      sizeof (GSList),
					      sizeof (GSList) * allocator->n_preallocs,
					      G_ALLOC_ONLY);
      allocator->free_lists = NULL;
    }

  allocator->is_unused = FALSE;
}

void
g_slist_push_allocator (GAllocator *allocator)
{
  G_LOCK (current_allocator);
  g_slist_validate_allocator (allocator);
  allocator->last = current_allocator;
  current_allocator = allocator;
  G_UNLOCK (current_allocator);
}

void
g_slist_pop_allocator (void)
{
  G_LOCK (current_allocator);
  if (current_allocator)
    {
      GAllocator *allocator;

      allocator = current_allocator;
      current_allocator = allocator->last;
      allocator->last = NULL;
      allocator->is_unused = TRUE;
    }
  G_UNLOCK (current_allocator);
}

GSList*
g_slist_alloc (void)
{
  GSList *list;

  G_LOCK (current_allocator);
  if (!current_allocator)
    {
      GAllocator *allocator = g_allocator_new ("GLib default GSList allocator",
					       128);
      g_slist_validate_allocator (allocator);
      allocator->last = NULL;
      current_allocator = allocator; 
    }
  if (!current_allocator->free_lists)
    {
      list = g_chunk_new (GSList, current_allocator->mem_chunk);
      list->data = NULL;
    }
  else
    {
      if (current_allocator->free_lists->data)
	{
	  list = current_allocator->free_lists->data;
	  current_allocator->free_lists->data = list->next;
	  list->data = NULL;
	}
      else
	{
	  list = current_allocator->free_lists;
	  current_allocator->free_lists = list->next;
	}
    }
  G_UNLOCK (current_allocator);
  
  list->next = NULL;

  return list;
}

void
g_slist_free (GSList *list)
{
  if (list)
    {
      list->data = list->next;
      G_LOCK (current_allocator);
      list->next = current_allocator->free_lists;
      current_allocator->free_lists = list;
      G_UNLOCK (current_allocator);
    }
}

void
g_slist_free_1 (GSList *list)
{
  if (list)
    {
      list->data = NULL;
      G_LOCK (current_allocator);
      list->next = current_allocator->free_lists;
      current_allocator->free_lists = list;
      G_UNLOCK (current_allocator);
    }
}

GSList*
g_slist_append (GSList   *list,
		gpointer  data)
{
  GSList *new_list;
  GSList *last;

  new_list = g_slist_alloc ();
  new_list->data = data;

  if (list)
    {
      last = g_slist_last (list);
      /* g_assert (last != NULL); */
      last->next = new_list;

      return list;
    }
  else
      return new_list;
}

GSList*
g_slist_prepend (GSList   *list,
		 gpointer  data)
{
  GSList *new_list;

  new_list = g_slist_alloc ();
  new_list->data = data;
  new_list->next = list;

  return new_list;
}

GSList*
g_slist_insert (GSList   *list,
		gpointer  data,
		gint      position)
{
  GSList *prev_list;
  GSList *tmp_list;
  GSList *new_list;

  if (position < 0)
    return g_slist_append (list, data);
  else if (position == 0)
    return g_slist_prepend (list, data);

  new_list = g_slist_alloc ();
  new_list->data = data;

  if (!list)
    return new_list;

  prev_list = NULL;
  tmp_list = list;

  while ((position-- > 0) && tmp_list)
    {
      prev_list = tmp_list;
      tmp_list = tmp_list->next;
    }

  if (prev_list)
    {
      new_list->next = prev_list->next;
      prev_list->next = new_list;
    }
  else
    {
      new_list->next = list;
      list = new_list;
    }

  return list;
}

GSList *
g_slist_concat (GSList *list1, GSList *list2)
{
  if (list2)
    {
      if (list1)
	g_slist_last (list1)->next = list2;
      else
	list1 = list2;
    }

  return list1;
}

GSList*
g_slist_remove (GSList   *list,
		gpointer  data)
{
  GSList *tmp;
  GSList *prev;

  prev = NULL;
  tmp = list;

  while (tmp)
    {
      if (tmp->data == data)
	{
	  if (prev)
	    prev->next = tmp->next;
	  if (list == tmp)
	    list = list->next;

	  tmp->next = NULL;
	  g_slist_free (tmp);

	  break;
	}

      prev = tmp;
      tmp = tmp->next;
    }

  return list;
}

GSList*
g_slist_remove_link (GSList *list,
		     GSList *link)
{
  GSList *tmp;
  GSList *prev;

  prev = NULL;
  tmp = list;

  while (tmp)
    {
      if (tmp == link)
	{
	  if (prev)
	    prev->next = tmp->next;
	  if (list == tmp)
	    list = list->next;

	  tmp->next = NULL;
	  break;
	}

      prev = tmp;
      tmp = tmp->next;
    }

  return list;
}

GSList*
g_slist_copy (GSList *list)
{
  GSList *new_list = NULL;

  if (list)
    {
      GSList *last;

      new_list = g_slist_alloc ();
      new_list->data = list->data;
      last = new_list;
      list = list->next;
      while (list)
	{
	  last->next = g_slist_alloc ();
	  last = last->next;
	  last->data = list->data;
	  list = list->next;
	}
    }

  return new_list;
}

GSList*
g_slist_reverse (GSList *list)
{
  GSList *tmp;
  GSList *prev;
  GSList *last;

  last = NULL;
  prev = NULL;

  while (list)
    {
      last = list;

      tmp = list->next;
      list->next = prev;

      prev = list;
      list = tmp;
    }

  return last;
}

GSList*
g_slist_nth (GSList *list,
	     guint   n)
{
  while ((n-- > 0) && list)
    list = list->next;

  return list;
}

gpointer
g_slist_nth_data (GSList   *list,
		  guint     n)
{
  while ((n-- > 0) && list)
    list = list->next;

  return list ? list->data : NULL;
}

GSList*
g_slist_find (GSList   *list,
	      gpointer  data)
{
  while (list)
    {
      if (list->data == data)
	break;
      list = list->next;
    }

  return list;
}

GSList*
g_slist_find_custom (GSList      *list,
		     gpointer     data,
		     GCompareFunc func)
{
  g_return_val_if_fail (func != NULL, list);

  while (list)
    {
      if (! func (list->data, data))
	return list;
      list = list->next;
    }

  return NULL;
}

gint
g_slist_position (GSList *list,
		  GSList *link)
{
  gint i;

  i = 0;
  while (list)
    {
      if (list == link)
	return i;
      i++;
      list = list->next;
    }

  return -1;
}

gint
g_slist_index (GSList   *list,
	       gpointer data)
{
  gint i;

  i = 0;
  while (list)
    {
      if (list->data == data)
	return i;
      i++;
      list = list->next;
    }

  return -1;
}

GSList*
g_slist_last (GSList *list)
{
  if (list)
    {
      while (list->next)
	list = list->next;
    }

  return list;
}

guint
g_slist_length (GSList *list)
{
  guint length;

  length = 0;
  while (list)
    {
      length++;
      list = list->next;
    }

  return length;
}

void
g_slist_foreach (GSList   *list,
		 GFunc     func,
		 gpointer  user_data)
{
  while (list)
    {
      (*func) (list->data, user_data);
      list = list->next;
    }
}

GSList*
g_slist_insert_sorted (GSList       *list,
                       gpointer      data,
                       GCompareFunc  func)
{
  GSList *tmp_list = list;
  GSList *prev_list = NULL;
  GSList *new_list;
  gint cmp;
 
  g_return_val_if_fail (func != NULL, list);

  if (!list)
    {
      new_list = g_slist_alloc();
      new_list->data = data;
      return new_list;
    }
 
  cmp = (*func) (data, tmp_list->data);
 
  while ((tmp_list->next) && (cmp > 0))
    {
      prev_list = tmp_list;
      tmp_list = tmp_list->next;
      cmp = (*func) (data, tmp_list->data);
    }

  new_list = g_slist_alloc();
  new_list->data = data;

  if ((!tmp_list->next) && (cmp > 0))
    {
      tmp_list->next = new_list;
      return list;
    }
  
  if (prev_list)
    {
      prev_list->next = new_list;
      new_list->next = tmp_list;
      return list;
    }
  else
    {
      new_list->next = list;
      return new_list;
    }
}

static GSList* 
g_slist_sort_merge  (GSList      *l1, 
		     GSList      *l2,
		     GCompareFunc compare_func)
{
  GSList list, *l;

  l=&list;

  while (l1 && l2)
    {
      if (compare_func(l1->data,l2->data) < 0)
        {
	  l=l->next=l1;
	  l1=l1->next;
        } 
      else 
	{
	  l=l->next=l2;
	  l2=l2->next;
        }
    }
  l->next= l1 ? l1 : l2;
  
  return list.next;
}

GSList* 
g_slist_sort (GSList       *list,
	      GCompareFunc compare_func)
{
  GSList *l1, *l2;

  if (!list) 
    return NULL;
  if (!list->next) 
    return list;

  l1 = list; 
  l2 = list->next;

  while ((l2 = l2->next) != NULL)
    {
      if ((l2 = l2->next) == NULL) 
	break;
      l1=l1->next;
    }
  l2 = l1->next; 
  l1->next = NULL;

  return g_slist_sort_merge (g_slist_sort (list, compare_func),
			     g_slist_sort (l2,   compare_func),
			     compare_func);
}
