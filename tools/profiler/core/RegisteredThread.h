/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RegisteredThread_h
#define RegisteredThread_h

#include "platform.h"
#include "ThreadInfo.h"

#include "mozilla/NotNull.h"
#include "mozilla/ProfilerLabels.h"  // for ProfilingStackOwner
#include "mozilla/RefPtr.h"
#include "nsIEventTarget.h"
#include "nsIThread.h"

namespace mozilla {
class ProfilingStackOwner;
}

// This class contains the state for a single thread that is accessible without
// protection from gPSMutex in platform.cpp. Because there is no external
// protection against data races, it must provide internal protection. Hence
// the "Racy" prefix.
//
class RacyRegisteredThread final {
 public:
  explicit RacyRegisteredThread(ProfilerThreadId aThreadId);

  MOZ_COUNTED_DTOR(RacyRegisteredThread)

  void SetIsBeingProfiled(bool aIsBeingProfiled) {
    mIsBeingProfiled = aIsBeingProfiled;
  }

  bool IsBeingProfiled() const { return mIsBeingProfiled; }

  // This is called on every profiler restart. Put things that should happen at
  // that time here.
  void ReinitializeOnResume() {
    // This is needed to cause an initial sample to be taken from sleeping
    // threads that had been observed prior to the profiler stopping and
    // restarting. Otherwise sleeping threads would not have any samples to
    // copy forward while sleeping.
    (void)mSleep.compareExchange(SLEEPING_OBSERVED, SLEEPING_NOT_OBSERVED);
  }

  // This returns true for the second and subsequent calls in each sleep cycle.
  bool CanDuplicateLastSampleDueToSleep() {
    if (mSleep == AWAKE) {
      return false;
    }

    if (mSleep.compareExchange(SLEEPING_NOT_OBSERVED, SLEEPING_OBSERVED)) {
      return false;
    }

    return true;
  }

  // Call this whenever the current thread sleeps. Calling it twice in a row
  // without an intervening setAwake() call is an error.
  void SetSleeping() {
    MOZ_ASSERT(mSleep == AWAKE);
    mSleep = SLEEPING_NOT_OBSERVED;
  }

  // Call this whenever the current thread wakes. Calling it twice in a row
  // without an intervening setSleeping() call is an error.
  void SetAwake() {
    MOZ_ASSERT(mSleep != AWAKE);
    mSleep = AWAKE;
  }

  bool IsSleeping() { return mSleep != AWAKE; }

  ProfilerThreadId ThreadId() const { return mThreadId; }

  class ProfilingStack& ProfilingStack() {
    return mProfilingStackOwner->ProfilingStack();
  }
  const class ProfilingStack& ProfilingStack() const {
    return mProfilingStackOwner->ProfilingStack();
  }

  mozilla::ProfilingStackOwner& ProfilingStackOwner() {
    return *mProfilingStackOwner;
  }

 private:
  mozilla::NotNull<RefPtr<mozilla::ProfilingStackOwner>> mProfilingStackOwner;

  // mThreadId contains the thread ID of the current thread. It is safe to read
  // this from multiple threads concurrently, as it will never be mutated.
  const ProfilerThreadId mThreadId;

  // mSleep tracks whether the thread is sleeping, and if so, whether it has
  // been previously observed. This is used for an optimization: in some cases,
  // when a thread is asleep, we duplicate the previous sample, which is
  // cheaper than taking a new sample.
  //
  // mSleep is atomic because it is accessed from multiple threads.
  //
  // - It is written only by this thread, via setSleeping() and setAwake().
  //
  // - It is read by SamplerThread::Run().
  //
  // There are two cases where racing between threads can cause an issue.
  //
  // - If CanDuplicateLastSampleDueToSleep() returns false but that result is
  //   invalidated before being acted upon, we will take a full sample
  //   unnecessarily. This is additional work but won't cause any correctness
  //   issues. (In actual fact, this case is impossible. In order to go from
  //   CanDuplicateLastSampleDueToSleep() returning false to it returning true
  //   requires an intermediate call to it in order for mSleep to go from
  //   SLEEPING_NOT_OBSERVED to SLEEPING_OBSERVED.)
  //
  // - If CanDuplicateLastSampleDueToSleep() returns true but that result is
  //   invalidated before being acted upon -- i.e. the thread wakes up before
  //   DuplicateLastSample() is called -- we will duplicate the previous
  //   sample. This is inaccurate, but only slightly... we will effectively
  //   treat the thread as having slept a tiny bit longer than it really did.
  //
  // This latter inaccuracy could be avoided by moving the
  // CanDuplicateLastSampleDueToSleep() check within the thread-freezing code,
  // e.g. the section where Tick() is called. But that would reduce the
  // effectiveness of the optimization because more code would have to be run
  // before we can tell that duplication is allowed.
  //
  static const int AWAKE = 0;
  static const int SLEEPING_NOT_OBSERVED = 1;
  static const int SLEEPING_OBSERVED = 2;
  mozilla::Atomic<int> mSleep;

  // Is this thread being profiled? (e.g., should markers be recorded?)
  mozilla::Atomic<bool, mozilla::MemoryOrdering::Relaxed> mIsBeingProfiled;
};

