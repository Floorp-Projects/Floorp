/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTimerImpl.h"

#include <utility>

#include "GeckoProfiler.h"
#include "TimerThread.h"
#include "mozilla/Atomics.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Logging.h"
#include "mozilla/Mutex.h"
#include "mozilla/ResultExtensions.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h"
#include "pratom.h"
#ifdef MOZ_TASK_TRACER
#  include "GeckoTaskTracerImpl.h"
using namespace mozilla::tasktracer;
#endif

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

static TimerThread* gThread = nullptr;

// This module prints info about the precision of timers.
static mozilla::LazyLogModule sTimerLog("nsTimerImpl");

mozilla::LogModule* GetTimerLog() { return sTimerLog; }

TimeStamp NS_GetTimerDeadlineHintOnCurrentThread(TimeStamp aDefault,
                                                 uint32_t aSearchBound) {
  return gThread
             ? gThread->FindNextFireTimeForCurrentThread(aDefault, aSearchBound)
             : TimeStamp();
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
    nsTimerCallbackFunc aCallback, void* aClosure, uint32_t aDelay,
    uint32_t aType, nsTimerNameCallbackFunc aNameCallback,
    nsIEventTarget* aTarget) {
  nsCOMPtr<nsITimer> timer;
  MOZ_TRY(NS_NewTimerWithFuncCallback(getter_AddRefs(timer), aCallback,
                                      aClosure, aDelay, aType, aNameCallback,
                                      aTarget));
  return std::move(timer);
}
nsresult NS_NewTimerWithFuncCallback(nsITimer** aTimer,
                                     nsTimerCallbackFunc aCallback,
                                     void* aClosure, uint32_t aDelay,
                                     uint32_t aType,
                                     nsTimerNameCallbackFunc aNameCallback,
                                     nsIEventTarget* aTarget) {
  auto timer = nsTimer::WithEventTarget(aTarget);

  MOZ_TRY(timer->InitWithNameableFuncCallback(aCallback, aClosure, aDelay,
                                              aType, aNameCallback));
  timer.forget(aTimer);
  return NS_OK;
}

// This module prints info about which timers are firing, which is useful for
// wakeups for the purposes of power profiling. Set the following environment
// variable before starting the browser.
//
//   MOZ_LOG=TimerFirings:4
//
// Then a line will be printed for every timer that fires. The name used for a
// |Callback::Type::Function| timer depends on the circumstances.
//
// - If it was explicitly named (e.g. it was initialized with
//   InitWithNamedFuncCallback()) then that explicit name will be shown.
//
// - Otherwise, if we are on a platform that supports function name lookup
//   (Mac or Linux) then the looked-up name will be shown with a
//   "[from dladdr]" annotation. On Mac the looked-up name will be immediately
//   useful. On Linux it'll need post-processing with `tools/rb/fix_stacks.py`.
//
// - Otherwise, no name will be printed. If many timers hit this case then
//   you'll need to re-run the workload on a Mac to find out which timers they
//   are, and then give them explicit names.
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

double nsTimerImpl::sDeltaSumSquared = 0;
double nsTimerImpl::sDeltaSum = 0;
double nsTimerImpl::sDeltaNum = 0;

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
      mFiring(0) {
  // XXX some code creates timers during xpcom shutdown, when threads are no
  // longer available, so we cannot turn this on yet.
  // MOZ_ASSERT(mEventTarget);
}

// static
nsresult nsTimerImpl::Startup() {
  nsresult rv;

  gThread = new TimerThread();

  NS_ADDREF(gThread);
  rv = gThread->InitLocks();

  if (NS_FAILED(rv)) {
    NS_RELEASE(gThread);
  }

  return rv;
}

void nsTimerImpl::Shutdown() {
  if (MOZ_LOG_TEST(GetTimerLog(), LogLevel::Debug)) {
    double mean = 0, stddev = 0;
    myNS_MeanAndStdDev(sDeltaNum, sDeltaSum, sDeltaSumSquared, &mean, &stddev);

    MOZ_LOG(GetTimerLog(), LogLevel::Debug,
            ("sDeltaNum = %f, sDeltaSum = %f, sDeltaSumSquared = %f\n",
             sDeltaNum, sDeltaSum, sDeltaSumSquared));
    MOZ_LOG(GetTimerLog(), LogLevel::Debug,
            ("mean: %fms, stddev: %fms\n", mean, stddev));
  }

  if (!gThread) {
    return;
  }

  gThread->Shutdown();
  NS_RELEASE(gThread);
}

nsresult nsTimerImpl::InitCommon(uint32_t aDelayMS, uint32_t aType,
                                 Callback&& aNewCallback) {
  return InitCommon(TimeDuration::FromMilliseconds(aDelayMS), aType,
                    std::move(aNewCallback));
}

