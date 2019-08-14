/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTimerImpl.h"
#include "TimerThread.h"

#include "nsThreadUtils.h"
#include "pratom.h"

#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "mozilla/Services.h"
#include "mozilla/ChaosMode.h"
#include "mozilla/ArenaAllocator.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/OperatorNewExtensions.h"

#include <math.h>

using namespace mozilla;
#ifdef MOZ_TASK_TRACER
#  include "GeckoTaskTracerImpl.h"
using namespace mozilla::tasktracer;
#endif

NS_IMPL_ISUPPORTS(TimerThread, nsIRunnable, nsIObserver)

TimerThread::TimerThread()
    : mInitialized(false),
      mMonitor("TimerThread.mMonitor"),
      mShutdown(false),
      mWaiting(false),
      mNotified(false),
      mSleeping(false),
      mAllowedEarlyFiringMicroseconds(0) {}

TimerThread::~TimerThread() {
  mThread = nullptr;

  NS_ASSERTION(mTimers.IsEmpty(), "Timers remain in TimerThread::~TimerThread");
}

nsresult TimerThread::InitLocks() { return NS_OK; }

namespace {

class TimerObserverRunnable : public Runnable {
 public:
  explicit TimerObserverRunnable(nsIObserver* aObserver)
      : mozilla::Runnable("TimerObserverRunnable"), mObserver(aObserver) {}

  NS_DECL_NSIRUNNABLE

 private:
  nsCOMPtr<nsIObserver> mObserver;
};

NS_IMETHODIMP
TimerObserverRunnable::Run() {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(mObserver, "sleep_notification", false);
    observerService->AddObserver(mObserver, "wake_notification", false);
    observerService->AddObserver(mObserver, "suspend_process_notification",
                                 false);
    observerService->AddObserver(mObserver, "resume_process_notification",
                                 false);
  }
  return NS_OK;
}

}  // namespace

namespace {

// TimerEventAllocator is a thread-safe allocator used only for nsTimerEvents.
// It's needed to avoid contention over the default allocator lock when
// firing timer events (see bug 733277).  The thread-safety is required because
// nsTimerEvent objects are allocated on the timer thread, and freed on another
// thread.  Because TimerEventAllocator has its own lock, contention over that
// lock is limited to the allocation and deallocation of nsTimerEvent objects.
//
// Because this is layered over ArenaAllocator, it never shrinks -- even
// "freed" nsTimerEvents aren't truly freed, they're just put onto a free-list
// for later recycling.  So the amount of memory consumed will always be equal
// to the high-water mark consumption.  But nsTimerEvents are small and it's
// unusual to have more than a few hundred of them, so this shouldn't be a
// problem in practice.

class TimerEventAllocator {
 private:
  struct FreeEntry {
    FreeEntry* mNext;
  };

  ArenaAllocator<4096> mPool;
  FreeEntry* mFirstFree;
  mozilla::Monitor mMonitor;

 public:
  TimerEventAllocator()
      : mPool(),
        mFirstFree(nullptr),
        // Timer thread state may be accessed during GC, so uses of this monitor
        // are not preserved when recording/replaying.
        mMonitor("TimerEventAllocator", recordreplay::Behavior::DontPreserve) {}

  ~TimerEventAllocator() {}

  void* Alloc(size_t aSize);
  void Free(void* aPtr);
};

}  // namespace

// This is a nsICancelableRunnable because we can dispatch it to Workers and
// those can be shut down at any time, and in these cases, Cancel() is called
// instead of Run().
class nsTimerEvent final : public CancelableRunnable {
 public:
  NS_IMETHOD Run() override;

  nsresult Cancel() override {
    mTimer->Cancel();
    return NS_OK;
  }

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
  NS_IMETHOD GetName(nsACString& aName) override;
#endif

  explicit nsTimerEvent(already_AddRefed<nsTimerImpl> aTimer)
      : mozilla::CancelableRunnable("nsTimerEvent"),
        mTimer(aTimer),
        mGeneration(mTimer->GetGeneration()) {
    // Note: We override operator new for this class, and the override is
    // fallible!
    sAllocatorUsers++;

    if (MOZ_LOG_TEST(GetTimerLog(), LogLevel::Debug)) {
      mInitTime = TimeStamp::Now();
    }
  }

  static void Init();
  static void Shutdown();
  static void DeleteAllocatorIfNeeded();

