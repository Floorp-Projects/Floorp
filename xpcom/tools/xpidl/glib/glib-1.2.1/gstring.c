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

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "glib.h"


typedef struct _GRealStringChunk GRealStringChunk;
typedef struct _GRealString      GRealString;

struct _GRealStringChunk
{
  GHashTable *const_table;
  GSList     *storage_list;
  gint        storage_next;
  gint        this_size;
  gint        default_size;
};

struct _GRealString
{
  gchar *str;
  gint   len;
  gint   alloc;
};

G_LOCK_DEFINE_STATIC (string_mem_chunk);
static GMemChunk *string_mem_chunk = NULL;

/* Hash Functions.
 */

gint
g_str_equal (gconstpointer v, gconstpointer v2)
{
  return strcmp ((const gchar*) v, (const gchar*)v2) == 0;
}

/* a char* hash function from ASU */
guint
g_str_hash (gconstpointer v)
{
  const char *s = (char*)v;
  const char *p;
  guint h=0, g;

  for(p = s; *p != '\0'; p += 1) {
    h = ( h << 4 ) + *p;
    if ( ( g = h & 0xf0000000 ) ) {
      h = h ^ (g >> 24);
      h = h ^ g;
    }
  }

  return h /* % M */;
}


/* String Chunks.
 */

GStringChunk*
g_string_chunk_new (gint default_size)
{
  GRealStringChunk *new_chunk = g_new (GRealStringChunk, 1);
  gint size = 1;

  while (size < default_size)
    size <<= 1;

  new_chunk->const_table       = NULL;
  new_chunk->storage_list      = NULL;
  new_chunk->storage_next      = size;
  new_chunk->default_size      = size;
  new_chunk->this_size         = size;

  return (GStringChunk*) new_chunk;
}

void
g_string_chunk_free (GStringChunk *fchunk)
{
  GRealStringChunk *chunk = (GRealStringChunk*) fchunk;
  GSList *tmp_list;

  g_return_if_fail (chunk != NULL);

  if (chunk->storage_list)
    {
      for (tmp_list = chunk->storage_list; tmp_list; tmp_list = tmp_list->next)
	g_free (tmp_list->data);

      g_slist_free (chunk->storage_list);
    }

  if (chunk->const_table)
    g_hash_table_destroy (chunk->const_table);

  g_free (chunk);
}

gchar*
g_string_chunk_insert (GStringChunk *fchunk,
		       const gchar  *string)
{
  GRealStringChunk *chunk = (GRealStringChunk*) fchunk;
  gint len = strlen (string);
  char* pos;

  g_return_val_if_fail (chunk != NULL, NULL);

  if ((chunk->storage_next + len + 1) > chunk->this_size)
    {
      gint new_size = chunk->default_size;

      while (new_size < len+1)
	new_size <<= 1;

      chunk->storage_list = g_slist_prepend (chunk->storage_list,
					     g_new (char, new_size));

      chunk->this_size = new_size;
      chunk->storage_next = 0;
    }

  pos = ((char*)chunk->storage_list->data) + chunk->storage_next;

  strcpy (pos, string);

  chunk->storage_next += len + 1;

  return pos;
}

gchar*
g_string_chunk_insert_const (GStringChunk *fchunk,
			     const gchar  *string)
{
  GRealStringChunk *chunk = (GRealStringChunk*) fchunk;
  char* lookup;

  g_return_val_if_fail (chunk != NULL, NULL);

  if (!chunk->const_table)
    chunk->const_table = g_hash_table_new (g_str_hash, g_str_equal);

  lookup = (char*) g_hash_table_lookup (chunk->const_table, (gchar *)string);

  if (!lookup)
    {
      lookup = g_string_chunk_insert (fchunk, string);
      g_hash_table_insert (chunk->const_table, lookup, lookup);
    }

  return lookup;
}

/* Strings.
 */
static gint
nearest_pow (gint num)
{
  gint n = 1;

  while (n < num)
    n <<= 1;

  return n;
}

static void
g_string_maybe_expand (GRealString* string, gint len)
{
  if (string->len + len >= string->alloc)
    {
      string->alloc = nearest_pow (string->len + len + 1);
      string->str = g_realloc (string->str, string->alloc);
    }
}

GString*
g_string_sized_new (guint dfl_size)
{
  GRealString *string;

  G_LOCK (string_mem_chunk);
  if (!string_mem_chunk)
    string_mem_chunk = g_mem_chunk_new ("string mem chunk",
					sizeof (GRealString),
					1024, G_ALLOC_AND_FREE);

  string = g_chunk_new (GRealString, string_mem_chunk);
  G_UNLOCK (string_mem_chunk);

  string->alloc = 0;
  string->len   = 0;
  string->str   = NULL;

  g_string_maybe_expand (string, MAX (dfl_size, 2));
  string->str[0] = 0;

  return (GString*) string;
}

GString*
g_string_new (const gchar *init)
{
  GString *string;

  string = g_string_sized_new (2);

  if (init)
    g_string_append (string, init);

  return string;
}

