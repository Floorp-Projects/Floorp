/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsTimerImpl.h"
#include "TimerThread.h"
#include "nsAutoLock.h"

#include "nsVoidArray.h"

#include "nsIEventQueue.h"

#include "prmem.h"

static PRInt32          gGenerator = 0;
static TimerThread*     gThread = nsnull;
static PRBool           gFireOnIdle = PR_FALSE;
static nsTimerManager*  gManager = nsnull;

#ifdef DEBUG_TIMERS
#include <math.h>

double nsTimerImpl::sDeltaSumSquared = 0;
double nsTimerImpl::sDeltaSum = 0;
double nsTimerImpl::sDeltaNum = 0;

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

NS_IMPL_THREADSAFE_QUERY_INTERFACE2(nsTimerImpl, nsITimer, nsITimerInternal)
NS_IMPL_THREADSAFE_ADDREF(nsTimerImpl)

NS_IMETHODIMP_(nsrefcnt) nsTimerImpl::Release(void)
{
  nsrefcnt count;

  NS_PRECONDITION(0 != mRefCnt, "dup release");
  count = PR_AtomicDecrement((PRInt32 *)&mRefCnt);
  NS_LOG_RELEASE(this, count, "nsTimerImpl");
  if (count == 0) {
    mRefCnt = 1; /* stabilize */

    /* enable this to find non-threadsafe destructors: */
    /* NS_ASSERT_OWNINGTHREAD(nsTimerImpl); */
    NS_DELETEXPCOM(this);
    return 0;
  }

  // If only one reference remains, and mArmed is set, then the ref must be
  // from the TimerThread::mTimers array, so we Cancel this timer to remove
  // the mTimers element, and return 0 if Cancel in fact disarmed the timer.
  //
  // We use an inlined version of nsTimerImpl::Cancel here to check for the
  // NS_ERROR_NOT_AVAILABLE code returned by gThread->RemoveTimer when this
  // timer is not found in the mTimers array -- i.e., when the timer was not
  // in fact armed once we acquired TimerThread::mLock, in spite of mArmed
  // being true here.  That can happen if the armed timer is being fired by
  // TimerThread::Run as we race and test mArmed just before it is cleared by
  // the timer thread.  If the RemoveTimer call below doesn't find this timer
  // in the mTimers array, then the last ref to this timer is held manually
  // and temporarily by the TimerThread, so we should fall through to the
  // final return and return 1, not 0.
  //
  // The original version of this thread-based timer code kept weak refs from
  // TimerThread::mTimers, removing this timer's weak ref in the destructor,
  // but that leads to double-destructions in the race described above, and
  // adding mArmed doesn't help, because destructors can't be deferred, once
  // begun.  But by combining reference-counting and a specialized Release
  // method with "is this timer still in the mTimers array once we acquire
  // the TimerThread's lock" testing, we defer destruction until we're sure
  // that only one thread has its hot little hands on this timer.
  //
  // Note that both approaches preclude a timer creator, and everyone else
  // except the TimerThread who might have a strong ref, from dropping all
  // their strong refs without implicitly canceling the timer.  Timers need
  // non-mTimers-element strong refs to stay alive.

  if (count == 1 && mArmed) {
    mCanceled = PR_TRUE;

    if (NS_SUCCEEDED(gThread->RemoveTimer(this)))
      return 0;
  }

  return count;
}

nsTimerImpl::nsTimerImpl() :
  mClosure(nsnull),
  mCallbackType(CALLBACK_TYPE_UNKNOWN),
  mIdle(PR_TRUE),
  mFiring(PR_FALSE),
  mArmed(PR_FALSE),
  mCanceled(PR_FALSE),
  mGeneration(0),
  mDelay(0),
  mTimeout(0)
{
  // XXXbsmedberg: shouldn't this be in Init()?
  nsIThread::GetCurrent(getter_AddRefs(mCallingThread));

  mCallback.c = nsnull;

#ifdef DEBUG_TIMERS
  mStart = 0;
  mStart2 = 0;
#endif
}

nsTimerImpl::~nsTimerImpl()
{
  ReleaseCallback();
}

//static
nsresult
nsTimerImpl::Startup()
{
  nsresult rv;

  gThread = new TimerThread();
  if (!gThread) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(gThread);
  rv = gThread->InitLocks();

  if (NS_FAILED(rv)) {
    NS_RELEASE(gThread);
  }

  return rv;
}