  static void* operator new(size_t aSize) noexcept(true) {
    return sAllocator->Alloc(aSize);
  }
  void operator delete(void* aPtr) {
    sAllocator->Free(aPtr);
    DeleteAllocatorIfNeeded();
  }

  already_AddRefed<nsTimerImpl> ForgetTimer() { return mTimer.forget(); }

 private:
  nsTimerEvent(const nsTimerEvent&) = delete;
  nsTimerEvent& operator=(const nsTimerEvent&) = delete;
  nsTimerEvent& operator=(const nsTimerEvent&&) = delete;

  ~nsTimerEvent() {
    MOZ_ASSERT(!sCanDeleteAllocator || sAllocatorUsers > 0,
               "This will result in us attempting to deallocate the "
               "nsTimerEvent allocator twice");
    sAllocatorUsers--;
  }

  TimeStamp mInitTime;
  RefPtr<nsTimerImpl> mTimer;
  const int32_t mGeneration;

  static TimerEventAllocator* sAllocator;

  // Timer thread state may be accessed during GC, so uses of this atomic are
  // not preserved when recording/replaying.
  static Atomic<int32_t, SequentiallyConsistent,
                recordreplay::Behavior::DontPreserve>
      sAllocatorUsers;
  static bool sCanDeleteAllocator;
};

TimerEventAllocator* nsTimerEvent::sAllocator = nullptr;
Atomic<int32_t, SequentiallyConsistent, recordreplay::Behavior::DontPreserve>
    nsTimerEvent::sAllocatorUsers;
bool nsTimerEvent::sCanDeleteAllocator = false;

namespace {

void* TimerEventAllocator::Alloc(size_t aSize) {
  MOZ_ASSERT(aSize == sizeof(nsTimerEvent));

  mozilla::MonitorAutoLock lock(mMonitor);

  void* p;
  if (mFirstFree) {
    p = mFirstFree;
    mFirstFree = mFirstFree->mNext;
  } else {
    p = mPool.Allocate(aSize, fallible);
  }

  return p;
}

void TimerEventAllocator::Free(void* aPtr) {
  mozilla::MonitorAutoLock lock(mMonitor);

  FreeEntry* entry = reinterpret_cast<FreeEntry*>(aPtr);

  entry->mNext = mFirstFree;
  mFirstFree = entry;
}

}  // namespace

void nsTimerEvent::Init() { sAllocator = new TimerEventAllocator(); }

void nsTimerEvent::Shutdown() {
  sCanDeleteAllocator = true;
  DeleteAllocatorIfNeeded();
}

void nsTimerEvent::DeleteAllocatorIfNeeded() {
  if (sCanDeleteAllocator && sAllocatorUsers == 0) {
    delete sAllocator;
    sAllocator = nullptr;
  }
}

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
NS_IMETHODIMP
nsTimerEvent::GetName(nsACString& aName) {
  bool current;
  MOZ_RELEASE_ASSERT(
      NS_SUCCEEDED(mTimer->mEventTarget->IsOnCurrentThread(&current)) &&
      current);

  mTimer->GetName(aName);
  return NS_OK;
}
#endif

NS_IMETHODIMP
nsTimerEvent::Run() {
  if (MOZ_LOG_TEST(GetTimerLog(), LogLevel::Debug)) {
    TimeStamp now = TimeStamp::Now();
    MOZ_LOG(GetTimerLog(), LogLevel::Debug,
            ("[this=%p] time between PostTimerEvent() and Fire(): %fms\n", this,
             (now - mInitTime).ToMilliseconds()));
  }

  mTimer->Fire(mGeneration);

  return NS_OK;
}

