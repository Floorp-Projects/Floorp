/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
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

#include "mozilla/Attributes.h"
#include "mozilla/Monitor.h"
#include "mozilla/TimeStamp.h"

class TimerThread MOZ_FINAL : public nsIRunnable,
                              public nsIObserver
{
public:
  typedef mozilla::Monitor Monitor;
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  TimerThread();
  NS_HIDDEN_(nsresult) InitLocks();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIOBSERVER
  
  NS_HIDDEN_(nsresult) Init();
  NS_HIDDEN_(nsresult) Shutdown();

  nsresult AddTimer(nsTimerImpl *aTimer);
  nsresult TimerDelayChanged(nsTimerImpl *aTimer);
  nsresult RemoveTimer(nsTimerImpl *aTimer);

#define FILTER_DURATION         1e3     /* one second */
#define FILTER_FEEDBACK_MAX     100     /* 1/10th of a second */

  void UpdateFilter(uint32_t aDelay, TimeStamp aTimeout,
                    TimeStamp aNow);

  void DoBeforeSleep();
  void DoAfterSleep();

  bool IsOnTimerThread() const
  {
    return mThread == NS_GetCurrentThread();
  }

private:
  ~TimerThread();

  int32_t mInitInProgress;
  bool    mInitialized;

  // These two internal helper methods must be called while mLock is held.
  // AddTimerInternal returns the position where the timer was added in the
  // list, or -1 if it failed.
  int32_t AddTimerInternal(nsTimerImpl *aTimer);
  bool    RemoveTimerInternal(nsTimerImpl *aTimer);
  void    ReleaseTimerInternal(nsTimerImpl *aTimer);

  nsCOMPtr<nsIThread> mThread;
  Monitor mMonitor;

  bool mShutdown;
  bool mWaiting;
  bool mSleeping;
  
  nsTArray<nsTimerImpl*> mTimers;

#define DELAY_LINE_LENGTH_LOG2  5
#define DELAY_LINE_LENGTH_MASK  ((1u << DELAY_LINE_LENGTH_LOG2) - 1)
#define DELAY_LINE_LENGTH       (1u << DELAY_LINE_LENGTH_LOG2)

  int32_t  mDelayLine[DELAY_LINE_LENGTH]; // milliseconds
  uint32_t mDelayLineCounter;
  uint32_t mMinTimerPeriod;     // milliseconds
  TimeDuration mTimeoutAdjustment;
};

struct TimerAdditionComparator {
  TimerAdditionComparator(const mozilla::TimeStamp &aNow,
                          const mozilla::TimeDuration &aTimeoutAdjustment,
                          nsTimerImpl *aTimerToInsert) :
    now(aNow),
    timeoutAdjustment(aTimeoutAdjustment)
#ifdef DEBUG
    , timerToInsert(aTimerToInsert)
#endif
  {}

  PRBool LessThan(nsTimerImpl *fromArray, nsTimerImpl *newTimer) const {
    NS_ABORT_IF_FALSE(newTimer == timerToInsert, "Unexpected timer ordering");

    // Skip any overdue timers.

    // XXXbz why?  Given our definition of overdue in terms of
    // mTimeoutAdjustment, aTimer might be overdue already!  Why not
    // just fire timers in order?
    return now >= fromArray->mTimeout + timeoutAdjustment ||
           fromArray->mTimeout <= newTimer->mTimeout;
  }

  PRBool Equals(nsTimerImpl* fromArray, nsTimerImpl* newTimer) const {
    return PR_FALSE;
  }

private:
  const mozilla::TimeStamp &now;
  const mozilla::TimeDuration &timeoutAdjustment;
#ifdef DEBUG
  const nsTimerImpl * const timerToInsert;
#endif
};

#endif /* TimerThread_h___ */
