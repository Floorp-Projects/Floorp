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
#include <string.h>

static void completion_check_cache (GCompletion* cmp,
				    gchar**	 new_prefix);

GCompletion* 
g_completion_new (GCompletionFunc func)
{
  GCompletion* gcomp;
  
  gcomp = g_new (GCompletion, 1);
  gcomp->items = NULL;
  gcomp->cache = NULL;
  gcomp->prefix = NULL;
  gcomp->func = func;

  return gcomp;
}

void 
g_completion_add_items (GCompletion* cmp,
			GList*	     items)
{
  GList* it;
  
  g_return_if_fail (cmp != NULL);
  g_return_if_fail (items != NULL);
  
  /* optimize adding to cache? */
  if (cmp->cache)
    {
      g_list_free (cmp->cache);
      cmp->cache = NULL;
    }

  if (cmp->prefix)
    {
      g_free (cmp->prefix);
      cmp->prefix = NULL;
    }
  
  it = items;
  while (it)
    {
      cmp->items = g_list_prepend (cmp->items, it->data);
      it = it->next;
    }
}

void 
g_completion_remove_items (GCompletion* cmp,
			   GList*	items)
{
  GList* it;
  
  g_return_if_fail (cmp != NULL);
  g_return_if_fail (items != NULL);
  
  it = items;
  while (cmp->items && it)
    {
      cmp->items = g_list_remove (cmp->items, it->data);
      it = it->next;
    }

  it = items;
  while (cmp->cache && it)
    {
      cmp->cache = g_list_remove(cmp->cache, it->data);
      it = it->next;
    }
}

void 
g_completion_clear_items (GCompletion* cmp)
{
  g_return_if_fail (cmp != NULL);
  
  g_list_free (cmp->items);
  cmp->items = NULL;
  g_list_free (cmp->cache);
  cmp->cache = NULL;
  g_free (cmp->prefix);
  cmp->prefix = NULL;
}

static void   
completion_check_cache (GCompletion* cmp,
			gchar**	     new_prefix)
{
  register GList* list;
  register gint len;
  register gint i;
  register gint plen;
  gchar* postfix=NULL;
  gchar* s;
  
  if (!new_prefix)
    return;
  if (!cmp->cache)
    {
      *new_prefix = NULL;
      return;
    }
  
  len = strlen(cmp->prefix);
  list = cmp->cache;
  s = cmp->func ? cmp->func (list->data) : (gchar*) list->data;
  postfix = s + len;
  plen = strlen (postfix);
  list = list->next;
  
  while (list && plen)
    {
      s = cmp->func ? cmp->func (list->data) : (gchar*) list->data;
      s += len;
      for (i = 0; i < plen; ++i)
	{
	  if (postfix[i] != s[i])
	    break;
	}
      plen = i;
      list = list->next;
    }
  
  *new_prefix = g_new0 (gchar, len + plen + 1);
  strncpy (*new_prefix, cmp->prefix, len);
  strncpy (*new_prefix + len, postfix, plen);
}

GList* 
g_completion_complete (GCompletion* cmp,
		       gchar*	    prefix,
		       gchar**	    new_prefix)
{
  gint plen, len;
  gint done = 0;
  GList* list;
  
  g_return_val_if_fail (cmp != NULL, NULL);
  g_return_val_if_fail (prefix != NULL, NULL);
  
  len = strlen (prefix);
  if (cmp->prefix && cmp->cache)
    {
      plen = strlen (cmp->prefix);
      if (plen <= len && !strncmp (prefix, cmp->prefix, plen))
	{ 
	  /* use the cache */
	  list = cmp->cache;
	  while (list)
	    {
	      if (strncmp (prefix,
			   cmp->func ? cmp->func (list->data) : (gchar*) list->data,
			   len))
		{
		  list = g_list_remove_link (cmp->cache, list);
		  if (list != cmp->cache)
		    cmp->cache = list;
		}
	      else
		list = list->next;
	    }
	  done = 1;
	}
    }
  
  if (!done)
    {
      /* normal code */
      g_list_free (cmp->cache);
      cmp->cache = NULL;
      list = cmp->items;
      while (*prefix && list)
	{
	  if (!strncmp (prefix,
			cmp->func ? cmp->func (list->data) : (gchar*) list->data,
			len))
	    cmp->cache = g_list_prepend (cmp->cache, list->data);
	  list = list->next;
	}
    }
  if (cmp->prefix)
    {
      g_free (cmp->prefix);
      cmp->prefix = NULL;
    }
  if (cmp->cache)
    cmp->prefix = g_strdup (prefix);
  completion_check_cache (cmp, new_prefix);
  
  return *prefix ? cmp->cache : cmp->items;
}

void 
g_completion_free (GCompletion* cmp)
{
  g_return_if_fail (cmp != NULL);
  
  g_completion_clear_items (cmp);
  g_free (cmp);
}

#ifdef TEST_COMPLETION
#include <stdio.h>
int
main (int   argc,
      char* argv[])
{
  FILE *file;
  gchar buf[1024];
  GList *list;
  GList *result;
  GList *tmp;
  GCompletion *cmp;
  gint i;
  gchar *longp = NULL;
  
  if (argc < 3)
    {
      g_warning ("Usage: %s filename prefix1 [prefix2 ...]\n", argv[0]);
      return 1;
    }
  
  file = fopen (argv[1], "r");
  if (!file)
    {
      g_warning ("Cannot open %s\n", argv[1]);
      return 1;
    }
  
  cmp = g_completion_new (NULL);
  list = g_list_alloc ();
  while (fgets (buf, 1024, file))
    {
      list->data = g_strdup (buf);
      g_completion_add_items (cmp, list);
    }
  fclose (file);
  
  for (i = 2; i < argc; ++i)
    {
      printf ("COMPLETING: %s\n", argv[i]);
      result = g_completion_complete (cmp, argv[i], &longp);
      g_list_foreach (result, (GFunc) printf, NULL);
      printf ("LONG MATCH: %s\n", longp);
      g_free (longp);
      longp = NULL;
    }
  
  g_list_foreach (cmp->items, (GFunc) g_free, NULL);
  g_completion_free (cmp);
  g_list_free (list);
  
  return 0;
}
#endif