nsresult nsTimerImpl::InitCommon(const TimeDuration& aDelay, uint32_t aType,
                                 Callback&& newCallback) {
  mMutex.AssertCurrentThreadOwns();

  if (!gThread) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (!mEventTarget) {
    NS_ERROR("mEventTarget is NULL");
    return NS_ERROR_NOT_INITIALIZED;
  }

  gThread->RemoveTimer(this);
  mCallback.swap(newCallback);
  ++mGeneration;

  mType = (uint8_t)aType;
  mDelay = aDelay;
  mTimeout = TimeStamp::Now() + mDelay;

  return gThread->AddTimer(this);
}

nsresult nsTimerImpl::InitWithFuncCallbackCommon(nsTimerCallbackFunc aFunc,
                                                 void* aClosure,
                                                 uint32_t aDelay,
                                                 uint32_t aType,
                                                 const Callback::Name& aName) {
  if (NS_WARN_IF(!aFunc)) {
    return NS_ERROR_INVALID_ARG;
  }

  Callback cb;  // Goes out of scope after the unlock, prevents deadlock
  cb.mType = Callback::Type::Function;
  cb.mCallback.c = aFunc;
  cb.mClosure = aClosure;
  cb.mName = aName;

  MutexAutoLock lock(mMutex);
  return InitCommon(aDelay, aType, std::move(cb));
}

nsresult nsTimerImpl::InitWithNamedFuncCallback(nsTimerCallbackFunc aFunc,
                                                void* aClosure, uint32_t aDelay,
                                                uint32_t aType,
                                                const char* aNameString) {
  Callback::Name name(aNameString);
  return InitWithFuncCallbackCommon(aFunc, aClosure, aDelay, aType, name);
}

nsresult nsTimerImpl::InitWithNameableFuncCallback(
    nsTimerCallbackFunc aFunc, void* aClosure, uint32_t aDelay, uint32_t aType,
    nsTimerNameCallbackFunc aNameFunc) {
  Callback::Name name(aNameFunc);
  return InitWithFuncCallbackCommon(aFunc, aClosure, aDelay, aType, name);
}

nsresult nsTimerImpl::InitWithCallback(nsITimerCallback* aCallback,
                                       uint32_t aDelay, uint32_t aType) {
  return InitHighResolutionWithCallback(
      aCallback, TimeDuration::FromMilliseconds(aDelay), aType);
}

nsresult nsTimerImpl::InitHighResolutionWithCallback(
    nsITimerCallback* aCallback, const TimeDuration& aDelay, uint32_t aType) {
  if (NS_WARN_IF(!aCallback)) {
    return NS_ERROR_INVALID_ARG;
  }

  Callback cb;  // Goes out of scope after the unlock, prevents deadlock
  cb.mType = Callback::Type::Interface;
  cb.mCallback.i = aCallback;
  NS_ADDREF(cb.mCallback.i);

  MutexAutoLock lock(mMutex);
  return InitCommon(aDelay, aType, std::move(cb));
}

nsresult nsTimerImpl::Init(nsIObserver* aObserver, uint32_t aDelay,
                           uint32_t aType) {
  if (NS_WARN_IF(!aObserver)) {
    return NS_ERROR_INVALID_ARG;
  }

  Callback cb;  // Goes out of scope after the unlock, prevents deadlock
  cb.mType = Callback::Type::Observer;
  cb.mCallback.o = aObserver;
  NS_ADDREF(cb.mCallback.o);

  MutexAutoLock lock(mMutex);
  return InitCommon(aDelay, aType, std::move(cb));
}

nsresult nsTimerImpl::Cancel() {
  CancelImpl(false);
  return NS_OK;
}

