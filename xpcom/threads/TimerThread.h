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

#include <algorithm>

namespace mozilla {
class TimeStamp;
} // namespace mozilla

class TimerThread final
  : public nsIRunnable
  , public nsIObserver
{
public:
  typedef mozilla::Monitor Monitor;
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  TimerThread();
  nsresult InitLocks();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIOBSERVER

  nsresult Init();
  nsresult Shutdown();

  nsresult AddTimer(nsTimerImpl* aTimer);
  nsresult RemoveTimer(nsTimerImpl* aTimer);

  void DoBeforeSleep();
  void DoAfterSleep();

  bool IsOnTimerThread() const
  {
    return mThread == NS_GetCurrentThread();
  }

private:
  ~TimerThread();

  mozilla::Atomic<bool> mInitInProgress;
  bool    mInitialized;

  // These internal helper methods must be called while mMonitor is held.
  // AddTimerInternal returns false if the insertion failed.
  bool    AddTimerInternal(nsTimerImpl* aTimer);
  bool    RemoveTimerInternal(nsTimerImpl* aTimer);
  void    RemoveLeadingCanceledTimersInternal();
  void    RemoveFirstTimerInternal();

  already_AddRefed<nsTimerImpl> PostTimerEvent(already_AddRefed<nsTimerImpl> aTimerRef);

  nsCOMPtr<nsIThread> mThread;
  Monitor mMonitor;

  bool mShutdown;
  bool mWaiting;
  bool mNotified;
  bool mSleeping;

  class Entry final : public nsTimerImplHolder
  {
    const TimeStamp mTimeout;

  public:
    Entry(const TimeStamp& aMinTimeout, const TimeStamp& aTimeout,
          nsTimerImpl* aTimerImpl)
      : nsTimerImplHolder(aTimerImpl)
      , mTimeout(std::max(aMinTimeout, aTimeout))
    {
    }

    nsTimerImpl*
    Value() const
    {
      return mTimerImpl;
    }

    already_AddRefed<nsTimerImpl>
    Take()
    {
      if (mTimerImpl) {
        mTimerImpl->SetHolder(nullptr);
      }
      return mTimerImpl.forget();
    }

    static bool
    UniquePtrLessThan(UniquePtr<Entry>& aLeft, UniquePtr<Entry>& aRight)
    {
      // This is reversed because std::push_heap() sorts the "largest" to
      // the front of the heap.  We want that to be the earliest timer.
      return aRight->mTimeout < aLeft->mTimeout;
    }
  };

  nsTArray<UniquePtr<Entry>> mTimers;
};

struct TimerAdditionComparator
{
  TimerAdditionComparator(const mozilla::TimeStamp& aNow,
                          nsTimerImpl* aTimerToInsert) :
    now(aNow)
#ifdef DEBUG
    , timerToInsert(aTimerToInsert)
#endif
  {
  }

  bool LessThan(nsTimerImpl* aFromArray, nsTimerImpl* aNewTimer) const
  {
    MOZ_ASSERT(aNewTimer == timerToInsert, "Unexpected timer ordering");

    // Skip any overdue timers.
    return aFromArray->mTimeout <= now ||
           aFromArray->mTimeout <= aNewTimer->mTimeout;
  }

  bool Equals(nsTimerImpl* aFromArray, nsTimerImpl* aNewTimer) const
  {
    return false;
  }

private:
  const mozilla::TimeStamp& now;
#ifdef DEBUG
  const nsTimerImpl* const timerToInsert;
#endif
};

#endif /* TimerThread_h___ */
