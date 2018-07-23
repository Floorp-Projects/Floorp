/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_Lock_h
#define mozilla_recordreplay_Lock_h

#include "mozilla/PodOperations.h"
#include "mozilla/Types.h"

#include "File.h"

namespace mozilla {
namespace recordreplay {

// Recorded Locks Overview.
//
// Each platform has some types used for native locks (e.g. pthread_mutex_t or
// CRITICAL_SECTION). System APIs which operate on these native locks are
// redirected so that lock behavior can be tracked. If a native lock is
// created when thread events are not passed through then that native lock is
// recorded, and lock acquire orders will be replayed in the same order with
// which they originally occurred.

// Information about a recorded lock.
class Lock
{
  // Unique ID for this lock.
  size_t mId;

public:
  explicit Lock(size_t aId)
    : mId(aId)
  {
    MOZ_ASSERT(aId);
  }

  size_t Id() { return mId; }

  // When recording, this is called after the lock has been acquired, and
  // records the acquire in the lock's acquire order stream. When replaying,
  // this is called before the lock has been acquired, and blocks the thread
  // until it is next in line to acquire the lock before acquiring it via
  // aCallback.
  void Enter(const std::function<void()>& aCallback);

  // Create a new Lock corresponding to a native lock, with a fresh ID.
  static void New(void* aNativeLock);

  // Destroy any Lock associated with a native lock.
  static void Destroy(void* aNativeLock);

  // Get the recorded Lock for a native lock if there is one, otherwise null.
  static Lock* Find(void* aNativeLock);

  // Initialize locking state.
  static void InitializeLocks();

  // Note that new data has been read into a lock's acquires stream.
  static void LockAquiresUpdated(size_t aLockId);
};

} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_Lock_h
