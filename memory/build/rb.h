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

#include "mozilla/Alignment.h"
#include "Utils.h"

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

/* Tree structure. */
template<typename T, typename Trait>
class RedBlackTree
{
public:
  void Init() { mRoot = nullptr; }

  T* First(T* aStart = nullptr)
  {
    return First(reinterpret_cast<TreeNode*>(aStart));
  }

  T* Last(T* aStart = nullptr)
  {
    return Last(reinterpret_cast<TreeNode*>(aStart));
  }

  T* Next(T* aNode)
  {
    return Next(reinterpret_cast<TreeNode*>(aNode));
  }

  T* Prev(T* aNode)
  {
    return Prev(reinterpret_cast<TreeNode*>(aNode));
  }

  T* Search(T* aKey)
  {
    return Search(reinterpret_cast<TreeNode*>(aKey));
  }

  /* Find a match if it exists. Otherwise, find the next greater node, if one
   * exists */
  T* SearchOrNext(T* aKey)
  {
    return SearchOrNext(reinterpret_cast<TreeNode*>(aKey));
  }

  void Insert(T* aNode)
  {
    Insert(reinterpret_cast<TreeNode*>(aNode));
  }

  void Remove(T* aNode)
  {
    return Remove(reinterpret_cast<TreeNode*>(aNode));
  }

  /* Helper class to avoid having all the tree traversal code further below
   * have to use Trait::GetTreeNode, adding visual noise. */
  struct TreeNode : public T
  {
    TreeNode* Left()
    {
      return (TreeNode*)Trait::GetTreeNode(this).Left();
    }

    void SetLeft(T* aValue)
    {
      Trait::GetTreeNode(this).SetLeft(aValue);
    }

    TreeNode* Right()
    {
      return (TreeNode*)Trait::GetTreeNode(this).Right();
    }

    void SetRight(T* aValue)
    {
      Trait::GetTreeNode(this).SetRight(aValue);
    }

    NodeColor Color()
    {
      return Trait::GetTreeNode(this).Color();
    }

    bool IsRed()
    {
      return Trait::GetTreeNode(this).IsRed();
    }

    bool IsBlack()
    {
      return Trait::GetTreeNode(this).IsBlack();
    }

    void SetColor(NodeColor aColor)
    {
      Trait::GetTreeNode(this).SetColor(aColor);
    }
  };

private:
  TreeNode* mRoot;

  TreeNode* First(TreeNode* aStart)
  {
    TreeNode* ret;
    for (ret = aStart ? aStart : mRoot; ret && ret->Left(); ret = ret->Left()) {
    }
    return ret;
  }

  TreeNode* Last(TreeNode* aStart)
  {
    TreeNode* ret;
    for (ret = aStart ? aStart : mRoot; ret && ret->Right();
         ret = ret->Right()) {
    }
    return ret;
  }

  TreeNode* Next(TreeNode* aNode)
  {
    TreeNode* ret;
    if (aNode->Right()) {
      ret = First(aNode->Right());
    } else {
      TreeNode* rbp_n_t = mRoot;
      MOZ_ASSERT(rbp_n_t);
      ret = nullptr;
      while (true) {
        int rbp_n_cmp = Trait::Compare(aNode, rbp_n_t);
        if (rbp_n_cmp < 0) {
          ret = rbp_n_t;
          rbp_n_t = rbp_n_t->Left();
        } else if (rbp_n_cmp > 0) {
          rbp_n_t = rbp_n_t->Right();
        } else {
          break;
        }
        MOZ_ASSERT(rbp_n_t);
      }
    }
    return ret;
  }

  TreeNode* Prev(TreeNode* aNode)
  {
    TreeNode* ret;
    if (aNode->Left()) {
      ret = Last(aNode->Left());
    } else {
      TreeNode* rbp_p_t = mRoot;
      MOZ_ASSERT(rbp_p_t);
      ret = nullptr;
      while (true) {
        int rbp_p_cmp = Trait::Compare(aNode, rbp_p_t);
        if (rbp_p_cmp < 0) {
          rbp_p_t = rbp_p_t->Left();
        } else if (rbp_p_cmp > 0) {
          ret = rbp_p_t;
          rbp_p_t = rbp_p_t->Right();
        } else {
          break;
        }
        MOZ_ASSERT(rbp_p_t);
      }
    }
    return ret;
  }

