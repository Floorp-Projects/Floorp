/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by the Initial Developer are
 * Copyright (C) 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 */

#include "nsTimerImpl.h"
#include "TimerThread.h"

#include "nsAutoLock.h"
#include "pratom.h"

#include "nsIServiceManager.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(TimerThread, nsIRunnable)

TimerThread::TimerThread() :
  mLock(nsnull),
  mCondVar(nsnull),
  mShutdown(PR_FALSE),
  mWaiting(PR_FALSE),
  mDelayLineCounter(0),
  mMinTimerPeriod(0),
  mTimeoutAdjustment(0)
{
}

TimerThread::~TimerThread()
{
  if (mCondVar)
    PR_DestroyCondVar(mCondVar);
  if (mLock)
    PR_DestroyLock(mLock);

  mThread = nsnull;

  PRInt32 n = mTimers.Count();
  while (--n >= 0) {
    nsTimerImpl *timer = NS_STATIC_CAST(nsTimerImpl *, mTimers[n]);
    NS_RELEASE(timer);
  }
}

nsresult TimerThread::Init()
{
  if (mThread)
    return NS_OK;

  mLock = PR_NewLock();
  if (!mLock)
    return NS_ERROR_OUT_OF_MEMORY;

  mCondVar = PR_NewCondVar(mLock);
  if (!mCondVar)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv;
  mEventQueueService = do_GetService("@mozilla.org/event-queue-service;1", &rv);
  if (NS_FAILED(rv))
    return rv;

  // We hold on to mThread to keep the thread alive.
  rv = NS_NewThread(getter_AddRefs(mThread),
                    NS_STATIC_CAST(nsIRunnable*, this),
                    0,
                    PR_JOINABLE_THREAD,
                    PR_PRIORITY_NORMAL,
                    PR_GLOBAL_THREAD);

  return rv;
}

nsresult TimerThread::Shutdown()
{
  if (!mThread)
    return NS_ERROR_NOT_INITIALIZED;

  {   // lock scope
    nsAutoLock lock(mLock);

    mShutdown = PR_TRUE;

    // notify the cond var so that Run() can return
    if (mCondVar && mWaiting)
      PR_NotifyCondVar(mCondVar);

    nsTimerImpl *timer;
    for (PRInt32 i = mTimers.Count() - 1; i >= 0; i--) {
      timer = NS_STATIC_CAST(nsTimerImpl*, mTimers[i]);
      RemoveTimerInternal(timer);
    }
  }

  mThread->Join();    // wait for the thread to die
  return NS_OK;
}

// Keep track of how early (positive slack) or late (negative slack) timers
// are running, and use the filtered slack number to adaptively estimate how
// early timers should fire to be "on time".
void TimerThread::UpdateFilter(PRUint32 aDelay, PRIntervalTime aTimeout,
                               PRIntervalTime aNow)
{
  PRInt32 slack = (PRInt32) (aTimeout - aNow);
  double smoothSlack = 0;
  PRUint32 i, filterLength;
  static PRIntervalTime kFilterFeedbackMaxTicks =
    PR_MillisecondsToInterval(FILTER_FEEDBACK_MAX);

  if (slack > 0) {
    if (slack > (PRInt32)kFilterFeedbackMaxTicks)
      slack = kFilterFeedbackMaxTicks;
  } else {
    if (slack < -(PRInt32)kFilterFeedbackMaxTicks)
      slack = -(PRInt32)kFilterFeedbackMaxTicks;
  }
  mDelayLine[mDelayLineCounter & DELAY_LINE_LENGTH_MASK] = slack;
  if (++mDelayLineCounter < DELAY_LINE_LENGTH) {
    // Startup mode: accumulate a full delay line before filtering.
    PR_ASSERT(mTimeoutAdjustment == 0);
    filterLength = 0;
  } else {
    // Past startup: compute number of filter taps based on mMinTimerPeriod.
    if (mMinTimerPeriod == 0) {
      mMinTimerPeriod = (aDelay != 0) ? aDelay : 1;
    } else if (aDelay != 0 && aDelay < mMinTimerPeriod) {
      mMinTimerPeriod = aDelay;
    }

    filterLength = (PRUint32) (FILTER_DURATION / mMinTimerPeriod);
    if (filterLength > DELAY_LINE_LENGTH)
      filterLength = DELAY_LINE_LENGTH;
    else if (filterLength < 4)
      filterLength = 4;

    for (i = 1; i <= filterLength; i++)
      smoothSlack += mDelayLine[(mDelayLineCounter-i) & DELAY_LINE_LENGTH_MASK];
    smoothSlack /= filterLength;

    // XXXbe do we need amplification?  hacking a fudge factor, need testing...
    mTimeoutAdjustment = (PRInt32) (smoothSlack * 1.5);
  }

#ifdef DEBUG_TIMERS
  if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
    PR_LOG(gTimerLog, PR_LOG_DEBUG,
           ("UpdateFilter: smoothSlack = %g, filterLength = %u\n",
            smoothSlack, filterLength));
  }
#endif
}

