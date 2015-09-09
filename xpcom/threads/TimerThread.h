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
  nsresult TimerDelayChanged(nsTimerImpl* aTimer);
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

  // These two internal helper methods must be called while mMonitor is held.
  // AddTimerInternal returns the position where the timer was added in the
  // list, or -1 if it failed.
  int32_t AddTimerInternal(nsTimerImpl* aTimer);
  bool    RemoveTimerInternal(nsTimerImpl* aTimer);
  void    ReleaseTimerInternal(nsTimerImpl* aTimer);

  already_AddRefed<nsTimerImpl> PostTimerEvent(already_AddRefed<nsTimerImpl> aTimerRef);

  nsCOMPtr<nsIThread> mThread;
  Monitor mMonitor;

  bool mShutdown;
  bool mWaiting;
  bool mNotified;
  bool mSleeping;

  nsTArray<nsTimerImpl*> mTimers;
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
