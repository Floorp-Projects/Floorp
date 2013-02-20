/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTimerImpl.h"
#include "TimerThread.h"
#include "nsAutoPtr.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h"
#include "sampler.h"
#include NEW_H
#include "nsFixedSizeAllocator.h"

using mozilla::TimeDuration;
using mozilla::TimeStamp;

static int32_t          gGenerator = 0;
static TimerThread*     gThread = nullptr;

#ifdef DEBUG_TIMERS

PRLogModuleInfo*
GetTimerLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("nsTimerImpl");
  return sLog;
}

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

namespace {

// TimerEventAllocator is a fixed size allocator class which is used in order
// to avoid the default allocator lock contention when firing timer events.
// It is a thread-safe wrapper around nsFixedSizeAllocator.  The thread-safety
// is required because nsTimerEvent objects are allocated on the timer thread,
// and freed on another thread.  Since this is a TimerEventAllocator specific
// lock, the lock contention issue is only limited to the allocation and
// deallocation of nsTimerEvent objects.
class TimerEventAllocator : public nsFixedSizeAllocator {
public:
    TimerEventAllocator() :
      mMonitor("TimerEventAllocator")
  {
  }

  void* Alloc(size_t aSize)
  {
    mozilla::MonitorAutoLock lock(mMonitor);
    return nsFixedSizeAllocator::Alloc(aSize);
  }
  void Free(void* aPtr, size_t aSize)
  {
    mozilla::MonitorAutoLock lock(mMonitor);
    nsFixedSizeAllocator::Free(aPtr, aSize);
  }

private:
  mozilla::Monitor mMonitor;
};

}

class nsTimerEvent : public nsRunnable {
public:
  NS_IMETHOD Run();

  nsTimerEvent(nsTimerImpl *timer, int32_t generation)
    : mTimer(timer), mGeneration(generation) {
    // timer is already addref'd for us
    MOZ_COUNT_CTOR(nsTimerEvent);

    MOZ_ASSERT(gThread->IsOnTimerThread(),
               "nsTimer must always be allocated on the timer thread");

    PR_ATOMIC_INCREMENT(&sAllocatorUsers);
  }

#ifdef DEBUG_TIMERS
  TimeStamp mInitTime;
#endif

  static void Init();
  static void Shutdown();
  static void DeleteAllocatorIfNeeded();

  static void* operator new(size_t size) CPP_THROW_NEW {
    return sAllocator->Alloc(size);
  }
  void operator delete(void* p) {
    sAllocator->Free(p, sizeof(nsTimerEvent));
    DeleteAllocatorIfNeeded();
  }

private:
  nsTimerEvent(); // Not implemented
  ~nsTimerEvent() {
#ifdef DEBUG
    if (mTimer)
      NS_WARNING("leaking reference to nsTimerImpl");
#endif
    MOZ_COUNT_DTOR(nsTimerEvent);

    MOZ_ASSERT(!sCanDeleteAllocator || sAllocatorUsers > 0,
               "This will result in us attempting to deallocate the nsTimerEvent allocator twice");
    PR_ATOMIC_DECREMENT(&sAllocatorUsers);
  }

  nsTimerImpl *mTimer;
  int32_t      mGeneration;

  static TimerEventAllocator* sAllocator;
  static int32_t sAllocatorUsers;
  static bool sCanDeleteAllocator;
};

TimerEventAllocator* nsTimerEvent::sAllocator = nullptr;
int32_t nsTimerEvent::sAllocatorUsers = 0;
bool nsTimerEvent::sCanDeleteAllocator = false;

NS_IMPL_THREADSAFE_QUERY_INTERFACE1(nsTimerImpl, nsITimer)
NS_IMPL_THREADSAFE_ADDREF(nsTimerImpl)

NS_IMETHODIMP_(nsrefcnt) nsTimerImpl::Release(void)
{
  nsrefcnt count;

  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
  count = NS_AtomicDecrementRefcnt(mRefCnt);
  NS_LOG_RELEASE(this, count, "nsTimerImpl");
  if (count == 0) {
    mRefCnt = 1; /* stabilize */

    /* enable this to find non-threadsafe destructors: */
    /* NS_ASSERT_OWNINGTHREAD(nsTimerImpl); */
    delete this;
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
    mCanceled = true;

    MOZ_ASSERT(gThread, "Armed timer exists after the thread timer stopped.");
    if (NS_SUCCEEDED(gThread->RemoveTimer(this)))
      return 0;
  }

  return count;
}

