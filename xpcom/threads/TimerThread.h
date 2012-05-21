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

#include "nsTArray.h"

#include "mozilla/Monitor.h"
#include "mozilla/TimeStamp.h"

class TimerThread : public nsIRunnable,
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

  void UpdateFilter(PRUint32 aDelay, TimeStamp aTimeout,
                    TimeStamp aNow);

  void DoBeforeSleep();
  void DoAfterSleep();

private:
  ~TimerThread();

  PRInt32 mInitInProgress;
  bool    mInitialized;

  // These two internal helper methods must be called while mLock is held.
  // AddTimerInternal returns the position where the timer was added in the
  // list, or -1 if it failed.
  PRInt32 AddTimerInternal(nsTimerImpl *aTimer);
  bool    RemoveTimerInternal(nsTimerImpl *aTimer);
  void    ReleaseTimerInternal(nsTimerImpl *aTimer);

  nsCOMPtr<nsIThread> mThread;
  Monitor mMonitor;

  bool mShutdown;
  bool mWaiting;
  bool mSleeping;
  
  nsTArray<nsTimerImpl*> mTimers;

#define DELAY_LINE_LENGTH_LOG2  5
#define DELAY_LINE_LENGTH_MASK  PR_BITMASK(DELAY_LINE_LENGTH_LOG2)
#define DELAY_LINE_LENGTH       PR_BIT(DELAY_LINE_LENGTH_LOG2)

  PRInt32  mDelayLine[DELAY_LINE_LENGTH]; // milliseconds
  PRUint32 mDelayLineCounter;
  PRUint32 mMinTimerPeriod;     // milliseconds
  TimeDuration mTimeoutAdjustment;
};

#endif /* TimerThread_h___ */
