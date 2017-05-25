/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// An interface for read-write locks.

#ifndef mozilla_RWLock_h
#define mozilla_RWLock_h

#include "mozilla/Assertions.h"
#include "mozilla/BlockingResourceBase.h"
#include "mozilla/GuardObjects.h"

#ifndef XP_WIN
#include <pthread.h>
#endif

namespace mozilla {

// A RWLock is similar to a Mutex, but whereas a Mutex permits only a single
// reader thread or a single writer thread to access a piece of data, a
// RWLock distinguishes between readers and writers: you may have multiple
// reader threads concurrently accessing a piece of data or a single writer
// thread.  This difference should guide your usage of RWLock: if you are not
// reading the data from multiple threads simultaneously or you are writing
// to the data roughly as often as read from it, then Mutex will suit your
// purposes just fine.
//
// You should be using the AutoReadLock and AutoWriteLock classes, below,
// for RAII read locking and write locking, respectively.  If you really must
// take a read lock manually, call the ReadLock method; to relinquish that
// read lock, call the ReadUnlock method.  Similarly, WriteLock and WriteUnlock
// perform the same operations, but for write locks.
//
// It is unspecified what happens when a given thread attempts to acquire the
// same lock in multiple ways; some underlying implementations of RWLock do
// support acquiring a read lock multiple times on a given thread, but you
// should not rely on this behavior.
//
// It is unspecified whether RWLock gives priority to waiting readers or
// a waiting writer when unlocking.
class RWLock : public BlockingResourceBase
{
public:
  explicit RWLock(const char* aName);

  // Windows rwlocks don't need any special handling to be destroyed, but
  // POSIX ones do.
#ifdef XP_WIN
  ~RWLock() = default;
#else
  ~RWLock();
#endif

#ifdef DEBUG
  bool LockedForWritingByCurrentThread();
  void ReadLock();
  void ReadUnlock();
  void WriteLock();
  void WriteUnlock();
#else
  void ReadLock() { ReadLockInternal(); }
  void ReadUnlock() { ReadUnlockInternal(); }
  void WriteLock() { WriteLockInternal(); }
  void WriteUnlock() { WriteUnlockInternal(); }
#endif

private:
  void ReadLockInternal();
  void ReadUnlockInternal();
  void WriteLockInternal();
  void WriteUnlockInternal();

  RWLock() = delete;
  RWLock(const RWLock&) = delete;
  RWLock& operator=(const RWLock&) = delete;

#ifndef XP_WIN
  pthread_rwlock_t mRWLock;
#else
  // SRWLock is pointer-sized.  We declare it in such a fashion here to
  // avoid pulling in windows.h wherever this header is used.
  void* mRWLock;
#endif

#ifdef DEBUG
  // We record the owning thread for write locks only.
  PRThread* mOwningThread;
#endif
};

// Read lock and unlock a RWLock with RAII semantics.  Much preferred to bare
// calls to ReadLock() and ReadUnlock().
class MOZ_RAII AutoReadLock final
{
public:
  explicit AutoReadLock(RWLock& aLock MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mLock(&aLock)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    MOZ_ASSERT(mLock, "null lock");
    mLock->ReadLock();
  }

  ~AutoReadLock()
  {
    mLock->ReadUnlock();
  }

 private:
  AutoReadLock() = delete;
  AutoReadLock(const AutoReadLock&) = delete;
  AutoReadLock& operator=(const AutoReadLock&) = delete;

  RWLock* mLock;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

// Write lock and unlock a RWLock with RAII semantics.  Much preferred to bare
// calls to WriteLock() and WriteUnlock().
class MOZ_RAII AutoWriteLock final
{
public:
  explicit AutoWriteLock(RWLock& aLock MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mLock(&aLock)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    MOZ_ASSERT(mLock, "null lock");
    mLock->WriteLock();
  }

  ~AutoWriteLock()
  {
    mLock->WriteUnlock();
  }

 private:
  AutoWriteLock() = delete;
  AutoWriteLock(const AutoWriteLock&) = delete;
  AutoWriteLock& operator=(const AutoWriteLock&) = delete;

  RWLock* mLock;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} // namespace mozilla

#endif // mozilla_RWLock_h