void
g_string_free (GString *string,
	       gint free_segment)
{
  g_return_if_fail (string != NULL);

  if (free_segment)
    g_free (string->str);

  G_LOCK (string_mem_chunk);
  g_mem_chunk_free (string_mem_chunk, string);
  G_UNLOCK (string_mem_chunk);
}

GString*
g_string_assign (GString *lval,
		 const gchar *rval)
{
  g_string_truncate (lval, 0);
  g_string_append (lval, rval);

  return lval;
}

GString*
g_string_truncate (GString* fstring,
		   gint len)
{
  GRealString *string = (GRealString*)fstring;

  g_return_val_if_fail (string != NULL, NULL);

  string->len = len;

  string->str[len] = 0;

  return fstring;
}

GString*
g_string_append (GString *fstring,
		 const gchar *val)
{
  GRealString *string = (GRealString*)fstring;
  int len;

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (val != NULL, fstring);
  
  len = strlen (val);
  g_string_maybe_expand (string, len);

  strcpy (string->str + string->len, val);

  string->len += len;

  return fstring;
}

GString*
g_string_append_c (GString *fstring,
		   gchar c)
{
  GRealString *string = (GRealString*)fstring;

  g_return_val_if_fail (string != NULL, NULL);
  g_string_maybe_expand (string, 1);

  string->str[string->len++] = c;
  string->str[string->len] = 0;

  return fstring;
}

GString*
g_string_prepend (GString *fstring,
		  const gchar *val)
{
  GRealString *string = (GRealString*)fstring;
  gint len;

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (val != NULL, fstring);

  len = strlen (val);
  g_string_maybe_expand (string, len);

  g_memmove (string->str + len, string->str, string->len);

  strncpy (string->str, val, len);

  string->len += len;

  string->str[string->len] = 0;

  return fstring;
}

GString*
g_string_prepend_c (GString *fstring,
		    gchar    c)
{
  GRealString *string = (GRealString*)fstring;

  g_return_val_if_fail (string != NULL, NULL);
  g_string_maybe_expand (string, 1);

  g_memmove (string->str + 1, string->str, string->len);

  string->str[0] = c;

  string->len += 1;

  string->str[string->len] = 0;

  return fstring;
}

GString*
g_string_insert (GString     *fstring,
		 gint         pos,
		 const gchar *val)
{
  GRealString *string = (GRealString*)fstring;
  gint len;

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (val != NULL, fstring);
  g_return_val_if_fail (pos >= 0, fstring);
  g_return_val_if_fail (pos <= string->len, fstring);

  len = strlen (val);
  g_string_maybe_expand (string, len);

  g_memmove (string->str + pos + len, string->str + pos, string->len - pos);

  strncpy (string->str + pos, val, len);

  string->len += len;

  string->str[string->len] = 0;

  return fstring;
}

GString *
g_string_insert_c (GString *fstring,
		   gint     pos,
		   gchar    c)
{
  GRealString *string = (GRealString*)fstring;

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (pos <= string->len, fstring);

  g_string_maybe_expand (string, 1);

  g_memmove (string->str + pos + 1, string->str + pos, string->len - pos);

  string->str[pos] = c;

  string->len += 1;

  string->str[string->len] = 0;

  return fstring;
}

GString*
g_string_erase (GString *fstring,
		gint pos,
		gint len)
{
  GRealString *string = (GRealString*)fstring;

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (len >= 0, fstring);
  g_return_val_if_fail (pos >= 0, fstring);
  g_return_val_if_fail (pos <= string->len, fstring);
  g_return_val_if_fail (pos + len <= string->len, fstring);

  if (pos + len < string->len)
    g_memmove (string->str + pos, string->str + pos + len, string->len - (pos + len));

  string->len -= len;
  
  string->str[string->len] = 0;

  return fstring;
}

GString*
g_string_down (GString *fstring)
{
  GRealString *string = (GRealString*)fstring;
  gchar *s;

  g_return_val_if_fail (string != NULL, NULL);

  s = string->str;

  while (*s)
    {
      *s = tolower (*s);
      s++;
    }

  return fstring;
}

GString*
g_string_up (GString *fstring)
{
  GRealString *string = (GRealString*)fstring;
  gchar *s;

  g_return_val_if_fail (string != NULL, NULL);

  s = string->str;

  while (*s)
    {
      *s = toupper (*s);
      s++;
    }

  return fstring;
}

static void
g_string_sprintfa_int (GString     *string,
		       const gchar *fmt,
		       va_list      args)
{
  gchar *buffer;

  buffer = g_strdup_vprintf (fmt, args);
  g_string_append (string, buffer);
  g_free (buffer);
}

void
g_string_sprintf (GString *string,
		  const gchar *fmt,
		  ...)
{
  va_list args;

  g_string_truncate (string, 0);

  va_start (args, fmt);
  g_string_sprintfa_int (string, fmt, args);
  va_end (args);
}

void
g_string_sprintfa (GString *string,
		   const gchar *fmt,
		   ...)
{
  va_list args;

  va_start (args, fmt);
  g_string_sprintfa_int (string, fmt, args);
  va_end (args);
}
