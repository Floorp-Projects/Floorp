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

#include <string.h>
#include "glib.h"


#define MIN_ARRAY_SIZE  16


typedef struct _GRealArray  GRealArray;

struct _GRealArray
{
  guint8 *data;
  guint   len;
  guint   alloc;
  guint   elt_size;
  guint   zero_terminated : 1;
  guint   clear : 1;
};


static gint g_nearest_pow        (gint        num);
static void g_array_maybe_expand (GRealArray *array,
				  gint        len);

static GMemChunk *array_mem_chunk = NULL;
G_LOCK_DEFINE_STATIC (array_mem_chunk);

GArray*
g_array_new (gboolean zero_terminated,
	     gboolean clear,
	     guint    elt_size)
{
  GRealArray *array;

  G_LOCK (array_mem_chunk);
  if (!array_mem_chunk)
    array_mem_chunk = g_mem_chunk_new ("array mem chunk",
				       sizeof (GRealArray),
				       1024, G_ALLOC_AND_FREE);

  array = g_chunk_new (GRealArray, array_mem_chunk);
  G_UNLOCK (array_mem_chunk);

  array->data            = NULL;
  array->len             = 0;
  array->alloc           = 0;
  array->zero_terminated = (zero_terminated ? 1 : 0);
  array->clear           = (clear ? 1 : 0);
  array->elt_size        = elt_size;

  return (GArray*) array;
}

void
g_array_free (GArray  *array,
	      gboolean free_segment)
{
  if (free_segment)
    g_free (array->data);

  G_LOCK (array_mem_chunk);
  g_mem_chunk_free (array_mem_chunk, array);
  G_UNLOCK (array_mem_chunk);
}

GArray*
g_array_append_vals (GArray       *farray,
		     gconstpointer data,
		     guint         len)
{
  GRealArray *array = (GRealArray*) farray;

  g_array_maybe_expand (array, len);

  memcpy (array->data + array->elt_size * array->len, data, array->elt_size * len);

  array->len += len;

  return farray;
}

GArray*
g_array_prepend_vals (GArray        *farray,
		      gconstpointer  data,
		      guint          len)
{
  GRealArray *array = (GRealArray*) farray;

  g_array_maybe_expand (array, len);

  g_memmove (array->data + array->elt_size * len, array->data, array->elt_size * array->len);

  memcpy (array->data, data, len * array->elt_size);

  array->len += len;

  return farray;
}

GArray*
g_array_insert_vals (GArray        *farray,
		     guint          index,
		     gconstpointer  data,
		     guint          len)
{
  GRealArray *array = (GRealArray*) farray;

  g_array_maybe_expand (array, len);

  g_memmove (array->data + array->elt_size * (len + index), 
	     array->data + array->elt_size * index, 
	     array->elt_size * (array->len - index));

  memcpy (array->data + array->elt_size * index, data, len * array->elt_size);

  array->len += len;

  return farray;
}

GArray*
g_array_set_size (GArray *farray,
		  guint   length)
{
  GRealArray *array = (GRealArray*) farray;

  if (array->len < length)
    g_array_maybe_expand (array, length - array->len);

  array->len = length;

  return farray;
}

GArray*
g_array_remove_index (GArray* farray,
		      guint index)
{
  GRealArray* array = (GRealArray*) farray;

  g_return_val_if_fail (array, NULL);

  g_return_val_if_fail (index >= 0 && index < array->len, NULL);

  if (index != array->len - 1)
      g_memmove (array->data + array->elt_size * index, 
		 array->data + array->elt_size * (index + 1), 
		 array->elt_size * (array->len - index - 1));
  
  if (array->zero_terminated)
    memset (array->data + array->elt_size * (array->len - 1), 0, 
	    array->elt_size);

  array->len -= 1;

  return farray;
}

GArray*
g_array_remove_index_fast (GArray* farray,
			   guint index)
{
  GRealArray* array = (GRealArray*) farray;

  g_return_val_if_fail (array, NULL);

  g_return_val_if_fail (index >= 0 && index < array->len, NULL);

  if (index != array->len - 1)
    g_memmove (array->data + array->elt_size * index, 
	       array->data + array->elt_size * (array->len - 1), 
	       array->elt_size);
  
  if (array->zero_terminated)
    memset (array->data + array->elt_size * (array->len - 1), 0, 
	    array->elt_size);

  array->len -= 1;

  return farray;
}

static gint
g_nearest_pow (gint num)
{
  gint n = 1;

  while (n < num)
    n <<= 1;

  return n;
}

static void
g_array_maybe_expand (GRealArray *array,
		      gint        len)
{
  guint want_alloc = (array->len + len + array->zero_terminated) * array->elt_size;

  if (want_alloc > array->alloc)
    {
      guint old_alloc = array->alloc;

      array->alloc = g_nearest_pow (want_alloc);
      array->alloc = MAX (array->alloc, MIN_ARRAY_SIZE);

      array->data = g_realloc (array->data, array->alloc);

      if (array->clear || array->zero_terminated)
	memset (array->data + old_alloc, 0, array->alloc - old_alloc);
    }
}

/* Pointer Array
 */

typedef struct _GRealPtrArray  GRealPtrArray;

struct _GRealPtrArray
{
  gpointer *pdata;
  guint     len;
  guint     alloc;
};

static void g_ptr_array_maybe_expand (GRealPtrArray *array,
				      gint           len);

static GMemChunk *ptr_array_mem_chunk = NULL;
G_LOCK_DEFINE_STATIC (ptr_array_mem_chunk);


