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

#include "nsIEventQueue.h"

static TimerThread *gThread = nsnull;

#include "prmem.h"
#include "prinit.h"

#ifdef DEBUG_TIMERS
#include <math.h>

double nsTimerImpl::sDeltaSumSquared = 0;
double nsTimerImpl::sDeltaSum = 0;
double nsTimerImpl::sNum = 0;

static void
myNS_MeanAndStdDev(double n, double sumOfValues, double sumOfSquaredValues,
                   double *meanResult, double *stdDevResult)
{
  double mean = 0.0, var = 0.0, stdDev = 0.0;
  if (n > 0.0 && sumOfValues >= 0) {
    mean = sumOfValues / n;
    double temp = (n * sumOfSquaredValues) - (sumOfValues * sumOfValues);
    if (temp < 0.0 || n <= 1)
      var = 0.0;
    else
      var = temp / (n * (n - 1));
    // for some reason, Windows says sqrt(0.0) is "-1.#J" (?!) so do this:
    stdDev = var != 0.0 ? sqrt(var) : 0.0;
  }
  *meanResult = mean;
  *stdDevResult = stdDev;
}
#endif

NS_IMPL_THREADSAFE_ISUPPORTS1(nsTimerImpl, nsITimer)


PR_STATIC_CALLBACK(PRStatus) InitThread(void)
{
  gThread = new TimerThread();
  if (!gThread) return PR_FAILURE;

  gThread->AddRef();

  nsresult rv = gThread->Init();
  if (NS_FAILED(rv)) return PR_FAILURE;

  return PR_SUCCESS;
}

nsTimerImpl::nsTimerImpl() :
  mClosure(nsnull),
  mCallbackType(CALLBACK_TYPE_UNKNOWN),
  mFiring(PR_FALSE),
  mCancelled(PR_FALSE)
{
  NS_INIT_REFCNT();
  nsIThread::GetCurrent(getter_AddRefs(mCallingThread));

  static PRCallOnceType once;
  PR_CallOnce(&once, InitThread);

  mCallback.c = nsnull;

#ifdef DEBUG_TIMERS
  mStart = 0;
  mStart2 = 0;
#endif
}

nsTimerImpl::~nsTimerImpl()
{
  if (mCallbackType == CALLBACK_TYPE_INTERFACE)
    NS_RELEASE(mCallback.i);

  if (gThread)
    gThread->RemoveTimer(this);
}


void nsTimerImpl::Shutdown()
{
#ifdef DEBUG_TIMERS
  if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
    double mean = 0, stddev = 0;
    myNS_MeanAndStdDev(sNum, sDeltaSum, sDeltaSumSquared, &mean, &stddev);

    PR_LOG(gTimerLog, PR_LOG_DEBUG, ("sNum = %f, sDeltaSum = %f, sDeltaSumSquared = %f\n", sNum, sDeltaSum, sDeltaSumSquared));
    PR_LOG(gTimerLog, PR_LOG_DEBUG, ("mean: %fms, stddev: %fms\n", mean, stddev));
  }
#endif

  if (!gThread)
    return;

  gThread->Shutdown();
  gThread->Release();
  gThread = nsnull;
}


NS_IMETHODIMP nsTimerImpl::Init(nsTimerCallbackFunc aFunc,
                                void *aClosure,
                                PRUint32 aDelay,
                                PRUint32 aPriority,
                                PRUint32 aType)
{
  SetDelayInternal(aDelay);

  mCallback.c = aFunc;
  mCallbackType = CALLBACK_TYPE_FUNC;

  mClosure = aClosure;

  mPriority = (PRUint8)aPriority;
  mType = (PRUint8)aType;

  if (!gThread)
    return NS_ERROR_FAILURE;

  gThread->AddTimer(this);

  return NS_OK;
}

NS_IMETHODIMP nsTimerImpl::Init(nsITimerCallback *aCallback,
                                PRUint32 aDelay,
                                PRUint32 aPriority,
                                PRUint32 aType)
{
  SetDelayInternal(aDelay);

  mCallback.i = aCallback;
  NS_ADDREF(mCallback.i);
  mCallbackType = CALLBACK_TYPE_INTERFACE;

  mPriority = (PRUint8)aPriority;
  mType = (PRUint8)aType;

  if (!gThread)
    return NS_ERROR_FAILURE;

  gThread->AddTimer(this);

  return NS_OK;
}

NS_IMETHODIMP_(void) nsTimerImpl::Cancel()
{
  mCancelled = PR_TRUE;
  mClosure = nsnull;

  if (gThread)
    gThread->RemoveTimer(this);
}

NS_IMETHODIMP_(void) nsTimerImpl::SetDelay(PRUint32 aDelay)
{
  SetDelayInternal(aDelay);

  if (!mFiring && gThread)
    gThread->TimerDelayChanged(this);
}

NS_IMETHODIMP_(void) nsTimerImpl::SetPriority(PRUint32 aPriority)
{
  mPriority = (PRUint8)aPriority;
}

NS_IMETHODIMP_(void) nsTimerImpl::SetType(PRUint32 aType)
{
  mType = (PRUint8)aType;
  // XXX if this is called, we should change the actual type.. this could effect
  // repeating timers.  we need to ensure in Process() that if mType has changed
  // during the callback that we don't end up with the timer in the queue twice.
}