void nsTimerImpl::Shutdown()
{
#ifdef DEBUG_TIMERS
  if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
    double mean = 0, stddev = 0;
    myNS_MeanAndStdDev(sDeltaNum, sDeltaSum, sDeltaSumSquared, &mean, &stddev);

    PR_LOG(gTimerLog, PR_LOG_DEBUG, ("sDeltaNum = %f, sDeltaSum = %f, sDeltaSumSquared = %f\n", sDeltaNum, sDeltaSum, sDeltaSumSquared));
    PR_LOG(gTimerLog, PR_LOG_DEBUG, ("mean: %fms, stddev: %fms\n", mean, stddev));
  }
#endif

  if (!gThread)
    return;

  gThread->Shutdown();
  NS_RELEASE(gThread);

  gFireOnIdle = PR_FALSE;
}


nsresult nsTimerImpl::InitCommon(PRUint32 aType, PRUint32 aDelay)
{
  nsresult rv;

  NS_ENSURE_TRUE(gThread, NS_ERROR_NOT_INITIALIZED);

  rv = gThread->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  /**
   * In case of re-Init, both with and without a preceding Cancel, clear the
   * mCanceled flag and assign a new mGeneration.  But first, remove any armed
   * timer from the timer thread's list.
   *
   * If we are racing with the timer thread to remove this timer and we lose,
   * the RemoveTimer call made here will fail to find this timer in the timer
   * thread's list, and will return false harmlessly.  We test mArmed here to
   * avoid the small overhead in RemoveTimer of locking the timer thread and
   * checking its list for this timer.  It's safe to test mArmed even though
   * it might be cleared on another thread in the next cycle (or even already
   * be cleared by another CPU whose store hasn't reached our CPU's cache),
   * because RemoveTimer is idempotent.
   */
  if (mArmed)
    gThread->RemoveTimer(this);
  mCanceled = PR_FALSE;
  mGeneration = PR_AtomicIncrement(&gGenerator);

  mType = (PRUint8)aType;
  SetDelayInternal(aDelay);

  return gThread->AddTimer(this);
}

NS_IMETHODIMP nsTimerImpl::InitWithFuncCallback(nsTimerCallbackFunc aFunc,
                                                void *aClosure,
                                                PRUint32 aDelay,
                                                PRUint32 aType)
{
  ReleaseCallback();
  mCallbackType = CALLBACK_TYPE_FUNC;
  mCallback.c = aFunc;
  mClosure = aClosure;

  return InitCommon(aType, aDelay);
}

NS_IMETHODIMP nsTimerImpl::InitWithCallback(nsITimerCallback *aCallback,
                                            PRUint32 aDelay,
                                            PRUint32 aType)
{
  ReleaseCallback();
  mCallbackType = CALLBACK_TYPE_INTERFACE;
  mCallback.i = aCallback;
  NS_ADDREF(mCallback.i);

  return InitCommon(aType, aDelay);
}

NS_IMETHODIMP nsTimerImpl::Init(nsIObserver *aObserver,
                                PRUint32 aDelay,
                                PRUint32 aType)
{
  ReleaseCallback();
  mCallbackType = CALLBACK_TYPE_OBSERVER;
  mCallback.o = aObserver;
  NS_ADDREF(mCallback.o);

  return InitCommon(aType, aDelay);
}

NS_IMETHODIMP nsTimerImpl::Cancel()
{
  mCanceled = PR_TRUE;

  if (gThread)
    gThread->RemoveTimer(this);

  return NS_OK;
}

NS_IMETHODIMP nsTimerImpl::SetDelay(PRUint32 aDelay)
{
  // If we're already repeating precisely, update mTimeout now so that the
  // new delay takes effect in the future.
  if (mTimeout != 0 && mType == TYPE_REPEATING_PRECISE)
    mTimeout = PR_IntervalNow();

  SetDelayInternal(aDelay);

  if (!mFiring && gThread)
    gThread->TimerDelayChanged(this);

  return NS_OK;
}

NS_IMETHODIMP nsTimerImpl::GetDelay(PRUint32* aDelay)
{
  *aDelay = mDelay;
  return NS_OK;
}

NS_IMETHODIMP nsTimerImpl::SetType(PRUint32 aType)
{
  mType = (PRUint8)aType;
  // XXX if this is called, we should change the actual type.. this could effect
  // repeating timers.  we need to ensure in Fire() that if mType has changed
  // during the callback that we don't end up with the timer in the queue twice.
  return NS_OK;
}

NS_IMETHODIMP nsTimerImpl::GetType(PRUint32* aType)
{
  *aType = mType;
  return NS_OK;
}


NS_IMETHODIMP nsTimerImpl::GetClosure(void** aClosure)
{
  *aClosure = mClosure;
  return NS_OK;
}


NS_IMETHODIMP nsTimerImpl::GetIdle(PRBool *aIdle)
{
  *aIdle = mIdle;
  return NS_OK;
}

NS_IMETHODIMP nsTimerImpl::SetIdle(PRBool aIdle)
{
  mIdle = aIdle;
  return NS_OK;
}