  TreeNode* Search(TreeNode* aKey)
  {
    TreeNode* ret = mRoot;
    int rbp_se_cmp;
    while (ret && (rbp_se_cmp = Trait::Compare(aKey, ret)) != 0) {
      if (rbp_se_cmp < 0) {
        ret = ret->Left();
      } else {
        ret = ret->Right();
      }
    }
    return ret;
  }

  TreeNode* SearchOrNext(TreeNode* aKey)
  {
    TreeNode* ret = nullptr;
    TreeNode* rbp_ns_t = mRoot;
    while (rbp_ns_t) {
      int rbp_ns_cmp = Trait::Compare(aKey, rbp_ns_t);
      if (rbp_ns_cmp < 0) {
        ret = rbp_ns_t;
        rbp_ns_t = rbp_ns_t->Left();
      } else if (rbp_ns_cmp > 0) {
        rbp_ns_t = rbp_ns_t->Right();
      } else {
        ret = rbp_ns_t;
        break;
      }
    }
    return ret;
  }

  void Insert(TreeNode* aNode)
  {
    // rbp_i_s is only used as a placeholder for its RedBlackTreeNode. Use
    // AlignedStorage2 to avoid running the TreeNode base class constructor.
    mozilla::AlignedStorage2<TreeNode> rbp_i_s;
    TreeNode *rbp_i_g, *rbp_i_p, *rbp_i_c, *rbp_i_t, *rbp_i_u;
    int rbp_i_cmp = 0;
    rbp_i_g = nullptr;
    rbp_i_p = rbp_i_s.addr();
    rbp_i_p->SetLeft(mRoot);
    rbp_i_p->SetRight(nullptr);
    rbp_i_p->SetColor(NodeColor::Black);
    rbp_i_c = mRoot;
    /* Iteratively search down the tree for the insertion point,
     * splitting 4-nodes as they are encountered. At the end of each
     * iteration, rbp_i_g->rbp_i_p->rbp_i_c is a 3-level path down
     * the tree, assuming a sufficiently deep tree. */
    while (rbp_i_c) {
      rbp_i_t = rbp_i_c->Left();
      rbp_i_u = rbp_i_t ? rbp_i_t->Left() : nullptr;
      if (rbp_i_t && rbp_i_u && rbp_i_t->IsRed() && rbp_i_u->IsRed()) {
        /* rbp_i_c is the top of a logical 4-node, so split it.
         * This iteration does not move down the tree, due to the
         * disruptiveness of node splitting.
         *
         * Rotate right. */
        rbp_i_t = RotateRight(rbp_i_c);
        /* Pass red links up one level. */
        rbp_i_u = rbp_i_t->Left();
        rbp_i_u->SetColor(NodeColor::Black);
        if (rbp_i_p->Left() == rbp_i_c) {
          rbp_i_p->SetLeft(rbp_i_t);
          rbp_i_c = rbp_i_t;
        } else {
          /* rbp_i_c was the right child of rbp_i_p, so rotate
           * left in order to maintain the left-leaning invariant. */
          MOZ_ASSERT(rbp_i_p->Right() == rbp_i_c);
          rbp_i_p->SetRight(rbp_i_t);
          rbp_i_u = LeanLeft(rbp_i_p);
          if (rbp_i_g->Left() == rbp_i_p) {
            rbp_i_g->SetLeft(rbp_i_u);
          } else {
            MOZ_ASSERT(rbp_i_g->Right() == rbp_i_p);
            rbp_i_g->SetRight(rbp_i_u);
          }
          rbp_i_p = rbp_i_u;
          rbp_i_cmp = Trait::Compare(aNode, rbp_i_p);
          if (rbp_i_cmp < 0) {
            rbp_i_c = rbp_i_p->Left();
          } else {
            MOZ_ASSERT(rbp_i_cmp > 0);
            rbp_i_c = rbp_i_p->Right();
          }
          continue;
        }
      }
      rbp_i_g = rbp_i_p;
      rbp_i_p = rbp_i_c;
      rbp_i_cmp = Trait::Compare(aNode, rbp_i_c);
      if (rbp_i_cmp < 0) {
        rbp_i_c = rbp_i_c->Left();
      } else {
        MOZ_ASSERT(rbp_i_cmp > 0);
        rbp_i_c = rbp_i_c->Right();
      }
    }
    /* rbp_i_p now refers to the node under which to insert. */
    aNode->SetLeft(nullptr);
    aNode->SetRight(nullptr);
    aNode->SetColor(NodeColor::Red);
    if (rbp_i_cmp > 0) {
      rbp_i_p->SetRight(aNode);
      rbp_i_t = LeanLeft(rbp_i_p);
      if (rbp_i_g->Left() == rbp_i_p) {
        rbp_i_g->SetLeft(rbp_i_t);
      } else if (rbp_i_g->Right() == rbp_i_p) {
        rbp_i_g->SetRight(rbp_i_t);
      }
    } else {
      rbp_i_p->SetLeft(aNode);
    }
    /* Update the root and make sure that it is black. */
    mRoot = rbp_i_s.addr()->Left();
    mRoot->SetColor(NodeColor::Black);
  }