// This class contains information that's relevant to a single thread only
// while that thread is running and registered with the profiler, but
// regardless of whether the profiler is running. All accesses to it are
// protected by the profiler state lock.
class RegisteredThread final {
 public:
  RegisteredThread(ThreadInfo* aInfo, nsIThread* aThread, void* aStackTop);
  ~RegisteredThread();

  class RacyRegisteredThread& RacyRegisteredThread() {
    return mRacyRegisteredThread;
  }
  const class RacyRegisteredThread& RacyRegisteredThread() const {
    return mRacyRegisteredThread;
  }

  PlatformData* GetPlatformData() const { return mPlatformData.get(); }
  const void* StackTop() const { return mStackTop; }

  // aDelay is the time the event that is currently running on the thread
  // was queued before starting to run (if a PrioritizedEventQueue
  // (i.e. MainThread), this will be 0 for any event at a lower priority
  // than Input).
  // aRunning is the time the event has been running.  If no event is
  // running these will both be TimeDuration() (i.e. 0).  Both are out
  // params, and are always set.  Their initial value is discarded.
  void GetRunningEventDelay(const mozilla::TimeStamp& aNow,
                            mozilla::TimeDuration& aDelay,
                            mozilla::TimeDuration& aRunning);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  // Set the JSContext of the thread to be sampled. Sampling cannot begin until
  // this has been set.
  void SetJSContext(JSContext* aContext);

  void ClearJSContext() {
    // This function runs on-thread.
    mContext = nullptr;
  }

  JSContext* GetJSContext() const { return mContext; }

  const RefPtr<ThreadInfo> Info() const { return mThreadInfo; }
  const nsCOMPtr<nsIEventTarget> GetEventTarget() const { return mThread; }
  void ResetMainThread(nsIThread* aThread) { mThread = aThread; }

  // Request that this thread start JS sampling. JS sampling won't actually
  // start until a subsequent PollJSSampling() call occurs *and* mContext has
  // been set.
  void StartJSSampling(uint32_t aJSFlags) {
    // This function runs on-thread or off-thread.

    MOZ_RELEASE_ASSERT(mJSSampling == INACTIVE ||
                       mJSSampling == INACTIVE_REQUESTED);
    mJSSampling = ACTIVE_REQUESTED;
    mJSFlags = aJSFlags;
  }

  // Request that this thread stop JS sampling. JS sampling won't actually stop
  // until a subsequent PollJSSampling() call occurs.
  void StopJSSampling() {
    // This function runs on-thread or off-thread.

    MOZ_RELEASE_ASSERT(mJSSampling == ACTIVE ||
                       mJSSampling == ACTIVE_REQUESTED);
    mJSSampling = INACTIVE_REQUESTED;
  }

  // Poll to see if JS sampling should be started/stopped.
  void PollJSSampling();

 private:
  class RacyRegisteredThread mRacyRegisteredThread;

  const UniquePlatformData mPlatformData;
  const void* mStackTop;

  const RefPtr<ThreadInfo> mThreadInfo;
  nsCOMPtr<nsIThread> mThread;

  // If this is a JS thread, this is its JSContext, which is required for any
  // JS sampling.
  JSContext* mContext;

  // The profiler needs to start and stop JS sampling of JS threads at various
  // times. However, the JS engine can only do the required actions on the
  // JS thread itself ("on-thread"), not from another thread ("off-thread").
  // Therefore, we have the following two-step process.
  //
  // - The profiler requests (on-thread or off-thread) that the JS sampling be
  //   started/stopped, by changing mJSSampling to the appropriate REQUESTED
  //   state.
  //
  // - The relevant JS thread polls (on-thread) for changes to mJSSampling.
  //   When it sees a REQUESTED state, it performs the appropriate actions to
  //   actually start/stop JS sampling, and changes mJSSampling out of the
  //   REQUESTED state.
  //
  // The state machine is as follows.
  //
  //             INACTIVE --> ACTIVE_REQUESTED
  //                  ^       ^ |
  //                  |     _/  |
  //                  |   _/    |
  //                  |  /      |
  //                  | v       v
  //   INACTIVE_REQUESTED <-- ACTIVE
  //
  // The polling is done in the following two ways.
  //
  // - Via the interrupt callback mechanism; the JS thread must call
  //   profiler_js_interrupt_callback() from its own interrupt callback.
  //   This is how sampling must be started/stopped for threads where the
  //   request was made off-thread.
  //
  // - When {Start,Stop}JSSampling() is called on-thread, we can immediately
  //   follow it with a PollJSSampling() call to avoid the delay between the
  //   two steps. Likewise, setJSContext() calls PollJSSampling().
  //
  // One non-obvious thing about all this: these JS sampling requests are made
  // on all threads, even non-JS threads. mContext needs to also be set (via
  // setJSContext(), which can only happen for JS threads) for any JS sampling
  // to actually happen.
  //
  enum {
    INACTIVE = 0,
    ACTIVE_REQUESTED = 1,
    ACTIVE = 2,
    INACTIVE_REQUESTED = 3,
  } mJSSampling;

  uint32_t mJSFlags;

  bool JSTracerEnabled() {
    return mJSFlags & uint32_t(JSInstrumentationFlags::TraceLogging);
  }

  bool JSAllocationsEnabled() {
    return mJSFlags & uint32_t(JSInstrumentationFlags::Allocations);
  }
};

#endif  // RegisteredThread_h
