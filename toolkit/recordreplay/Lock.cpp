/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Lock.h"

#include "mozilla/StaticMutex.h"

#include "ChunkAllocator.h"
#include "InfallibleVector.h"
#include "SpinLock.h"
#include "Thread.h"

#include <unordered_map>

namespace mozilla {
namespace recordreplay {

// The total number of locks that have been created. Reserved IDs:
// 0: Locks that are not recorded.
// 1: Used by gAtomicLock for atomic accesses.
//
// This is only used while recording, and increments gradually as locks are
// created.
static const size_t gAtomicLockId = 1;
static Atomic<size_t, SequentiallyConsistent, Behavior::DontPreserve> gNumLocks;

struct LockAcquires
{
  // List of thread acquire orders for the lock. This is protected by the lock
  // itself.
  Stream* mAcquires;

  // During replay, the next thread id to acquire the lock. Writes to this are
  // protected by the lock itself, though reads may occur on other threads.
  Atomic<size_t, SequentiallyConsistent, Behavior::DontPreserve> mNextOwner;

  static const size_t NoNextOwner = 0;

  void ReadAndNotifyNextOwner(Thread* aCurrentThread) {
    MOZ_RELEASE_ASSERT(IsReplaying());
    if (mAcquires->AtEnd()) {
      mNextOwner = NoNextOwner;
    } else {
      mNextOwner = mAcquires->ReadScalar();
      if (mNextOwner != aCurrentThread->Id()) {
        Thread::Notify(mNextOwner);
      }
    }
  }
};

// Acquires for each lock, indexed by the lock ID.
static ChunkAllocator<LockAcquires> gLockAcquires;

///////////////////////////////////////////////////////////////////////////////
// Locking Interface
///////////////////////////////////////////////////////////////////////////////

// Table mapping native lock pointers to the associated Lock structure, for
// every recorded lock in existence.
typedef std::unordered_map<void*, Lock*> LockMap;
static LockMap* gLocks;
static ReadWriteSpinLock gLocksLock;

/* static */ void
Lock::New(void* aNativeLock)
{
  if (AreThreadEventsPassedThrough() || HasDivergedFromRecording()) {
    Destroy(aNativeLock); // Clean up any old lock, as below.
    return;
  }

  MOZ_RELEASE_ASSERT(!AreThreadEventsDisallowed());
  Thread* thread = Thread::Current();

  RecordReplayAssert("CreateLock");

  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::CreateLock);

  size_t id;
  if (IsRecording()) {
    id = gNumLocks++;
  }
  thread->Events().RecordOrReplayScalar(&id);

  LockAcquires* info = gLockAcquires.Create(id);
  info->mAcquires = gRecordingFile->OpenStream(StreamName::Lock, id);

  if (IsReplaying()) {
    info->ReadAndNotifyNextOwner(thread);
  }

  // Tolerate new locks being created with identical pointers, even if there
  // was no DestroyLock call for the old one.
  Destroy(aNativeLock);

  AutoWriteSpinLock ex(gLocksLock);
  thread->BeginDisallowEvents();

  if (!gLocks) {
    gLocks = new LockMap();
  }

  gLocks->insert(LockMap::value_type(aNativeLock, new Lock(id)));

  thread->EndDisallowEvents();
}

/* static */ void
Lock::Destroy(void* aNativeLock)
{
  Lock* lock = nullptr;
  {
    AutoWriteSpinLock ex(gLocksLock);
    if (gLocks) {
      LockMap::iterator iter = gLocks->find(aNativeLock);
      if (iter != gLocks->end()) {
        lock = iter->second;
        gLocks->erase(iter);
      }
    }
  }
  delete lock;
}

/* static */ Lock*
Lock::Find(void* aNativeLock)
{
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());

  AutoReadSpinLock ex(gLocksLock);

  if (gLocks) {
    LockMap::iterator iter = gLocks->find(aNativeLock);
    if (iter != gLocks->end()) {
      // Now that we know the lock is recorded, check whether thread events
      // should be generated right now. Doing things in this order avoids
      // reentrancy issues when initializing the thread-local state used by
      // these calls.
      if (AreThreadEventsPassedThrough() || HasDivergedFromRecording()) {
        return nullptr;
      }
      return iter->second;
    }
  }

  return nullptr;
}

void
Lock::Enter(const std::function<void()>& aCallback)
{
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough() && !HasDivergedFromRecording());
  MOZ_RELEASE_ASSERT(!AreThreadEventsDisallowed());

  RecordReplayAssert("Lock %d", (int) mId);

  // Include an event in each thread's record when a lock acquire begins. This
  // is not required by the replay but is used to check that lock acquire order
  // is consistent with the recording and that we will fail explicitly instead
  // of deadlocking.
  Thread* thread = Thread::Current();
  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::Lock);
  thread->Events().CheckInput(mId);

  LockAcquires* acquires = gLockAcquires.Get(mId);
  if (IsRecording()) {
    acquires->mAcquires->WriteScalar(thread->Id());
  } else {
    // Wait until this thread is next in line to acquire the lock.
    while (thread->Id() != acquires->mNextOwner) {
      Thread::Wait();
    }
    // Acquire the lock before updating the next owner.
    aCallback();
    acquires->ReadAndNotifyNextOwner(thread);
  }
}

struct AtomicLock : public detail::MutexImpl
{
  using detail::MutexImpl::lock;
  using detail::MutexImpl::unlock;
};

// Lock which is held during code sections that run atomically. This is a
// PRLock instead of an OffTheBooksMutex because the latter performs atomic
// operations during initialization.
static AtomicLock* gAtomicLock = nullptr;

/* static */ void
Lock::InitializeLocks()
{
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());
  gNumLocks = gAtomicLockId;

  gAtomicLock = new AtomicLock();
  MOZ_RELEASE_ASSERT(!IsRecording() || gNumLocks == gAtomicLockId + 1);
}

/* static */ void
Lock::LockAquiresUpdated(size_t aLockId)
{
  LockAcquires* acquires = gLockAcquires.MaybeGet(aLockId);
  if (acquires && acquires->mAcquires && acquires->mNextOwner == LockAcquires::NoNextOwner) {
    acquires->ReadAndNotifyNextOwner(Thread::Current());
  }
}

extern "C" {

MOZ_EXPORT void
RecordReplayInterface_InternalBeginOrderedAtomicAccess()
{
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  if (!gInitializationFailureMessage) {
    gAtomicLock->lock();
  }
}

MOZ_EXPORT void
RecordReplayInterface_InternalEndOrderedAtomicAccess()
{
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  if (!gInitializationFailureMessage) {
    gAtomicLock->unlock();
  }
}

} // extern "C"

} // namespace recordreplay
} // namespace mozilla