  void Remove(TreeNode* aNode)
  {
    // rbp_r_s is only used as a placeholder for its RedBlackTreeNode. Use
    // AlignedStorage2 to avoid running the TreeNode base class constructor.
    mozilla::AlignedStorage2<TreeNode> rbp_r_s;
    TreeNode *rbp_r_p, *rbp_r_c, *rbp_r_xp, *rbp_r_t, *rbp_r_u;
    int rbp_r_cmp;
    rbp_r_p = rbp_r_s.addr();
    rbp_r_p->SetLeft(mRoot);
    rbp_r_p->SetRight(nullptr);
    rbp_r_p->SetColor(NodeColor::Black);
    rbp_r_c = mRoot;
    rbp_r_xp = nullptr;
    /* Iterate down the tree, but always transform 2-nodes to 3- or
     * 4-nodes in order to maintain the invariant that the current
     * node is not a 2-node. This allows simple deletion once a leaf
     * is reached. Handle the root specially though, since there may
     * be no way to convert it from a 2-node to a 3-node. */
    rbp_r_cmp = Trait::Compare(aNode, rbp_r_c);
    if (rbp_r_cmp < 0) {
      rbp_r_t = rbp_r_c->Left();
      rbp_r_u = rbp_r_t ? rbp_r_t->Left() : nullptr;
      if ((!rbp_r_t || rbp_r_t->IsBlack()) &&
          (!rbp_r_u || rbp_r_u->IsBlack())) {
        /* Apply standard transform to prepare for left move. */
        rbp_r_t = MoveRedLeft(rbp_r_c);
        rbp_r_t->SetColor(NodeColor::Black);
        rbp_r_p->SetLeft(rbp_r_t);
        rbp_r_c = rbp_r_t;
      } else {
        /* Move left. */
        rbp_r_p = rbp_r_c;
        rbp_r_c = rbp_r_c->Left();
      }
    } else {
      if (rbp_r_cmp == 0) {
        MOZ_ASSERT(aNode == rbp_r_c);
        if (!rbp_r_c->Right()) {
          /* Delete root node (which is also a leaf node). */
          if (rbp_r_c->Left()) {
            rbp_r_t = LeanRight(rbp_r_c);
            rbp_r_t->SetRight(nullptr);
          } else {
            rbp_r_t = nullptr;
          }
          rbp_r_p->SetLeft(rbp_r_t);
        } else {
          /* This is the node we want to delete, but we will
           * instead swap it with its successor and delete the
           * successor. Record enough information to do the
           * swap later. rbp_r_xp is the aNode's parent. */
          rbp_r_xp = rbp_r_p;
          rbp_r_cmp = 1; /* Note that deletion is incomplete. */
        }
      }
      if (rbp_r_cmp == 1) {
        if (rbp_r_c->Right() && (!rbp_r_c->Right()->Left() ||
                                 rbp_r_c->Right()->Left()->IsBlack())) {
          rbp_r_t = rbp_r_c->Left();
          if (rbp_r_t->IsRed()) {
            /* Standard transform. */
            rbp_r_t = MoveRedRight(rbp_r_c);
          } else {
            /* Root-specific transform. */
            rbp_r_c->SetColor(NodeColor::Red);
            rbp_r_u = rbp_r_t->Left();
            if (rbp_r_u && rbp_r_u->IsRed()) {
              rbp_r_u->SetColor(NodeColor::Black);
              rbp_r_t = RotateRight(rbp_r_c);
              rbp_r_u = RotateLeft(rbp_r_c);
              rbp_r_t->SetRight(rbp_r_u);
            } else {
              rbp_r_t->SetColor(NodeColor::Red);
              rbp_r_t = RotateLeft(rbp_r_c);
            }
          }
          rbp_r_p->SetLeft(rbp_r_t);
          rbp_r_c = rbp_r_t;
        } else {
          /* Move right. */
          rbp_r_p = rbp_r_c;
          rbp_r_c = rbp_r_c->Right();
        }
      }
    }
    if (rbp_r_cmp != 0) {
      while (true) {
        MOZ_ASSERT(rbp_r_p);
        rbp_r_cmp = Trait::Compare(aNode, rbp_r_c);
        if (rbp_r_cmp < 0) {
          rbp_r_t = rbp_r_c->Left();
          if (!rbp_r_t) {
            /* rbp_r_c now refers to the successor node to
             * relocate, and rbp_r_xp/aNode refer to the
             * context for the relocation. */
            if (rbp_r_xp->Left() == aNode) {
              rbp_r_xp->SetLeft(rbp_r_c);
            } else {
              MOZ_ASSERT(rbp_r_xp->Right() == (aNode));
              rbp_r_xp->SetRight(rbp_r_c);
            }
            rbp_r_c->SetLeft(aNode->Left());
            rbp_r_c->SetRight(aNode->Right());
            rbp_r_c->SetColor(aNode->Color());
            if (rbp_r_p->Left() == rbp_r_c) {
              rbp_r_p->SetLeft(nullptr);
            } else {
              MOZ_ASSERT(rbp_r_p->Right() == rbp_r_c);
              rbp_r_p->SetRight(nullptr);
            }
            break;
          }
          rbp_r_u = rbp_r_t->Left();
          if (rbp_r_t->IsBlack() && (!rbp_r_u || rbp_r_u->IsBlack())) {
            rbp_r_t = MoveRedLeft(rbp_r_c);
            if (rbp_r_p->Left() == rbp_r_c) {
              rbp_r_p->SetLeft(rbp_r_t);
            } else {
              rbp_r_p->SetRight(rbp_r_t);
            }
            rbp_r_c = rbp_r_t;
          } else {
            rbp_r_p = rbp_r_c;
            rbp_r_c = rbp_r_c->Left();
          }
        } else {
          /* Check whether to delete this node (it has to be
           * the correct node and a leaf node). */
          if (rbp_r_cmp == 0) {
            MOZ_ASSERT(aNode == rbp_r_c);
            if (!rbp_r_c->Right()) {
              /* Delete leaf node. */
              if (rbp_r_c->Left()) {
                rbp_r_t = LeanRight(rbp_r_c);
                rbp_r_t->SetRight(nullptr);
              } else {
                rbp_r_t = nullptr;
              }
              if (rbp_r_p->Left() == rbp_r_c) {
                rbp_r_p->SetLeft(rbp_r_t);
              } else {
                rbp_r_p->SetRight(rbp_r_t);
              }
              break;
            }
            /* This is the node we want to delete, but we
             * will instead swap it with its successor
             * and delete the successor. Record enough
             * information to do the swap later.
             * rbp_r_xp is aNode's parent. */
            rbp_r_xp = rbp_r_p;
          }
          rbp_r_t = rbp_r_c->Right();
          rbp_r_u = rbp_r_t->Left();
          if (!rbp_r_u || rbp_r_u->IsBlack()) {
            rbp_r_t = MoveRedRight(rbp_r_c);
            if (rbp_r_p->Left() == rbp_r_c) {
              rbp_r_p->SetLeft(rbp_r_t);
            } else {
              rbp_r_p->SetRight(rbp_r_t);
            }
            rbp_r_c = rbp_r_t;
          } else {
            rbp_r_p = rbp_r_c;
            rbp_r_c = rbp_r_c->Right();
          }
        }
      }
    }
    /* Update root. */
    mRoot = rbp_r_s.addr()->Left();
  }