nsTimerImpl::nsTimerImpl() :
  mClosure(nullptr),
  mCallbackType(CALLBACK_TYPE_UNKNOWN),
  mFiring(false),
  mArmed(false),
  mCanceled(false),
  mGeneration(0),
  mDelay(0)
{
  // XXXbsmedberg: shouldn't this be in Init()?
  mEventTarget = static_cast<nsIEventTarget*>(NS_GetCurrentThread());

  mCallback.c = nullptr;
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

  nsTimerEvent::Init();

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
  if (PR_LOG_TEST(GetTimerLog(), PR_LOG_DEBUG)) {
    double mean = 0, stddev = 0;
    myNS_MeanAndStdDev(sDeltaNum, sDeltaSum, sDeltaSumSquared, &mean, &stddev);

    PR_LOG(GetTimerLog(), PR_LOG_DEBUG, ("sDeltaNum = %f, sDeltaSum = %f, sDeltaSumSquared = %f\n", sDeltaNum, sDeltaSum, sDeltaSumSquared));
    PR_LOG(GetTimerLog(), PR_LOG_DEBUG, ("mean: %fms, stddev: %fms\n", mean, stddev));
  }
#endif

  if (!gThread)
    return;

  gThread->Shutdown();
  NS_RELEASE(gThread);

  nsTimerEvent::Shutdown();
}


nsresult nsTimerImpl::InitCommon(uint32_t aType, uint32_t aDelay)
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
  mCanceled = false;
  mTimeout = TimeStamp();
  mGeneration = PR_ATOMIC_INCREMENT(&gGenerator);

  mType = (uint8_t)aType;
  SetDelayInternal(aDelay);

  return gThread->AddTimer(this);
}

NS_IMETHODIMP nsTimerImpl::InitWithFuncCallback(nsTimerCallbackFunc aFunc,
                                                void *aClosure,
                                                uint32_t aDelay,
                                                uint32_t aType)
{
  NS_ENSURE_ARG_POINTER(aFunc);
  
  ReleaseCallback();
  mCallbackType = CALLBACK_TYPE_FUNC;
  mCallback.c = aFunc;
  mClosure = aClosure;

  return InitCommon(aType, aDelay);
}

NS_IMETHODIMP nsTimerImpl::InitWithCallback(nsITimerCallback *aCallback,
                                            uint32_t aDelay,
                                            uint32_t aType)
{
  NS_ENSURE_ARG_POINTER(aCallback);

  ReleaseCallback();
  mCallbackType = CALLBACK_TYPE_INTERFACE;
  mCallback.i = aCallback;
  NS_ADDREF(mCallback.i);

  return InitCommon(aType, aDelay);
}

NS_IMETHODIMP nsTimerImpl::Init(nsIObserver *aObserver,
                                uint32_t aDelay,
                                uint32_t aType)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  ReleaseCallback();
  mCallbackType = CALLBACK_TYPE_OBSERVER;
  mCallback.o = aObserver;
  NS_ADDREF(mCallback.o);

  return InitCommon(aType, aDelay);
}

NS_IMETHODIMP nsTimerImpl::Cancel()
{
  mCanceled = true;

  if (gThread)
    gThread->RemoveTimer(this);

  ReleaseCallback();

  return NS_OK;
}

NS_IMETHODIMP nsTimerImpl::SetDelay(uint32_t aDelay)
{
  if (mCallbackType == CALLBACK_TYPE_UNKNOWN && mType == TYPE_ONE_SHOT) {
    // This may happen if someone tries to re-use a one-shot timer
    // by re-setting delay instead of reinitializing the timer.
    NS_ERROR("nsITimer->SetDelay() called when the "
             "one-shot timer is not set up.");
    return NS_ERROR_NOT_INITIALIZED;
  }

  // If we're already repeating precisely, update mTimeout now so that the
  // new delay takes effect in the future.
  if (!mTimeout.IsNull() && mType == TYPE_REPEATING_PRECISE)
    mTimeout = TimeStamp::Now();

  SetDelayInternal(aDelay);

  if (!mFiring && gThread)
    gThread->TimerDelayChanged(this);

  return NS_OK;
}

NS_IMETHODIMP nsTimerImpl::GetDelay(uint32_t* aDelay)
{
  *aDelay = mDelay;
  return NS_OK;
}

NS_IMETHODIMP nsTimerImpl::SetType(uint32_t aType)
{
  mType = (uint8_t)aType;
  // XXX if this is called, we should change the actual type.. this could effect
  // repeating timers.  we need to ensure in Fire() that if mType has changed
  // during the callback that we don't end up with the timer in the queue twice.
  return NS_OK;
}

NS_IMETHODIMP nsTimerImpl::GetType(uint32_t* aType)
{
  *aType = mType;
  return NS_OK;
}


NS_IMETHODIMP nsTimerImpl::GetClosure(void** aClosure)
{
  *aClosure = mClosure;
  return NS_OK;
}


