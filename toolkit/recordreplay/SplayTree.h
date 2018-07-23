/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_SplayTree_h
#define mozilla_recordreplay_SplayTree_h

#include "mozilla/Types.h"
#include "ProcessRecordReplay.h"

//#define ENABLE_COHERENCY_CHECKS

namespace mozilla {
namespace recordreplay {

/*
 * Class which represents a splay tree with nodes allocated from an alloc
 * policy.
 *
 * Splay trees are balanced binary search trees for which search, insert and
 * remove are all amortized O(log n).
 *
 * T indicates the type of tree elements, L has a Lookup type and a static
 * 'ssize_t compare(const L::Lookup&, const T&)' method ordering the elements.
 */
template <class T, class L, class AllocPolicy, size_t ChunkPages>
class SplayTree
{
  struct Node {
    T mItem;
    Node* mLeft;
    Node* mRight;
    Node* mParent;

    explicit Node(const T& aItem)
      : mItem(aItem), mLeft(nullptr), mRight(nullptr), mParent(nullptr)
    {}
  };

  AllocPolicy mAlloc;
  Node* mRoot;
  Node* mFreeList;

  SplayTree(const SplayTree&) = delete;
  SplayTree& operator=(const SplayTree&) = delete;

public:

  explicit SplayTree(const AllocPolicy& aAlloc = AllocPolicy())
    : mAlloc(aAlloc), mRoot(nullptr), mFreeList(nullptr)
  {}

  bool empty() const {
    return !mRoot;
  }

  void clear() {
    while (mRoot) {
      remove(mRoot);
    }
  }

  Maybe<T> maybeLookup(const typename L::Lookup& aLookup, bool aRemove = false) {
    if (!mRoot) {
      return Nothing();
    }
    Node* last = lookup(aLookup);
    splay(last);
    checkCoherency(mRoot, nullptr);
    Maybe<T> res;
    if (L::compare(aLookup, last->mItem) == 0) {
      res = Some(last->mItem);
      if (aRemove) {
        remove(last);
      }
    }
    return res;
  }

  // Lookup an item which matches aLookup, or the closest item less than it.
  Maybe<T> lookupClosestLessOrEqual(const typename L::Lookup& aLookup, bool aRemove = false) {
    if (!mRoot) {
      return Nothing();
    }
    Node* last = lookup(aLookup);
    Node* search = last;
    while (search && L::compare(aLookup, search->mItem) < 0) {
      search = search->mParent;
    }
    Maybe<T> res = search ? Some(search->mItem) : Nothing();
    if (aRemove && search) {
      remove(search);
    } else {
      splay(last);
    }
    checkCoherency(mRoot, nullptr);
    return res;
  }

  void insert(const typename L::Lookup& aLookup, const T& aValue) {
    MOZ_RELEASE_ASSERT(L::compare(aLookup, aValue) == 0);
    Node* element = allocateNode(aValue);
    if (!mRoot) {
      mRoot = element;
      return;
    }
    Node* last = lookup(aLookup);
    ssize_t cmp = L::compare(aLookup, last->mItem);

    Node** parentPointer;
    if (cmp < 0) {
      parentPointer = &last->mLeft;
    } else if (cmp > 0) {
      parentPointer = &last->mRight;
    } else {
      // The lookup matches an existing entry in the tree. Place it to the left
      // of the element just looked up.
      if (!last->mLeft) {
        parentPointer = &last->mLeft;
      } else {
        last = last->mLeft;
        while (last->mRight) {
          last = last->mRight;
        }
        parentPointer = &last->mRight;
      }
    }
    MOZ_RELEASE_ASSERT(!*parentPointer);
    *parentPointer = element;
    element->mParent = last;

    splay(element);
    checkCoherency(mRoot, nullptr);
  }

  class Iter {
    friend class SplayTree;

    SplayTree* mTree;
    Node* mNode;
    bool mRemoved;

    Iter(SplayTree* aTree, Node* aNode)
      : mTree(aTree), mNode(aNode), mRemoved(false)
    {}

  public:
    const T& ref() {
      return mNode->mItem;
    }

    bool done() {
      return !mNode;
    }

    Iter& operator++() {
      MOZ_RELEASE_ASSERT(!mRemoved);
      if (mNode->mRight) {
        mNode = mNode->mRight;
        while (mNode->mLeft) {
          mNode = mNode->mLeft;
        }
      } else {
        while (true) {
          Node* cur = mNode;
          mNode = mNode->mParent;
          if (!mNode || mNode->mLeft == cur) {
            break;
          }
        }
      }
      return *this;
    }

    void removeEntry() {
      mTree->remove(mNode);
      mRemoved = true;
    }
  };

  Iter begin() {
    Node* node = mRoot;
    while (node && node->mLeft) {
      node = node->mLeft;
    }
    return Iter(this, node);
  }

private:

  // Lookup an item matching aLookup, or the closest node to it.
  Node* lookup(const typename L::Lookup& aLookup) const {
    MOZ_RELEASE_ASSERT(mRoot);
    Node* node = mRoot;
    Node* parent;
    do {
      parent = node;
      ssize_t c = L::compare(aLookup, node->mItem);
      if (c == 0) {
        return node;
      }
      node = (c < 0) ? node->mLeft : node->mRight;
    } while (node);
    return parent;
  }