nsresult TimerThread::Init() {
  mMonitor.AssertCurrentThreadOwns();
  MOZ_LOG(GetTimerLog(), LogLevel::Debug,
          ("TimerThread::Init [%d]\n", mInitialized));

  if (!mInitialized) {
    nsTimerEvent::Init();

    // We hold on to mThread to keep the thread alive.
    nsresult rv =
        NS_NewNamedThread("Timer Thread", getter_AddRefs(mThread), this);
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

    mInitialized = true;
  }

  if (!mThread) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult TimerThread::Shutdown() {
  MOZ_LOG(GetTimerLog(), LogLevel::Debug, ("TimerThread::Shutdown begin\n"));

  if (!mThread) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsTArray<RefPtr<nsTimerImpl>> timers;
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
    // because call to timers' Cancel() (and release its self)
    // must not be done under the lock. Destructor of a callback
    // might potentially call some code reentering the same lock
    // that leads to unexpected behavior or deadlock.
    // See bug 422472.
    for (const UniquePtr<Entry>& entry : mTimers) {
      timers.AppendElement(entry->Take());
    }

    mTimers.Clear();
  }

  for (const RefPtr<nsTimerImpl>& timer : timers) {
    if (timer) {
      timer->Cancel();
    }
  }

  mThread->Shutdown();  // wait for the thread to die

  nsTimerEvent::Shutdown();

  MOZ_LOG(GetTimerLog(), LogLevel::Debug, ("TimerThread::Shutdown end\n"));
  return NS_OK;
}

namespace {

struct MicrosecondsToInterval {
  PRIntervalTime operator[](size_t aMs) const {
    return PR_MicrosecondsToInterval(aMs);
  }
};

struct IntervalComparator {
  int operator()(PRIntervalTime aInterval) const {
    return (0 < aInterval) ? -1 : 1;
  }
};

}  // namespace

