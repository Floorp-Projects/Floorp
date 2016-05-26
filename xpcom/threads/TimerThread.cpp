/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTimerImpl.h"
#include "TimerThread.h"

#include "nsThreadUtils.h"
#include "plarena.h"
#include "pratom.h"

#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "mozilla/Services.h"
#include "mozilla/ChaosMode.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/BinarySearch.h"

#include <math.h>

using namespace mozilla;
#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracerImpl.h"
using namespace mozilla::tasktracer;
#endif

NS_IMPL_ISUPPORTS(TimerThread, nsIRunnable, nsIObserver)

TimerThread::TimerThread() :
  mInitInProgress(false),
  mInitialized(false),
  mMonitor("TimerThread.mMonitor"),
  mShutdown(false),
  mWaiting(false),
  mNotified(false),
  mSleeping(false)
{
}

TimerThread::~TimerThread()
{
  mThread = nullptr;

  NS_ASSERTION(mTimers.IsEmpty(), "Timers remain in TimerThread::~TimerThread");
}

nsresult
TimerThread::InitLocks()
{
  return NS_OK;
}

namespace {

class TimerObserverRunnable : public Runnable
{
public:
  explicit TimerObserverRunnable(nsIObserver* aObserver)
    : mObserver(aObserver)
  {
  }

  NS_DECL_NSIRUNNABLE

private:
  nsCOMPtr<nsIObserver> mObserver;
};

NS_IMETHODIMP
TimerObserverRunnable::Run()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(mObserver, "sleep_notification", false);
    observerService->AddObserver(mObserver, "wake_notification", false);
    observerService->AddObserver(mObserver, "suspend_process_notification", false);
    observerService->AddObserver(mObserver, "resume_process_notification", false);
  }
  return NS_OK;
}

} // namespace

namespace {

// TimerEventAllocator is a thread-safe allocator used only for nsTimerEvents.
// It's needed to avoid contention over the default allocator lock when
// firing timer events (see bug 733277).  The thread-safety is required because
// nsTimerEvent objects are allocated on the timer thread, and freed on another
// thread.  Because TimerEventAllocator has its own lock, contention over that
// lock is limited to the allocation and deallocation of nsTimerEvent objects.
//
// Because this allocator is layered over PLArenaPool, it never shrinks -- even
// "freed" nsTimerEvents aren't truly freed, they're just put onto a free-list
// for later recycling.  So the amount of memory consumed will always be equal
// to the high-water mark consumption.  But nsTimerEvents are small and it's
// unusual to have more than a few hundred of them, so this shouldn't be a
// problem in practice.

class TimerEventAllocator
{
private:
  struct FreeEntry
  {
    FreeEntry* mNext;
  };

  PLArenaPool mPool;
  FreeEntry* mFirstFree;
  mozilla::Monitor mMonitor;

public:
  TimerEventAllocator()
    : mFirstFree(nullptr)
    , mMonitor("TimerEventAllocator")
  {
    PL_InitArenaPool(&mPool, "TimerEventPool", 4096, /* align = */ 0);
  }

  ~TimerEventAllocator()
  {
    PL_FinishArenaPool(&mPool);
  }

  void* Alloc(size_t aSize);
  void Free(void* aPtr);
};

} // namespace

// This is a nsICancelableRunnable because we can dispatch it to Workers and
// those can be shut down at any time, and in these cases, Cancel() is called
// instead of Run().
class nsTimerEvent : public CancelableRunnable
{
public:
  NS_IMETHOD Run() override;

  nsresult Cancel() override
  {
    // Since nsTimerImpl is not thread-safe, we should release |mTimer|
    // here in the target thread to avoid race condition. Otherwise,
    // ~nsTimerEvent() which calls nsTimerImpl::Release() could run in the
    // timer thread and result in race condition.
    mTimer = nullptr;
    return NS_OK;
  }