  TreeNode* RotateLeft(TreeNode* aNode)
  {
    TreeNode* node = aNode->Right();
    aNode->SetRight(node->Left());
    node->SetLeft(aNode);
    return node;
  }

  TreeNode* RotateRight(TreeNode* aNode)
  {
    TreeNode* node = aNode->Left();
    aNode->SetLeft(node->Right());
    node->SetRight(aNode);
    return node;
  }

  TreeNode* LeanLeft(TreeNode* aNode)
  {
    TreeNode* node = RotateLeft(aNode);
    NodeColor color = aNode->Color();
    node->SetColor(color);
    aNode->SetColor(NodeColor::Red);
    return node;
  }

  TreeNode* LeanRight(TreeNode* aNode)
  {
    TreeNode* node = RotateRight(aNode);
    NodeColor color = aNode->Color();
    node->SetColor(color);
    aNode->SetColor(NodeColor::Red);
    return node;
  }

  TreeNode* MoveRedLeft(TreeNode* aNode)
  {
    TreeNode* node;
    TreeNode *rbp_mrl_t, *rbp_mrl_u;
    rbp_mrl_t = aNode->Left();
    rbp_mrl_t->SetColor(NodeColor::Red);
    rbp_mrl_t = aNode->Right();
    rbp_mrl_u = rbp_mrl_t ? rbp_mrl_t->Left() : nullptr;
    if (rbp_mrl_u && rbp_mrl_u->IsRed()) {
      rbp_mrl_u = RotateRight(rbp_mrl_t);
      aNode->SetRight(rbp_mrl_u);
      node = RotateLeft(aNode);
      rbp_mrl_t = aNode->Right();
      if (rbp_mrl_t && rbp_mrl_t->IsRed()) {
        rbp_mrl_t->SetColor(NodeColor::Black);
        aNode->SetColor(NodeColor::Red);
        rbp_mrl_t = RotateLeft(aNode);
        node->SetLeft(rbp_mrl_t);
      } else {
        aNode->SetColor(NodeColor::Black);
      }
    } else {
      aNode->SetColor(NodeColor::Red);
      node = RotateLeft(aNode);
    }
    return node;
  }

