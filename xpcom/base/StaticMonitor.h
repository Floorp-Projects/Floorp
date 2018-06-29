/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StaticMonitor_h
#define mozilla_StaticMonitor_h

#include "mozilla/Atomics.h"
#include "mozilla/CondVar.h"

namespace mozilla {

class MOZ_ONLY_USED_TO_AVOID_STATIC_CONSTRUCTORS StaticMonitor
{
public:
  // In debug builds, check that mMutex is initialized for us as we expect by
  // the compiler.  In non-debug builds, don't declare a constructor so that
  // the compiler can see that the constructor is trivial.
#ifdef DEBUG
  StaticMonitor()
  {
    MOZ_ASSERT(!mMutex);
  }
#endif

  void Lock()
  {
    Mutex()->Lock();
  }

  void Unlock()
  {
    Mutex()->Unlock();
  }

  void Wait() { CondVar()->Wait(); }
  CVStatus Wait(TimeDuration aDuration) { return CondVar()->Wait(aDuration); }

  nsresult Notify() { return CondVar()->Notify(); }
  nsresult NotifyAll() { return CondVar()->NotifyAll(); }

  void AssertCurrentThreadOwns()
  {
#ifdef DEBUG
    Mutex()->AssertCurrentThreadOwns();
#endif
  }

private:
  OffTheBooksMutex* Mutex()
  {
    if (mMutex) {
      return mMutex;
    }

    OffTheBooksMutex* mutex = new OffTheBooksMutex("StaticMutex");
    if (!mMutex.compareExchange(nullptr, mutex)) {
      delete mutex;
    }

    return mMutex;
  }

  OffTheBooksCondVar* CondVar()
  {
    if (mCondVar) {
      return mCondVar;
    }

    OffTheBooksCondVar* condvar = new OffTheBooksCondVar(*Mutex(), "StaticCondVar");
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
  static void* operator new(size_t) CPP_THROW_NEW;
  static void operator delete(void*);
};

class MOZ_STACK_CLASS StaticMonitorAutoLock
{
public:
  explicit StaticMonitorAutoLock(StaticMonitor& aMonitor)
    : mMonitor(&aMonitor)
  {
    mMonitor->Lock();
  }

  ~StaticMonitorAutoLock()
  {
    mMonitor->Unlock();
  }

  void Wait() { mMonitor->Wait(); }
  CVStatus Wait(TimeDuration aDuration) { return mMonitor->Wait(aDuration); }

  nsresult Notify() { return mMonitor->Notify(); }
  nsresult NotifyAll() { return mMonitor->NotifyAll(); }

private:
  StaticMonitorAutoLock();
  StaticMonitorAutoLock(const StaticMonitorAutoLock&);
  StaticMonitorAutoLock& operator=(const StaticMonitorAutoLock&);
  static void* operator new(size_t) CPP_THROW_NEW;

  StaticMonitor* mMonitor;
};

} // namespace mozilla

#endif
