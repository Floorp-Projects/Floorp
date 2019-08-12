/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RegisteredThread_h
#define RegisteredThread_h

#include "platform.h"
#include "ProfilerMarker.h"
#include "ProfilerMarkerPayload.h"
#include "ThreadInfo.h"

#include "js/TraceLoggerAPI.h"
#include "jsapi.h"
#include "mozilla/UniquePtr.h"
#include "nsIEventTarget.h"

// This class contains the state for a single thread that is accessible without
// protection from gPSMutex in platform.cpp. Because there is no external
// protection against data races, it must provide internal protection. Hence
// the "Racy" prefix.
//
class RacyRegisteredThread final {
 public:
  explicit RacyRegisteredThread(int aThreadId)
      : mThreadId(aThreadId), mSleep(AWAKE), mIsBeingProfiled(false) {
    MOZ_COUNT_CTOR(RacyRegisteredThread);
  }

  ~RacyRegisteredThread() { MOZ_COUNT_DTOR(RacyRegisteredThread); }

  void SetIsBeingProfiled(bool aIsBeingProfiled) {
    mIsBeingProfiled = aIsBeingProfiled;
  }

  bool IsBeingProfiled() const { return mIsBeingProfiled; }

  void AddPendingMarker(const char* aMarkerName,
                        JS::ProfilingCategoryPair aCategoryPair,
                        mozilla::UniquePtr<ProfilerMarkerPayload> aPayload,
                        double aTime) {
    // Note: We don't assert on mIsBeingProfiled, because it could have changed
    // between the check in the caller and now.
    ProfilerMarker* marker = new ProfilerMarker(
        aMarkerName, aCategoryPair, mThreadId, std::move(aPayload), aTime);
    mPendingMarkers.insert(marker);
  }

  // Called within signal. Function must be reentrant.
  ProfilerMarkerLinkedList* GetPendingMarkers() {
    // The profiled thread is interrupted, so we can access the list safely.
    // Unless the profiled thread was in the middle of changing the list when
    // we interrupted it - in that case, accessList() will return null.
    return mPendingMarkers.accessList();
  }

  // This is called on every profiler restart. Put things that should happen at
  // that time here.
  void ReinitializeOnResume() {
    mPendingMarkers.reset();

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

  int ThreadId() const { return mThreadId; }

  class ProfilingStack& ProfilingStack() {
    return mProfilingStack;
  }
  const class ProfilingStack& ProfilingStack() const { return mProfilingStack; }

 private:
  class ProfilingStack mProfilingStack;

  // A list of pending markers that must be moved to the circular buffer.
  ProfilerSignalSafeLinkedList<ProfilerMarker> mPendingMarkers;

  // mThreadId contains the thread ID of the current thread. It is safe to read
  // this from multiple threads concurrently, as it will never be mutated.
  const int mThreadId;

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
  // Accesses to this atomic are not recorded by web replay as they may occur
  // at non-deterministic points.
  mozilla::Atomic<bool, mozilla::MemoryOrdering::Relaxed,
                  mozilla::recordreplay::Behavior::DontPreserve>
      mIsBeingProfiled;
};

// This class contains information that's relevant to a single thread only
// while that thread is running and registered with the profiler, but
// regardless of whether the profiler is running. All accesses to it are
// protected by the profiler state lock.
class RegisteredThread final {
 public:
  RegisteredThread(ThreadInfo* aInfo, nsIEventTarget* aThread, void* aStackTop);
  ~RegisteredThread();

  class RacyRegisteredThread& RacyRegisteredThread() {
    return mRacyRegisteredThread;
  }
  const class RacyRegisteredThread& RacyRegisteredThread() const {
    return mRacyRegisteredThread;
  }

  PlatformData* GetPlatformData() const { return mPlatformData.get(); }
  const void* StackTop() const { return mStackTop; }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  // Set the JSContext of the thread to be sampled. Sampling cannot begin until
  // this has been set.
  void SetJSContext(JSContext* aContext) {
    // This function runs on-thread.

    MOZ_ASSERT(aContext && !mContext);

    mContext = aContext;

    // We give the JS engine a non-owning reference to the ProfilingStack. It's
    // important that the JS engine doesn't touch this once the thread dies.
    js::SetContextProfilingStack(aContext,
                                 &RacyRegisteredThread().ProfilingStack());
  }

  void ClearJSContext() {
    // This function runs on-thread.
    mContext = nullptr;
  }

  JSContext* GetJSContext() const { return mContext; }

  const RefPtr<ThreadInfo> Info() const { return mThreadInfo; }
  const nsCOMPtr<nsIEventTarget> GetEventTarget() const { return mThread; }

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
  void PollJSSampling() {
    // This function runs on-thread.

    // We can't start/stop profiling until we have the thread's JSContext.
    if (mContext) {
      // It is possible for mJSSampling to go through the following sequences.
      //
      // - INACTIVE, ACTIVE_REQUESTED, INACTIVE_REQUESTED, INACTIVE
      //
      // - ACTIVE, INACTIVE_REQUESTED, ACTIVE_REQUESTED, ACTIVE
      //
      // Therefore, the if and else branches here aren't always interleaved.
      // This is ok because the JS engine can handle that.
      //
      if (mJSSampling == ACTIVE_REQUESTED) {
        mJSSampling = ACTIVE;
        js::EnableContextProfilingStack(mContext, true);
        JS_SetGlobalJitCompilerOption(mContext,
                                      JSJITCOMPILER_TRACK_OPTIMIZATIONS,
                                      TrackOptimizationsEnabled());
        if (JSTracerEnabled()) {
          JS::StartTraceLogger(mContext);
        }
        if (JSAllocationsEnabled()) {
          // TODO - This probability should not be hardcoded. See Bug 1547284.
          JS::EnableRecordingAllocations(
              mContext, profiler_add_js_allocation_marker, 0.01);
        }
        js::RegisterContextProfilingEventMarker(mContext,
                                                profiler_add_js_marker);

      } else if (mJSSampling == INACTIVE_REQUESTED) {
        mJSSampling = INACTIVE;
        js::EnableContextProfilingStack(mContext, false);
        if (JSTracerEnabled()) {
          JS::StopTraceLogger(mContext);
        }
        if (JSAllocationsEnabled()) {
          JS::DisableRecordingAllocations(mContext);
        }
      }
    }
  }

 private:
  class RacyRegisteredThread mRacyRegisteredThread;

  const UniquePlatformData mPlatformData;
  const void* mStackTop;

  const RefPtr<ThreadInfo> mThreadInfo;
  const nsCOMPtr<nsIEventTarget> mThread;

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

  bool TrackOptimizationsEnabled() {
    return mJSFlags & uint32_t(JSInstrumentationFlags::TrackOptimizations);
  }

  bool JSTracerEnabled() {
    return mJSFlags & uint32_t(JSInstrumentationFlags::TraceLogging);
  }

  bool JSAllocationsEnabled() {
    return mJSFlags & uint32_t(JSInstrumentationFlags::Allocations);
  }
};

#endif  // RegisteredThread_h
