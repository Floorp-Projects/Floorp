/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A lock that can be acquired multiple times on the same thread.

#ifndef mozilla_RecursiveMutex_h
#define mozilla_RecursiveMutex_h

#include "mozilla/ThreadSafety.h"
#include "mozilla/BlockingResourceBase.h"

#ifndef XP_WIN
#  include <pthread.h>
#endif

namespace mozilla {

class CAPABILITY RecursiveMutex : public BlockingResourceBase {
 public:
  explicit RecursiveMutex(const char* aName);
  ~RecursiveMutex();

#ifdef DEBUG
  void Lock() CAPABILITY_ACQUIRE();
  void Unlock() CAPABILITY_RELEASE();
#else
  void Lock() CAPABILITY_ACQUIRE() { LockInternal(); }
  void Unlock() CAPABILITY_RELEASE() { UnlockInternal(); }
#endif

#ifdef DEBUG
  /**
   * AssertCurrentThreadIn
   **/
  void AssertCurrentThreadIn() const ASSERT_CAPABILITY(this);
  /**
   * AssertNotCurrentThreadIn
   **/
  void AssertNotCurrentThreadIn() const EXCLUDES(this) {
    // Not currently implemented. See bug 476536 for discussion.
  }
#else
  void AssertCurrentThreadIn() const ASSERT_CAPABILITY(this) {}
  void AssertNotCurrentThreadIn() const EXCLUDES(this) {}
#endif

 private:
  RecursiveMutex() = delete;
  RecursiveMutex(const RecursiveMutex&) = delete;
  RecursiveMutex& operator=(const RecursiveMutex&) = delete;

  void LockInternal();
  void UnlockInternal();

#ifdef DEBUG
  PRThread* mOwningThread;
  size_t mEntryCount;
#endif

#if !defined(XP_WIN)
  pthread_mutex_t mMutex;
#else
  // We eschew including windows.h and using CRITICAL_SECTION here so that files
  // including us don't also pull in windows.h.  Just use a type that's big
  // enough for CRITICAL_SECTION, and we'll fix it up later.
  void* mMutex[6];
#endif
};

class MOZ_RAII SCOPED_CAPABILITY RecursiveMutexAutoLock {
 public:
  explicit RecursiveMutexAutoLock(RecursiveMutex& aRecursiveMutex)
      CAPABILITY_ACQUIRE(aRecursiveMutex)
      : mRecursiveMutex(&aRecursiveMutex) {
    NS_ASSERTION(mRecursiveMutex, "null mutex");
    mRecursiveMutex->Lock();
  }

  ~RecursiveMutexAutoLock(void) CAPABILITY_RELEASE() {
    mRecursiveMutex->Unlock();
  }

 private:
  RecursiveMutexAutoLock() = delete;
  RecursiveMutexAutoLock(const RecursiveMutexAutoLock&) = delete;
  RecursiveMutexAutoLock& operator=(const RecursiveMutexAutoLock&) = delete;
  static void* operator new(size_t) noexcept(true);

  mozilla::RecursiveMutex* mRecursiveMutex;
};

class MOZ_RAII SCOPED_CAPABILITY RecursiveMutexAutoUnlock {
 public:
  explicit RecursiveMutexAutoUnlock(RecursiveMutex& aRecursiveMutex)
      SCOPED_UNLOCK_RELEASE(aRecursiveMutex)
      : mRecursiveMutex(&aRecursiveMutex) {
    NS_ASSERTION(mRecursiveMutex, "null mutex");
    mRecursiveMutex->Unlock();
  }

  ~RecursiveMutexAutoUnlock(void) SCOPED_UNLOCK_REACQUIRE() {
    mRecursiveMutex->Lock();
  }

 private:
  RecursiveMutexAutoUnlock() = delete;
  RecursiveMutexAutoUnlock(const RecursiveMutexAutoUnlock&) = delete;
  RecursiveMutexAutoUnlock& operator=(const RecursiveMutexAutoUnlock&) = delete;
  static void* operator new(size_t) noexcept(true);

  mozilla::RecursiveMutex* mRecursiveMutex;
};

}  // namespace mozilla

#endif  // mozilla_RecursiveMutex_h
