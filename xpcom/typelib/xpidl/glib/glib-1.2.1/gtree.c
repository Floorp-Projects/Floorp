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


typedef struct _GRealTree  GRealTree;
typedef struct _GTreeNode  GTreeNode;

struct _GRealTree
{
  GTreeNode *root;
  GCompareFunc key_compare;
};

struct _GTreeNode
{
  gint balance;      /* height (left) - height (right) */
  GTreeNode *left;   /* left subtree */
  GTreeNode *right;  /* right subtree */
  gpointer key;      /* key for this node */
  gpointer value;    /* value stored at this node */
};


static GTreeNode* g_tree_node_new                   (gpointer        key,
						     gpointer        value);
static void       g_tree_node_destroy               (GTreeNode      *node);
static GTreeNode* g_tree_node_insert                (GTreeNode      *node,
						     GCompareFunc    compare,
						     gpointer        key,
						     gpointer        value,
						     gint           *inserted);
static GTreeNode* g_tree_node_remove                (GTreeNode      *node,
						     GCompareFunc    compare,
						     gpointer        key);
static GTreeNode* g_tree_node_balance               (GTreeNode      *node);
static GTreeNode* g_tree_node_remove_leftmost       (GTreeNode      *node,
						     GTreeNode     **leftmost);
static GTreeNode* g_tree_node_restore_left_balance  (GTreeNode      *node,
						     gint            old_balance);
static GTreeNode* g_tree_node_restore_right_balance (GTreeNode      *node,
						     gint            old_balance);
static gpointer   g_tree_node_lookup                (GTreeNode      *node,
						     GCompareFunc    compare,
						     gpointer        key);
static gint       g_tree_node_count                 (GTreeNode      *node);
static gint       g_tree_node_pre_order             (GTreeNode      *node,
						     GTraverseFunc   traverse_func,
						     gpointer        data);
static gint       g_tree_node_in_order              (GTreeNode      *node,
						     GTraverseFunc   traverse_func,
						     gpointer        data);
static gint       g_tree_node_post_order            (GTreeNode      *node,
						     GTraverseFunc   traverse_func,
						     gpointer        data);
static gpointer   g_tree_node_search                (GTreeNode      *node,
						     GSearchFunc     search_func,
						     gpointer        data);
static gint       g_tree_node_height                (GTreeNode      *node);
static GTreeNode* g_tree_node_rotate_left           (GTreeNode      *node);
static GTreeNode* g_tree_node_rotate_right          (GTreeNode      *node);
static void       g_tree_node_check                 (GTreeNode      *node);


G_LOCK_DEFINE_STATIC (g_tree_global);
static GMemChunk *node_mem_chunk = NULL;
static GTreeNode *node_free_list = NULL;


static GTreeNode*
g_tree_node_new (gpointer key,
		 gpointer value)
{
  GTreeNode *node;

  G_LOCK (g_tree_global);
  if (node_free_list)
    {
      node = node_free_list;
      node_free_list = node->right;
    }
  else
    {
      if (!node_mem_chunk)
	node_mem_chunk = g_mem_chunk_new ("GLib GTreeNode mem chunk",
					  sizeof (GTreeNode),
					  1024,
					  G_ALLOC_ONLY);

      node = g_chunk_new (GTreeNode, node_mem_chunk);
   }
  G_UNLOCK (g_tree_global);

  node->balance = 0;
  node->left = NULL;
  node->right = NULL;
  node->key = key;
  node->value = value;

  return node;
}

static void
g_tree_node_destroy (GTreeNode *node)
{
  if (node)
    {
      g_tree_node_destroy (node->right);
      g_tree_node_destroy (node->left);
      G_LOCK (g_tree_global);
      node->right = node_free_list;
      node_free_list = node;
      G_UNLOCK (g_tree_global);
   }
}


GTree*
g_tree_new (GCompareFunc key_compare_func)
{
  GRealTree *rtree;

  g_return_val_if_fail (key_compare_func != NULL, NULL);

  rtree = g_new (GRealTree, 1);
  rtree->root = NULL;
  rtree->key_compare = key_compare_func;

  return (GTree*) rtree;
}