  TreeNode* MoveRedRight(TreeNode* aNode)
  {
    TreeNode* node;
    TreeNode* rbp_mrr_t;
    rbp_mrr_t = aNode->Left();
    if (rbp_mrr_t && rbp_mrr_t->IsRed()) {
      TreeNode *rbp_mrr_u, *rbp_mrr_v;
      rbp_mrr_u = rbp_mrr_t->Right();
      rbp_mrr_v = rbp_mrr_u ? rbp_mrr_u->Left() : nullptr;
      if (rbp_mrr_v && rbp_mrr_v->IsRed()) {
        rbp_mrr_u->SetColor(aNode->Color());
        rbp_mrr_v->SetColor(NodeColor::Black);
        rbp_mrr_u = RotateLeft(rbp_mrr_t);
        aNode->SetLeft(rbp_mrr_u);
        node = RotateRight(aNode);
        rbp_mrr_t = RotateLeft(aNode);
        node->SetRight(rbp_mrr_t);
      } else {
        rbp_mrr_t->SetColor(aNode->Color());
        rbp_mrr_u->SetColor(NodeColor::Red);
        node = RotateRight(aNode);
        rbp_mrr_t = RotateLeft(aNode);
        node->SetRight(rbp_mrr_t);
      }
      aNode->SetColor(NodeColor::Red);
    } else {
      rbp_mrr_t->SetColor(NodeColor::Red);
      rbp_mrr_t = rbp_mrr_t->Left();
      if (rbp_mrr_t && rbp_mrr_t->IsRed()) {
        rbp_mrr_t->SetColor(NodeColor::Black);
        node = RotateRight(aNode);
        rbp_mrr_t = RotateLeft(aNode);
        node->SetRight(rbp_mrr_t);
      } else {
        node = RotateLeft(aNode);
      }
    }
    return node;
  }