void nsTimerImpl::Fire()
{
  if (mCanceled)
    return;

  PRIntervalTime now = PR_IntervalNow();
#ifdef DEBUG_TIMERS
  if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
    PRIntervalTime a = now - mStart; // actual delay in intervals
    PRUint32       b = PR_MillisecondsToInterval(mDelay); // expected delay in intervals
    PRUint32       d = PR_IntervalToMilliseconds((a > b) ? a - b : b - a); // delta in ms
    sDeltaSum += d;
    sDeltaSumSquared += double(d) * double(d);
    sDeltaNum++;

    PR_LOG(gTimerLog, PR_LOG_DEBUG, ("[this=%p] expected delay time %4dms\n", this, mDelay));
    PR_LOG(gTimerLog, PR_LOG_DEBUG, ("[this=%p] actual delay time   %4dms\n", this, PR_IntervalToMilliseconds(a)));
    PR_LOG(gTimerLog, PR_LOG_DEBUG, ("[this=%p] (mType is %d)       -------\n", this, mType));
    PR_LOG(gTimerLog, PR_LOG_DEBUG, ("[this=%p]     delta           %4dms\n", this, (a > b) ? (PRInt32)d : -(PRInt32)d));

    mStart = mStart2;
    mStart2 = 0;
  }
#endif

  PRIntervalTime timeout = mTimeout;
  if (mType == TYPE_REPEATING_PRECISE) {
    // Precise repeating timers advance mTimeout by mDelay without fail before
    // calling Fire().
    timeout -= PR_MillisecondsToInterval(mDelay);
  }
  gThread->UpdateFilter(mDelay, timeout, now);

  mFiring = PR_TRUE;

  switch (mCallbackType) {
    case CALLBACK_TYPE_FUNC:
      mCallback.c(this, mClosure);
      break;
    case CALLBACK_TYPE_INTERFACE:
      mCallback.i->Notify(this);
      break;
    case CALLBACK_TYPE_OBSERVER:
      mCallback.o->Observe(NS_STATIC_CAST(nsITimer*,this),
                           NS_TIMER_CALLBACK_TOPIC,
                           nsnull);
      break;
    default:;
  }

  mFiring = PR_FALSE;

#ifdef DEBUG_TIMERS
  if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
    PR_LOG(gTimerLog, PR_LOG_DEBUG,
           ("[this=%p] Took %dms to fire timer callback\n",
            this, PR_IntervalToMilliseconds(PR_IntervalNow() - now)));
  }
#endif

  if (mType == TYPE_REPEATING_SLACK) {
    SetDelayInternal(mDelay); // force mTimeout to be recomputed.
    if (gThread)
      gThread->AddTimer(this);
  }
}


struct TimerEventType : public PLEvent {
  PRInt32        mGeneration;
#ifdef DEBUG_TIMERS
  PRIntervalTime mInitTime;
#endif
};


void* handleTimerEvent(TimerEventType* event)
{
  nsTimerImpl* timer = NS_STATIC_CAST(nsTimerImpl*, event->owner);
  if (event->mGeneration != timer->GetGeneration())
    return nsnull;

#ifdef DEBUG_TIMERS
  if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
    PRIntervalTime now = PR_IntervalNow();
    PR_LOG(gTimerLog, PR_LOG_DEBUG,
           ("[this=%p] time between PostTimerEvent() and Fire(): %dms\n",
            event->owner, PR_IntervalToMilliseconds(now - event->mInitTime)));
  }
#endif

  if (gFireOnIdle) {
    PRBool idle = PR_FALSE;
    timer->GetIdle(&idle);
    if (idle) {
      NS_ASSERTION(gManager, "Global Thread Manager is null!");
      if (gManager)
        gManager->AddIdleTimer(timer);
      return nsnull;
    }
  }

  timer->Fire();

  return nsnull;
}

void destroyTimerEvent(TimerEventType* event)
{
  nsTimerImpl *timer = NS_STATIC_CAST(nsTimerImpl*, event->owner);
  NS_RELEASE(timer);
  PR_DELETE(event);
}


void nsTimerImpl::PostTimerEvent()
{
  // XXX we may want to reuse the PLEvent in the case of repeating timers.
  TimerEventType* event;

  // construct
  event = PR_NEW(TimerEventType);
  if (!event)
    return;

  // initialize
  PL_InitEvent((PLEvent*)event, this,
               (PLHandleEventProc)handleTimerEvent,
               (PLDestroyEventProc)destroyTimerEvent);

  // Since TimerThread addref'd 'this' for us, we don't need to addref here.
  // We will release in destroyMyEvent.  We do need to copy the generation
  // number from this timer into the event, so we can avoid firing a timer
  // that was re-initialized after being canceled.
  event->mGeneration = mGeneration;

#ifdef DEBUG_TIMERS
  if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
    event->mInitTime = PR_IntervalNow();
  }