NS_IMETHODIMP
TimerThread::Run() {
  NS_SetCurrentThreadName("Timer");

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
  BinarySearchIf(MicrosecondsToInterval(), 0, usForPosInterval,
                 IntervalComparator(), &usIntervalResolution);
  MOZ_ASSERT(PR_MicrosecondsToInterval(usIntervalResolution - 1) == 0);
  MOZ_ASSERT(PR_MicrosecondsToInterval(usIntervalResolution) == 1);

  // Half of the amount of microseconds needed to get positive PRIntervalTime.
  // We use this to decide how to round our wait times later
  mAllowedEarlyFiringMicroseconds = usIntervalResolution / 2;
  bool forceRunNextTimer = false;

  while (!mShutdown) {
    // Have to use PRIntervalTime here, since PR_WaitCondVar takes it
    TimeDuration waitFor;
    bool forceRunThisTimer = forceRunNextTimer;
    forceRunNextTimer = false;

    if (mSleeping) {
      // Sleep for 0.1 seconds while not firing timers.
      uint32_t milliseconds = 100;
      if (ChaosMode::isActive(ChaosFeature::TimerScheduling)) {
        milliseconds = ChaosMode::randomUint32LessThan(200);
      }
      waitFor = TimeDuration::FromMilliseconds(milliseconds);
    } else {
      waitFor = TimeDuration::Forever();
      TimeStamp now = TimeStamp::Now();

      RemoveLeadingCanceledTimersInternal();

      if (!mTimers.IsEmpty()) {
        if (now >= mTimers[0]->Value()->mTimeout || forceRunThisTimer) {
        next:
          // NB: AddRef before the Release under RemoveTimerInternal to avoid
          // mRefCnt passing through zero, in case all other refs than the one
          // from mTimers have gone away (the last non-mTimers[i]-ref's Release
          // must be racing with us, blocked in gThread->RemoveTimer waiting
          // for TimerThread::mMonitor, under nsTimerImpl::Release.

          RefPtr<nsTimerImpl> timerRef(mTimers[0]->Take());
          RemoveFirstTimerInternal();

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

      RemoveLeadingCanceledTimersInternal();

      if (!mTimers.IsEmpty()) {
        TimeStamp timeout = mTimers[0]->Value()->mTimeout;

        // Don't wait at all (even for PR_INTERVAL_NO_WAIT) if the next timer
        // is due now or overdue.
        //
        // Note that we can only sleep for integer values of a certain
        // resolution. We use mAllowedEarlyFiringMicroseconds, calculated
        // before, to do the optimal rounding (i.e., of how to decide what
        // interval is so small we should not wait at all).
        double microseconds = (timeout - now).ToMilliseconds() * 1000;

        if (ChaosMode::isActive(ChaosFeature::TimerScheduling)) {
          // The mean value of sFractions must be 1 to ensure that
          // the average of a long sequence of timeouts converges to the
          // actual sum of their times.
          static const float sFractions[] = {0.0f, 0.25f, 0.5f, 0.75f,
                                             1.0f, 1.75f, 2.75f};
          microseconds *= sFractions[ChaosMode::randomUint32LessThan(
              ArrayLength(sFractions))];
          forceRunNextTimer = true;
        }

        if (microseconds < mAllowedEarlyFiringMicroseconds) {
          forceRunNextTimer = false;
          goto next;  // round down; execute event now
        }
        waitFor = TimeDuration::FromMicroseconds(microseconds);
        if (waitFor.IsZero()) {
          // round up, wait the minimum time we can wait
          waitFor = TimeDuration::FromMicroseconds(1);
        }
      }

      if (MOZ_LOG_TEST(GetTimerLog(), LogLevel::Debug)) {
        if (waitFor == TimeDuration::Forever())
          MOZ_LOG(GetTimerLog(), LogLevel::Debug, ("waiting forever\n"));
        else
          MOZ_LOG(GetTimerLog(), LogLevel::Debug,
                  ("waiting for %f\n", waitFor.ToMilliseconds()));
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

nsresult TimerThread::AddTimer(nsTimerImpl* aTimer) {
  MonitorAutoLock lock(mMonitor);

  if (!aTimer->mEventTarget) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = Init();
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Add the timer to our list.
  if (!AddTimerInternal(aTimer)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Awaken the timer thread.
  if (mWaiting && mTimers[0]->Value() == aTimer) {
    mNotified = true;
    mMonitor.Notify();
  }

  return NS_OK;
}

nsresult TimerThread::RemoveTimer(nsTimerImpl* aTimer) {
  MonitorAutoLock lock(mMonitor);

  // Remove the timer from our array.  Tell callers that aTimer was not found
  // by returning NS_ERROR_NOT_AVAILABLE.

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

TimeStamp TimerThread::FindNextFireTimeForCurrentThread(TimeStamp aDefault,
                                                        uint32_t aSearchBound) {
  MonitorAutoLock lock(mMonitor);
  TimeStamp timeStamp = aDefault;
  uint32_t index = 0;

#ifdef DEBUG
  TimeStamp firstTimeStamp;
  Entry* initialFirstEntry = nullptr;
  if (!mTimers.IsEmpty()) {
    initialFirstEntry = mTimers[0].get();
    firstTimeStamp = mTimers[0]->Timeout();
  }
#endif

  auto end = mTimers.end();
  while (end != mTimers.begin()) {
    nsTimerImpl* timer = mTimers[0]->Value();
    if (timer) {
      if (timer->mTimeout > aDefault) {
        timeStamp = aDefault;
        break;
      }

      // Don't yield to timers created with the *_LOW_PRIORITY type.
      if (!timer->IsLowPriority()) {
        bool isOnCurrentThread = false;
        nsresult rv =
            timer->mEventTarget->IsOnCurrentThread(&isOnCurrentThread);
        if (NS_SUCCEEDED(rv) && isOnCurrentThread) {
          timeStamp = timer->mTimeout;
          break;
        }
      }

      if (++index > aSearchBound) {
        // Track the currently highest timeout so that we can bail out when we
        // reach the bound or when we find a timer for the current thread.
        // This won't give accurate information if we stop before finding
        // any timer for the current thread, but at least won't report too
        // long idle period.
        timeStamp = timer->mTimeout;
        break;
      }
    }

    std::pop_heap(mTimers.begin(), end, Entry::UniquePtrLessThan);
    --end;
  }

  while (end != mTimers.end()) {
    ++end;
    std::push_heap(mTimers.begin(), end, Entry::UniquePtrLessThan);
  }

#ifdef DEBUG
  if (!mTimers.IsEmpty()) {
    if (firstTimeStamp != mTimers[0]->Timeout()) {
      TimeStamp now = TimeStamp::Now();
      printf_stderr(
          "firstTimeStamp %f, mTimers[0]->Timeout() %f, "
          "initialFirstTimer %p, current first %p\n",
          (firstTimeStamp - now).ToMilliseconds(),
          (mTimers[0]->Timeout() - now).ToMilliseconds(), initialFirstEntry,
          mTimers[0].get());
    }
  }
  MOZ_ASSERT_IF(!mTimers.IsEmpty(), firstTimeStamp == mTimers[0]->Timeout());
#endif

  return timeStamp;
}

// This function must be called from within a lock
bool TimerThread::AddTimerInternal(nsTimerImpl* aTimer) {
  mMonitor.AssertCurrentThreadOwns();
  if (mShutdown) {
    return false;
  }

  TimeStamp now = TimeStamp::Now();

  UniquePtr<Entry>* entry = mTimers.AppendElement(
      MakeUnique<Entry>(now, aTimer->mTimeout, aTimer), mozilla::fallible);
  if (!entry) {
    return false;
  }

  std::push_heap(mTimers.begin(), mTimers.end(), Entry::UniquePtrLessThan);

#ifdef MOZ_TASK_TRACER
  // Caller of AddTimer is the parent task of its timer event, so we store the
  // TraceInfo here for later used.
  aTimer->GetTLSTraceInfo();
#endif

  return true;
}

bool TimerThread::RemoveTimerInternal(nsTimerImpl* aTimer) {
  mMonitor.AssertCurrentThreadOwns();
  if (!aTimer || !aTimer->mHolder) {
    return false;
  }
  aTimer->mHolder->Forget(aTimer);
  return true;
}

void TimerThread::RemoveLeadingCanceledTimersInternal() {
  mMonitor.AssertCurrentThreadOwns();

  // Move all canceled timers from the front of the list to
  // the back of the list using std::pop_heap().  We do this
  // without actually removing them from the list so we can
  // modify the nsTArray in a single bulk operation.
  auto sortedEnd = mTimers.end();
  while (sortedEnd != mTimers.begin() && !mTimers[0]->Value()) {
    std::pop_heap(mTimers.begin(), sortedEnd, Entry::UniquePtrLessThan);
    --sortedEnd;
  }

  // If there were no canceled timers then we are done.
  if (sortedEnd == mTimers.end()) {
    return;
  }

  // Finally, remove the canceled timers from the back of the
  // nsTArray.  Note, since std::pop_heap() uses iterators
  // we must convert to nsTArray indices and number of
  // elements here.
  mTimers.RemoveElementsAt(sortedEnd - mTimers.begin(),
                           mTimers.end() - sortedEnd);
}

void TimerThread::RemoveFirstTimerInternal() {
  mMonitor.AssertCurrentThreadOwns();
  MOZ_ASSERT(!mTimers.IsEmpty());
  std::pop_heap(mTimers.begin(), mTimers.end(), Entry::UniquePtrLessThan);
  mTimers.RemoveLastElement();
}

already_AddRefed<nsTimerImpl> TimerThread::PostTimerEvent(
    already_AddRefed<nsTimerImpl> aTimerRef) {
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

#ifdef MOZ_TASK_TRACER
  // During the dispatch of TimerEvent, we overwrite the current TraceInfo
  // partially with the info saved in timer earlier, and restore it back by
  // AutoSaveCurTraceInfo.
  AutoSaveCurTraceInfo saveCurTraceInfo;
  (timer->GetTracedTask()).SetTLSTraceInfo();
#endif

  nsCOMPtr<nsIEventTarget> target = timer->mEventTarget;

  void* p = nsTimerEvent::operator new(sizeof(nsTimerEvent));
  if (!p) {
    return timer.forget();
  }
  RefPtr<nsTimerEvent> event =
      ::new (KnownNotNull, p) nsTimerEvent(timer.forget());

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

void TimerThread::DoBeforeSleep() {
  // Mainthread
  MonitorAutoLock lock(mMonitor);
  mSleeping = true;
}

// Note: wake may be notified without preceding sleep notification
void TimerThread::DoAfterSleep() {
  // Mainthread
  MonitorAutoLock lock(mMonitor);
  mSleeping = false;

  // Wake up the timer thread to re-process the array to ensure the sleep delay
  // is correct, and fire any expired timers (perhaps quite a few)
  mNotified = true;
  mMonitor.Notify();
}

NS_IMETHODIMP
TimerThread::Observe(nsISupports* /* aSubject */, const char* aTopic,
                     const char16_t* /* aData */) {
  if (strcmp(aTopic, "sleep_notification") == 0 ||
      strcmp(aTopic, "suspend_process_notification") == 0) {
    DoBeforeSleep();
  } else if (strcmp(aTopic, "wake_notification") == 0 ||
             strcmp(aTopic, "resume_process_notification") == 0) {
    DoAfterSleep();
  }

  return NS_OK;
}

uint32_t TimerThread::AllowedEarlyFiringMicroseconds() const {
  return mAllowedEarlyFiringMicroseconds;
}
