/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xpcom_build_IOInterposerPrivate_h
#define xpcom_build_IOInterposerPrivate_h

/* This header file contains declarations for helper classes that are
   to be used exclusively by IOInterposer and its observers. This header
   file is not to be used by anything else and MUST NOT be exported! */

#include <prcvar.h>
#include <prlock.h>

#include "mozilla/ThreadSafety.h"

namespace mozilla {
namespace IOInterposer {

/**
 * The following classes are simple wrappers for PRLock and PRCondVar.
 * IOInterposer and friends use these instead of Mozilla::Mutex et al because
 * of the fact that IOInterposer is permitted to run until the process
 * terminates; we can't use anything that plugs into leak checkers or deadlock
 * detectors because IOInterposer will outlive those and generate false
 * positives.
 */

class MOZ_CAPABILITY Monitor {
 public:
  Monitor() : mLock(PR_NewLock()), mCondVar(PR_NewCondVar(mLock)) {}

  ~Monitor() {
    PR_DestroyCondVar(mCondVar);
    mCondVar = nullptr;
    PR_DestroyLock(mLock);
    mLock = nullptr;
  }

  void Lock() MOZ_CAPABILITY_ACQUIRE() { PR_Lock(mLock); }

  void Unlock() MOZ_CAPABILITY_RELEASE() { PR_Unlock(mLock); }

  bool Wait(PRIntervalTime aTimeout = PR_INTERVAL_NO_TIMEOUT)
      MOZ_REQUIRES(this) {
    return PR_WaitCondVar(mCondVar, aTimeout) == PR_SUCCESS;
  }

  bool Notify() { return PR_NotifyCondVar(mCondVar) == PR_SUCCESS; }

 private:
  PRLock* mLock;
  PRCondVar* mCondVar;
};

class MOZ_SCOPED_CAPABILITY MonitorAutoLock {
 public:
  explicit MonitorAutoLock(Monitor& aMonitor) MOZ_CAPABILITY_ACQUIRE(aMonitor)
      : mMonitor(aMonitor) {
    mMonitor.Lock();
  }

  ~MonitorAutoLock() MOZ_CAPABILITY_RELEASE() { mMonitor.Unlock(); }

 private:
  Monitor& mMonitor;
};

class MOZ_SCOPED_CAPABILITY MonitorAutoUnlock {
 public:
  explicit MonitorAutoUnlock(Monitor& aMonitor)
      MOZ_SCOPED_UNLOCK_RELEASE(aMonitor)
      : mMonitor(aMonitor) {
    mMonitor.Unlock();
  }

  ~MonitorAutoUnlock() MOZ_SCOPED_UNLOCK_REACQUIRE() { mMonitor.Lock(); }

 private:
  Monitor& mMonitor;
};

class MOZ_CAPABILITY Mutex {
 public:
  Mutex() : mPRLock(PR_NewLock()) {}

  ~Mutex() {
    PR_DestroyLock(mPRLock);
    mPRLock = nullptr;
  }

  void Lock() MOZ_CAPABILITY_ACQUIRE() { PR_Lock(mPRLock); }

  void Unlock() MOZ_CAPABILITY_RELEASE() { PR_Unlock(mPRLock); }

 private:
  PRLock* mPRLock;
};

class MOZ_SCOPED_CAPABILITY AutoLock {
 public:
  explicit AutoLock(Mutex& aLock) MOZ_CAPABILITY_ACQUIRE(aLock) : mLock(aLock) {
    mLock.Lock();
  }

  ~AutoLock() MOZ_CAPABILITY_RELEASE() { mLock.Unlock(); }

 private:
  Mutex& mLock;
};

}  // namespace IOInterposer
}  // namespace mozilla

#endif  // xpcom_build_IOInterposerPrivate_h
