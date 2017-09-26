/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Portions of this file were originally under the following license:
 *
 * Copyright (C) 2008 Jason Evans <jasone@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer
 *    unmodified other than the allowable addition of one or more
 *    copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 *
 * C++ template implementation of left-leaning red-black trees.
 *
 * Usage:
 *
 *   #define SIZEOF_PTR ...
 *   #define SIZEOF_PTR_2POW ...
 *
 *   #include <rb.h>
 *   ...
 *
 * All operations are done non-recursively.  Parent pointers are not used, and
 * color bits are stored in the least significant bit of right-child pointers,
 * thus making node linkage as compact as is possible for red-black trees.
 *
 * The RedBlackTree template expects two type arguments: the type of the nodes,
 * containing a RedBlackTreeNode, and a trait providing two methods:
 *  - a GetTreeNode method that returns a reference to the RedBlackTreeNode
 *    corresponding to a given node with the following signature:
 *      static RedBlackTreeNode<T>& GetTreeNode(T*)
 *  - a Compare function with the following signature:
 *      static int Compare(T* aNode, T* aOther)
 *                            ^^^^^
 *                         or aKey
 *
 * Interpretation of comparision function return values:
 *
 *   -1 : aNode <  aOther
 *    0 : aNode == aOther
 *    1 : aNode >  aOther
 *
 * In all cases, the aNode or aKey argument is the first argument to the
 * comparison function, which makes it possible to write comparison functions
 * that treat the first argument specially.
 *
 ******************************************************************************/

#ifndef RB_H_
#define RB_H_

enum NodeColor
{
  Black = 0,
  Red = 1,
};

/* Node structure. */
template <typename T>
class RedBlackTreeNode
{
  T* mLeft;
  /* The lowest bit is the color */
  T* mRightAndColor;

public:
  T* Left()
  {
    return mLeft;
  }

  void SetLeft(T* aValue)
  {
    mLeft = aValue;
  }

  T* Right()
  {
    return reinterpret_cast<T*>(
      reinterpret_cast<uintptr_t>(mRightAndColor) & uintptr_t(~1));
  }

  void SetRight(T* aValue)
  {
    mRightAndColor = reinterpret_cast<T*>(
      (reinterpret_cast<uintptr_t>(aValue) & uintptr_t(~1)) | Color());
  }

  NodeColor Color()
  {
    return static_cast<NodeColor>(reinterpret_cast<uintptr_t>(mRightAndColor) & 1);
  }

  bool IsBlack()
  {
    return Color() == NodeColor::Black;
  }

  bool IsRed()
  {
    return Color() == NodeColor::Red;
  }

  void SetColor(NodeColor aColor)
  {
    mRightAndColor = reinterpret_cast<T*>(
      (reinterpret_cast<uintptr_t>(mRightAndColor) & uintptr_t(~1)) | aColor);
  }
};

#define rb_node_field(a_node, a_field) a_field(a_node)

/* Tree operations. */
#define rbp_first(a_type, a_field, a_tree, a_root, r_node)                     \
  do {                                                                         \
    for ((r_node) = (a_root);                                                  \
         rb_node_field((r_node), a_field).Left() != &(a_tree)->rbt_nil;        \
         (r_node) = rb_node_field((r_node), a_field).Left()) {                 \
    }                                                                          \
  } while (0)

#define rbp_last(a_type, a_field, a_tree, a_root, r_node)                      \
  do {                                                                         \
    for ((r_node) = (a_root);                                                  \
         rb_node_field((r_node), a_field).Right() != &(a_tree)->rbt_nil;       \
         (r_node) = rb_node_field((r_node), a_field).Right()) {                \
    }                                                                          \
  } while (0)

#define rbp_next(a_type, a_field, a_cmp, a_tree, a_node, r_node)               \
  do {                                                                         \
    if (rb_node_field((a_node), a_field).Right() != &(a_tree)->rbt_nil) {      \
      rbp_first(a_type,                                                        \
                a_field,                                                       \
                a_tree,                                                        \
                rb_node_field((a_node), a_field).Right(),                      \
                (r_node));                                                     \
    } else {                                                                   \
      a_type* rbp_n_t = (a_tree)->rbt_root;                                    \
      MOZ_ASSERT(rbp_n_t != &(a_tree)->rbt_nil);                               \
      (r_node) = &(a_tree)->rbt_nil;                                           \
      while (true) {                                                           \
        int rbp_n_cmp = (a_cmp)((a_node), rbp_n_t);                            \
        if (rbp_n_cmp < 0) {                                                   \
          (r_node) = rbp_n_t;                                                  \
          rbp_n_t = rb_node_field(rbp_n_t, a_field).Left();                    \
        } else if (rbp_n_cmp > 0) {                                            \
          rbp_n_t = rb_node_field(rbp_n_t, a_field).Right();                   \
        } else {                                                               \
          break;                                                               \
        }                                                                      \
        MOZ_ASSERT(rbp_n_t != &(a_tree)->rbt_nil);                             \
      }                                                                        \
    }                                                                          \
  } while (0)

