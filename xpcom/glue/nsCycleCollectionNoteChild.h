/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This header will be included by headers that define refpointer and array classes
// in order to specialize CC helpers such as ImplCycleCollectionTraverse for them.

#ifndef nsCycleCollectionNoteChild_h__
#define nsCycleCollectionNoteChild_h__

#include "nsCycleCollectionTraversalCallback.h"
#include "mozilla/Likely.h"

enum {
  CycleCollectionEdgeNameArrayFlag = 1
};

// Just a helper for appending "[i]". Didn't want to pull in string headers here.
void
CycleCollectionNoteEdgeNameImpl(nsCycleCollectionTraversalCallback& aCallback,
                                const char* aName,
                                uint32_t aFlags = 0);

// Should be inlined so that in the no-debug-info case this is just a simple if().
MOZ_ALWAYS_INLINE void
CycleCollectionNoteEdgeName(nsCycleCollectionTraversalCallback& aCallback,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  if (MOZ_UNLIKELY(aCallback.WantDebugInfo())) {
    CycleCollectionNoteEdgeNameImpl(aCallback, aName, aFlags);
  }
}

#endif // nsCycleCollectionNoteChild_h__