  void remove(Node* aNode) {
    splay(aNode);
    MOZ_RELEASE_ASSERT(aNode && aNode == mRoot);

    // Find another node which can be swapped in for the root: either the
    // rightmost child of the root's left, or the leftmost child of the
    // root's right.
    Node* swap;
    Node* swapChild;
    if (mRoot->mLeft) {
      swap = mRoot->mLeft;
      while (swap->mRight) {
        swap = swap->mRight;
      }
      swapChild = swap->mLeft;
    } else if (mRoot->mRight) {
      swap = mRoot->mRight;
      while (swap->mLeft) {
        swap = swap->mLeft;
      }
      swapChild = swap->mRight;
    } else {
      freeNode(mRoot);
      mRoot = nullptr;
      return;
    }

    // The selected node has at most one child, in swapChild. Detach it
    // from the subtree by replacing it with that child.
    if (swap == swap->mParent->mLeft) {
      swap->mParent->mLeft = swapChild;
    } else {
      swap->mParent->mRight = swapChild;
    }
    if (swapChild) {
      swapChild->mParent = swap->mParent;
    }

    mRoot->mItem = swap->mItem;
    freeNode(swap);

    checkCoherency(mRoot, nullptr);
  }

  size_t NodesPerChunk() const {
    return ChunkPages * PageSize / sizeof(Node);
  }

  Node* allocateNode(const T& aValue) {
    if (!mFreeList) {
      Node* nodeArray = mAlloc.template pod_malloc<Node>(NodesPerChunk());
      for (size_t i = 0; i < NodesPerChunk() - 1; i++) {
        nodeArray[i].mLeft = &nodeArray[i + 1];
      }
      mFreeList = nodeArray;
    }
    Node* node = mFreeList;
    mFreeList = node->mLeft;
    new(node) Node(aValue);
    return node;
  }

  void freeNode(Node* aNode) {
    aNode->mLeft = mFreeList;
    mFreeList = aNode;
  }

  void splay(Node* aNode) {
    // Rotate the element until it is at the root of the tree. Performing
    // the rotations in this fashion preserves the amortized balancing of
    // the tree.
    MOZ_RELEASE_ASSERT(aNode);
    while (aNode != mRoot) {
      Node* parent = aNode->mParent;
      if (parent == mRoot) {
        // Zig rotation.
        rotate(aNode);
        MOZ_RELEASE_ASSERT(aNode == mRoot);
        return;
      }
      Node* grandparent = parent->mParent;
      if ((parent->mLeft == aNode) == (grandparent->mLeft == parent)) {
        // Zig-zig rotation.
        rotate(parent);
        rotate(aNode);
      } else {
        // Zig-zag rotation.
        rotate(aNode);
        rotate(aNode);
      }
    }
  }

  void rotate(Node* aNode) {
    // Rearrange nodes so that node becomes the parent of its current
    // parent, while preserving the sortedness of the tree.
    Node* parent = aNode->mParent;
    if (parent->mLeft == aNode) {
      //     x          y
      //   y  c  ==>  a  x
      //  a b           b c
      parent->mLeft = aNode->mRight;
      if (aNode->mRight) {
        aNode->mRight->mParent = parent;
      }
      aNode->mRight = parent;
    } else {
      MOZ_RELEASE_ASSERT(parent->mRight == aNode);
      //   x             y
      //  a  y   ==>   x  c
      //    b c       a b
      parent->mRight = aNode->mLeft;
      if (aNode->mLeft) {
        aNode->mLeft->mParent = parent;
      }
      aNode->mLeft = parent;
    }
    aNode->mParent = parent->mParent;
    parent->mParent = aNode;
    if (Node* grandparent = aNode->mParent) {
      if (grandparent->mLeft == parent) {
        grandparent->mLeft = aNode;
      } else {
        grandparent->mRight = aNode;
      }
    } else {
      mRoot = aNode;
    }
  }

#ifdef ENABLE_COHERENCY_CHECKS
  Node* checkCoherency(Node* aNode, Node* aMinimum) {
    if (!aNode) {
      MOZ_RELEASE_ASSERT(!mRoot);
      return nullptr;
    }
    MOZ_RELEASE_ASSERT(aNode->mParent || aNode == mRoot);
    MOZ_RELEASE_ASSERT(!aMinimum || L::compare(L::getLookup(aMinimum->mItem), aNode->mItem) <= 0);
    if (aNode->mLeft) {
      MOZ_RELEASE_ASSERT(aNode->mLeft->mParent == aNode);
      Node* leftMaximum = checkCoherency(aNode->mLeft, aMinimum);
      MOZ_RELEASE_ASSERT(L::compare(L::getLookup(leftMaximum->mItem), aNode->mItem) <= 0);
    }
    if (aNode->mRight) {
      MOZ_RELEASE_ASSERT(aNode->mRight->mParent == aNode);
      return checkCoherency(aNode->mRight, aNode);
    }
    return aNode;
  }
#else
  inline void checkCoherency(Node* aNode, Node* aMinimum) {}
#endif
};

} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_SplayTree_h