NS_IMETHODIMP nsTimerImpl::GetCallback(nsITimerCallback **aCallback)
{
  if (mCallbackType == CALLBACK_TYPE_INTERFACE)
    NS_IF_ADDREF(*aCallback = mCallback.i);
  else if (mTimerCallbackWhileFiring)
    NS_ADDREF(*aCallback = mTimerCallbackWhileFiring);
  else
    *aCallback = nullptr;

  return NS_OK;
}


NS_IMETHODIMP nsTimerImpl::GetTarget(nsIEventTarget** aTarget)
{
  NS_IF_ADDREF(*aTarget = mEventTarget);
  return NS_OK;
}


NS_IMETHODIMP nsTimerImpl::SetTarget(nsIEventTarget* aTarget)
{
  NS_ENSURE_TRUE(mCallbackType == CALLBACK_TYPE_UNKNOWN,
                 NS_ERROR_ALREADY_INITIALIZED);

  if (aTarget)
    mEventTarget = aTarget;
  else
    mEventTarget = static_cast<nsIEventTarget*>(NS_GetCurrentThread());
  return NS_OK;
}


void nsTimerImpl::Fire()
{
  if (mCanceled)
    return;

  SAMPLE_LABEL("Timer", "Fire");

  TimeStamp now = TimeStamp::Now();
#ifdef DEBUG_TIMERS
  if (PR_LOG_TEST(GetTimerLog(), PR_LOG_DEBUG)) {
    TimeDuration   a = now - mStart; // actual delay in intervals
    TimeDuration   b = TimeDuration::FromMilliseconds(mDelay); // expected delay in intervals
    TimeDuration   delta = (a > b) ? a - b : b - a;
    uint32_t       d = delta.ToMilliseconds(); // delta in ms
    sDeltaSum += d;
    sDeltaSumSquared += double(d) * double(d);
    sDeltaNum++;

    PR_LOG(GetTimerLog(), PR_LOG_DEBUG, ("[this=%p] expected delay time %4ums\n", this, mDelay));
    PR_LOG(GetTimerLog(), PR_LOG_DEBUG, ("[this=%p] actual delay time   %fms\n", this, a.ToMilliseconds()));
    PR_LOG(GetTimerLog(), PR_LOG_DEBUG, ("[this=%p] (mType is %d)       -------\n", this, mType));
    PR_LOG(GetTimerLog(), PR_LOG_DEBUG, ("[this=%p]     delta           %4dms\n", this, (a > b) ? (int32_t)d : -(int32_t)d));

    mStart = mStart2;
    mStart2 = TimeStamp();
  }
#endif

  TimeStamp timeout = mTimeout;
  if (IsRepeatingPrecisely()) {
    // Precise repeating timers advance mTimeout by mDelay without fail before
    // calling Fire().
    timeout -= TimeDuration::FromMilliseconds(mDelay);
  }

  if (mCallbackType == CALLBACK_TYPE_INTERFACE)
    mTimerCallbackWhileFiring = mCallback.i;
  mFiring = true;
  
  // Handle callbacks that re-init the timer, but avoid leaking.
  // See bug 330128.
  CallbackUnion callback = mCallback;
  unsigned callbackType = mCallbackType;
  if (callbackType == CALLBACK_TYPE_INTERFACE)
    NS_ADDREF(callback.i);
  else if (callbackType == CALLBACK_TYPE_OBSERVER)
    NS_ADDREF(callback.o);
  ReleaseCallback();

  switch (callbackType) {
    case CALLBACK_TYPE_FUNC:
      callback.c(this, mClosure);
      break;
    case CALLBACK_TYPE_INTERFACE:
      callback.i->Notify(this);
      break;
    case CALLBACK_TYPE_OBSERVER:
      callback.o->Observe(static_cast<nsITimer*>(this),
                          NS_TIMER_CALLBACK_TOPIC,
                          nullptr);
      break;
    default:;
  }

  // If the callback didn't re-init the timer, and it's not a one-shot timer,
  // restore the callback state.
  if (mCallbackType == CALLBACK_TYPE_UNKNOWN &&
      mType != TYPE_ONE_SHOT && !mCanceled) {
    mCallback = callback;
    mCallbackType = callbackType;
  } else {
    // The timer was a one-shot, or the callback was reinitialized.
    if (callbackType == CALLBACK_TYPE_INTERFACE)
      NS_RELEASE(callback.i);
    else if (callbackType == CALLBACK_TYPE_OBSERVER)
      NS_RELEASE(callback.o);
  }

  mFiring = false;
  mTimerCallbackWhileFiring = nullptr;

#ifdef DEBUG_TIMERS
  if (PR_LOG_TEST(GetTimerLog(), PR_LOG_DEBUG)) {
    PR_LOG(GetTimerLog(), PR_LOG_DEBUG,
           ("[this=%p] Took %fms to fire timer callback\n",
            this, (TimeStamp::Now() - now).ToMilliseconds()));
  }
#endif

  // Reschedule repeating timers, except REPEATING_PRECISE which already did
  // that in PostTimerEvent, but make sure that we aren't armed already (which
  // can happen if the callback reinitialized the timer).
  if (IsRepeating() && mType != TYPE_REPEATING_PRECISE && !mArmed) {
    if (mType == TYPE_REPEATING_SLACK)
      SetDelayInternal(mDelay); // force mTimeout to be recomputed.  For
                                // REPEATING_PRECISE_CAN_SKIP timers this has
                                // already happened.
    if (gThread)
      gThread->AddTimer(this);
  }
}

