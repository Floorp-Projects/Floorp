/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Lock.h"

#include "ChunkAllocator.h"
#include "InfallibleVector.h"
#include "SpinLock.h"
#include "Thread.h"

#include <unordered_map>

namespace mozilla {
namespace recordreplay {

// The total number of locks that have been created. Each Lock is given a
// non-zero id based on this counter.
static Atomic<size_t, SequentiallyConsistent, Behavior::DontPreserve> gNumLocks;

struct LockAcquires {
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

static Lock* CreateNewLock(Thread* aThread, size_t aId) {
  LockAcquires* info = gLockAcquires.Create(aId);
  info->mAcquires = gRecordingFile->OpenStream(StreamName::Lock, aId);

  if (IsReplaying()) {
    info->ReadAndNotifyNextOwner(aThread);
  }

  return new Lock(aId);
}

/* static */
void Lock::New(void* aNativeLock) {
  Thread* thread = Thread::Current();
  RecordingEventSection res(thread);
  if (!res.CanAccessEvents()) {
    Destroy(aNativeLock);  // Clean up any old lock, as below.
    return;
  }

  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::CreateLock);

  size_t id;
  if (IsRecording()) {
    id = gNumLocks++;
  }
  thread->Events().RecordOrReplayScalar(&id);

  Lock* lock = CreateNewLock(thread, id);

  // Tolerate new locks being created with identical pointers, even if there
  // was no explicit Destroy() call for the old one.
  Destroy(aNativeLock);

  AutoWriteSpinLock ex(gLocksLock);
  thread->BeginDisallowEvents();

  if (!gLocks) {
    gLocks = new LockMap();
  }

  gLocks->insert(LockMap::value_type(aNativeLock, lock));

  thread->EndDisallowEvents();
}

/* static */
void Lock::Destroy(void* aNativeLock) {
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

/* static */
Lock* Lock::Find(void* aNativeLock) {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());

  Maybe<AutoReadSpinLock> ex;
  ex.emplace(gLocksLock);

  if (gLocks) {
    LockMap::iterator iter = gLocks->find(aNativeLock);
    if (iter != gLocks->end()) {
      // Now that we know the lock is recorded, check whether thread events
      // should be generated right now. Doing things in this order avoids
      // reentrancy issues when initializing the thread-local state used by
      // these calls.
      Lock* lock = iter->second;
      if (AreThreadEventsPassedThrough()) {
        return nullptr;
      }
      if (HasDivergedFromRecording()) {
        // When diverged from the recording, don't allow uses of locks that are
        // held by idling threads that have not diverged from the recording.
        // This will cause the process to deadlock, so rewind instead.
        if (lock->mOwner && Thread::GetById(lock->mOwner)->ShouldIdle() &&
            Thread::CurrentIsMainThread()) {
          ex.reset();
          EnsureNotDivergedFromRecording();
          Unreachable();
        }
        return nullptr;
      }
      return lock;
    }
  }

  return nullptr;
}

void Lock::Enter() {
  Thread* thread = Thread::Current();

  RecordingEventSection res(thread);
  if (!res.CanAccessEvents()) {
    return;
  }

  // Include an event in each thread's record when a lock acquire begins. This
  // is not required by the replay but is used to check that lock acquire order
  // is consistent with the recording and that we will fail explicitly instead
  // of deadlocking.
  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::Lock);
  thread->Events().CheckInput(mId);