/* void Run(); */
NS_IMETHODIMP TimerThread::Run()
{
  nsAutoLock lock(mLock);

  while (!mShutdown) {
    PRIntervalTime now = PR_IntervalNow();
    nsTimerImpl *timer = nsnull;

    if (mTimers.Count() > 0) {
      timer = NS_STATIC_CAST(nsTimerImpl*, mTimers[0]);

      if (!TIMER_LESS_THAN(now, timer->mTimeout + mTimeoutAdjustment)) {
  next:
        // NB: AddRef before the Release under RemoveTimerInternal to avoid
        // mRefCnt passing through zero, in case all other refs than the one
        // from mTimers have gone away (the last non-mTimers[i]-ref's Release
        // must be racing with us, blocked in gThread->RemoveTimer waiting
        // for TimerThread::mLock, under nsTimerImpl::Release.

        NS_ADDREF(timer);
        RemoveTimerInternal(timer);

        // We release mLock around the Fire call, of course, to avoid deadlock.
        lock.unlock();

#ifdef DEBUG_TIMERS
        if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
          PR_LOG(gTimerLog, PR_LOG_DEBUG,
                 ("Timer thread woke up %dms from when it was supposed to\n",
                  (now >= timer->mTimeout)
                  ? PR_IntervalToMilliseconds(now - timer->mTimeout)
                  : -(PRInt32)PR_IntervalToMilliseconds(timer->mTimeout - now))
                );
        }
#endif

        // We are going to let the call to PostTimerEvent here handle the release of the
        // timer so that we don't end up releasing the timer on the TimerThread
        // instead of on the thread it targets.
        timer->PostTimerEvent();
        timer = nsnull;

        lock.lock();
        if (mShutdown)
          break;

        // Update now, as PostTimerEvent plus the locking may have taken a tick or two,
        // and we may goto next below.
        now = PR_IntervalNow();
      }
    }

    PRIntervalTime waitFor = PR_INTERVAL_NO_TIMEOUT;

    if (mTimers.Count() > 0) {
      timer = NS_STATIC_CAST(nsTimerImpl *, mTimers[0]);

      PRIntervalTime timeout = timer->mTimeout + mTimeoutAdjustment;

      // Don't wait at all (even for PR_INTERVAL_NO_WAIT) if the next timer is
      // due now or overdue.
      if (!TIMER_LESS_THAN(now, timeout))
        goto next;
      waitFor = timeout - now;
    }

#ifdef DEBUG_TIMERS
    if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
      if (waitFor == PR_INTERVAL_NO_TIMEOUT)
        PR_LOG(gTimerLog, PR_LOG_DEBUG,
               ("waiting for PR_INTERVAL_NO_TIMEOUT\n"));
      else
        PR_LOG(gTimerLog, PR_LOG_DEBUG,
               ("waiting for %u\n", PR_IntervalToMilliseconds(waitFor)));
    }
#endif

    mWaiting = PR_TRUE;
    PR_WaitCondVar(mCondVar, waitFor);
    mWaiting = PR_FALSE;
  }

  return NS_OK;
}

nsresult TimerThread::AddTimer(nsTimerImpl *aTimer)
{
  nsAutoLock lock(mLock);

  // Add the timer to our list.
  PRInt32 i = AddTimerInternal(aTimer);
  if (i < 0)
    return NS_ERROR_OUT_OF_MEMORY;

  // Awaken the timer thread.
  if (mCondVar && mWaiting && i == 0)
    PR_NotifyCondVar(mCondVar);

  return NS_OK;
}

nsresult TimerThread::TimerDelayChanged(nsTimerImpl *aTimer)
{
  nsAutoLock lock(mLock);

  // Our caller has a strong ref to aTimer, so it can't go away here under
  // ReleaseTimerInternal.
  RemoveTimerInternal(aTimer);

  PRInt32 i = AddTimerInternal(aTimer);
  if (i < 0)
    return NS_ERROR_OUT_OF_MEMORY;

  // Awaken the timer thread.
  if (mCondVar && mWaiting && i == 0)
    PR_NotifyCondVar(mCondVar);

  return NS_OK;
}

nsresult TimerThread::RemoveTimer(nsTimerImpl *aTimer)
{
  nsAutoLock lock(mLock);

  // Remove the timer from our array.  Tell callers that aTimer was not found
  // by returning NS_ERROR_NOT_AVAILABLE.  Unlike the TimerDelayChanged case
  // immediately above, our caller may be passing a (now-)weak ref in via the
  // aTimer param, specifically when nsTimerImpl::Release loses a race with
  // TimerThread::Run, must wait for the mLock auto-lock here, and during the
  // wait Run drops the only remaining ref to aTimer via RemoveTimerInternal.

  if (!RemoveTimerInternal(aTimer))
    return NS_ERROR_NOT_AVAILABLE;

  // Awaken the timer thread.
  if (mCondVar && mWaiting)
    PR_NotifyCondVar(mCondVar);

  return NS_OK;
}

// This function must be called from within a lock
PRInt32 TimerThread::AddTimerInternal(nsTimerImpl *aTimer)
{
  PRInt32 count = mTimers.Count();
  PRInt32 i = 0;
  for (; i < count; i++) {
    nsTimerImpl *timer = NS_STATIC_CAST(nsTimerImpl *, mTimers[i]);

    if (TIMER_LESS_THAN(aTimer->mTimeout, timer->mTimeout)) {
      break;
    }
  }

  if (!mTimers.InsertElementAt(aTimer, i))
    return -1;

  aTimer->mArmed = PR_TRUE;
  NS_ADDREF(aTimer);
  return i;
}

PRBool TimerThread::RemoveTimerInternal(nsTimerImpl *aTimer)
{
  if (!mTimers.RemoveElement(aTimer))
    return PR_FALSE;

  // Order is crucial here -- see nsTimerImpl::Release.
  aTimer->mArmed = PR_FALSE;
  NS_RELEASE(aTimer);
  return PR_TRUE;
}
