/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCycleCollectionTraversalCallback_h__
#define nsCycleCollectionTraversalCallback_h__

#include "nsISupports.h"

class nsCycleCollectionParticipant;

class NS_NO_VTABLE nsCycleCollectionTraversalCallback
{
public:
  // You must call DescribeRefCountedNode() with an accurate
  // refcount, otherwise cycle collection will fail, and probably crash.
  // If the callback cares about objname, it should put
  // WANT_DEBUG_INFO in mFlags.
  NS_IMETHOD_(void) DescribeRefCountedNode(nsrefcnt refcount,
                                           const char* objname) = 0;
  NS_IMETHOD_(void) DescribeGCedNode(bool ismarked,
                                     const char* objname) = 0;

  NS_IMETHOD_(void) NoteXPCOMChild(nsISupports *child) = 0;
  NS_IMETHOD_(void) NoteJSChild(void *child) = 0;
  NS_IMETHOD_(void) NoteNativeChild(void *child,
                                    nsCycleCollectionParticipant *helper) = 0;

  // Give a name to the edge associated with the next call to
  // NoteXPCOMChild, NoteJSChild, or NoteNativeChild.
  // Callbacks who care about this should set WANT_DEBUG_INFO in the
  // flags.
  NS_IMETHOD_(void) NoteNextEdgeName(const char* name) = 0;

  enum {
    // Values for flags:

    // Caller should call NoteNextEdgeName and pass useful objName
    // to DescribeRefCountedNode and DescribeGCedNode.
    WANT_DEBUG_INFO = (1<<0),

    // Caller should not skip objects that we know will be
    // uncollectable.
    WANT_ALL_TRACES = (1<<1)
  };
  uint32_t Flags() const { return mFlags; }
  bool WantDebugInfo() const { return (mFlags & WANT_DEBUG_INFO) != 0; }
  bool WantAllTraces() const { return (mFlags & WANT_ALL_TRACES) != 0; }
protected:
  nsCycleCollectionTraversalCallback() : mFlags(0) {}

  uint32_t mFlags;
};

#endif // nsCycleCollectionTraversalCallback_h__