#define rbp_prev(a_type, a_field, a_cmp, a_tree, a_node, r_node)               \
  do {                                                                         \
    if (rb_node_field((a_node), a_field).Left() != &(a_tree)->rbt_nil) {       \
      rbp_last(a_type,                                                         \
               a_field,                                                        \
               a_tree,                                                         \
               rb_node_field((a_node), a_field).Left(),                        \
               (r_node));                                                      \
    } else {                                                                   \
      a_type* rbp_p_t = (a_tree)->rbt_root;                                    \
      MOZ_ASSERT(rbp_p_t != &(a_tree)->rbt_nil);                               \
      (r_node) = &(a_tree)->rbt_nil;                                           \
      while (true) {                                                           \
        int rbp_p_cmp = (a_cmp)((a_node), rbp_p_t);                            \
        if (rbp_p_cmp < 0) {                                                   \
          rbp_p_t = rb_node_field(rbp_p_t, a_field).Left();                    \
        } else if (rbp_p_cmp > 0) {                                            \
          (r_node) = rbp_p_t;                                                  \
          rbp_p_t = rb_node_field(rbp_p_t, a_field).Right();                   \
        } else {                                                               \
          break;                                                               \
        }                                                                      \
        MOZ_ASSERT(rbp_p_t != &(a_tree)->rbt_nil);                             \
      }                                                                        \
    }                                                                          \
  } while (0)

#define rb_search(a_type, a_field, a_cmp, a_tree, a_key, r_node)               \
  do {                                                                         \
    int rbp_se_cmp;                                                            \
    (r_node) = (a_tree)->rbt_root;                                             \
    while ((r_node) != &(a_tree)->rbt_nil &&                                   \
           (rbp_se_cmp = (a_cmp)((a_key), (r_node))) != 0) {                   \
      if (rbp_se_cmp < 0) {                                                    \
        (r_node) = rb_node_field((r_node), a_field).Left();                    \
      } else {                                                                 \
        (r_node) = rb_node_field((r_node), a_field).Right();                   \
      }                                                                        \
    }                                                                          \
    if ((r_node) == &(a_tree)->rbt_nil) {                                      \
      (r_node) = nullptr;                                                      \
    }                                                                          \
  } while (0)

/*
 * Find a match if it exists.  Otherwise, find the next greater node, if one
 * exists.
 */