  /*
   * The iterator simulates recursion via an array of pointers that store the
   * current path.  This is critical to performance, since a series of calls to
   * rb_{next,prev}() would require time proportional to (n lg n), whereas this
   * implementation only requires time proportional to (n).
   *
   * Since the iterator caches a path down the tree, any tree modification may
   * cause the cached path to become invalid. Don't modify the tree during an
   * iteration.
   */

  /*
   * Size the path arrays such that they are always large enough, even if a
   * tree consumes all of memory.  Since each node must contain a minimum of
   * two pointers, there can never be more nodes than:
   *
   *   1 << ((sizeof(void*)<<3) - (log2(sizeof(void*))+1))
   *
   * Since the depth of a tree is limited to 3*lg(#nodes), the maximum depth
   * is:
   *
   *   (3 * ((sizeof(void*)<<3) - (log2(sizeof(void*))+1)))
   *
   * This works out to a maximum depth of 87 and 180 for 32- and 64-bit
   * systems, respectively (approximately 348 and 1440 bytes, respectively).
   */
public:
  class Iterator
  {
    TreeNode* mPath[3 * ((sizeof(void*) << 3) - (LOG2(sizeof(void*)) + 1))];
    unsigned mDepth;

  public:
    explicit Iterator(RedBlackTree<T, Trait>* aTree)
      : mDepth(0)
    {
      /* Initialize the path to contain the left spine. */
      if (aTree->mRoot) {
        TreeNode* node;
        mPath[mDepth++] = aTree->mRoot;
        while ((node = mPath[mDepth - 1]->Left())) {
          mPath[mDepth++] = node;
        }
      }
    }

    template<typename Iterator>
    class Item
    {
      Iterator* mIterator;
      T* mItem;

    public:
      Item(Iterator* aIterator, T* aItem)
        : mIterator(aIterator)
        , mItem(aItem)
      { }

      bool operator!=(const Item& aOther) const
      {
        return (mIterator != aOther.mIterator) || (mItem != aOther.mItem);
      }

      T* operator*() const { return mItem; }

      const Item& operator++()
      {
        mItem = mIterator->Next();
        return *this;
      }
    };

    Item<Iterator> begin()
    {
      return Item<Iterator>(this, mDepth > 0 ? mPath[mDepth - 1] : nullptr);
    }

    Item<Iterator> end()
    {
      return Item<Iterator>(this, nullptr);
    }

    TreeNode* Next()
    {
      TreeNode* node;
      if ((node = mPath[mDepth - 1]->Right())) {
        /* The successor is the left-most node in the right subtree. */
        mPath[mDepth++] = node;
        while ((node = mPath[mDepth - 1]->Left())) {
          mPath[mDepth++] = node;
        }
      } else {
        /* The successor is above the current node.  Unwind until a
         * left-leaning edge is removed from the path, of the path is empty. */
        for (mDepth--; mDepth > 0; mDepth--) {
          if (mPath[mDepth - 1]->Left() == mPath[mDepth]) {
            break;
          }
        }
      }
      return mDepth > 0 ? mPath[mDepth - 1] : nullptr;
    }
  };

  Iterator iter() { return Iterator(this); }
};

#endif /* RB_H_ */
