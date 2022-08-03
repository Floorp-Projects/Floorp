/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StaticMonitor_h
#define mozilla_StaticMonitor_h

#include "mozilla/Atomics.h"
#include "mozilla/CondVar.h"
#include "mozilla/ThreadSafety.h"

namespace mozilla {

class MOZ_ONLY_USED_TO_AVOID_STATIC_CONSTRUCTORS MOZ_CAPABILITY StaticMonitor {
 public:
  // In debug builds, check that mMutex is initialized for us as we expect by
  // the compiler.  In non-debug builds, don't declare a constructor so that
  // the compiler can see that the constructor is trivial.
#ifdef DEBUG
  StaticMonitor() { MOZ_ASSERT(!mMutex); }
#endif

  void Lock() MOZ_CAPABILITY_ACQUIRE() { Mutex()->Lock(); }

  void Unlock() MOZ_CAPABILITY_RELEASE() { Mutex()->Unlock(); }

  void Wait() { CondVar()->Wait(); }
  CVStatus Wait(TimeDuration aDuration) {
    AssertCurrentThreadOwns();
    return CondVar()->Wait(aDuration);
  }

  void Notify() { CondVar()->Notify(); }
  void NotifyAll() { CondVar()->NotifyAll(); }

  void AssertCurrentThreadOwns() MOZ_ASSERT_CAPABILITY(this) {
#ifdef DEBUG
    Mutex()->AssertCurrentThreadOwns();
#endif
  }

 private:
  OffTheBooksMutex* Mutex() {
    if (mMutex) {
      return mMutex;
    }

    OffTheBooksMutex* mutex = new OffTheBooksMutex("StaticMutex");
    if (!mMutex.compareExchange(nullptr, mutex)) {
      delete mutex;
    }

    return mMutex;
  }

  OffTheBooksCondVar* CondVar() {
    if (mCondVar) {
      return mCondVar;
    }

    OffTheBooksCondVar* condvar =
        new OffTheBooksCondVar(*Mutex(), "StaticCondVar");
    if (!mCondVar.compareExchange(nullptr, condvar)) {
      delete condvar;
    }

    return mCondVar;
  }

  Atomic<OffTheBooksMutex*> mMutex;
  Atomic<OffTheBooksCondVar*> mCondVar;

  // Disallow copy constructor, but only in debug mode.  We only define
  // a default constructor in debug mode (see above); if we declared
  // this constructor always, the compiler wouldn't generate a trivial
  // default constructor for us in non-debug mode.
#ifdef DEBUG
  StaticMonitor(const StaticMonitor& aOther);
#endif

  // Disallow these operators.
  StaticMonitor& operator=(const StaticMonitor& aRhs);
  static void* operator new(size_t) noexcept(true);
  static void operator delete(void*);
};

class MOZ_STACK_CLASS MOZ_SCOPED_CAPABILITY StaticMonitorAutoLock {
 public:
  explicit StaticMonitorAutoLock(StaticMonitor& aMonitor)
      MOZ_CAPABILITY_ACQUIRE(aMonitor)
      : mMonitor(&aMonitor) {
    mMonitor->Lock();
  }

  ~StaticMonitorAutoLock() MOZ_CAPABILITY_RELEASE() { mMonitor->Unlock(); }

  void Wait() { mMonitor->Wait(); }
  CVStatus Wait(TimeDuration aDuration) { return mMonitor->Wait(aDuration); }

  void Notify() { mMonitor->Notify(); }
  void NotifyAll() { mMonitor->NotifyAll(); }

 private:
  StaticMonitorAutoLock();
  StaticMonitorAutoLock(const StaticMonitorAutoLock&);
  StaticMonitorAutoLock& operator=(const StaticMonitorAutoLock&);
  static void* operator new(size_t) noexcept(true);

  StaticMonitor* mMonitor;
};

}  // namespace mozilla

#endif
