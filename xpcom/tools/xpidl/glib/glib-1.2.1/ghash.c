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


#define HASH_TABLE_MIN_SIZE 11
#define HASH_TABLE_MAX_SIZE 13845163


typedef struct _GHashNode      GHashNode;

struct _GHashNode
{
  gpointer key;
  gpointer value;
  GHashNode *next;
};

struct _GHashTable
{
  gint size;
  gint nnodes;
  guint frozen;
  GHashNode **nodes;
  GHashFunc hash_func;
  GCompareFunc key_compare_func;
};


static void		g_hash_table_resize	 (GHashTable	*hash_table);
static GHashNode**	g_hash_table_lookup_node (GHashTable	*hash_table,
						  gconstpointer	 key);
static GHashNode*	g_hash_node_new		 (gpointer	 key,
						  gpointer	 value);
static void		g_hash_node_destroy	 (GHashNode	*hash_node);
static void		g_hash_nodes_destroy	 (GHashNode	*hash_node);


G_LOCK_DEFINE_STATIC (g_hash_global);

static GMemChunk *node_mem_chunk = NULL;
static GHashNode *node_free_list = NULL;


GHashTable*
g_hash_table_new (GHashFunc    hash_func,
		  GCompareFunc key_compare_func)
{
  GHashTable *hash_table;
  guint i;
  
  hash_table = g_new (GHashTable, 1);
  hash_table->size = HASH_TABLE_MIN_SIZE;
  hash_table->nnodes = 0;
  hash_table->frozen = FALSE;
  hash_table->hash_func = hash_func ? hash_func : g_direct_hash;
  hash_table->key_compare_func = key_compare_func;
  hash_table->nodes = g_new (GHashNode*, hash_table->size);
  
  for (i = 0; i < hash_table->size; i++)
    hash_table->nodes[i] = NULL;
  
  return hash_table;
}

void
g_hash_table_destroy (GHashTable *hash_table)
{
  guint i;
  
  g_return_if_fail (hash_table != NULL);
  
  for (i = 0; i < hash_table->size; i++)
    g_hash_nodes_destroy (hash_table->nodes[i]);
  
  g_free (hash_table->nodes);
  g_free (hash_table);
}

static inline GHashNode**
g_hash_table_lookup_node (GHashTable	*hash_table,
			  gconstpointer	 key)
{
  GHashNode **node;
  
  node = &hash_table->nodes
    [(* hash_table->hash_func) (key) % hash_table->size];
  
  /* Hash table lookup needs to be fast.
   *  We therefore remove the extra conditional of testing
   *  whether to call the key_compare_func or not from
   *  the inner loop.
   */
  if (hash_table->key_compare_func)
    while (*node && !(*hash_table->key_compare_func) ((*node)->key, key))
      node = &(*node)->next;
  else
    while (*node && (*node)->key != key)
      node = &(*node)->next;
  
  return node;
}

gpointer
g_hash_table_lookup (GHashTable	  *hash_table,
		     gconstpointer key)
{
  GHashNode *node;
  
  g_return_val_if_fail (hash_table != NULL, NULL);
  
  node = *g_hash_table_lookup_node (hash_table, key);
  
  return node ? node->value : NULL;
}

void
g_hash_table_insert (GHashTable *hash_table,
		     gpointer	 key,
		     gpointer	 value)
{
  GHashNode **node;
  
  g_return_if_fail (hash_table != NULL);
  
  node = g_hash_table_lookup_node (hash_table, key);
  
  if (*node)
    {
      /* do not reset node->key in this place, keeping
       * the old key might be intended.
       * a g_hash_table_remove/g_hash_table_insert pair
       * can be used otherwise.
       *
       * node->key = key; */
      (*node)->value = value;
    }
  else
    {
      *node = g_hash_node_new (key, value);
      hash_table->nnodes++;
      if (!hash_table->frozen)
	g_hash_table_resize (hash_table);
    }
}

void
g_hash_table_remove (GHashTable	     *hash_table,
		     gconstpointer    key)
{
  GHashNode **node, *dest;
  
  g_return_if_fail (hash_table != NULL);
  
  node = g_hash_table_lookup_node (hash_table, key);

  if (*node)
    {
      dest = *node;
      (*node) = dest->next;
      g_hash_node_destroy (dest);
      hash_table->nnodes--;
  
      if (!hash_table->frozen)
        g_hash_table_resize (hash_table);
    }
}

gboolean
g_hash_table_lookup_extended (GHashTable	*hash_table,
			      gconstpointer	 lookup_key,
			      gpointer		*orig_key,
			      gpointer		*value)
{
  GHashNode *node;
  
  g_return_val_if_fail (hash_table != NULL, FALSE);
  
  node = *g_hash_table_lookup_node (hash_table, lookup_key);
  
  if (node)
    {
      if (orig_key)
	*orig_key = node->key;
      if (value)
	*value = node->value;
      return TRUE;
    }
  else
    return FALSE;
}

