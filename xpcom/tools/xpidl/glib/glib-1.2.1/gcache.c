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


typedef struct _GCacheNode  GCacheNode;
typedef struct _GRealCache  GRealCache;

struct _GCacheNode
{
  /* A reference counted node */
  gpointer value;
  gint ref_count;
};

struct _GRealCache
{
  /* Called to create a value from a key */
  GCacheNewFunc value_new_func;

  /* Called to destroy a value */
  GCacheDestroyFunc value_destroy_func;

  /* Called to duplicate a key */
  GCacheDupFunc key_dup_func;

  /* Called to destroy a key */
  GCacheDestroyFunc key_destroy_func;

  /* Associates keys with nodes */
  GHashTable *key_table;

  /* Associates nodes with keys */
  GHashTable *value_table;
};


static GCacheNode* g_cache_node_new     (gpointer value);
static void        g_cache_node_destroy (GCacheNode *node);


static GMemChunk *node_mem_chunk = NULL;
G_LOCK_DEFINE_STATIC (node_mem_chunk);

GCache*
g_cache_new (GCacheNewFunc      value_new_func,
	     GCacheDestroyFunc  value_destroy_func,
	     GCacheDupFunc      key_dup_func,
	     GCacheDestroyFunc  key_destroy_func,
	     GHashFunc          hash_key_func,
	     GHashFunc          hash_value_func,
	     GCompareFunc       key_compare_func)
{
  GRealCache *cache;

  g_return_val_if_fail (value_new_func != NULL, NULL);
  g_return_val_if_fail (value_destroy_func != NULL, NULL);
  g_return_val_if_fail (key_dup_func != NULL, NULL);
  g_return_val_if_fail (key_destroy_func != NULL, NULL);
  g_return_val_if_fail (hash_key_func != NULL, NULL);
  g_return_val_if_fail (hash_value_func != NULL, NULL);
  g_return_val_if_fail (key_compare_func != NULL, NULL);

  cache = g_new (GRealCache, 1);
  cache->value_new_func = value_new_func;
  cache->value_destroy_func = value_destroy_func;
  cache->key_dup_func = key_dup_func;
  cache->key_destroy_func = key_destroy_func;
  cache->key_table = g_hash_table_new (hash_key_func, key_compare_func);
  cache->value_table = g_hash_table_new (hash_value_func, NULL);

  return (GCache*) cache;
}

void
g_cache_destroy (GCache *cache)
{
  GRealCache *rcache;

  g_return_if_fail (cache != NULL);

  rcache = (GRealCache*) cache;
  g_hash_table_destroy (rcache->key_table);
  g_hash_table_destroy (rcache->value_table);
  g_free (rcache);
}

gpointer
g_cache_insert (GCache   *cache,
		gpointer  key)
{
  GRealCache *rcache;
  GCacheNode *node;
  gpointer value;

  g_return_val_if_fail (cache != NULL, NULL);

  rcache = (GRealCache*) cache;

  node = g_hash_table_lookup (rcache->key_table, key);
  if (node)
    {
      node->ref_count += 1;
      return node->value;
    }

  key = (* rcache->key_dup_func) (key);
  value = (* rcache->value_new_func) (key);
  node = g_cache_node_new (value);

  g_hash_table_insert (rcache->key_table, key, node);
  g_hash_table_insert (rcache->value_table, value, key);

  return node->value;
}

void
g_cache_remove (GCache   *cache,
		gpointer  value)
{
  GRealCache *rcache;
  GCacheNode *node;
  gpointer key;

  g_return_if_fail (cache != NULL);

  rcache = (GRealCache*) cache;

  key = g_hash_table_lookup (rcache->value_table, value);
  node = g_hash_table_lookup (rcache->key_table, key);

  node->ref_count -= 1;
  if (node->ref_count == 0)
    {
      g_hash_table_remove (rcache->value_table, value);
      g_hash_table_remove (rcache->key_table, key);

      (* rcache->key_destroy_func) (key);
      (* rcache->value_destroy_func) (node->value);
      g_cache_node_destroy (node);
    }
}

void
g_cache_key_foreach (GCache   *cache,
		     GHFunc    func,
		     gpointer  user_data)
{
  GRealCache *rcache;

  g_return_if_fail (cache != NULL);
  g_return_if_fail (func != NULL);

  rcache = (GRealCache*) cache;

  g_hash_table_foreach (rcache->value_table, func, user_data);
}

void
g_cache_value_foreach (GCache   *cache,
		       GHFunc    func,
		       gpointer  user_data)
{
  GRealCache *rcache;

  g_return_if_fail (cache != NULL);
  g_return_if_fail (func != NULL);

  rcache = (GRealCache*) cache;

  g_hash_table_foreach (rcache->key_table, func, user_data);
}


static GCacheNode*
g_cache_node_new (gpointer value)
{
  GCacheNode *node;

  G_LOCK (node_mem_chunk);
  if (!node_mem_chunk)
    node_mem_chunk = g_mem_chunk_new ("cache node mem chunk", sizeof (GCacheNode),
				      1024, G_ALLOC_AND_FREE);

  node = g_chunk_new (GCacheNode, node_mem_chunk);
  G_UNLOCK (node_mem_chunk);

  node->value = value;
  node->ref_count = 1;

  return node;
}

static void
g_cache_node_destroy (GCacheNode *node)
{
  G_LOCK (node_mem_chunk);
  g_mem_chunk_free (node_mem_chunk, node);
  G_UNLOCK (node_mem_chunk);
}