void
g_tree_destroy (GTree *tree)
{
  GRealTree *rtree;

  g_return_if_fail (tree != NULL);

  rtree = (GRealTree*) tree;

  g_tree_node_destroy (rtree->root);
  g_free (rtree);
}

void
g_tree_insert (GTree    *tree,
	       gpointer  key,
	       gpointer  value)
{
  GRealTree *rtree;
  gint inserted;

  g_return_if_fail (tree != NULL);

  rtree = (GRealTree*) tree;

  inserted = FALSE;
  rtree->root = g_tree_node_insert (rtree->root, rtree->key_compare,
				    key, value, &inserted);
}

void
g_tree_remove (GTree    *tree,
	       gpointer  key)
{
  GRealTree *rtree;

  g_return_if_fail (tree != NULL);

  rtree = (GRealTree*) tree;

  rtree->root = g_tree_node_remove (rtree->root, rtree->key_compare, key);
}

gpointer
g_tree_lookup (GTree    *tree,
	       gpointer  key)
{
  GRealTree *rtree;

  g_return_val_if_fail (tree != NULL, NULL);

  rtree = (GRealTree*) tree;

  return g_tree_node_lookup (rtree->root, rtree->key_compare, key);
}

void
g_tree_traverse (GTree         *tree,
		 GTraverseFunc  traverse_func,
		 GTraverseType  traverse_type,
		 gpointer       data)
{
  GRealTree *rtree;

  g_return_if_fail (tree != NULL);

  rtree = (GRealTree*) tree;

  if (!rtree->root)
    return;

  switch (traverse_type)
    {
    case G_PRE_ORDER:
      g_tree_node_pre_order (rtree->root, traverse_func, data);
      break;

    case G_IN_ORDER:
      g_tree_node_in_order (rtree->root, traverse_func, data);
      break;

    case G_POST_ORDER:
      g_tree_node_post_order (rtree->root, traverse_func, data);
      break;
    
    case G_LEVEL_ORDER:
      g_warning ("g_tree_traverse(): traverse type G_LEVEL_ORDER isn't implemented.");
      break;
    }
}

gpointer
g_tree_search (GTree       *tree,
	       GSearchFunc  search_func,
	       gpointer     data)
{
  GRealTree *rtree;

  g_return_val_if_fail (tree != NULL, NULL);

  rtree = (GRealTree*) tree;

  if (rtree->root)
    return g_tree_node_search (rtree->root, search_func, data);
  else
    return NULL;
}

gint
g_tree_height (GTree *tree)
{
  GRealTree *rtree;

  g_return_val_if_fail (tree != NULL, 0);

  rtree = (GRealTree*) tree;

  if (rtree->root)
    return g_tree_node_height (rtree->root);
  else
    return 0;
}

gint
g_tree_nnodes (GTree *tree)
{
  GRealTree *rtree;

  g_return_val_if_fail (tree != NULL, 0);

  rtree = (GRealTree*) tree;

  if (rtree->root)
    return g_tree_node_count (rtree->root);
  else
    return 0;
}

static GTreeNode*
g_tree_node_insert (GTreeNode    *node,
		    GCompareFunc  compare,
		    gpointer      key,
		    gpointer      value,
		    gint         *inserted)
{
  gint old_balance;
  gint cmp;

  if (!node)
    {
      *inserted = TRUE;
      return g_tree_node_new (key, value);
    }

  cmp = (* compare) (key, node->key);
  if (cmp == 0)
    {
      *inserted = FALSE;
      node->value = value;
      return node;
    }

  if (cmp < 0)
    {
      if (node->left)
	{
	  old_balance = node->left->balance;
	  node->left = g_tree_node_insert (node->left, compare, key, value, inserted);

	  if ((old_balance != node->left->balance) && node->left->balance)
	    node->balance -= 1;
	}
      else
	{
	  *inserted = TRUE;
	  node->left = g_tree_node_new (key, value);
	  node->balance -= 1;
	}
    }
  else if (cmp > 0)
    {
      if (node->right)
	{
	  old_balance = node->right->balance;
	  node->right = g_tree_node_insert (node->right, compare, key, value, inserted);

	  if ((old_balance != node->right->balance) && node->right->balance)
	    node->balance += 1;
	}
      else
	{
	  *inserted = TRUE;
	  node->right = g_tree_node_new (key, value);
	  node->balance += 1;
	}
    }

  if (*inserted)
    {
      if ((node->balance < -1) || (node->balance > 1))
	node = g_tree_node_balance (node);
    }

  return node;
}

