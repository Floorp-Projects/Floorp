/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This header will be included by headers that define refpointer and array classes
// in order to specialize CC helpers such as ImplCycleCollectionTraverse for them.

#ifndef nsCycleCollectionNoteChild_h__
#define nsCycleCollectionNoteChild_h__

#include "nsCycleCollectionTraversalCallback.h"
#include "mozilla/Likely.h"
#include "mozilla/TypeTraits.h"

enum
{
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

#define NS_CYCLE_COLLECTION_INNERCLASS                                         \
        cycleCollection

#define NS_CYCLE_COLLECTION_INNERNAME                                          \
        _cycleCollectorGlobal

#define NS_CYCLE_COLLECTION_PARTICIPANT(_class)                                \
        _class::NS_CYCLE_COLLECTION_INNERCLASS::GetParticipant()

template<typename T>
nsISupports*
ToSupports(T* aPtr, typename T::NS_CYCLE_COLLECTION_INNERCLASS* aDummy = 0)
{
  return T::NS_CYCLE_COLLECTION_INNERCLASS::Upcast(aPtr);
}

// The default implementation of this class template is empty, because it
// should never be used: see the partial specializations below.
template<typename T,
         bool IsXPCOM = mozilla::IsBaseOf<nsISupports, T>::value>
struct CycleCollectionNoteChildImpl
{
};

template<typename T>
struct CycleCollectionNoteChildImpl<T, true>
{
  static void Run(nsCycleCollectionTraversalCallback& aCallback, T* aChild)
  {
    aCallback.NoteXPCOMChild(ToSupports(aChild));
  }
};

template<typename T>
struct CycleCollectionNoteChildImpl<T, false>
{
  static void Run(nsCycleCollectionTraversalCallback& aCallback, T* aChild)
  {
    aCallback.NoteNativeChild(aChild, NS_CYCLE_COLLECTION_PARTICIPANT(T));
  }
};

// We declare CycleCollectionNoteChild in 3-argument and 4-argument variants,
// rather than using default arguments, so that forward declarations work
// regardless of header inclusion order.
template<typename T>
inline void
CycleCollectionNoteChild(nsCycleCollectionTraversalCallback& aCallback,
                         T* aChild, const char* aName, uint32_t aFlags)
{
  CycleCollectionNoteEdgeName(aCallback, aName, aFlags);
  CycleCollectionNoteChildImpl<T>::Run(aCallback, aChild);
}

template<typename T>
inline void
CycleCollectionNoteChild(nsCycleCollectionTraversalCallback& aCallback,
                         T* aChild, const char* aName)
{
  CycleCollectionNoteChild(aCallback, aChild, aName, 0);
}

#endif // nsCycleCollectionNoteChild_h__
