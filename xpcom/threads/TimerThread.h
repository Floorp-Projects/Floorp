/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TimerThread_h___
#define TimerThread_h___

#include "nsIObserver.h"
#include "nsIRunnable.h"
#include "nsIThread.h"

#include "nsTimerImpl.h"
#include "nsThreadUtils.h"

#include "nsTArray.h"

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/Monitor.h"
#include "mozilla/ProfilerUtils.h"
#include "mozilla/UniquePtr.h"

#include <algorithm>

namespace mozilla {
class TimeStamp;
}  // namespace mozilla

class TimerThread final : public mozilla::Runnable, public nsIObserver {
 public:
  typedef mozilla::Monitor Monitor;
  typedef mozilla::MutexAutoLock MutexAutoLock;
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  TimerThread();
  nsresult InitLocks();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIOBSERVER

  nsresult Shutdown();

  nsresult AddTimer(nsTimerImpl* aTimer, const MutexAutoLock& aProofOfLock);
  nsresult RemoveTimer(nsTimerImpl* aTimer, const MutexAutoLock& aProofOfLock);
  TimeStamp FindNextFireTimeForCurrentThread(TimeStamp aDefault,
                                             uint32_t aSearchBound);

  void DoBeforeSleep();
  void DoAfterSleep();

  bool IsOnTimerThread() const {
    return mThread->SerialEventTarget()->IsOnCurrentThread();
  }

  uint32_t AllowedEarlyFiringMicroseconds() const;

 private:
  ~TimerThread();

  bool mInitialized;

  // These internal helper methods must be called while mMonitor is held.
  // AddTimerInternal returns false if the insertion failed.
  bool AddTimerInternal(nsTimerImpl* aTimer);
  bool RemoveTimerInternal(nsTimerImpl* aTimer);
  void RemoveLeadingCanceledTimersInternal();
  void RemoveFirstTimerInternal();
  nsresult Init();

  void PostTimerEvent(already_AddRefed<nsTimerImpl> aTimerRef);

  nsCOMPtr<nsIThread> mThread;
  Monitor mMonitor;

  bool mShutdown;
  bool mWaiting;
  bool mNotified;
  bool mSleeping;

  class Entry final : public nsTimerImplHolder {
    const TimeStamp mTimeout;

   public:
    Entry(const TimeStamp& aMinTimeout, const TimeStamp& aTimeout,
          nsTimerImpl* aTimerImpl)
        : nsTimerImplHolder(aTimerImpl),
          mTimeout(std::max(aMinTimeout, aTimeout)) {}

    nsTimerImpl* Value() const { return mTimerImpl; }

    already_AddRefed<nsTimerImpl> Take() {
      if (mTimerImpl) {
        mTimerImpl->SetHolder(nullptr);
      }
      return mTimerImpl.forget();
    }

    static bool UniquePtrLessThan(mozilla::UniquePtr<Entry>& aLeft,
                                  mozilla::UniquePtr<Entry>& aRight) {
      // This is reversed because std::push_heap() sorts the "largest" to
      // the front of the heap.  We want that to be the earliest timer.
      return aRight->mTimeout < aLeft->mTimeout;
    }

    TimeStamp Timeout() const { return mTimeout; }
  };

  nsTArray<mozilla::UniquePtr<Entry>> mTimers;
  uint32_t mAllowedEarlyFiringMicroseconds;
  ProfilerThreadId mProfilerThreadId;
};

#endif /* TimerThread_h___ */
