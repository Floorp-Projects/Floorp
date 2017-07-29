/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SizeOfState_h
#define SizeOfState_h

#include "mozilla/fallible.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Unused.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"

// This file includes types that are useful during memory reporting, but which
// cannot be put into mfbt/MemoryReporting.h because they depend on things that
// are not in MFBT.

namespace mozilla {

// A table of seen pointers. Useful when measuring structures that contain
// nodes that may be pointed to from multiple places, e.g. via RefPtr (in C++
// code) or Arc (in Rust code).
class SeenPtrs : public nsTHashtable<nsPtrHashKey<const void>>
{
public:
  // Returns true if we have seen this pointer before, false otherwise. Also
  // remembers this pointer for later queries.
  bool HaveSeenPtr(const void* aPtr)
  {
    uint32_t oldCount = Count();

    mozilla::Unused << PutEntry(aPtr, fallible);

    // If the counts match, there are two possibilities.
    //
    // - Lookup succeeded: we've seen the pointer before, and didn't need to
    //   add a new entry.
    //
    // - PutEntry() tried to add the entry and failed due to lack of memory. In
    //   this case we can't tell if this pointer has been seen before (because
    //   the table is in an unreliable state and may have dropped previous
    //   insertions). When doing memory reporting it's better to err on the
    //   side of under-reporting rather than over-reporting, so we assume we've
    //   seen the pointer before.
    //
    return oldCount == Count();
  }
};

// Memory reporting state. Some memory measuring functions
// (SizeOfIncludingThis(), etc.) just need a MallocSizeOf parameter, but some
// also need a record of pointers that have been seen and should not be
// re-measured. This class encapsulates both of those things.
class SizeOfState
{
public:
  explicit SizeOfState(MallocSizeOf aMallocSizeOf)
    : mMallocSizeOf(aMallocSizeOf)
  {}

  bool HaveSeenPtr(const void* aPtr) { return mSeenPtrs.HaveSeenPtr(aPtr); }

  MallocSizeOf mMallocSizeOf;
  SeenPtrs mSeenPtrs;
};

} // namespace mozilla

#endif // SizeOfState_h