  nsTimerEvent()
    : mTimer()
    , mGeneration(0)
  {
    MOZ_COUNT_CTOR(nsTimerEvent);

    // Note: We override operator new for this class, and the override is
    // fallible!
    sAllocatorUsers++;
  }

  TimeStamp mInitTime;

  static void Init();
  static void Shutdown();
  static void DeleteAllocatorIfNeeded();

  static void* operator new(size_t aSize) CPP_THROW_NEW
  {
    return sAllocator->Alloc(aSize);
  }
  void operator delete(void* aPtr)
  {
    sAllocator->Free(aPtr);
    DeleteAllocatorIfNeeded();
  }

  already_AddRefed<nsTimerImpl> ForgetTimer()
  {
    return mTimer.forget();
  }

  void SetTimer(already_AddRefed<nsTimerImpl> aTimer)
  {
    mTimer = aTimer;
    mGeneration = mTimer->GetGeneration();
  }

private:
  nsTimerEvent(const nsTimerEvent&) = delete;
  nsTimerEvent& operator=(const nsTimerEvent&) = delete;
  nsTimerEvent& operator=(const nsTimerEvent&&) = delete;

  ~nsTimerEvent()
  {
    MOZ_COUNT_DTOR(nsTimerEvent);

    MOZ_ASSERT(!sCanDeleteAllocator || sAllocatorUsers > 0,
               "This will result in us attempting to deallocate the nsTimerEvent allocator twice");
    sAllocatorUsers--;
  }

  RefPtr<nsTimerImpl> mTimer;
  int32_t      mGeneration;

  static TimerEventAllocator* sAllocator;
  static Atomic<int32_t> sAllocatorUsers;
  static bool sCanDeleteAllocator;
};

TimerEventAllocator* nsTimerEvent::sAllocator = nullptr;
Atomic<int32_t> nsTimerEvent::sAllocatorUsers;
bool nsTimerEvent::sCanDeleteAllocator = false;

namespace {

void*
TimerEventAllocator::Alloc(size_t aSize)
{
  MOZ_ASSERT(aSize == sizeof(nsTimerEvent));

  mozilla::MonitorAutoLock lock(mMonitor);

  void* p;
  if (mFirstFree) {
    p = mFirstFree;
    mFirstFree = mFirstFree->mNext;
  } else {
    PL_ARENA_ALLOCATE(p, &mPool, aSize);
    if (!p) {
      return nullptr;
    }
  }

  return p;
}

void
TimerEventAllocator::Free(void* aPtr)
{
  mozilla::MonitorAutoLock lock(mMonitor);

  FreeEntry* entry = reinterpret_cast<FreeEntry*>(aPtr);

  entry->mNext = mFirstFree;
  mFirstFree = entry;
}

} // namespace

void
nsTimerEvent::Init()
{
  sAllocator = new TimerEventAllocator();
}

void
nsTimerEvent::Shutdown()
{
  sCanDeleteAllocator = true;
  DeleteAllocatorIfNeeded();
}

void
nsTimerEvent::DeleteAllocatorIfNeeded()
{
  if (sCanDeleteAllocator && sAllocatorUsers == 0) {
    delete sAllocator;
    sAllocator = nullptr;
  }
}

NS_IMETHODIMP
nsTimerEvent::Run()
{
  MOZ_ASSERT(mTimer);

  if (mGeneration != mTimer->GetGeneration()) {
    return NS_OK;
  }

  if (MOZ_LOG_TEST(GetTimerLog(), LogLevel::Debug)) {
    TimeStamp now = TimeStamp::Now();
    MOZ_LOG(GetTimerLog(), LogLevel::Debug,
           ("[this=%p] time between PostTimerEvent() and Fire(): %fms\n",
            this, (now - mInitTime).ToMilliseconds()));
  }

  mTimer->Fire();

  // We call Cancel() to correctly release mTimer.
  // Read more in the Cancel() implementation.
  return Cancel();
}

