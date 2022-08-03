/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTimerImpl.h"

#include <utility>

#include "TimerThread.h"
#include "mozilla/Atomics.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Logging.h"
#include "mozilla/Mutex.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticMutex.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h"
#include "pratom.h"

#ifdef XP_WIN
#  include <process.h>
#  ifndef getpid
#    define getpid _getpid
#  endif
#else
#  include <unistd.h>
#endif

using mozilla::Atomic;
using mozilla::LogLevel;
using mozilla::MakeRefPtr;
using mozilla::MutexAutoLock;
using mozilla::TimeDuration;
using mozilla::TimeStamp;

// Holds the timer thread and manages all interactions with it
// under a locked mutex. This wrapper is not destroyed until after
// nsThreadManager shutdown to ensure we don't UAF during an offthread access to
// the timer thread.
class TimerThreadWrapper {
 public:
  constexpr TimerThreadWrapper() : mThread(nullptr){};
  ~TimerThreadWrapper() = default;

  nsresult Init();
  void Shutdown();

  nsresult AddTimer(nsTimerImpl* aTimer, const MutexAutoLock& aProofOfLock)
      MOZ_REQUIRES(aTimer->mMutex);
  nsresult RemoveTimer(nsTimerImpl* aTimer, const MutexAutoLock& aProofOfLock)
      MOZ_REQUIRES(aTimer->mMutex);
  TimeStamp FindNextFireTimeForCurrentThread(TimeStamp aDefault,
                                             uint32_t aSearchBound);
  uint32_t AllowedEarlyFiringMicroseconds();

 private:
  static mozilla::StaticMutex sMutex;
  TimerThread* mThread MOZ_GUARDED_BY(sMutex);
};

mozilla::StaticMutex TimerThreadWrapper::sMutex;

nsresult TimerThreadWrapper::Init() {
  mozilla::StaticMutexAutoLock lock(sMutex);
  mThread = new TimerThread();

  NS_ADDREF(mThread);

  return NS_OK;
}

void TimerThreadWrapper::Shutdown() {
  RefPtr<TimerThread> thread;

  {
    mozilla::StaticMutexAutoLock lock(sMutex);
    if (!mThread) {
      return;
    }
    thread = mThread;
  }
  // Shutdown calls |nsTimerImpl::Cancel| which needs to make a call into
  // |RemoveTimer|. This can't be done under the lock.
  thread->Shutdown();

  {
    mozilla::StaticMutexAutoLock lock(sMutex);
    NS_RELEASE(mThread);
  }
}