static GTreeNode*
g_tree_node_remove (GTreeNode    *node,
		    GCompareFunc  compare,
		    gpointer      key)
{
  GTreeNode *new_root;
  gint old_balance;
  gint cmp;

  if (!node)
    return NULL;

  cmp = (* compare) (key, node->key);
  if (cmp == 0)
    {
      GTreeNode *garbage;

      garbage = node;

      if (!node->right)
	{
	  node = node->left;
	}
      else
	{
	  old_balance = node->right->balance;
	  node->right = g_tree_node_remove_leftmost (node->right, &new_root);
	  new_root->left = node->left;
	  new_root->right = node->right;
	  new_root->balance = node->balance;
	  node = g_tree_node_restore_right_balance (new_root, old_balance);
	}

      G_LOCK (g_tree_global);
      garbage->right = node_free_list;
      node_free_list = garbage;
      G_UNLOCK (g_tree_global);
   }
  else if (cmp < 0)
    {
      if (node->left)
	{
	  old_balance = node->left->balance;
	  node->left = g_tree_node_remove (node->left, compare, key);
	  node = g_tree_node_restore_left_balance (node, old_balance);
	}
    }
  else if (cmp > 0)
    {
      if (node->right)
	{
	  old_balance = node->right->balance;
	  node->right = g_tree_node_remove (node->right, compare, key);
	  node = g_tree_node_restore_right_balance (node, old_balance);
	}
    }

  return node;
}

static GTreeNode*
g_tree_node_balance (GTreeNode *node)
{
  if (node->balance < -1)
    {
      if (node->left->balance > 0)
	node->left = g_tree_node_rotate_left (node->left);
      node = g_tree_node_rotate_right (node);
    }
  else if (node->balance > 1)
    {
      if (node->right->balance < 0)
	node->right = g_tree_node_rotate_right (node->right);
      node = g_tree_node_rotate_left (node);
    }

  return node;
}

static GTreeNode*
g_tree_node_remove_leftmost (GTreeNode  *node,
			     GTreeNode **leftmost)
{
  gint old_balance;

  if (!node->left)
    {
      *leftmost = node;
      return node->right;
    }

  old_balance = node->left->balance;
  node->left = g_tree_node_remove_leftmost (node->left, leftmost);
  return g_tree_node_restore_left_balance (node, old_balance);
}

static GTreeNode*
g_tree_node_restore_left_balance (GTreeNode *node,
				  gint       old_balance)
{
  if (!node->left)
    node->balance += 1;
  else if ((node->left->balance != old_balance) &&
	   (node->left->balance == 0))
    node->balance += 1;

  if (node->balance > 1)
    return g_tree_node_balance (node);
  return node;
}

static GTreeNode*
g_tree_node_restore_right_balance (GTreeNode *node,
				   gint       old_balance)
{
  if (!node->right)
    node->balance -= 1;
  else if ((node->right->balance != old_balance) &&
	   (node->right->balance == 0))
    node->balance -= 1;

  if (node->balance < -1)
    return g_tree_node_balance (node);
  return node;
}

static gpointer
g_tree_node_lookup (GTreeNode    *node,
		    GCompareFunc  compare,
		    gpointer      key)
{
  gint cmp;

  if (!node)
    return NULL;

  cmp = (* compare) (key, node->key);
  if (cmp == 0)
    return node->value;

  if (cmp < 0)
    {
      if (node->left)
	return g_tree_node_lookup (node->left, compare, key);
    }
  else if (cmp > 0)
    {
      if (node->right)
	return g_tree_node_lookup (node->right, compare, key);
    }

  return NULL;
}

static gint
g_tree_node_count (GTreeNode *node)
{
  gint count;

  count = 1;
  if (node->left)
    count += g_tree_node_count (node->left);
  if (node->right)
    count += g_tree_node_count (node->right);

  return count;
}

static gint
g_tree_node_pre_order (GTreeNode     *node,
		       GTraverseFunc  traverse_func,
		       gpointer       data)
{
  if ((*traverse_func) (node->key, node->value, data))
    return TRUE;
  if (node->left)
    {
      if (g_tree_node_pre_order (node->left, traverse_func, data))
	return TRUE;
    }
  if (node->right)
    {
      if (g_tree_node_pre_order (node->right, traverse_func, data))
	return TRUE;
    }

  return FALSE;
}