nsresult
TimerThread::Init()
{
  MOZ_LOG(GetTimerLog(), LogLevel::Debug,
         ("TimerThread::Init [%d]\n", mInitialized));

  if (mInitialized) {
    if (!mThread) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  nsTimerEvent::Init();

  if (mInitInProgress.exchange(true) == false) {
    // We hold on to mThread to keep the thread alive.
    nsresult rv = NS_NewThread(getter_AddRefs(mThread), this);
    if (NS_FAILED(rv)) {
      mThread = nullptr;
    } else {
      RefPtr<TimerObserverRunnable> r = new TimerObserverRunnable(this);
      if (NS_IsMainThread()) {
        r->Run();
      } else {
        NS_DispatchToMainThread(r);
      }
    }

    {
      MonitorAutoLock lock(mMonitor);
      mInitialized = true;
      mMonitor.NotifyAll();
    }
  } else {
    MonitorAutoLock lock(mMonitor);
    while (!mInitialized) {
      mMonitor.Wait();
    }
  }

  if (!mThread) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
TimerThread::Shutdown()
{
  MOZ_LOG(GetTimerLog(), LogLevel::Debug, ("TimerThread::Shutdown begin\n"));

  if (!mThread) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsTArray<nsTimerImpl*> timers;
  {
    // lock scope
    MonitorAutoLock lock(mMonitor);

    mShutdown = true;

    // notify the cond var so that Run() can return
    if (mWaiting) {
      mNotified = true;
      mMonitor.Notify();
    }

    // Need to copy content of mTimers array to a local array
    // because call to timers' ReleaseCallback() (and release its self)
    // must not be done under the lock. Destructor of a callback
    // might potentially call some code reentering the same lock
    // that leads to unexpected behavior or deadlock.
    // See bug 422472.
    timers.AppendElements(mTimers);
    mTimers.Clear();
  }

  uint32_t timersCount = timers.Length();
  for (uint32_t i = 0; i < timersCount; i++) {
    nsTimerImpl* timer = timers[i];
    timer->ReleaseCallback();
    ReleaseTimerInternal(timer);
  }

  mThread->Shutdown();    // wait for the thread to die

  nsTimerEvent::Shutdown();

  MOZ_LOG(GetTimerLog(), LogLevel::Debug, ("TimerThread::Shutdown end\n"));
  return NS_OK;
}

#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif

namespace {

struct MicrosecondsToInterval
{
  PRIntervalTime operator[](size_t aMs) const {
    return PR_MicrosecondsToInterval(aMs);
  }
};

struct IntervalComparator
{
  int operator()(PRIntervalTime aInterval) const {
    return (0 < aInterval) ? -1 : 1;
  }
};

} // namespace

NS_IMETHODIMP
TimerThread::Run()
{
  PR_SetCurrentThreadName("Timer");

#ifdef MOZ_NUWA_PROCESS
  if (IsNuwaProcess()) {
    NuwaMarkCurrentThread(nullptr, nullptr);
  }
#endif

  MonitorAutoLock lock(mMonitor);

  // We need to know how many microseconds give a positive PRIntervalTime. This
  // is platform-dependent and we calculate it at runtime, finding a value |v|
  // such that |PR_MicrosecondsToInterval(v) > 0| and then binary-searching in
  // the range [0, v) to find the ms-to-interval scale.
  uint32_t usForPosInterval = 1;
  while (PR_MicrosecondsToInterval(usForPosInterval) == 0) {
    usForPosInterval <<= 1;
  }

  size_t usIntervalResolution;
  BinarySearchIf(MicrosecondsToInterval(), 0, usForPosInterval, IntervalComparator(), &usIntervalResolution);
  MOZ_ASSERT(PR_MicrosecondsToInterval(usIntervalResolution - 1) == 0);
  MOZ_ASSERT(PR_MicrosecondsToInterval(usIntervalResolution) == 1);

  // Half of the amount of microseconds needed to get positive PRIntervalTime.
  // We use this to decide how to round our wait times later
  int32_t halfMicrosecondsIntervalResolution = usIntervalResolution / 2;
  bool forceRunNextTimer = false;

  while (!mShutdown) {
    // Have to use PRIntervalTime here, since PR_WaitCondVar takes it
    PRIntervalTime waitFor;
    bool forceRunThisTimer = forceRunNextTimer;
    forceRunNextTimer = false;

    if (mSleeping
#ifdef MOZ_NUWA_PROCESS
        || IsNuwaProcess() // Don't fire timers or deadlock will result.
#endif
        ) {
      // Sleep for 0.1 seconds while not firing timers.
      uint32_t milliseconds = 100;
      if (ChaosMode::isActive(ChaosFeature::TimerScheduling)) {
        milliseconds = ChaosMode::randomUint32LessThan(200);
      }
      waitFor = PR_MillisecondsToInterval(milliseconds);
    } else {
      waitFor = PR_INTERVAL_NO_TIMEOUT;
      TimeStamp now = TimeStamp::Now();
      nsTimerImpl* timer = nullptr;

      if (!mTimers.IsEmpty()) {
        timer = mTimers[0];

        if (now >= timer->mTimeout || forceRunThisTimer) {
    next:
          // NB: AddRef before the Release under RemoveTimerInternal to avoid
          // mRefCnt passing through zero, in case all other refs than the one
          // from mTimers have gone away (the last non-mTimers[i]-ref's Release
          // must be racing with us, blocked in gThread->RemoveTimer waiting
          // for TimerThread::mMonitor, under nsTimerImpl::Release.

          RefPtr<nsTimerImpl> timerRef(timer);
          RemoveTimerInternal(timer);
          timer = nullptr;

          MOZ_LOG(GetTimerLog(), LogLevel::Debug,
                 ("Timer thread woke up %fms from when it was supposed to\n",
                  fabs((now - timerRef->mTimeout).ToMilliseconds())));

          // We are going to let the call to PostTimerEvent here handle the
          // release of the timer so that we don't end up releasing the timer
          // on the TimerThread instead of on the thread it targets.
          timerRef = PostTimerEvent(timerRef.forget());

          if (timerRef) {
            // We got our reference back due to an error.
            // Unhook the nsRefPtr, and release manually so we can get the
            // refcount.
            nsrefcnt rc = timerRef.forget().take()->Release();
            (void)rc;

            // The nsITimer interface requires that its users keep a reference
            // to the timers they use while those timers are initialized but
            // have not yet fired.  If this ever happens, it is a bug in the
            // code that created and used the timer.
            //
            // Further, note that this should never happen even with a
            // misbehaving user, because nsTimerImpl::Release checks for a
            // refcount of 1 with an armed timer (a timer whose only reference
            // is from the timer thread) and when it hits this will remove the
            // timer from the timer thread and thus destroy the last reference,
            // preventing this situation from occurring.
            MOZ_ASSERT(rc != 0, "destroyed timer off its target thread!");
          }

          if (mShutdown) {
            break;
          }

          // Update now, as PostTimerEvent plus the locking may have taken a
          // tick or two, and we may goto next below.
          now = TimeStamp::Now();
        }
      }

      if (!mTimers.IsEmpty()) {
        timer = mTimers[0];

        TimeStamp timeout = timer->mTimeout;

        // Don't wait at all (even for PR_INTERVAL_NO_WAIT) if the next timer
        // is due now or overdue.
        //
        // Note that we can only sleep for integer values of a certain
        // resolution. We use halfMicrosecondsIntervalResolution, calculated
        // before, to do the optimal rounding (i.e., of how to decide what
        // interval is so small we should not wait at all).
        double microseconds = (timeout - now).ToMilliseconds() * 1000;

        if (ChaosMode::isActive(ChaosFeature::TimerScheduling)) {
          // The mean value of sFractions must be 1 to ensure that
          // the average of a long sequence of timeouts converges to the
          // actual sum of their times.
          static const float sFractions[] = {
            0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 1.75f, 2.75f
          };
          microseconds *=
            sFractions[ChaosMode::randomUint32LessThan(ArrayLength(sFractions))];
          forceRunNextTimer = true;
        }

        if (microseconds < halfMicrosecondsIntervalResolution) {
          forceRunNextTimer = false;
          goto next; // round down; execute event now
        }
        waitFor = PR_MicrosecondsToInterval(
          static_cast<uint32_t>(microseconds)); // Floor is accurate enough.
        if (waitFor == 0) {
          waitFor = 1;  // round up, wait the minimum time we can wait
        }
      }

      if (MOZ_LOG_TEST(GetTimerLog(), LogLevel::Debug)) {
        if (waitFor == PR_INTERVAL_NO_TIMEOUT)
          MOZ_LOG(GetTimerLog(), LogLevel::Debug,
                 ("waiting for PR_INTERVAL_NO_TIMEOUT\n"));
        else
          MOZ_LOG(GetTimerLog(), LogLevel::Debug,
                 ("waiting for %u\n", PR_IntervalToMilliseconds(waitFor)));
      }
    }

    mWaiting = true;
    mNotified = false;
    mMonitor.Wait(waitFor);
    if (mNotified) {
      forceRunNextTimer = false;
    }
    mWaiting = false;
  }

  return NS_OK;
}

nsresult
TimerThread::AddTimer(nsTimerImpl* aTimer)
{
  MonitorAutoLock lock(mMonitor);

  // Add the timer to our list.
  int32_t i = AddTimerInternal(aTimer);
  if (i < 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Awaken the timer thread.
  if (mWaiting && i == 0) {
    mNotified = true;
    mMonitor.Notify();
  }

  return NS_OK;
}

nsresult
TimerThread::TimerDelayChanged(nsTimerImpl* aTimer)
{
  MonitorAutoLock lock(mMonitor);

  // Our caller has a strong ref to aTimer, so it can't go away here under
  // ReleaseTimerInternal.
  RemoveTimerInternal(aTimer);

  int32_t i = AddTimerInternal(aTimer);
  if (i < 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Awaken the timer thread.
  if (mWaiting && i == 0) {
    mNotified = true;
    mMonitor.Notify();
  }

  return NS_OK;
}

nsresult
TimerThread::RemoveTimer(nsTimerImpl* aTimer)
{
  MonitorAutoLock lock(mMonitor);

  // Remove the timer from our array.  Tell callers that aTimer was not found
  // by returning NS_ERROR_NOT_AVAILABLE.  Unlike the TimerDelayChanged case
  // immediately above, our caller may be passing a (now-)weak ref in via the
  // aTimer param, specifically when nsTimerImpl::Release loses a race with
  // TimerThread::Run, must wait for the mMonitor auto-lock here, and during the
  // wait Run drops the only remaining ref to aTimer via RemoveTimerInternal.

  if (!RemoveTimerInternal(aTimer)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Awaken the timer thread.
  if (mWaiting) {
    mNotified = true;
    mMonitor.Notify();
  }

  return NS_OK;
}

// This function must be called from within a lock
int32_t
TimerThread::AddTimerInternal(nsTimerImpl* aTimer)
{
  mMonitor.AssertCurrentThreadOwns();
  if (mShutdown) {
    return -1;
  }

  TimeStamp now = TimeStamp::Now();

  TimerAdditionComparator c(now, aTimer);
  nsTimerImpl** insertSlot = mTimers.InsertElementSorted(aTimer, c);

  if (!insertSlot) {
    return -1;
  }

  aTimer->mArmed = true;
  NS_ADDREF(aTimer);

#ifdef MOZ_TASK_TRACER
  // Caller of AddTimer is the parent task of its timer event, so we store the
  // TraceInfo here for later used.
  aTimer->GetTLSTraceInfo();
#endif

  return insertSlot - mTimers.Elements();
}

bool
TimerThread::RemoveTimerInternal(nsTimerImpl* aTimer)
{
  mMonitor.AssertCurrentThreadOwns();
  if (!mTimers.RemoveElement(aTimer)) {
    return false;
  }

  ReleaseTimerInternal(aTimer);
  return true;
}

void
TimerThread::ReleaseTimerInternal(nsTimerImpl* aTimer)
{
  if (!mShutdown) {
    // copied to a local array before releasing in shutdown
    mMonitor.AssertCurrentThreadOwns();
  }
  // Order is crucial here -- see nsTimerImpl::Release.
  aTimer->mArmed = false;
  NS_RELEASE(aTimer);
}

already_AddRefed<nsTimerImpl>
TimerThread::PostTimerEvent(already_AddRefed<nsTimerImpl> aTimerRef)
{
  mMonitor.AssertCurrentThreadOwns();

  RefPtr<nsTimerImpl> timer(aTimerRef);
  if (!timer->mEventTarget) {
    NS_ERROR("Attempt to post timer event to NULL event target");
    return timer.forget();
  }

  // XXX we may want to reuse this nsTimerEvent in the case of repeating timers.

  // Since we already addref'd 'timer', we don't need to addref here.
  // We will release either in ~nsTimerEvent(), or pass the reference back to
  // the caller. We need to copy the generation number from this timer into the
  // event, so we can avoid firing a timer that was re-initialized after being
  // canceled.

  RefPtr<nsTimerEvent> event = new nsTimerEvent;
  if (!event) {
    return timer.forget();
  }

  if (MOZ_LOG_TEST(GetTimerLog(), LogLevel::Debug)) {
    event->mInitTime = TimeStamp::Now();
  }

  // If this is a repeating precise timer, we need to calculate the time for
  // the next timer to fire before we make the callback. But don't re-arm.
  if (timer->IsRepeatingPrecisely()) {
    timer->SetDelayInternal(timer->mDelay);
  }

#ifdef MOZ_TASK_TRACER
  // During the dispatch of TimerEvent, we overwrite the current TraceInfo
  // partially with the info saved in timer earlier, and restore it back by
  // AutoSaveCurTraceInfo.
  AutoSaveCurTraceInfo saveCurTraceInfo;
  (timer->GetTracedTask()).SetTLSTraceInfo();
#endif

  nsIEventTarget* target = timer->mEventTarget;
  event->SetTimer(timer.forget());

  nsresult rv;
  {
    // We release mMonitor around the Dispatch because if this timer is targeted
    // at the TimerThread we'll deadlock.
    MonitorAutoUnlock unlock(mMonitor);
    rv = target->Dispatch(event, NS_DISPATCH_NORMAL);
  }

  if (NS_FAILED(rv)) {
    timer = event->ForgetTimer();
    RemoveTimerInternal(timer);
    return timer.forget();
  }

  return nullptr;
}

void
TimerThread::DoBeforeSleep()
{
  // Mainthread
  MonitorAutoLock lock(mMonitor);
  mSleeping = true;
}

// Note: wake may be notified without preceding sleep notification
void
TimerThread::DoAfterSleep()
{
  // Mainthread
  MonitorAutoLock lock(mMonitor);
  mSleeping = false;

  // Wake up the timer thread to re-process the array to ensure the sleep delay is correct,
  // and fire any expired timers (perhaps quite a few)
  mNotified = true;
  mMonitor.Notify();
}


NS_IMETHODIMP
TimerThread::Observe(nsISupports* /* aSubject */, const char* aTopic,
                     const char16_t* /* aData */)
{
  if (strcmp(aTopic, "sleep_notification") == 0 ||
      strcmp(aTopic, "suspend_process_notification") == 0) {
    DoBeforeSleep();
  } else if (strcmp(aTopic, "wake_notification") == 0 ||
             strcmp(aTopic, "resume_process_notification") == 0) {
    DoAfterSleep();
  }

  return NS_OK;
}