nsresult TimerThreadWrapper::AddTimer(nsTimerImpl* aTimer,
                                      const MutexAutoLock& aProofOfLock) {
  mozilla::StaticMutexAutoLock lock(sMutex);
  if (mThread) {
    return mThread->AddTimer(aTimer, aProofOfLock);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

nsresult TimerThreadWrapper::RemoveTimer(nsTimerImpl* aTimer,
                                         const MutexAutoLock& aProofOfLock) {
  mozilla::StaticMutexAutoLock lock(sMutex);
  if (mThread) {
    return mThread->RemoveTimer(aTimer, aProofOfLock);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

TimeStamp TimerThreadWrapper::FindNextFireTimeForCurrentThread(
    TimeStamp aDefault, uint32_t aSearchBound) {
  mozilla::StaticMutexAutoLock lock(sMutex);
  return mThread
             ? mThread->FindNextFireTimeForCurrentThread(aDefault, aSearchBound)
             : TimeStamp();
}

uint32_t TimerThreadWrapper::AllowedEarlyFiringMicroseconds() {
  mozilla::StaticMutexAutoLock lock(sMutex);
  return mThread ? mThread->AllowedEarlyFiringMicroseconds() : 0;
}

static TimerThreadWrapper gThreadWrapper;

// This module prints info about the precision of timers.
static mozilla::LazyLogModule sTimerLog("nsTimerImpl");

mozilla::LogModule* GetTimerLog() { return sTimerLog; }

TimeStamp NS_GetTimerDeadlineHintOnCurrentThread(TimeStamp aDefault,
                                                 uint32_t aSearchBound) {
  return gThreadWrapper.FindNextFireTimeForCurrentThread(aDefault,
                                                         aSearchBound);
}

already_AddRefed<nsITimer> NS_NewTimer() { return NS_NewTimer(nullptr); }

already_AddRefed<nsITimer> NS_NewTimer(nsIEventTarget* aTarget) {
  return nsTimer::WithEventTarget(aTarget).forget();
}

mozilla::Result<nsCOMPtr<nsITimer>, nsresult> NS_NewTimerWithObserver(
    nsIObserver* aObserver, uint32_t aDelay, uint32_t aType,
    nsIEventTarget* aTarget) {
  nsCOMPtr<nsITimer> timer;
  MOZ_TRY(NS_NewTimerWithObserver(getter_AddRefs(timer), aObserver, aDelay,
                                  aType, aTarget));
  return std::move(timer);
}
nsresult NS_NewTimerWithObserver(nsITimer** aTimer, nsIObserver* aObserver,
                                 uint32_t aDelay, uint32_t aType,
                                 nsIEventTarget* aTarget) {
  auto timer = nsTimer::WithEventTarget(aTarget);

  MOZ_TRY(timer->Init(aObserver, aDelay, aType));
  timer.forget(aTimer);
  return NS_OK;
}

mozilla::Result<nsCOMPtr<nsITimer>, nsresult> NS_NewTimerWithCallback(
    nsITimerCallback* aCallback, uint32_t aDelay, uint32_t aType,
    nsIEventTarget* aTarget) {
  nsCOMPtr<nsITimer> timer;
  MOZ_TRY(NS_NewTimerWithCallback(getter_AddRefs(timer), aCallback, aDelay,
                                  aType, aTarget));
  return std::move(timer);
}
nsresult NS_NewTimerWithCallback(nsITimer** aTimer, nsITimerCallback* aCallback,
                                 uint32_t aDelay, uint32_t aType,
                                 nsIEventTarget* aTarget) {
  auto timer = nsTimer::WithEventTarget(aTarget);

  MOZ_TRY(timer->InitWithCallback(aCallback, aDelay, aType));
  timer.forget(aTimer);
  return NS_OK;
}

mozilla::Result<nsCOMPtr<nsITimer>, nsresult> NS_NewTimerWithCallback(
    nsITimerCallback* aCallback, const TimeDuration& aDelay, uint32_t aType,
    nsIEventTarget* aTarget) {
  nsCOMPtr<nsITimer> timer;
  MOZ_TRY(NS_NewTimerWithCallback(getter_AddRefs(timer), aCallback, aDelay,
                                  aType, aTarget));
  return std::move(timer);
}
nsresult NS_NewTimerWithCallback(nsITimer** aTimer, nsITimerCallback* aCallback,
                                 const TimeDuration& aDelay, uint32_t aType,
                                 nsIEventTarget* aTarget) {
  auto timer = nsTimer::WithEventTarget(aTarget);

  MOZ_TRY(timer->InitHighResolutionWithCallback(aCallback, aDelay, aType));
  timer.forget(aTimer);
  return NS_OK;
}

mozilla::Result<nsCOMPtr<nsITimer>, nsresult> NS_NewTimerWithCallback(
    std::function<void(nsITimer*)>&& aCallback, uint32_t aDelay, uint32_t aType,
    const char* aNameString, nsIEventTarget* aTarget) {
  nsCOMPtr<nsITimer> timer;
  MOZ_TRY(NS_NewTimerWithCallback(getter_AddRefs(timer), std::move(aCallback),
                                  aDelay, aType, aNameString, aTarget));
  return timer;
}
nsresult NS_NewTimerWithCallback(nsITimer** aTimer,
                                 std::function<void(nsITimer*)>&& aCallback,
                                 uint32_t aDelay, uint32_t aType,
                                 const char* aNameString,
                                 nsIEventTarget* aTarget) {
  return NS_NewTimerWithCallback(aTimer, std::move(aCallback),
                                 TimeDuration::FromMilliseconds(aDelay), aType,
                                 aNameString, aTarget);
}

mozilla::Result<nsCOMPtr<nsITimer>, nsresult> NS_NewTimerWithCallback(
    std::function<void(nsITimer*)>&& aCallback, const TimeDuration& aDelay,
    uint32_t aType, const char* aNameString, nsIEventTarget* aTarget) {
  nsCOMPtr<nsITimer> timer;
  MOZ_TRY(NS_NewTimerWithCallback(getter_AddRefs(timer), std::move(aCallback),
                                  aDelay, aType, aNameString, aTarget));
  return timer;
}
nsresult NS_NewTimerWithCallback(nsITimer** aTimer,
                                 std::function<void(nsITimer*)>&& aCallback,
                                 const TimeDuration& aDelay, uint32_t aType,
                                 const char* aNameString,
                                 nsIEventTarget* aTarget) {
  RefPtr<nsTimer> timer = nsTimer::WithEventTarget(aTarget);

  MOZ_TRY(timer->InitWithClosureCallback(std::move(aCallback), aDelay, aType,
                                         aNameString));
  timer.forget(aTimer);
  return NS_OK;
}

mozilla::Result<nsCOMPtr<nsITimer>, nsresult> NS_NewTimerWithFuncCallback(
    nsTimerCallbackFunc aCallback, void* aClosure, uint32_t aDelay,
    uint32_t aType, const char* aNameString, nsIEventTarget* aTarget) {
  nsCOMPtr<nsITimer> timer;
  MOZ_TRY(NS_NewTimerWithFuncCallback(getter_AddRefs(timer), aCallback,
                                      aClosure, aDelay, aType, aNameString,
                                      aTarget));
  return std::move(timer);
}
nsresult NS_NewTimerWithFuncCallback(nsITimer** aTimer,
                                     nsTimerCallbackFunc aCallback,
                                     void* aClosure, uint32_t aDelay,
                                     uint32_t aType, const char* aNameString,
                                     nsIEventTarget* aTarget) {
  auto timer = nsTimer::WithEventTarget(aTarget);

  MOZ_TRY(timer->InitWithNamedFuncCallback(aCallback, aClosure, aDelay, aType,
                                           aNameString));
  timer.forget(aTimer);
  return NS_OK;
}

mozilla::Result<nsCOMPtr<nsITimer>, nsresult> NS_NewTimerWithFuncCallback(
    nsTimerCallbackFunc aCallback, void* aClosure, const TimeDuration& aDelay,
    uint32_t aType, const char* aNameString, nsIEventTarget* aTarget) {
  nsCOMPtr<nsITimer> timer;
  MOZ_TRY(NS_NewTimerWithFuncCallback(getter_AddRefs(timer), aCallback,
                                      aClosure, aDelay, aType, aNameString,
                                      aTarget));
  return std::move(timer);
}
nsresult NS_NewTimerWithFuncCallback(nsITimer** aTimer,
                                     nsTimerCallbackFunc aCallback,
                                     void* aClosure, const TimeDuration& aDelay,
                                     uint32_t aType, const char* aNameString,
                                     nsIEventTarget* aTarget) {
  auto timer = nsTimer::WithEventTarget(aTarget);

  MOZ_TRY(timer->InitHighResolutionWithNamedFuncCallback(
      aCallback, aClosure, aDelay, aType, aNameString));
  timer.forget(aTimer);
  return NS_OK;
}

// This module prints info about which timers are firing, which is useful for
// wakeups for the purposes of power profiling. Set the following environment
// variable before starting the browser.
//
//   MOZ_LOG=TimerFirings:4
//
// Then a line will be printed for every timer that fires.
//
// If you redirect this output to a file called "out", you can then
// post-process it with a command something like the following.
//
//   cat out | grep timer | sort | uniq -c | sort -r -n
//
// This will show how often each unique line appears, with the most common ones
// first.
//
// More detailed docs are here:
// https://developer.mozilla.org/en-US/docs/Mozilla/Performance/TimerFirings_logging
//
static mozilla::LazyLogModule sTimerFiringsLog("TimerFirings");

static mozilla::LogModule* GetTimerFiringsLog() { return sTimerFiringsLog; }

#include <math.h>

/* static */
mozilla::StaticMutex nsTimerImpl::sDeltaMutex;
/* static */
double nsTimerImpl::sDeltaSumSquared MOZ_GUARDED_BY(nsTimerImpl::sDeltaMutex) =
    0;
/* static */
double nsTimerImpl::sDeltaSum MOZ_GUARDED_BY(nsTimerImpl::sDeltaMutex) = 0;
/* static */
double nsTimerImpl::sDeltaNum MOZ_GUARDED_BY(nsTimerImpl::sDeltaMutex) = 0;

static void myNS_MeanAndStdDev(double n, double sumOfValues,
                               double sumOfSquaredValues, double* meanResult,
                               double* stdDevResult) {
  double mean = 0.0, var = 0.0, stdDev = 0.0;
  if (n > 0.0 && sumOfValues >= 0) {
    mean = sumOfValues / n;
    double temp = (n * sumOfSquaredValues) - (sumOfValues * sumOfValues);
    if (temp < 0.0 || n <= 1) {
      var = 0.0;
    } else {
      var = temp / (n * (n - 1));
    }
    // for some reason, Windows says sqrt(0.0) is "-1.#J" (?!) so do this:
    stdDev = var != 0.0 ? sqrt(var) : 0.0;
  }
  *meanResult = mean;
  *stdDevResult = stdDev;
}

NS_IMPL_QUERY_INTERFACE(nsTimer, nsITimer)
NS_IMPL_ADDREF(nsTimer)

NS_IMETHODIMP_(MozExternalRefCountType)
nsTimer::Release(void) {
  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "nsTimer");

  if (count == 1) {
    // Last ref, in nsTimerImpl::mITimer. Make sure the cycle is broken.
    mImpl->CancelImpl(true);
  } else if (count == 0) {
    delete this;
  }

  return count;
}

nsTimerImpl::nsTimerImpl(nsITimer* aTimer, nsIEventTarget* aTarget)
    : mEventTarget(aTarget),
      mHolder(nullptr),
      mType(0),
      mGeneration(0),
      mITimer(aTimer),
      mMutex("nsTimerImpl::mMutex"),
      mCallback(UnknownCallback{}),
      mFiring(0) {
  // XXX some code creates timers during xpcom shutdown, when threads are no
  // longer available, so we cannot turn this on yet.
  // MOZ_ASSERT(mEventTarget);
}

// static
nsresult nsTimerImpl::Startup() { return gThreadWrapper.Init(); }

void nsTimerImpl::Shutdown() {
  if (MOZ_LOG_TEST(GetTimerLog(), LogLevel::Debug)) {
    mozilla::StaticMutexAutoLock lock(sDeltaMutex);
    double mean = 0, stddev = 0;
    myNS_MeanAndStdDev(sDeltaNum, sDeltaSum, sDeltaSumSquared, &mean, &stddev);

    MOZ_LOG(GetTimerLog(), LogLevel::Debug,
            ("sDeltaNum = %f, sDeltaSum = %f, sDeltaSumSquared = %f\n",
             sDeltaNum, sDeltaSum, sDeltaSumSquared));
    MOZ_LOG(GetTimerLog(), LogLevel::Debug,
            ("mean: %fms, stddev: %fms\n", mean, stddev));
  }

  gThreadWrapper.Shutdown();
}

nsresult nsTimerImpl::InitCommon(const TimeDuration& aDelay, uint32_t aType,
                                 Callback&& newCallback,
                                 const MutexAutoLock& aProofOfLock) {
  if (!mEventTarget) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  gThreadWrapper.RemoveTimer(this, aProofOfLock);

  // If we have an existing callback, using `swap` ensures it's destroyed after
  // the mutex is unlocked in our caller.
  std::swap(mCallback, newCallback);
  ++mGeneration;

  mType = (uint8_t)aType;
  mDelay = aDelay;
  mTimeout = TimeStamp::Now() + mDelay;

  return gThreadWrapper.AddTimer(this, aProofOfLock);
}

nsresult nsTimerImpl::InitWithNamedFuncCallback(nsTimerCallbackFunc aFunc,
                                                void* aClosure, uint32_t aDelay,
                                                uint32_t aType,
                                                const char* aName) {
  return InitHighResolutionWithNamedFuncCallback(
      aFunc, aClosure, TimeDuration::FromMilliseconds(aDelay), aType, aName);
}

nsresult nsTimerImpl::InitHighResolutionWithNamedFuncCallback(
    nsTimerCallbackFunc aFunc, void* aClosure, const TimeDuration& aDelay,
    uint32_t aType, const char* aName) {
  if (NS_WARN_IF(!aFunc)) {
    return NS_ERROR_INVALID_ARG;
  }

  Callback cb{FuncCallback{aFunc, aClosure, aName}};

  MutexAutoLock lock(mMutex);
  return InitCommon(aDelay, aType, std::move(cb), lock);
}

nsresult nsTimerImpl::InitWithCallback(nsITimerCallback* aCallback,
                                       uint32_t aDelayInMs, uint32_t aType) {
  return InitHighResolutionWithCallback(
      aCallback, TimeDuration::FromMilliseconds(aDelayInMs), aType);
}

nsresult nsTimerImpl::InitHighResolutionWithCallback(
    nsITimerCallback* aCallback, const TimeDuration& aDelay, uint32_t aType) {
  if (NS_WARN_IF(!aCallback)) {
    return NS_ERROR_INVALID_ARG;
  }

  // Goes out of scope after the unlock, prevents deadlock
  Callback cb{nsCOMPtr{aCallback}};

  MutexAutoLock lock(mMutex);
  return InitCommon(aDelay, aType, std::move(cb), lock);
}

nsresult nsTimerImpl::Init(nsIObserver* aObserver, uint32_t aDelayInMs,
                           uint32_t aType) {
  if (NS_WARN_IF(!aObserver)) {
    return NS_ERROR_INVALID_ARG;
  }

  Callback cb{nsCOMPtr{aObserver}};

  MutexAutoLock lock(mMutex);
  return InitCommon(TimeDuration::FromMilliseconds(aDelayInMs), aType,
                    std::move(cb), lock);
}

nsresult nsTimerImpl::InitWithClosureCallback(
    std::function<void(nsITimer*)>&& aCallback, const TimeDuration& aDelay,
    uint32_t aType, const char* aNameString) {
  if (NS_WARN_IF(!aCallback)) {
    return NS_ERROR_INVALID_ARG;
  }

  Callback cb{ClosureCallback{std::move(aCallback), aNameString}};

  MutexAutoLock lock(mMutex);
  return InitCommon(aDelay, aType, std::move(cb), lock);
}

nsresult nsTimerImpl::Cancel() {
  CancelImpl(false);
  return NS_OK;
}

void nsTimerImpl::CancelImpl(bool aClearITimer) {
  Callback cbTrash{UnknownCallback{}};
  RefPtr<nsITimer> timerTrash;

  {
    MutexAutoLock lock(mMutex);
    gThreadWrapper.RemoveTimer(this, lock);

    // The swap ensures our callback isn't dropped until after the mutex is
    // unlocked.
    std::swap(cbTrash, mCallback);
    ++mGeneration;

    // Don't clear this if we're firing; once Fire returns, we'll get this call
    // again.
    if (aClearITimer && !mFiring) {
      MOZ_RELEASE_ASSERT(
          mITimer,
          "mITimer was nulled already! "
          "This indicates that someone has messed up the refcount on nsTimer!");
      timerTrash.swap(mITimer);
    }
  }
}

nsresult nsTimerImpl::SetDelay(uint32_t aDelay) {
  MutexAutoLock lock(mMutex);
  if (GetCallback().is<UnknownCallback>() && !IsRepeating()) {
    // This may happen if someone tries to re-use a one-shot timer
    // by re-setting delay instead of reinitializing the timer.
    NS_ERROR(
        "nsITimer->SetDelay() called when the "
        "one-shot timer is not set up.");
    return NS_ERROR_NOT_INITIALIZED;
  }

  bool reAdd = false;
  reAdd = NS_SUCCEEDED(gThreadWrapper.RemoveTimer(this, lock));

  mDelay = TimeDuration::FromMilliseconds(aDelay);
  mTimeout = TimeStamp::Now() + mDelay;

  if (reAdd) {
    gThreadWrapper.AddTimer(this, lock);
  }

  return NS_OK;
}

nsresult nsTimerImpl::GetDelay(uint32_t* aDelay) {
  MutexAutoLock lock(mMutex);
  *aDelay = mDelay.ToMilliseconds();
  return NS_OK;
}

nsresult nsTimerImpl::SetType(uint32_t aType) {
  MutexAutoLock lock(mMutex);
  mType = (uint8_t)aType;
  // XXX if this is called, we should change the actual type.. this could effect
  // repeating timers.  we need to ensure in Fire() that if mType has changed
  // during the callback that we don't end up with the timer in the queue twice.
  return NS_OK;
}

nsresult nsTimerImpl::GetType(uint32_t* aType) {
  MutexAutoLock lock(mMutex);
  *aType = mType;
  return NS_OK;
}

nsresult nsTimerImpl::GetClosure(void** aClosure) {
  MutexAutoLock lock(mMutex);
  if (GetCallback().is<FuncCallback>()) {
    *aClosure = GetCallback().as<FuncCallback>().mClosure;
  } else {
    *aClosure = nullptr;
  }
  return NS_OK;
}

nsresult nsTimerImpl::GetCallback(nsITimerCallback** aCallback) {
  MutexAutoLock lock(mMutex);
  if (GetCallback().is<InterfaceCallback>()) {
    NS_IF_ADDREF(*aCallback = GetCallback().as<InterfaceCallback>());
  } else {
    *aCallback = nullptr;
  }
  return NS_OK;
}

nsresult nsTimerImpl::GetTarget(nsIEventTarget** aTarget) {
  MutexAutoLock lock(mMutex);
  NS_IF_ADDREF(*aTarget = mEventTarget);
  return NS_OK;
}

nsresult nsTimerImpl::SetTarget(nsIEventTarget* aTarget) {
  MutexAutoLock lock(mMutex);
  if (NS_WARN_IF(!mCallback.is<UnknownCallback>())) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  if (aTarget) {
    mEventTarget = aTarget;
  } else {
    mEventTarget = mozilla::GetCurrentSerialEventTarget();
  }
  return NS_OK;
}

nsresult nsTimerImpl::GetAllowedEarlyFiringMicroseconds(uint32_t* aValueOut) {
  *aValueOut = gThreadWrapper.AllowedEarlyFiringMicroseconds();
  return NS_OK;
}

void nsTimerImpl::Fire(int32_t aGeneration) {
  uint8_t oldType;
  uint32_t oldDelay;
  TimeStamp oldTimeout;
  Callback callbackDuringFire{UnknownCallback{}};
  nsCOMPtr<nsITimer> timer;

  {
    // Don't fire callbacks or fiddle with refcounts when the mutex is locked.
    // If some other thread Cancels/Inits after this, they're just too late.
    MutexAutoLock lock(mMutex);
    if (aGeneration != mGeneration) {
      // This timer got rescheduled or cancelled before we fired, so ignore this
      // firing
      return;
    }

    // We modify mTimeout, so we must not be in the current TimerThread's
    // mTimers list.  Adding to that list calls SetHolder(), so use mHolder
    // as a proxy to know if we're in the list
    MOZ_ASSERT(!mHolder);

    ++mFiring;
    callbackDuringFire = mCallback;
    oldType = mType;
    oldDelay = mDelay.ToMilliseconds();
    oldTimeout = mTimeout;
    // Ensure that the nsITimer does not unhook from the nsTimerImpl during
    // Fire; this will cause null pointer crashes if the user of the timer drops
    // its reference, and then uses the nsITimer* passed in the callback.
    timer = mITimer;
  }

  AUTO_PROFILER_LABEL("nsTimerImpl::Fire", OTHER);

  TimeStamp fireTime = TimeStamp::Now();
  if (MOZ_LOG_TEST(GetTimerLog(), LogLevel::Debug)) {
    TimeDuration delta = fireTime - oldTimeout;
    int32_t d = delta.ToMilliseconds();  // delta in ms
    {
      mozilla::StaticMutexAutoLock lock(sDeltaMutex);
      sDeltaSum += abs(d);
      sDeltaSumSquared += double(d) * double(d);
      sDeltaNum++;
    }

    MOZ_LOG(GetTimerLog(), LogLevel::Debug,
            ("[this=%p] expected delay time %4ums\n", this, oldDelay));
    MOZ_LOG(GetTimerLog(), LogLevel::Debug,
            ("[this=%p] actual delay time   %4dms\n", this, oldDelay + d));
    MOZ_LOG(GetTimerLog(), LogLevel::Debug,
            ("[this=%p] (mType is %d)       -------\n", this, oldType));
    MOZ_LOG(GetTimerLog(), LogLevel::Debug,
            ("[this=%p]     delta           %4dms\n", this, d));
  }

  if (MOZ_LOG_TEST(GetTimerFiringsLog(), LogLevel::Debug)) {
    LogFiring(callbackDuringFire, oldType, oldDelay);
  }

  callbackDuringFire.match(
      [](const UnknownCallback&) {},
      [&](const InterfaceCallback& i) { i->Notify(timer); },
      [&](const ObserverCallback& o) {
        o->Observe(timer, NS_TIMER_CALLBACK_TOPIC, nullptr);
      },
      [&](const FuncCallback& f) { f.mFunc(timer, f.mClosure); },
      [&](const ClosureCallback& c) { c.mFunc(timer); });

  TimeStamp now = TimeStamp::Now();

  MutexAutoLock lock(mMutex);
  if (aGeneration == mGeneration) {
    if (IsRepeating()) {
      // Repeating timer has not been re-init or canceled; reschedule
      if (IsSlack()) {
        mTimeout = now + mDelay;
      } else {
        if (mDelay) {
          // If we are late enough finishing the callback that we have missed
          // some firings, do not attempt to play catchup, just get back on the
          // cadence we're supposed to maintain.
          unsigned missedFirings =
              static_cast<unsigned>((now - mTimeout) / mDelay);
          mTimeout += mDelay * (missedFirings + 1);
        } else {
          // Can we stop allowing repeating timers with delay 0?
          mTimeout = now;
        }
      }
      gThreadWrapper.AddTimer(this, lock);
    } else {
      // Non-repeating timer that has not been re-scheduled. Clear.
      // XXX(nika): Other callsites seem to go to some effort to avoid
      // destroying mCallback when it's held. Why not this one?
      mCallback = mozilla::AsVariant(UnknownCallback{});
    }
  }

  --mFiring;

  MOZ_LOG(GetTimerLog(), LogLevel::Debug,
          ("[this=%p] Took %fms to fire timer callback\n", this,
           (now - fireTime).ToMilliseconds()));
}

// See the big comment above GetTimerFiringsLog() to understand this code.
void nsTimerImpl::LogFiring(const Callback& aCallback, uint8_t aType,
                            uint32_t aDelay) {
  const char* typeStr;
  switch (aType) {
    case nsITimer::TYPE_ONE_SHOT:
      typeStr = "ONE_SHOT  ";
      break;
    case nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY:
      typeStr = "ONE_LOW   ";
      break;
    case nsITimer::TYPE_REPEATING_SLACK:
      typeStr = "SLACK     ";
      break;
    case nsITimer::TYPE_REPEATING_SLACK_LOW_PRIORITY:
      typeStr = "SLACK_LOW ";
      break;
    case nsITimer::TYPE_REPEATING_PRECISE: /* fall through */
    case nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP:
      typeStr = "PRECISE   ";
      break;
    default:
      MOZ_CRASH("bad type");
  }

  aCallback.match(
      [&](const UnknownCallback&) {
        MOZ_LOG(
            GetTimerFiringsLog(), LogLevel::Debug,
            ("[%d]     ??? timer (%s, %5d ms)\n", getpid(), typeStr, aDelay));
      },
      [&](const InterfaceCallback& i) {
        MOZ_LOG(GetTimerFiringsLog(), LogLevel::Debug,
                ("[%d]   iface timer (%s %5d ms): %p\n", getpid(), typeStr,
                 aDelay, i.get()));
      },
      [&](const ObserverCallback& o) {
        MOZ_LOG(GetTimerFiringsLog(), LogLevel::Debug,
                ("[%d]     obs timer (%s %5d ms): %p\n", getpid(), typeStr,
                 aDelay, o.get()));
      },
      [&](const FuncCallback& f) {
        MOZ_LOG(GetTimerFiringsLog(), LogLevel::Debug,
                ("[%d]      fn timer (%s %5d ms): %s\n", getpid(), typeStr,
                 aDelay, f.mName));
      },
      [&](const ClosureCallback& c) {
        MOZ_LOG(GetTimerFiringsLog(), LogLevel::Debug,
                ("[%d] closure timer (%s %5d ms): %s\n", getpid(), typeStr,
                 aDelay, c.mName));
      });
}

void nsTimerImpl::GetName(nsACString& aName,
                          const MutexAutoLock& aProofOfLock) {
  GetCallback().match(
      [&](const UnknownCallback&) { aName.AssignLiteral("Canceled_timer"); },
      [&](const InterfaceCallback& i) {
        if (nsCOMPtr<nsINamed> named = do_QueryInterface(i)) {
          named->GetName(aName);
        } else {
          aName.AssignLiteral("Anonymous_interface_timer");
        }
      },
      [&](const ObserverCallback& o) {
        if (nsCOMPtr<nsINamed> named = do_QueryInterface(o)) {
          named->GetName(aName);
        } else {
          aName.AssignLiteral("Anonymous_observer_timer");
        }
      },
      [&](const FuncCallback& f) { aName.Assign(f.mName); },
      [&](const ClosureCallback& c) { aName.Assign(c.mName); });
}

void nsTimerImpl::GetName(nsACString& aName) {
  MutexAutoLock lock(mMutex);
  GetName(aName, lock);
}

void nsTimerImpl::SetHolder(nsTimerImplHolder* aHolder) { mHolder = aHolder; }

nsTimer::~nsTimer() = default;

size_t nsTimer::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) {
  return aMallocSizeOf(this);
}

/* static */
RefPtr<nsTimer> nsTimer::WithEventTarget(nsIEventTarget* aTarget) {
  if (!aTarget) {
    aTarget = mozilla::GetCurrentSerialEventTarget();
  }
  return do_AddRef(new nsTimer(aTarget));
}

/* static */
nsresult nsTimer::XPCOMConstructor(REFNSIID aIID, void** aResult) {
  *aResult = nullptr;
  auto timer = WithEventTarget(nullptr);

  return timer->QueryInterface(aIID, aResult);
}