GPtrArray*
g_ptr_array_new (void)
{
  GRealPtrArray *array;

  G_LOCK (ptr_array_mem_chunk);
  if (!ptr_array_mem_chunk)
    ptr_array_mem_chunk = g_mem_chunk_new ("array mem chunk",
					   sizeof (GRealPtrArray),
					   1024, G_ALLOC_AND_FREE);

  array = g_chunk_new (GRealPtrArray, ptr_array_mem_chunk);
  G_UNLOCK (ptr_array_mem_chunk);

  array->pdata = NULL;
  array->len = 0;
  array->alloc = 0;

  return (GPtrArray*) array;
}

void
g_ptr_array_free (GPtrArray   *array,
		  gboolean  free_segment)
{
  g_return_if_fail (array);

  if (free_segment)
    g_free (array->pdata);

  G_LOCK (ptr_array_mem_chunk);
  g_mem_chunk_free (ptr_array_mem_chunk, array);
  G_UNLOCK (ptr_array_mem_chunk);
}

static void
g_ptr_array_maybe_expand (GRealPtrArray *array,
			  gint        len)
{
  guint old_alloc;

  if ((array->len + len) > array->alloc)
    {
      old_alloc = array->alloc;

      array->alloc = g_nearest_pow (array->len + len);
      array->alloc = MAX (array->alloc, MIN_ARRAY_SIZE);
      if (array->pdata)
	array->pdata = g_realloc (array->pdata, sizeof(gpointer) * array->alloc);
      else
	array->pdata = g_new0 (gpointer, array->alloc);

      memset (array->pdata + old_alloc, 0, array->alloc - old_alloc);
    }
}

void
g_ptr_array_set_size  (GPtrArray   *farray,
		       gint	     length)
{
  GRealPtrArray* array = (GRealPtrArray*) farray;

  g_return_if_fail (array);

  if (length > array->len)
    g_ptr_array_maybe_expand (array, (length - array->len));

  array->len = length;
}

gpointer
g_ptr_array_remove_index (GPtrArray* farray,
			  guint index)
{
  GRealPtrArray* array = (GRealPtrArray*) farray;
  gpointer result;

  g_return_val_if_fail (array, NULL);

  g_return_val_if_fail (index >= 0 && index < array->len, NULL);

  result = array->pdata[index];
  
  if (index != array->len - 1)
    g_memmove (array->pdata + index, array->pdata + index + 1, 
	       sizeof (gpointer) * (array->len - index - 1));
  
  array->pdata[array->len - 1] = NULL;

  array->len -= 1;

  return result;
}

gpointer
g_ptr_array_remove_index_fast (GPtrArray* farray,
			       guint index)
{
  GRealPtrArray* array = (GRealPtrArray*) farray;
  gpointer result;

  g_return_val_if_fail (array, NULL);

  g_return_val_if_fail (index >= 0 && index < array->len, NULL);

  result = array->pdata[index];
  
  if (index != array->len - 1)
    array->pdata[index] = array->pdata[array->len - 1];

  array->pdata[array->len - 1] = NULL;

  array->len -= 1;

  return result;
}

gboolean
g_ptr_array_remove (GPtrArray* farray,
		    gpointer data)
{
  GRealPtrArray* array = (GRealPtrArray*) farray;
  int i;

  g_return_val_if_fail (array, FALSE);

  for (i = 0; i < array->len; i += 1)
    {
      if (array->pdata[i] == data)
	{
	  g_ptr_array_remove_index (farray, i);
	  return TRUE;
	}
    }

  return FALSE;
}

gboolean
g_ptr_array_remove_fast (GPtrArray* farray,
			 gpointer data)
{
  GRealPtrArray* array = (GRealPtrArray*) farray;
  int i;

  g_return_val_if_fail (array, FALSE);

  for (i = 0; i < array->len; i += 1)
    {
      if (array->pdata[i] == data)
	{
	  g_ptr_array_remove_index_fast (farray, i);
	  return TRUE;
	}
    }

  return FALSE;
}

void
g_ptr_array_add (GPtrArray* farray,
		 gpointer data)
{
  GRealPtrArray* array = (GRealPtrArray*) farray;

  g_return_if_fail (array);

  g_ptr_array_maybe_expand (array, 1);

  array->pdata[array->len++] = data;
}

/* Byte arrays
 */

GByteArray* g_byte_array_new      (void)
{
  return (GByteArray*) g_array_new (FALSE, FALSE, 1);
}

void	    g_byte_array_free     (GByteArray *array,
			           gboolean    free_segment)
{
  g_array_free ((GArray*) array, free_segment);
}

GByteArray* g_byte_array_append   (GByteArray *array,
				   const guint8 *data,
				   guint       len)
{
  g_array_append_vals ((GArray*) array, (guint8*)data, len);

  return array;
}

GByteArray* g_byte_array_prepend  (GByteArray *array,
				   const guint8 *data,
				   guint       len)
{
  g_array_prepend_vals ((GArray*) array, (guint8*)data, len);

  return array;
}

GByteArray* g_byte_array_set_size (GByteArray *array,
				   guint       length)
{
  g_array_set_size ((GArray*) array, length);

  return array;
}

GByteArray* g_byte_array_remove_index (GByteArray *array,
				       guint index)
{
  g_array_remove_index((GArray*) array, index);

  return array;
}

GByteArray* g_byte_array_remove_index_fast (GByteArray *array,
						   guint index)
{
  g_array_remove_index_fast((GArray*) array, index);

  return array;
}