#endif

  // If this is a repeating precise timer, we need to calculate the time for
  // the next timer to fire before we make the callback.
  if (mType == TYPE_REPEATING_PRECISE) {
    SetDelayInternal(mDelay);
    if (gThread)
      gThread->AddTimer(this);
  }

  PRThread *thread;
  nsresult rv = mCallingThread->GetPRThread(&thread);
  if (NS_FAILED(rv)) {
    NS_WARNING("Dropping timer event because thread is dead");
    return;
  }

  nsCOMPtr<nsIEventQueue> queue;
  if (gThread)
    gThread->mEventQueueService->GetThreadEventQueue(thread, getter_AddRefs(queue));
  if (queue)
    queue->PostEvent(event);
}

void nsTimerImpl::SetDelayInternal(PRUint32 aDelay)
{
  PRIntervalTime delayInterval = PR_MillisecondsToInterval(aDelay);
  if (delayInterval > DELAY_INTERVAL_MAX) {
    delayInterval = DELAY_INTERVAL_MAX;
    aDelay = PR_IntervalToMilliseconds(delayInterval);
  }

  mDelay = aDelay;

  PRIntervalTime now = PR_IntervalNow();
  if (mTimeout == 0 || mType != TYPE_REPEATING_PRECISE)
    mTimeout = now;

  mTimeout += delayInterval;

#ifdef DEBUG_TIMERS
  if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
    if (mStart == 0)
      mStart = now;
    else
      mStart2 = now;
  }
#endif
}

/**
 * Timer Manager code
 */

NS_IMPL_THREADSAFE_ISUPPORTS1(nsTimerManager, nsITimerManager)

nsTimerManager::nsTimerManager()
{
  mLock = PR_NewLock();
  gManager = this;
}

nsTimerManager::~nsTimerManager()
{
  gManager = nsnull;
  PR_DestroyLock(mLock);

  nsTimerImpl *theTimer;
  PRInt32 count = mIdleTimers.Count();

  for (PRInt32 i = 0; i < count; i++) {
    theTimer = NS_STATIC_CAST(nsTimerImpl*, mIdleTimers[i]);
    NS_IF_RELEASE(theTimer);
  }
}

NS_IMETHODIMP nsTimerManager::SetUseIdleTimers(PRBool aUseIdleTimers)
{
  if (aUseIdleTimers == PR_FALSE && gFireOnIdle == PR_TRUE)
    return NS_ERROR_FAILURE;

  gFireOnIdle = aUseIdleTimers;

  return NS_OK;
}

NS_IMETHODIMP nsTimerManager::GetUseIdleTimers(PRBool *aUseIdleTimers)
{
  *aUseIdleTimers = gFireOnIdle;
  return NS_OK;
}

NS_IMETHODIMP nsTimerManager::HasIdleTimers(PRBool *aHasTimers)
{
  nsAutoLock lock (mLock);
  PRUint32 count = mIdleTimers.Count();
  *aHasTimers = (count != 0);
  return NS_OK;
}

nsresult nsTimerManager::AddIdleTimer(nsITimer* timer)
{
  if (!timer)
    return NS_ERROR_FAILURE;
  nsAutoLock lock(mLock);
  mIdleTimers.AppendElement(timer);
  NS_ADDREF(timer);
  return NS_OK;
}

NS_IMETHODIMP nsTimerManager::FireNextIdleTimer()
{
  if (!gFireOnIdle || !nsIThread::IsMainThread()) {
    return NS_OK;
  }

  nsTimerImpl *theTimer = nsnull;

  {
    nsAutoLock lock (mLock);
    PRUint32 count = mIdleTimers.Count();
    
    if (count == 0) 
      return NS_OK;
    
    theTimer = NS_STATIC_CAST(nsTimerImpl*, mIdleTimers[0]);
    mIdleTimers.RemoveElement(theTimer);
  }

  theTimer->Fire();

  NS_RELEASE(theTimer);

  return NS_OK;
}


// NOT FOR PUBLIC CONSUMPTION!
nsresult
NS_NewTimer(nsITimer* *aResult, nsTimerCallbackFunc aCallback, void *aClosure,
            PRUint32 aDelay, PRUint32 aType)
{
    nsTimerImpl* timer = new nsTimerImpl();
    if (timer == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(timer);

    nsresult rv = timer->InitWithFuncCallback(aCallback, aClosure, 
                                              aDelay, aType);
    if (NS_FAILED(rv)) {
        NS_RELEASE(timer);
        return rv;
    }

    *aResult = timer;
    return NS_OK;
}
