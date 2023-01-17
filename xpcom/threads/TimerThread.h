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

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIOBSERVER

  nsresult Shutdown();

  nsresult AddTimer(nsTimerImpl* aTimer, const MutexAutoLock& aProofOfLock)
      MOZ_REQUIRES(aTimer->mMutex);
  nsresult RemoveTimer(nsTimerImpl* aTimer, const MutexAutoLock& aProofOfLock)
      MOZ_REQUIRES(aTimer->mMutex);
  TimeStamp FindNextFireTimeForCurrentThread(TimeStamp aDefault,
                                             uint32_t aSearchBound);

  void DoBeforeSleep();
  void DoAfterSleep();

  bool IsOnTimerThread() const {
    return mThread->SerialEventTarget()->IsOnCurrentThread();
  }

  uint32_t AllowedEarlyFiringMicroseconds();

 private:
  ~TimerThread();

  bool mInitialized;

  // These internal helper methods must be called while mMonitor is held.
  // AddTimerInternal returns false if the insertion failed.
  bool AddTimerInternal(nsTimerImpl* aTimer) MOZ_REQUIRES(mMonitor);
  bool RemoveTimerInternal(nsTimerImpl* aTimer)
      MOZ_REQUIRES(mMonitor, aTimer->mMutex);
  void RemoveLeadingCanceledTimersInternal() MOZ_REQUIRES(mMonitor);
  void RemoveFirstTimerInternal() MOZ_REQUIRES(mMonitor);
  nsresult Init() MOZ_REQUIRES(mMonitor);

  void PostTimerEvent(already_AddRefed<nsTimerImpl> aTimerRef)
      MOZ_REQUIRES(mMonitor);

  nsCOMPtr<nsIThread> mThread;
  // Lock ordering requirements:
  // (optional) ThreadWrapper::sMutex ->
  // (optional) nsTimerImpl::mMutex   ->
  // TimerThread::mMonitor
  Monitor mMonitor;

  bool mShutdown MOZ_GUARDED_BY(mMonitor);
  bool mWaiting MOZ_GUARDED_BY(mMonitor);
  bool mNotified MOZ_GUARDED_BY(mMonitor);
  bool mSleeping MOZ_GUARDED_BY(mMonitor);

  class Entry final : public nsTimerImplHolder {
    const TimeStamp mTimeout;

   public:
    // Entries are created with the TimerImpl's mutex held.
    // nsTimerImplHolder() will call SetHolder()
    Entry(const TimeStamp& aMinTimeout, const TimeStamp& aTimeout,
          nsTimerImpl* aTimerImpl)
        : nsTimerImplHolder(aTimerImpl),
          mTimeout(std::max(aMinTimeout, aTimeout)) {}

    nsTimerImpl* Value() const { return mTimerImpl; }

    // Called with the Monitor held, but not the TimerImpl's mutex
    already_AddRefed<nsTimerImpl> Take() {
      if (mTimerImpl) {
        MOZ_ASSERT(mTimerImpl->mHolder == this);
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

  nsTArray<mozilla::UniquePtr<Entry>> mTimers MOZ_GUARDED_BY(mMonitor);
  // Set only at the start of the thread's Run():
  uint32_t mAllowedEarlyFiringMicroseconds MOZ_GUARDED_BY(mMonitor);
  ProfilerThreadId mProfilerThreadId MOZ_GUARDED_BY(mMonitor);
};

#endif /* TimerThread_h___ */