void nsTimerEvent::Init()
{
  sAllocator = new TimerEventAllocator();
  static const size_t kBucketSizes[] = {sizeof(nsTimerEvent)};
  static const int32_t kNumBuckets = mozilla::ArrayLength(kBucketSizes);
  static const int32_t kInitialPoolSize = 4096;
  sAllocator->Init("TimerEventPool", kBucketSizes, kNumBuckets, kInitialPoolSize);
}

void nsTimerEvent::Shutdown()
{
  sCanDeleteAllocator = true;
  DeleteAllocatorIfNeeded();
}

void nsTimerEvent::DeleteAllocatorIfNeeded()
{
  if (sCanDeleteAllocator && sAllocatorUsers == 0) {
    delete sAllocator;
    sAllocator = nullptr;
  }
}

NS_IMETHODIMP nsTimerEvent::Run()
{
  nsRefPtr<nsTimerImpl> timer;
  timer.swap(mTimer);

  if (mGeneration != timer->GetGeneration())
    return NS_OK;

#ifdef DEBUG_TIMERS
  if (PR_LOG_TEST(GetTimerLog(), PR_LOG_DEBUG)) {
    TimeStamp now = TimeStamp::Now();
    PR_LOG(GetTimerLog(), PR_LOG_DEBUG,
           ("[this=%p] time between PostTimerEvent() and Fire(): %fms\n",
            this, (now - mInitTime).ToMilliseconds()));
  }
#endif

  timer->Fire();

  return NS_OK;
}

nsresult nsTimerImpl::PostTimerEvent()
{
  // XXX we may want to reuse this nsTimerEvent in the case of repeating timers.

  // Since TimerThread addref'd 'this' for us, we don't need to addref here.
  // We will release in destroyMyEvent.  We need to copy the generation number
  // from this timer into the event, so we can avoid firing a timer that was
  // re-initialized after being canceled.

  nsRefPtr<nsTimerEvent> event = new nsTimerEvent(this, mGeneration);
  if (!event)
    return NS_ERROR_OUT_OF_MEMORY;

#ifdef DEBUG_TIMERS
  if (PR_LOG_TEST(GetTimerLog(), PR_LOG_DEBUG)) {
    event->mInitTime = TimeStamp::Now();
  }
#endif

  // If this is a repeating precise timer, we need to calculate the time for
  // the next timer to fire before we make the callback.
  if (IsRepeatingPrecisely()) {
    SetDelayInternal(mDelay);

    // But only re-arm REPEATING_PRECISE timers.
    if (gThread && mType == TYPE_REPEATING_PRECISE) {
      nsresult rv = gThread->AddTimer(this);
      if (NS_FAILED(rv))
        return rv;
    }
  }

  nsresult rv = mEventTarget->Dispatch(event, NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv) && gThread)
    gThread->RemoveTimer(this);
  return rv;
}

void nsTimerImpl::SetDelayInternal(uint32_t aDelay)
{
  TimeDuration delayInterval = TimeDuration::FromMilliseconds(aDelay);

  mDelay = aDelay;

  TimeStamp now = TimeStamp::Now();
  if (mTimeout.IsNull() || mType != TYPE_REPEATING_PRECISE)
    mTimeout = now;

  mTimeout += delayInterval;

#ifdef DEBUG_TIMERS
  if (PR_LOG_TEST(GetTimerLog(), PR_LOG_DEBUG)) {
    if (mStart.IsNull())
      mStart = now;
    else
      mStart2 = now;
  }
#endif
}

// NOT FOR PUBLIC CONSUMPTION!
nsresult
NS_NewTimer(nsITimer* *aResult, nsTimerCallbackFunc aCallback, void *aClosure,
            uint32_t aDelay, uint32_t aType)
{
    nsTimerImpl* timer = new nsTimerImpl();
    if (timer == nullptr)
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