static gint
g_tree_node_in_order (GTreeNode     *node,
		      GTraverseFunc  traverse_func,
		      gpointer       data)
{
  if (node->left)
    {
      if (g_tree_node_in_order (node->left, traverse_func, data))
	return TRUE;
    }
  if ((*traverse_func) (node->key, node->value, data))
    return TRUE;
  if (node->right)
    {
      if (g_tree_node_in_order (node->right, traverse_func, data))
	return TRUE;
    }

  return FALSE;
}

static gint
g_tree_node_post_order (GTreeNode     *node,
			GTraverseFunc  traverse_func,
			gpointer       data)
{
  if (node->left)
    {
      if (g_tree_node_post_order (node->left, traverse_func, data))
	return TRUE;
    }
  if (node->right)
    {
      if (g_tree_node_post_order (node->right, traverse_func, data))
	return TRUE;
    }
  if ((*traverse_func) (node->key, node->value, data))
    return TRUE;

  return FALSE;
}

static gpointer
g_tree_node_search (GTreeNode   *node,
		    GSearchFunc  search_func,
		    gpointer     data)
{
  gint dir;

  if (!node)
    return NULL;

  do {
    dir = (* search_func) (node->key, data);
    if (dir == 0)
      return node->value;

    if (dir < 0)
      node = node->left;
    else if (dir > 0)
      node = node->right;
  } while (node && (dir != 0));

  return NULL;
}

static gint
g_tree_node_height (GTreeNode *node)
{
  gint left_height;
  gint right_height;

  if (node)
    {
      left_height = 0;
      right_height = 0;

      if (node->left)
	left_height = g_tree_node_height (node->left);

      if (node->right)
	right_height = g_tree_node_height (node->right);

      return MAX (left_height, right_height) + 1;
    }

  return 0;
}

static GTreeNode*
g_tree_node_rotate_left (GTreeNode *node)
{
  GTreeNode *left;
  GTreeNode *right;
  gint a_bal;
  gint b_bal;

  left = node->left;
  right = node->right;

  node->right = right->left;
  right->left = node;

  a_bal = node->balance;
  b_bal = right->balance;

  if (b_bal <= 0)
    {
      if (a_bal >= 1)
	right->balance = b_bal - 1;
      else
	right->balance = a_bal + b_bal - 2;
      node->balance = a_bal - 1;
    }
  else
    {
      if (a_bal <= b_bal)
	right->balance = a_bal - 2;
      else
	right->balance = b_bal - 1;
      node->balance = a_bal - b_bal - 1;
    }

  return right;
}

static GTreeNode*
g_tree_node_rotate_right (GTreeNode *node)
{
  GTreeNode *left;
  GTreeNode *right;
  gint a_bal;
  gint b_bal;

  left = node->left;
  right = node->right;

  node->left = left->right;
  left->right = node;

  a_bal = node->balance;
  b_bal = left->balance;

  if (b_bal <= 0)
    {
      if (b_bal > a_bal)
	left->balance = b_bal + 1;
      else
	left->balance = a_bal + 2;
      node->balance = a_bal - b_bal + 1;
    }
  else
    {
      if (a_bal <= -1)
	left->balance = b_bal + 1;
      else
	left->balance = a_bal + b_bal + 2;
      node->balance = a_bal + 1;
    }

  return left;
}

static void
g_tree_node_check (GTreeNode *node)
{
  gint left_height;
  gint right_height;
  gint balance;
  
  if (node)
    {
      left_height = 0;
      right_height = 0;
      
      if (node->left)
	left_height = g_tree_node_height (node->left);
      if (node->right)
	right_height = g_tree_node_height (node->right);
      
      balance = right_height - left_height;
      if (balance != node->balance)
	g_log (g_log_domain_glib, G_LOG_LEVEL_INFO,
	       "g_tree_node_check: failed: %d ( %d )\n",
	       balance, node->balance);
      
      if (node->left)
	g_tree_node_check (node->left);
      if (node->right)
	g_tree_node_check (node->right);
    }
}
