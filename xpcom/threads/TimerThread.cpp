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

#undef ACCEPT_WRONG_TIMES

#ifdef ACCEPT_WRONG_TIMES
// allow the thread to wake up and process timers +/- 3ms of when they
// are really supposed to fire.
static const PRIntervalTime kThreeMS = PR_MillisecondsToInterval(3);
#endif

TimerThread::TimerThread() :
  mCondVar(nsnull),
  mLock(nsnull),
  mProcessing(PR_FALSE),
  mWaiting(PR_FALSE)
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

/* void Run(); */
NS_IMETHODIMP TimerThread::Run()
{
  mProcessing = PR_TRUE;

  while (mProcessing) {
    nsTimerImpl *theTimer = nsnull;

    if (mTimers.Count() > 0) {
      nsAutoLock lock(mLock);
      nsTimerImpl *timer = NS_STATIC_CAST(nsTimerImpl*, mTimers[0]);

      PRIntervalTime itIsNow = PR_IntervalNow();
#ifdef ACCEPT_WRONG_TIMES
      if (itIsNow + kThreeMS > timer->mTimeout - kThreeMS)
#else
      if (itIsNow >= timer->mTimeout)
#endif
      {
        RemoveTimerInternal(timer);
        theTimer = timer;
        NS_ADDREF(theTimer);
      }
    }

    if (theTimer) {
#ifdef DEBUG_TIMERS
      if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
        PRIntervalTime now = PR_IntervalNow();
        PR_LOG(gTimerLog, PR_LOG_DEBUG, ("Timer thread woke up %dms from when it was supposed to\n",
          ((now > theTimer->mTimeout) ? PR_IntervalToMilliseconds(now - theTimer->mTimeout) :
                                        -(PRInt32)PR_IntervalToMilliseconds(theTimer->mTimeout - now))));
      }
#endif

      // We are going to let the call to Fire here handle the release of the timer so that
      // we don't end up releasing the timer on the TimerThread
      theTimer->Fire();

      theTimer = nsnull;
    }

    nsAutoLock lock(mLock);

    PRIntervalTime waitFor = PR_INTERVAL_NO_TIMEOUT;

    if (mTimers.Count() > 0) {
      PRIntervalTime now = PR_IntervalNow();
      PRIntervalTime timeout = NS_STATIC_CAST(nsTimerImpl *, mTimers[0])->mTimeout;

#ifdef ACCEPT_WRONG_TIMES
      if (timeout > now + kThreeMS)
#else
      if (timeout > now)
#endif
        waitFor = timeout - now;
      else
        waitFor = PR_INTERVAL_NO_WAIT;
    }

#ifdef DEBUG_TIMERS
    if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
      if (waitFor == PR_INTERVAL_NO_TIMEOUT)
        PR_LOG(gTimerLog, PR_LOG_DEBUG, ("waiting for PR_INTERVAL_NO_TIMEOUT\n"));
      else if (waitFor == PR_INTERVAL_NO_WAIT)
        PR_LOG(gTimerLog, PR_LOG_DEBUG, ("waiting for PR_INTERVAL_NO_WAIT\n"));
      else
        PR_LOG(gTimerLog, PR_LOG_DEBUG, ("waiting for %u\n", PR_IntervalToMilliseconds(waitFor)));
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