void nsTimerImpl::Process()
{
  if (mCancelled)
    return;

#ifdef DEBUG_TIMERS
  PRIntervalTime now;
  if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
    now = PR_IntervalNow();
    PRIntervalTime a = now - mStart; // actual delay in intervals
    PRUint32       b = PR_MillisecondsToInterval(mDelay); // expected delay in intervals
    PRUint32       d = PR_IntervalToMilliseconds((a > b) ? a - b : 0); // delta in ms
    sDeltaSum += d;
    sDeltaSumSquared += double(d) * double(d);
    sNum++;

    PR_LOG(gTimerLog, PR_LOG_DEBUG, ("[this=%p] expected delay time %dms\n", this, mDelay));
    PR_LOG(gTimerLog, PR_LOG_DEBUG, ("[this=%p] actual delay time   %dms\n", this, PR_IntervalToMilliseconds(a)));
    PR_LOG(gTimerLog, PR_LOG_DEBUG, ("[this=%p]                     -------\n", this));
    PR_LOG(gTimerLog, PR_LOG_DEBUG, ("[this=%p]     delta           %dms\n", this, d));

    mStart = mStart2;
    mStart2 = 0;
  }
#endif

  mFiring = PR_TRUE;

  if (mCallback.c) {
    if (mCallbackType == CALLBACK_TYPE_FUNC)
      (*mCallback.c)(this, mClosure);
    else if (mCallbackType == CALLBACK_TYPE_INTERFACE)
      mCallback.i->Notify(this);
    /* else the timer has been canceled, and we shouldn't do anything */
  }

  mFiring = PR_FALSE;

#ifdef DEBUG_TIMERS
  if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
    PR_LOG(gTimerLog, PR_LOG_DEBUG, ("[this=%p] Took %dms to fire process timer callback\n", this,
                                     PR_IntervalToMilliseconds(PR_IntervalNow() - now)));
  }
#endif

  if (mType == NS_TYPE_REPEATING_SLACK) {
    SetDelayInternal(mDelay); // force mTimeout to be recomputed.
    if (gThread)
      gThread->AddTimer(this);
  }
}


struct MyEventType {
  PLEvent	e;
  // arguments follow...
#ifdef DEBUG_TIMERS
  PRIntervalTime mInit;
#endif
};


void* handleMyEvent(MyEventType* event)
{
#ifdef DEBUG_TIMERS
  if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
    PR_LOG(gTimerLog, PR_LOG_DEBUG, ("[this=%p] time between Fire() and Process(): %dms\n",
                                     event->e.owner, PR_IntervalToMilliseconds(PR_IntervalNow() - event->mInit)));
  }
#endif
  NS_STATIC_CAST(nsTimerImpl*, event->e.owner)->Process();
  return NULL;
}

void destroyMyEvent(MyEventType* event)
{
  nsTimerImpl *timer = NS_STATIC_CAST(nsTimerImpl*, event->e.owner);
  NS_RELEASE(timer);
  PR_DELETE(event);
}


void nsTimerImpl::Fire()
{
  // XXX we may want to reuse the PLEvent in the case of repeating timers.
  MyEventType* event;

  // construct
  event = PR_NEW(MyEventType);
  if (event == NULL) return;

  // initialize
  PL_InitEvent((PLEvent*)event, this,
			         (PLHandleEventProc)handleMyEvent,
			         (PLDestroyEventProc)destroyMyEvent);

  // Since TimerThread addref'd 'this' for us, we don't need to addref here.  We will release
  // in destroyMyEvent.

#ifdef DEBUG_TIMERS
  if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
    event->mInit = PR_IntervalNow();
  }
#endif

  // If this is a repeating precise timer, we need to calulate the time for the next timer to fire
  // prior to making the callback.
  if (mType == NS_TYPE_REPEATING_PRECISE) {
    SetDelayInternal(mDelay);
    if (gThread)
      gThread->AddTimer(this);
  }

  PRThread *thread;
  mCallingThread->GetPRThread(&thread);

  nsCOMPtr<nsIEventQueue> queue;
  if (gThread)
    gThread->mEventQueueService->GetThreadEventQueue(thread, getter_AddRefs(queue));
  if (queue)
    queue->PostEvent(&event->e);
}

void nsTimerImpl::SetDelayInternal(PRUint32 aDelay)
{
  mDelay = aDelay;

  PRIntervalTime now = PR_IntervalNow();

  mTimeout = now + PR_MillisecondsToInterval(mDelay);

  if (mTimeout < now) { // we overflowed
    mTimeout = PRIntervalTime(-1);
  }
          
#ifdef DEBUG_TIMERS
  if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
    if (mStart == 0)
      mStart = now;
    else
      mStart2 = now;
  }
#endif
}

nsresult
NS_NewTimer(nsITimer* *aResult, nsTimerCallbackFunc aCallback, void *aClosure,
            PRUint32 aDelay, PRUint32 aPriority, PRUint32 aType)
{
    nsTimerImpl* timer = new nsTimerImpl();
    if (timer == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(timer);

    nsresult rv = timer->Init(aCallback, aClosure, aDelay, aPriority, aType);
    if (NS_FAILED(rv)) {
        NS_RELEASE(timer);
        return rv;
    }

    *aResult = timer;
    return NS_OK;
}
