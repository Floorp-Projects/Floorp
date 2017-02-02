/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SyncProfile.h"

SyncProfile::SyncProfile(int aThreadId, PseudoStack* aStack)
  : ThreadInfo("SyncProfile", aThreadId, /* isMainThread = */ false, aStack,
               /* stackTop = */ nullptr)
  , mOwnerState(REFERENCED)
{
  MOZ_COUNT_CTOR(SyncProfile);
  SetProfile(new ProfileBuffer(GET_BACKTRACE_DEFAULT_ENTRY));
}

SyncProfile::~SyncProfile()
{
  MOZ_COUNT_DTOR(SyncProfile);
}

bool
SyncProfile::ShouldDestroy()
{
  mozilla::MutexAutoLock lock(GetMutex());
  if (mOwnerState == OWNED) {
    mOwnerState = OWNER_DESTROYING;
    return true;
  }
  mOwnerState = ORPHANED;
  return false;
}

void
SyncProfile::EndUnwind()
{
  if (mOwnerState != ORPHANED) {
    mOwnerState = OWNED;
  }
  // Save mOwnerState before we release the mutex
  OwnerState ownerState = mOwnerState;
  ThreadInfo::EndUnwind();
  if (ownerState == ORPHANED) {
    delete this;
  }
}

// SyncProfiles' stacks are deduplicated in the context of the containing
// profile in which the backtrace is as a marker payload.
void
SyncProfile::StreamJSON(SpliceableJSONWriter& aWriter, UniqueStacks& aUniqueStacks)
{
  ThreadInfo::StreamSamplesAndMarkers(aWriter, /* aSinceTime = */ 0, aUniqueStacks);
}