#define rb_nsearch(a_type, a_field, a_cmp, a_tree, a_key, r_node)              \
  do {                                                                         \
    a_type* rbp_ns_t = (a_tree)->rbt_root;                                     \
    (r_node) = nullptr;                                                        \
    while (rbp_ns_t != &(a_tree)->rbt_nil) {                                   \
      int rbp_ns_cmp = (a_cmp)((a_key), rbp_ns_t);                             \
      if (rbp_ns_cmp < 0) {                                                    \
        (r_node) = rbp_ns_t;                                                   \
        rbp_ns_t = rb_node_field(rbp_ns_t, a_field).Left();                    \
      } else if (rbp_ns_cmp > 0) {                                             \
        rbp_ns_t = rb_node_field(rbp_ns_t, a_field).Right();                   \
      } else {                                                                 \
        (r_node) = rbp_ns_t;                                                   \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
  } while (0)

/*
 * Find a match if it exists.  Otherwise, find the previous lesser node, if one
 * exists.
 */
#define rbp_rotate_left(a_type, a_field, a_node, r_node)                       \
  do {                                                                         \
    (r_node) = rb_node_field((a_node), a_field).Right();                       \
    rb_node_field((a_node), a_field)                                           \
      .SetRight(rb_node_field((r_node), a_field).Left());                      \
    rb_node_field((r_node), a_field).SetLeft((a_node));                        \
  } while (0)

#define rbp_rotate_right(a_type, a_field, a_node, r_node)                      \
  do {                                                                         \
    (r_node) = rb_node_field((a_node), a_field).Left();                        \
    rb_node_field((a_node), a_field)                                           \
      .SetLeft(rb_node_field((r_node), a_field).Right());                      \
    rb_node_field((r_node), a_field).SetRight((a_node));                       \
  } while (0)

#define rbp_lean_left(a_type, a_field, a_node, r_node)                         \
  do {                                                                         \
    NodeColor rbp_ll_color;                                                    \
    rbp_rotate_left(a_type, a_field, (a_node), (r_node));                      \
    rbp_ll_color = rb_node_field((a_node), a_field).Color();                   \
    rb_node_field((r_node), a_field).SetColor(rbp_ll_color);                   \
    rb_node_field((a_node), a_field).SetColor(NodeColor::Red);                 \
  } while (0)

#define rbp_lean_right(a_type, a_field, a_node, r_node)                        \
  do {                                                                         \
    NodeColor rbp_lr_color;                                                    \
    rbp_rotate_right(a_type, a_field, (a_node), (r_node));                     \
    rbp_lr_color = rb_node_field((a_node), a_field).Color();                   \
    rb_node_field((r_node), a_field).SetColor(rbp_lr_color);                   \
    rb_node_field((a_node), a_field).SetColor(NodeColor::Red);                 \
  } while (0)

#define rbp_move_red_left(a_type, a_field, a_node, r_node)                     \
  do {                                                                         \
    a_type *rbp_mrl_t, *rbp_mrl_u;                                             \
    rbp_mrl_t = rb_node_field((a_node), a_field).Left();                       \
    rb_node_field(rbp_mrl_t, a_field).SetColor(NodeColor::Red);                \
    rbp_mrl_t = rb_node_field((a_node), a_field).Right();                      \
    rbp_mrl_u = rb_node_field(rbp_mrl_t, a_field).Left();                      \
    if (rb_node_field(rbp_mrl_u, a_field).IsRed()) {                           \
      rbp_rotate_right(a_type, a_field, rbp_mrl_t, rbp_mrl_u);                 \
      rb_node_field((a_node), a_field).SetRight(rbp_mrl_u);                    \
      rbp_rotate_left(a_type, a_field, (a_node), (r_node));                    \
      rbp_mrl_t = rb_node_field((a_node), a_field).Right();                    \
      if (rb_node_field(rbp_mrl_t, a_field).IsRed()) {                         \
        rb_node_field(rbp_mrl_t, a_field).SetColor(NodeColor::Black);          \
        rb_node_field((a_node), a_field).SetColor(NodeColor::Red);             \
        rbp_rotate_left(a_type, a_field, (a_node), rbp_mrl_t);                 \
        rb_node_field((r_node), a_field).SetLeft(rbp_mrl_t);                   \
      } else {                                                                 \
        rb_node_field((a_node), a_field).SetColor(NodeColor::Black);           \
      }                                                                        \
    } else {                                                                   \
      rb_node_field((a_node), a_field).SetColor(NodeColor::Red);               \
      rbp_rotate_left(a_type, a_field, (a_node), (r_node));                    \
    }                                                                          \
  } while (0)

#define rbp_move_red_right(a_type, a_field, a_node, r_node)                    \
  do {                                                                         \
    a_type* rbp_mrr_t;                                                         \
    rbp_mrr_t = rb_node_field((a_node), a_field).Left();                       \
    if (rb_node_field(rbp_mrr_t, a_field).IsRed()) {                           \
      a_type *rbp_mrr_u, *rbp_mrr_v;                                           \
      rbp_mrr_u = rb_node_field(rbp_mrr_t, a_field).Right();                   \
      rbp_mrr_v = rb_node_field(rbp_mrr_u, a_field).Left();                    \
      if (rb_node_field(rbp_mrr_v, a_field).IsRed()) {                         \
        rb_node_field(rbp_mrr_u, a_field)                                      \
          .SetColor(rb_node_field((a_node), a_field).Color());                 \
        rb_node_field(rbp_mrr_v, a_field).SetColor(NodeColor::Black);          \
        rbp_rotate_left(a_type, a_field, rbp_mrr_t, rbp_mrr_u);                \
        rb_node_field((a_node), a_field).SetLeft(rbp_mrr_u);                   \
        rbp_rotate_right(a_type, a_field, (a_node), (r_node));                 \
        rbp_rotate_left(a_type, a_field, (a_node), rbp_mrr_t);                 \
        rb_node_field((r_node), a_field).SetRight(rbp_mrr_t);                  \
      } else {                                                                 \
        rb_node_field(rbp_mrr_t, a_field)                                      \
          .SetColor(rb_node_field((a_node), a_field).Color());                 \
        rb_node_field(rbp_mrr_u, a_field).SetColor(NodeColor::Red);            \
        rbp_rotate_right(a_type, a_field, (a_node), (r_node));                 \
        rbp_rotate_left(a_type, a_field, (a_node), rbp_mrr_t);                 \
        rb_node_field((r_node), a_field).SetRight(rbp_mrr_t);                  \
      }                                                                        \
      rb_node_field((a_node), a_field).SetColor(NodeColor::Red);               \
    } else {                                                                   \
      rb_node_field(rbp_mrr_t, a_field).SetColor(NodeColor::Red);              \
      rbp_mrr_t = rb_node_field(rbp_mrr_t, a_field).Left();                    \
      if (rb_node_field(rbp_mrr_t, a_field).IsRed()) {                         \
        rb_node_field(rbp_mrr_t, a_field).SetColor(NodeColor::Black);          \
        rbp_rotate_right(a_type, a_field, (a_node), (r_node));                 \
        rbp_rotate_left(a_type, a_field, (a_node), rbp_mrr_t);                 \
        rb_node_field((r_node), a_field).SetRight(rbp_mrr_t);                  \
      } else {                                                                 \
        rbp_rotate_left(a_type, a_field, (a_node), (r_node));                  \
      }                                                                        \
    }                                                                          \
  } while (0)

#define rb_insert(a_type, a_field, a_cmp, a_tree, a_node)                      \
  do {                                                                         \
    a_type rbp_i_s;                                                            \
    a_type *rbp_i_g, *rbp_i_p, *rbp_i_c, *rbp_i_t, *rbp_i_u;                   \
    int rbp_i_cmp = 0;                                                         \
    rbp_i_g = &(a_tree)->rbt_nil;                                              \
    rb_node_field(&rbp_i_s, a_field).SetLeft((a_tree)->rbt_root);              \
    rb_node_field(&rbp_i_s, a_field).SetRight(&(a_tree)->rbt_nil);             \
    rb_node_field(&rbp_i_s, a_field).SetColor(NodeColor::Black);               \
    rbp_i_p = &rbp_i_s;                                                        \
    rbp_i_c = (a_tree)->rbt_root;                                              \
    /* Iteratively search down the tree for the insertion point,      */       \
    /* splitting 4-nodes as they are encountered.  At the end of each */       \
    /* iteration, rbp_i_g->rbp_i_p->rbp_i_c is a 3-level path down    */       \
    /* the tree, assuming a sufficiently deep tree.                   */       \
    while (rbp_i_c != &(a_tree)->rbt_nil) {                                    \
      rbp_i_t = rb_node_field(rbp_i_c, a_field).Left();                        \
      rbp_i_u = rb_node_field(rbp_i_t, a_field).Left();                        \
      if (rb_node_field(rbp_i_t, a_field).IsRed() &&                           \
          rb_node_field(rbp_i_u, a_field).IsRed()) {                           \
        /* rbp_i_c is the top of a logical 4-node, so split it.   */           \
        /* This iteration does not move down the tree, due to the */           \
        /* disruptiveness of node splitting.                      */           \
        /*                                                        */           \
        /* Rotate right.                                          */           \
        rbp_rotate_right(a_type, a_field, rbp_i_c, rbp_i_t);                   \
        /* Pass red links up one level.                           */           \
        rbp_i_u = rb_node_field(rbp_i_t, a_field).Left();                      \
        rb_node_field(rbp_i_u, a_field).SetColor(NodeColor::Black);            \
        if (rb_node_field(rbp_i_p, a_field).Left() == rbp_i_c) {               \
          rb_node_field(rbp_i_p, a_field).SetLeft(rbp_i_t);                    \
          rbp_i_c = rbp_i_t;                                                   \
        } else {                                                               \
          /* rbp_i_c was the right child of rbp_i_p, so rotate  */             \
          /* left in order to maintain the left-leaning         */             \
          /* invariant.                                         */             \
          MOZ_ASSERT(rb_node_field(rbp_i_p, a_field).Right() == rbp_i_c);      \
          rb_node_field(rbp_i_p, a_field).SetRight(rbp_i_t);                   \
          rbp_lean_left(a_type, a_field, rbp_i_p, rbp_i_u);                    \
          if (rb_node_field(rbp_i_g, a_field).Left() == rbp_i_p) {             \
            rb_node_field(rbp_i_g, a_field).SetLeft(rbp_i_u);                  \
          } else {                                                             \
            MOZ_ASSERT(rb_node_field(rbp_i_g, a_field).Right() == rbp_i_p);    \
            rb_node_field(rbp_i_g, a_field).SetRight(rbp_i_u);                 \
          }                                                                    \
          rbp_i_p = rbp_i_u;                                                   \
          rbp_i_cmp = (a_cmp)((a_node), rbp_i_p);                              \
          if (rbp_i_cmp < 0) {                                                 \
            rbp_i_c = rb_node_field(rbp_i_p, a_field).Left();                  \
          } else {                                                             \
            MOZ_ASSERT(rbp_i_cmp > 0);                                         \
            rbp_i_c = rb_node_field(rbp_i_p, a_field).Right();                 \
          }                                                                    \
          continue;                                                            \
        }                                                                      \
      }                                                                        \
      rbp_i_g = rbp_i_p;                                                       \
      rbp_i_p = rbp_i_c;                                                       \
      rbp_i_cmp = (a_cmp)((a_node), rbp_i_c);                                  \
      if (rbp_i_cmp < 0) {                                                     \
        rbp_i_c = rb_node_field(rbp_i_c, a_field).Left();                      \
      } else {                                                                 \
        MOZ_ASSERT(rbp_i_cmp > 0);                                             \
        rbp_i_c = rb_node_field(rbp_i_c, a_field).Right();                     \
      }                                                                        \
    }                                                                          \
    /* rbp_i_p now refers to the node under which to insert.          */       \
    rb_node_field(a_node, a_field).SetLeft(&(a_tree)->rbt_nil);                \
    rb_node_field(a_node, a_field).SetRight(&(a_tree)->rbt_nil);               \
    rb_node_field(a_node, a_field).SetColor(NodeColor::Red);                   \
    if (rbp_i_cmp > 0) {                                                       \
      rb_node_field(rbp_i_p, a_field).SetRight((a_node));                      \
      rbp_lean_left(a_type, a_field, rbp_i_p, rbp_i_t);                        \
      if (rb_node_field(rbp_i_g, a_field).Left() == rbp_i_p) {                 \
        rb_node_field(rbp_i_g, a_field).SetLeft(rbp_i_t);                      \
      } else if (rb_node_field(rbp_i_g, a_field).Right() == rbp_i_p) {         \
        rb_node_field(rbp_i_g, a_field).SetRight(rbp_i_t);                     \
      }                                                                        \
    } else {                                                                   \
      rb_node_field(rbp_i_p, a_field).SetLeft((a_node));                       \
    }                                                                          \
    /* Update the root and make sure that it is black.                */       \
    (a_tree)->rbt_root = rb_node_field(&rbp_i_s, a_field).Left();              \
    rb_node_field((a_tree)->rbt_root, a_field).SetColor(NodeColor::Black);     \
  } while (0)

#define rb_remove(a_type, a_field, a_cmp, a_tree, a_node)                      \
  do {                                                                         \
    a_type rbp_r_s;                                                            \
    a_type *rbp_r_p, *rbp_r_c, *rbp_r_xp, *rbp_r_t, *rbp_r_u;                  \
    int rbp_r_cmp;                                                             \
    rb_node_field(&rbp_r_s, a_field).SetLeft((a_tree)->rbt_root);              \
    rb_node_field(&rbp_r_s, a_field).SetRight(&(a_tree)->rbt_nil);             \
    rb_node_field(&rbp_r_s, a_field).SetColor(NodeColor::Black);               \
    rbp_r_p = &rbp_r_s;                                                        \
    rbp_r_c = (a_tree)->rbt_root;                                              \
    rbp_r_xp = &(a_tree)->rbt_nil;                                             \
    /* Iterate down the tree, but always transform 2-nodes to 3- or   */       \
    /* 4-nodes in order to maintain the invariant that the current    */       \
    /* node is not a 2-node.  This allows simple deletion once a leaf */       \
    /* is reached.  Handle the root specially though, since there may */       \
    /* be no way to convert it from a 2-node to a 3-node.             */       \
    rbp_r_cmp = (a_cmp)((a_node), rbp_r_c);                                    \
    if (rbp_r_cmp < 0) {                                                       \
      rbp_r_t = rb_node_field(rbp_r_c, a_field).Left();                        \
      rbp_r_u = rb_node_field(rbp_r_t, a_field).Left();                        \
      if (rb_node_field(rbp_r_t, a_field).IsBlack() &&                         \
          rb_node_field(rbp_r_u, a_field).IsBlack()) {                         \
        /* Apply standard transform to prepare for left move.     */           \
        rbp_move_red_left(a_type, a_field, rbp_r_c, rbp_r_t);                  \
        rb_node_field(rbp_r_t, a_field).SetColor(NodeColor::Black);            \
        rb_node_field(rbp_r_p, a_field).SetLeft(rbp_r_t);                      \
        rbp_r_c = rbp_r_t;                                                     \
      } else {                                                                 \
        /* Move left.                                             */           \
        rbp_r_p = rbp_r_c;                                                     \
        rbp_r_c = rb_node_field(rbp_r_c, a_field).Left();                      \
      }                                                                        \
    } else {                                                                   \
      if (rbp_r_cmp == 0) {                                                    \
        MOZ_ASSERT((a_node) == rbp_r_c);                                       \
        if (rb_node_field(rbp_r_c, a_field).Right() == &(a_tree)->rbt_nil) {   \
          /* Delete root node (which is also a leaf node).      */             \
          if (rb_node_field(rbp_r_c, a_field).Left() != &(a_tree)->rbt_nil) {  \
            rbp_lean_right(a_type, a_field, rbp_r_c, rbp_r_t);                 \
            rb_node_field(rbp_r_t, a_field).SetRight(&(a_tree)->rbt_nil);      \
          } else {                                                             \
            rbp_r_t = &(a_tree)->rbt_nil;                                      \
          }                                                                    \
          rb_node_field(rbp_r_p, a_field).SetLeft(rbp_r_t);                    \
        } else {                                                               \
          /* This is the node we want to delete, but we will    */             \
          /* instead swap it with its successor and delete the  */             \
          /* successor.  Record enough information to do the    */             \
          /* swap later.  rbp_r_xp is the a_node's parent.      */             \
          rbp_r_xp = rbp_r_p;                                                  \
          rbp_r_cmp = 1; /* Note that deletion is incomplete.   */             \
        }                                                                      \
      }                                                                        \
      if (rbp_r_cmp == 1) {                                                    \
        if (rb_node_field(                                                     \
              rb_node_field(rb_node_field(rbp_r_c, a_field).Right(), a_field)  \
                .Left(),                                                       \
              a_field)                                                         \
              .IsBlack()) {                                                    \
          rbp_r_t = rb_node_field(rbp_r_c, a_field).Left();                    \
          if (rb_node_field(rbp_r_t, a_field).IsRed()) {                       \
            /* Standard transform.                            */               \
            rbp_move_red_right(a_type, a_field, rbp_r_c, rbp_r_t);             \
          } else {                                                             \
            /* Root-specific transform.                       */               \
            rb_node_field(rbp_r_c, a_field).SetColor(NodeColor::Red);          \
            rbp_r_u = rb_node_field(rbp_r_t, a_field).Left();                  \
            if (rb_node_field(rbp_r_u, a_field).IsRed()) {                     \
              rb_node_field(rbp_r_u, a_field).SetColor(NodeColor::Black);      \
              rbp_rotate_right(a_type, a_field, rbp_r_c, rbp_r_t);             \
              rbp_rotate_left(a_type, a_field, rbp_r_c, rbp_r_u);              \
              rb_node_field(rbp_r_t, a_field).SetRight(rbp_r_u);               \
            } else {                                                           \
              rb_node_field(rbp_r_t, a_field).SetColor(NodeColor::Red);        \
              rbp_rotate_left(a_type, a_field, rbp_r_c, rbp_r_t);              \
            }                                                                  \
          }                                                                    \
          rb_node_field(rbp_r_p, a_field).SetLeft(rbp_r_t);                    \
          rbp_r_c = rbp_r_t;                                                   \
        } else {                                                               \
          /* Move right.                                        */             \
          rbp_r_p = rbp_r_c;                                                   \
          rbp_r_c = rb_node_field(rbp_r_c, a_field).Right();                   \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    if (rbp_r_cmp != 0) {                                                      \
      while (true) {                                                           \
        MOZ_ASSERT(rbp_r_p != &(a_tree)->rbt_nil);                             \
        rbp_r_cmp = (a_cmp)((a_node), rbp_r_c);                                \
        if (rbp_r_cmp < 0) {                                                   \
          rbp_r_t = rb_node_field(rbp_r_c, a_field).Left();                    \
          if (rbp_r_t == &(a_tree)->rbt_nil) {                                 \
            /* rbp_r_c now refers to the successor node to    */               \
            /* relocate, and rbp_r_xp/a_node refer to the     */               \
            /* context for the relocation.                    */               \
            if (rb_node_field(rbp_r_xp, a_field).Left() == (a_node)) {         \
              rb_node_field(rbp_r_xp, a_field).SetLeft(rbp_r_c);               \
            } else {                                                           \
              MOZ_ASSERT(rb_node_field(rbp_r_xp, a_field).Right() ==           \
                         (a_node));                                            \
              rb_node_field(rbp_r_xp, a_field).SetRight(rbp_r_c);              \
            }                                                                  \
            rb_node_field(rbp_r_c, a_field)                                    \
              .SetLeft(rb_node_field((a_node), a_field).Left());               \
            rb_node_field(rbp_r_c, a_field)                                    \
              .SetRight(rb_node_field((a_node), a_field).Right());             \
            rb_node_field(rbp_r_c, a_field)                                    \
              .SetColor(rb_node_field((a_node), a_field).Color());             \
            if (rb_node_field(rbp_r_p, a_field).Left() == rbp_r_c) {           \
              rb_node_field(rbp_r_p, a_field).SetLeft(&(a_tree)->rbt_nil);     \
            } else {                                                           \
              MOZ_ASSERT(rb_node_field(rbp_r_p, a_field).Right() == rbp_r_c);  \
              rb_node_field(rbp_r_p, a_field).SetRight(&(a_tree)->rbt_nil);    \
            }                                                                  \
            break;                                                             \
          }                                                                    \
          rbp_r_u = rb_node_field(rbp_r_t, a_field).Left();                    \
          if (rb_node_field(rbp_r_t, a_field).IsBlack() &&                     \
              rb_node_field(rbp_r_u, a_field).IsBlack()) {                     \
            rbp_move_red_left(a_type, a_field, rbp_r_c, rbp_r_t);              \
            if (rb_node_field(rbp_r_p, a_field).Left() == rbp_r_c) {           \
              rb_node_field(rbp_r_p, a_field).SetLeft(rbp_r_t);                \
            } else {                                                           \
              rb_node_field(rbp_r_p, a_field).SetRight(rbp_r_t);               \
            }                                                                  \
            rbp_r_c = rbp_r_t;                                                 \
          } else {                                                             \
            rbp_r_p = rbp_r_c;                                                 \
            rbp_r_c = rb_node_field(rbp_r_c, a_field).Left();                  \
          }                                                                    \
        } else {                                                               \
          /* Check whether to delete this node (it has to be    */             \
          /* the correct node and a leaf node).                 */             \
          if (rbp_r_cmp == 0) {                                                \
            MOZ_ASSERT((a_node) == rbp_r_c);                                   \
            if (rb_node_field(rbp_r_c, a_field).Right() ==                     \
                &(a_tree)->rbt_nil) {                                          \
              /* Delete leaf node.                          */                 \
              if (rb_node_field(rbp_r_c, a_field).Left() !=                    \
                  &(a_tree)->rbt_nil) {                                        \
                rbp_lean_right(a_type, a_field, rbp_r_c, rbp_r_t);             \
                rb_node_field(rbp_r_t, a_field).SetRight(&(a_tree)->rbt_nil);  \
              } else {                                                         \
                rbp_r_t = &(a_tree)->rbt_nil;                                  \
              }                                                                \
              if (rb_node_field(rbp_r_p, a_field).Left() == rbp_r_c) {         \
                rb_node_field(rbp_r_p, a_field).SetLeft(rbp_r_t);              \
              } else {                                                         \
                rb_node_field(rbp_r_p, a_field).SetRight(rbp_r_t);             \
              }                                                                \
              break;                                                           \
            } else {                                                           \
              /* This is the node we want to delete, but we */                 \
              /* will instead swap it with its successor    */                 \
              /* and delete the successor.  Record enough   */                 \
              /* information to do the swap later.          */                 \
              /* rbp_r_xp is a_node's parent.               */                 \
              rbp_r_xp = rbp_r_p;                                              \
            }                                                                  \
          }                                                                    \
          rbp_r_t = rb_node_field(rbp_r_c, a_field).Right();                   \
          rbp_r_u = rb_node_field(rbp_r_t, a_field).Left();                    \
          if (rb_node_field(rbp_r_u, a_field).IsBlack()) {                     \
            rbp_move_red_right(a_type, a_field, rbp_r_c, rbp_r_t);             \
            if (rb_node_field(rbp_r_p, a_field).Left() == rbp_r_c) {           \
              rb_node_field(rbp_r_p, a_field).SetLeft(rbp_r_t);                \
            } else {                                                           \
              rb_node_field(rbp_r_p, a_field).SetRight(rbp_r_t);               \
            }                                                                  \
            rbp_r_c = rbp_r_t;                                                 \
          } else {                                                             \
            rbp_r_p = rbp_r_c;                                                 \
            rbp_r_c = rb_node_field(rbp_r_c, a_field).Right();                 \
          }                                                                    \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    /* Update root.                                                   */       \
    (a_tree)->rbt_root = rb_node_field(&rbp_r_s, a_field).Left();              \
  } while (0)

/* Tree structure. */
template<typename T, typename Trait>
struct RedBlackTree
{
  T* rbt_root;
  T rbt_nil;

  void Init()
  {
    rbt_root = &rbt_nil;
    rb_node_field(&rbt_nil, Trait::GetTreeNode).SetLeft(&rbt_nil);
    rb_node_field(&rbt_nil, Trait::GetTreeNode).SetRight(&rbt_nil);
    rb_node_field(&rbt_nil, Trait::GetTreeNode).SetColor(NodeColor::Black);
  }

  T* First()
  {
    T* ret;
    rbp_first(T, Trait::GetTreeNode, this, rbt_root, (ret));
    return (ret == &rbt_nil) ? nullptr : ret;
  }

  T* Last()
  {
    T* ret;
    rbp_last(T, Trait::GetTreeNode, this, rbt_root, ret);
    return (ret == &rbt_nil) ? nullptr : ret;
  }

  T* Next(T* aNode)
  {
    T* ret;
    rbp_next(T, Trait::GetTreeNode, Trait::Compare, this, (aNode), (ret));
    return (ret == &rbt_nil) ? nullptr : ret;
  }

  T* Prev(T* aNode)
  {
    T* ret;
    rbp_prev(T, Trait::GetTreeNode, Trait::Compare, this, (aNode), (ret));
    return (ret == &rbt_nil) ? nullptr : ret;
  }

  T* Search(T* aKey)
  {
    T* ret;
    rb_search(T, Trait::GetTreeNode, Trait::Compare, this, aKey, ret);
    return ret;
  }

  /* Find a match if it exists. Otherwise, find the next greater node, if one
   * exists */
  T* SearchOrNext(T* aKey)
  {
    T* ret;
    rb_nsearch(T, Trait::GetTreeNode, Trait::Compare, this, aKey, ret);
    return ret;
  }

  void Insert(T* aNode)
  {
    rb_insert(T, Trait::GetTreeNode, Trait::Compare, this, aNode);
  }

  void Remove(T* aNode)
  {
    rb_remove(T, Trait::GetTreeNode, Trait::Compare, this, aNode);
  }
};

/*
 * The iterators simulate recursion via an array of pointers that store the
 * current path.  This is critical to performance, since a series of calls to
 * rb_{next,prev}() would require time proportional to (n lg n), whereas this
 * implementation only requires time proportional to (n).
 *
 * Since the iterators cache a path down the tree, any tree modification may
 * cause the cached path to become invalid.  In order to continue iteration,
 * use something like the following sequence:
 *
 *   {
 *       a_type *node, *tnode;
 *
 *       rb_foreach_begin(a_type, a_field, a_tree, node) {
 *           ...
 *           rb_next(a_type, a_field, a_cmp, a_tree, node, tnode);
 *           rb_remove(a_type, a_field, a_cmp, a_tree, node);
 *           ...
 *       } rb_foreach_end(a_type, a_field, a_tree, node)
 *   }
 *
 * Note that this idiom is not advised if every iteration modifies the tree,
 * since in that case there is no algorithmic complexity improvement over a
 * series of rb_{next,prev}() calls, thus making the setup overhead wasted
 * effort.
 */

/*
 * Size the path arrays such that they are always large enough, even if a
 * tree consumes all of memory.  Since each node must contain a minimum of
 * two pointers, there can never be more nodes than:
 *
 *   1 << ((SIZEOF_PTR<<3) - (SIZEOF_PTR_2POW+1))
 *
 * Since the depth of a tree is limited to 3*lg(#nodes), the maximum depth
 * is:
 *
 *   (3 * ((SIZEOF_PTR<<3) - (SIZEOF_PTR_2POW+1)))
 *
 * This works out to a maximum depth of 87 and 180 for 32- and 64-bit
 * systems, respectively (approximately 348 and 1440 bytes, respectively).
 */
#define rbp_f_height (3 * ((SIZEOF_PTR << 3) - (SIZEOF_PTR_2POW + 1)))

#define rb_foreach_begin(a_type, a_field, a_tree, a_var)                       \
  {                                                                            \
    {                                                                          \
      /* Initialize the path to contain the left spine.             */         \
      a_type* rbp_f_path[rbp_f_height];                                        \
      a_type* rbp_f_node;                                                      \
      bool rbp_f_synced = false;                                               \
      unsigned rbp_f_depth = 0;                                                \
      if ((a_tree)->rbt_root != &(a_tree)->rbt_nil) {                          \
        rbp_f_path[rbp_f_depth] = (a_tree)->rbt_root;                          \
        rbp_f_depth++;                                                         \
        while (                                                                \
          (rbp_f_node =                                                        \
             rb_node_field(rbp_f_path[rbp_f_depth - 1], a_field).Left()) !=    \
          &(a_tree)->rbt_nil) {                                                \
          rbp_f_path[rbp_f_depth] = rbp_f_node;                                \
          rbp_f_depth++;                                                       \
        }                                                                      \
      }                                                                        \
      /* While the path is non-empty, iterate.                      */         \
      while (rbp_f_depth > 0) {                                                \
        (a_var) = rbp_f_path[rbp_f_depth - 1];

#define rb_foreach_end(a_type, a_field, a_tree, a_var)                         \
  if (rbp_f_synced) {                                                          \
    rbp_f_synced = false;                                                      \
    continue;                                                                  \
  }                                                                            \
  /* Find the successor.                                    */                 \
  if ((rbp_f_node =                                                            \
         rb_node_field(rbp_f_path[rbp_f_depth - 1], a_field).Right()) !=       \
      &(a_tree)->rbt_nil) {                                                    \
    /* The successor is the left-most node in the right   */                   \
    /* subtree.                                           */                   \
    rbp_f_path[rbp_f_depth] = rbp_f_node;                                      \
    rbp_f_depth++;                                                             \
    while ((rbp_f_node =                                                       \
              rb_node_field(rbp_f_path[rbp_f_depth - 1], a_field).Left()) !=   \
           &(a_tree)->rbt_nil) {                                               \
      rbp_f_path[rbp_f_depth] = rbp_f_node;                                    \
      rbp_f_depth++;                                                           \
    }                                                                          \
  } else {                                                                     \
    /* The successor is above the current node.  Unwind   */                   \
    /* until a left-leaning edge is removed from the      */                   \
    /* path, or the path is empty.                        */                   \
    for (rbp_f_depth--; rbp_f_depth > 0; rbp_f_depth--) {                      \
      if (rb_node_field(rbp_f_path[rbp_f_depth - 1], a_field).Left() ==        \
          rbp_f_path[rbp_f_depth]) {                                           \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
  }                                                                            \
  }                                                                            \
  }                                                                            \
  }

#endif /* RB_H_ */