void
g_hash_table_freeze (GHashTable *hash_table)
{
  g_return_if_fail (hash_table != NULL);
  
  hash_table->frozen++;
}

void
g_hash_table_thaw (GHashTable *hash_table)
{
  g_return_if_fail (hash_table != NULL);
  
  if (hash_table->frozen)
    if (!(--hash_table->frozen))
      g_hash_table_resize (hash_table);
}

guint
g_hash_table_foreach_remove (GHashTable	*hash_table,
			     GHRFunc	 func,
			     gpointer	 user_data)
{
  GHashNode *node, *prev;
  guint i;
  guint deleted = 0;
  
  g_return_val_if_fail (hash_table != NULL, 0);
  g_return_val_if_fail (func != NULL, 0);
  
  for (i = 0; i < hash_table->size; i++)
    {
    restart:
      
      prev = NULL;
      
      for (node = hash_table->nodes[i]; node; prev = node, node = node->next)
	{
	  if ((* func) (node->key, node->value, user_data))
	    {
	      deleted += 1;
	      
	      hash_table->nnodes -= 1;
	      
	      if (prev)
		{
		  prev->next = node->next;
		  g_hash_node_destroy (node);
		  node = prev;
		}
	      else
		{
		  hash_table->nodes[i] = node->next;
		  g_hash_node_destroy (node);
		  goto restart;
		}
	    }
	}
    }
  
  if (!hash_table->frozen)
    g_hash_table_resize (hash_table);
  
  return deleted;
}

void
g_hash_table_foreach (GHashTable *hash_table,
		      GHFunc	  func,
		      gpointer	  user_data)
{
  GHashNode *node;
  gint i;
  
  g_return_if_fail (hash_table != NULL);
  g_return_if_fail (func != NULL);
  
  for (i = 0; i < hash_table->size; i++)
    for (node = hash_table->nodes[i]; node; node = node->next)
      (* func) (node->key, node->value, user_data);
}

/* Returns the number of elements contained in the hash table. */
guint
g_hash_table_size (GHashTable *hash_table)
{
  g_return_val_if_fail (hash_table != NULL, 0);
  
  return hash_table->nnodes;
}

static void
g_hash_table_resize (GHashTable *hash_table)
{
  GHashNode **new_nodes;
  GHashNode *node;
  GHashNode *next;
  gfloat nodes_per_list;
  guint hash_val;
  gint new_size;
  gint i;
  
  nodes_per_list = (gfloat) hash_table->nnodes / (gfloat) hash_table->size;
  
  if ((nodes_per_list > 0.3 || hash_table->size <= HASH_TABLE_MIN_SIZE) &&
      (nodes_per_list < 3.0 || hash_table->size >= HASH_TABLE_MAX_SIZE))
    return;
  
  new_size = CLAMP(g_spaced_primes_closest (hash_table->nnodes),
		   HASH_TABLE_MIN_SIZE,
		   HASH_TABLE_MAX_SIZE);
  new_nodes = g_new0 (GHashNode*, new_size);
  
  for (i = 0; i < hash_table->size; i++)
    for (node = hash_table->nodes[i]; node; node = next)
      {
	next = node->next;

	hash_val = (* hash_table->hash_func) (node->key) % new_size;

	node->next = new_nodes[hash_val];
	new_nodes[hash_val] = node;
      }
  
  g_free (hash_table->nodes);
  hash_table->nodes = new_nodes;
  hash_table->size = new_size;
}

static GHashNode*
g_hash_node_new (gpointer key,
		 gpointer value)
{
  GHashNode *hash_node;
  
  G_LOCK (g_hash_global);
  if (node_free_list)
    {
      hash_node = node_free_list;
      node_free_list = node_free_list->next;
    }
  else
    {
      if (!node_mem_chunk)
	node_mem_chunk = g_mem_chunk_new ("hash node mem chunk",
					  sizeof (GHashNode),
					  1024, G_ALLOC_ONLY);
      
      hash_node = g_chunk_new (GHashNode, node_mem_chunk);
    }
  G_UNLOCK (g_hash_global);
  
  hash_node->key = key;
  hash_node->value = value;
  hash_node->next = NULL;
  
  return hash_node;
}

static void
g_hash_node_destroy (GHashNode *hash_node)
{
  G_LOCK (g_hash_global);
  hash_node->next = node_free_list;
  node_free_list = hash_node;
  G_UNLOCK (g_hash_global);
}

static void
g_hash_nodes_destroy (GHashNode *hash_node)
{
  if (hash_node)
    {
      GHashNode *node = hash_node;
  
      while (node->next)
        node = node->next;
  
      G_LOCK (g_hash_global);
      node->next = node_free_list;
      node_free_list = hash_node;
      G_UNLOCK (g_hash_global);
    }
}