void nsTimerImpl::CancelImpl(bool aClearITimer) {
  Callback cbTrash;
  RefPtr<nsITimer> timerTrash;

  {
    MutexAutoLock lock(mMutex);
    if (gThread) {
      gThread->RemoveTimer(this);
    }

    cbTrash.swap(mCallback);
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
  if (GetCallback().mType == Callback::Type::Unknown && !IsRepeating()) {
    // This may happen if someone tries to re-use a one-shot timer
    // by re-setting delay instead of reinitializing the timer.
    NS_ERROR(
        "nsITimer->SetDelay() called when the "
        "one-shot timer is not set up.");
    return NS_ERROR_NOT_INITIALIZED;
  }

  bool reAdd = false;
  if (gThread) {
    reAdd = NS_SUCCEEDED(gThread->RemoveTimer(this));
  }

  mDelay = TimeDuration::FromMilliseconds(aDelay);
  mTimeout = TimeStamp::Now() + mDelay;

  if (reAdd) {
    gThread->AddTimer(this);
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
  *aClosure = GetCallback().mClosure;
  return NS_OK;
}

nsresult nsTimerImpl::GetCallback(nsITimerCallback** aCallback) {
  MutexAutoLock lock(mMutex);
  if (GetCallback().mType == Callback::Type::Interface) {
    NS_IF_ADDREF(*aCallback = GetCallback().mCallback.i);
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
  if (NS_WARN_IF(mCallback.mType != Callback::Type::Unknown)) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  if (aTarget) {
    mEventTarget = aTarget;
  } else {
    mEventTarget = mozilla::GetCurrentThreadEventTarget();
  }
  return NS_OK;
}

nsresult nsTimerImpl::GetAllowedEarlyFiringMicroseconds(uint32_t* aValueOut) {
  *aValueOut = gThread ? gThread->AllowedEarlyFiringMicroseconds() : 0;
  return NS_OK;
}

void nsTimerImpl::Fire(int32_t aGeneration) {
  uint8_t oldType;
  uint32_t oldDelay;
  TimeStamp oldTimeout;
  Callback callbackDuringFire;
  nsCOMPtr<nsITimer> kungFuDeathGrip;

  {
    // Don't fire callbacks or fiddle with refcounts when the mutex is locked.
    // If some other thread Cancels/Inits after this, they're just too late.
    MutexAutoLock lock(mMutex);
    if (aGeneration != mGeneration) {
      return;
    }

    ++mFiring;
    callbackDuringFire = mCallback;
    oldType = mType;
    oldDelay = mDelay.ToMilliseconds();
    oldTimeout = mTimeout;
    // Ensure that the nsITimer does not unhook from the nsTimerImpl during
    // Fire; this will cause null pointer crashes if the user of the timer drops
    // its reference, and then uses the nsITimer* passed in the callback.
    kungFuDeathGrip = mITimer;
  }

  AUTO_PROFILER_LABEL("nsTimerImpl::Fire", OTHER);

  TimeStamp now = TimeStamp::Now();
  if (MOZ_LOG_TEST(GetTimerLog(), LogLevel::Debug)) {
    TimeDuration delta = now - oldTimeout;
    int32_t d = delta.ToMilliseconds();  // delta in ms
    sDeltaSum += abs(d);
    sDeltaSumSquared += double(d) * double(d);
    sDeltaNum++;

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

  switch (callbackDuringFire.mType) {
    case Callback::Type::Function:
      callbackDuringFire.mCallback.c(mITimer, callbackDuringFire.mClosure);
      break;
    case Callback::Type::Interface:
      callbackDuringFire.mCallback.i->Notify(mITimer);
      break;
    case Callback::Type::Observer:
      callbackDuringFire.mCallback.o->Observe(mITimer, NS_TIMER_CALLBACK_TOPIC,
                                              nullptr);
      break;
    default:;
  }

  MutexAutoLock lock(mMutex);
  if (aGeneration == mGeneration) {
    if (IsRepeating()) {
      // Repeating timer has not been re-init or canceled; reschedule
      if (IsSlack()) {
        mTimeout = TimeStamp::Now() + mDelay;
      } else {
        mTimeout = mTimeout + mDelay;
      }
      if (gThread) {
        gThread->AddTimer(this);
      }
    } else {
      // Non-repeating timer that has not been re-scheduled. Clear.
      mCallback.clear();
    }
  }

  --mFiring;

  MOZ_LOG(GetTimerLog(), LogLevel::Debug,
          ("[this=%p] Took %fms to fire timer callback\n", this,
           (TimeStamp::Now() - now).ToMilliseconds()));
}

#if defined(HAVE_DLADDR) && defined(HAVE___CXA_DEMANGLE)
#  define USE_DLADDR 1
#endif

#ifdef USE_DLADDR
#  include <cxxabi.h>
#  include <dlfcn.h>
#endif

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

  switch (aCallback.mType) {
    case Callback::Type::Function: {
      bool needToFreeName = false;
      const char* annotation = "";
      const char* name;
      static const size_t buflen = 1024;
      char buf[buflen];

      if (aCallback.mName.is<Callback::NameString>()) {
        name = aCallback.mName.as<Callback::NameString>();

      } else if (aCallback.mName.is<Callback::NameFunc>()) {
        aCallback.mName.as<Callback::NameFunc>()(
            mITimer, /* aAnonymize = */ false, aCallback.mClosure, buf, buflen);
        name = buf;

      } else {
        MOZ_ASSERT(aCallback.mName.is<Callback::NameNothing>());
#ifdef USE_DLADDR
        annotation = "[from dladdr] ";

        Dl_info info;
        void* addr = reinterpret_cast<void*>(aCallback.mCallback.c);
        if (dladdr(addr, &info) == 0) {
          name = "???[dladdr: failed]";

        } else if (info.dli_sname) {
          int status;
          name = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
          if (status == 0) {
            // Success. Because we didn't pass in a buffer to __cxa_demangle it
            // allocates its own one with malloc() which we must free() later.
            MOZ_ASSERT(name);
            needToFreeName = true;
          } else if (status == -1) {
            name = "???[__cxa_demangle: OOM]";
          } else if (status == -2) {
            name = "???[__cxa_demangle: invalid mangled name]";
          } else if (status == -3) {
            name = "???[__cxa_demangle: invalid argument]";
          } else {
            name = "???[__cxa_demangle: unexpected status value]";
          }

        } else if (info.dli_fname) {
          // The "#0: " prefix is necessary for `fix_stacks.py` to interpret
          // this string as something to convert.
          snprintf(buf, buflen, "#0: ???[%s +0x%" PRIxPTR "]\n", info.dli_fname,
                   uintptr_t(addr) - uintptr_t(info.dli_fbase));
          name = buf;

        } else {
          name = "???[dladdr: no symbol or shared object obtained]";
        }
#else
        name = "???[dladdr is unimplemented or doesn't work well on this OS]";
#endif
      }

      MOZ_LOG(GetTimerFiringsLog(), LogLevel::Debug,
              ("[%d]    fn timer (%s %5d ms): %s%s\n", getpid(), typeStr,
               aDelay, annotation, name));

      if (needToFreeName) {
        free(const_cast<char*>(name));
      }

      break;
    }

    case Callback::Type::Interface: {
      MOZ_LOG(GetTimerFiringsLog(), LogLevel::Debug,
              ("[%d] iface timer (%s %5d ms): %p\n", getpid(), typeStr, aDelay,
               aCallback.mCallback.i));
      break;
    }

    case Callback::Type::Observer: {
      MOZ_LOG(GetTimerFiringsLog(), LogLevel::Debug,
              ("[%d]   obs timer (%s %5d ms): %p\n", getpid(), typeStr, aDelay,
               aCallback.mCallback.o));
      break;
    }

    case Callback::Type::Unknown:
    default: {
      MOZ_LOG(GetTimerFiringsLog(), LogLevel::Debug,
              ("[%d]   ??? timer (%s, %5d ms)\n", getpid(), typeStr, aDelay));
      break;
    }
  }
}

void nsTimerImpl::GetName(nsACString& aName) {
  MutexAutoLock lock(mMutex);
  Callback& cb(GetCallback());
  switch (cb.mType) {
    case Callback::Type::Function:
      if (cb.mName.is<Callback::NameString>()) {
        aName.Assign(cb.mName.as<Callback::NameString>());
      } else if (cb.mName.is<Callback::NameFunc>()) {
        static const size_t buflen = 1024;
        char buf[buflen];
        cb.mName.as<Callback::NameFunc>()(mITimer, /* aAnonymize = */ true,
                                          cb.mClosure, buf, buflen);
        aName.Assign(buf);
      } else {
        MOZ_ASSERT(cb.mName.is<Callback::NameNothing>());
        aName.AssignLiteral("Anonymous_callback_timer");
      }
      break;

    case Callback::Type::Interface:
      if (nsCOMPtr<nsINamed> named = do_QueryInterface(cb.mCallback.i)) {
        named->GetName(aName);
      } else {
        aName.AssignLiteral("Anonymous_interface_timer");
      }
      break;

    case Callback::Type::Observer:
      if (nsCOMPtr<nsINamed> named = do_QueryInterface(cb.mCallback.o)) {
        named->GetName(aName);
      } else {
        aName.AssignLiteral("Anonymous_observer_timer");
      }
      break;

    case Callback::Type::Unknown:
      aName.AssignLiteral("Canceled_timer");
      break;
  }
}

void nsTimerImpl::SetHolder(nsTimerImplHolder* aHolder) { mHolder = aHolder; }

nsTimer::~nsTimer() = default;

size_t nsTimer::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this);
}

/* static */
RefPtr<nsTimer> nsTimer::WithEventTarget(nsIEventTarget* aTarget) {
  if (!aTarget) {
    aTarget = mozilla::GetCurrentThreadEventTarget();
  }
  return do_AddRef(new nsTimer(aTarget));
}

/* static */
nsresult nsTimer::XPCOMConstructor(nsISupports* aOuter, REFNSIID aIID,
                                   void** aResult) {
  *aResult = nullptr;
  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }

  auto timer = WithEventTarget(nullptr);

  return timer->QueryInterface(aIID, aResult);
}

/* static */
const nsTimerImpl::Callback::NameNothing nsTimerImpl::Callback::Nothing = 0;

#ifdef MOZ_TASK_TRACER
void nsTimerImpl::GetTLSTraceInfo() { mTracedTask.GetTLSTraceInfo(); }

TracedTaskCommon nsTimerImpl::GetTracedTask() { return mTracedTask; }

#endif