  LockAcquires* acquires = gLockAcquires.Get(mId);
  if (IsRecording()) {
    acquires->mAcquires->WriteScalar(thread->Id());
  } else {
    // Wait until this thread is next in line to acquire the lock, or until it
    // has been instructed to diverge from the recording.
    while (thread->Id() != acquires->mNextOwner &&
           !thread->MaybeDivergeFromRecording()) {
      Thread::Wait();
    }
    if (!thread->HasDivergedFromRecording()) {
      mOwner = thread->Id();
    }
  }
}

void Lock::Exit() {
  Thread* thread = Thread::Current();
  if (IsReplaying() && !thread->HasDivergedFromRecording()) {
    mOwner = 0;

    // Notify the next owner before releasing the lock.
    LockAcquires* acquires = gLockAcquires.Get(mId);
    acquires->ReadAndNotifyNextOwner(thread);
  }
}

/* static */
void Lock::LockAquiresUpdated(size_t aLockId) {
  LockAcquires* acquires = gLockAcquires.MaybeGet(aLockId);
  if (acquires && acquires->mAcquires &&
      acquires->mNextOwner == LockAcquires::NoNextOwner) {
    acquires->ReadAndNotifyNextOwner(Thread::Current());
  }
}

// We use a set of Locks to record and replay the order in which atomic
// accesses occur. Each lock describes the acquire order for a disjoint set of
// values; this is done to reduce contention between threads, and ensures that
// when the same value pointer is used in two ordered atomic accesses, those
// accesses will replay in the same order as they did while recording.
// Instead of using platform mutexes, we manage the Locks directly to avoid
// overhead in Lock::Find. Atomics accesses are a major source of recording
// overhead, which we want to minimize.
static const size_t NumAtomicLocks = 89;
static Lock** gAtomicLocks;

// While recording, these locks prevent multiple threads from simultaneously
// owning the same atomic lock.
static SpinLock* gAtomicLockOwners;

/* static */
void Lock::InitializeLocks() {
  Thread* thread = Thread::Current();

  gNumLocks = 1;
  gAtomicLocks = new Lock*[NumAtomicLocks];
  for (size_t i = 0; i < NumAtomicLocks; i++) {
    gAtomicLocks[i] = CreateNewLock(thread, gNumLocks++);
  }
  if (IsRecording()) {
    gAtomicLockOwners = new SpinLock[NumAtomicLocks];
    PodZero(gAtomicLockOwners, NumAtomicLocks);
  }
}

extern "C" {

MOZ_EXPORT void RecordReplayInterface_InternalBeginOrderedAtomicAccess(
    const void* aValue) {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());

  Thread* thread = Thread::Current();

  // Determine which atomic lock to use for this access.
  size_t atomicId;
  {
    RecordingEventSection res(thread);
    if (!res.CanAccessEvents()) {
      return;
    }

    thread->Events().RecordOrReplayThreadEvent(ThreadEvent::AtomicAccess);

    atomicId = IsRecording() ? (HashGeneric(aValue) % NumAtomicLocks) : 0;
    thread->Events().RecordOrReplayScalar(&atomicId);
    MOZ_RELEASE_ASSERT(atomicId < NumAtomicLocks);
  }

  // When recording, hold a spin lock so that no other thread can access this
  // same atomic until this access ends. When replaying, we don't need to hold
  // any actual lock, as the atomic access cannot race and the Lock structure
  // ensures that accesses happen in the same order.
  if (IsRecording()) {
    gAtomicLockOwners[atomicId].Lock();
  }

  gAtomicLocks[atomicId]->Enter();

  MOZ_RELEASE_ASSERT(thread->AtomicLockId().isNothing());
  thread->AtomicLockId().emplace(atomicId);
}

MOZ_EXPORT void RecordReplayInterface_InternalEndOrderedAtomicAccess() {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());

  Thread* thread = Thread::Current();
  if (!thread || thread->PassThroughEvents() ||
      thread->HasDivergedFromRecording()) {
    return;
  }

  MOZ_RELEASE_ASSERT(thread->AtomicLockId().isSome());
  size_t atomicId = thread->AtomicLockId().ref();
  thread->AtomicLockId().reset();

  if (IsRecording()) {
    gAtomicLockOwners[atomicId].Unlock();
  }

  gAtomicLocks[atomicId]->Exit();
}

}  // extern "C"

}  // namespace recordreplay
}  // namespace mozilla
