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
  mProcessing(PR_FALSE),
  mWaiting(PR_FALSE),
  mDelayLineCounter(0),
  mMinTimerPeriod(0),
  mTimeoutAdjustment(0)
{
  NS_INIT_REFCNT();
}

TimerThread::~TimerThread()
{
  if (mCondVar)
    PR_DestroyCondVar(mCondVar);
  if (mLock)
    PR_DestroyLock(mLock);

  mThread = nsnull;
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

    mProcessing = PR_FALSE;

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
  mProcessing = PR_TRUE;

  while (mProcessing) {
    PRIntervalTime now = PR_IntervalNow();
    nsTimerImpl *timer = nsnull;

    if (mTimers.Count() > 0) {
      timer = NS_STATIC_CAST(nsTimerImpl*, mTimers[0]);

      if (now >= timer->mTimeout + mTimeoutAdjustment) {
  next:
        RemoveTimerInternal(timer);
        NS_ADDREF(timer);

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

        // We are going to let the call to Fire here handle the release of the
        // timer so that we don't end up releasing the timer on the TimerThread
        // instead of on the thread it targets.
        timer->Fire();
        timer = nsnull;

        lock.lock();
        if (!mProcessing)
          break;

        // Update now, as Fire plus the locking may have taken a tick or two,
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
      if (now >= timeout)
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

  /* add the timer from our list */
  PRInt32 i = AddTimerInternal(aTimer);

  if (mCondVar && mWaiting && i == 0)
    PR_NotifyCondVar(mCondVar);

  return NS_OK;
}

nsresult TimerThread::TimerDelayChanged(nsTimerImpl *aTimer)
{
  nsAutoLock lock(mLock);

  RemoveTimerInternal(aTimer);
  AddTimerInternal(aTimer);

  return NS_OK;
}

nsresult TimerThread::RemoveTimer(nsTimerImpl *aTimer)
{
  nsAutoLock lock(mLock);

  /* remove the timer from our list */
  RemoveTimerInternal(aTimer);

  return NS_OK;
}

// This function must be called from within a lock
PRInt32 TimerThread::AddTimerInternal(nsTimerImpl *aTimer)
{
  PRInt32 count = mTimers.Count();
  PRInt32 i = 0;
  for (; i < count; i++) {
    nsTimerImpl *timer = NS_STATIC_CAST(nsTimerImpl *, mTimers[i]);

    if (aTimer->mTimeout < timer->mTimeout) {
      break;
    }
  }
  mTimers.InsertElementAt(aTimer, i);

  return i;
}
