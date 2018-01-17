/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ThreadInfo_h
#define ThreadInfo_h

#include "mozilla/NotNull.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtrExtensions.h"

#include "platform.h"
#include "ProfileBuffer.h"
#include "js/ProfilingStack.h"

// This class contains the info for a single thread that is accessible without
// protection from gPSMutex in platform.cpp. Because there is no external
// protection against data races, it must provide internal protection. Hence
// the "Racy" prefix.
//
class RacyThreadInfo final : public PseudoStack
{
public:
  explicit RacyThreadInfo(int aThreadId)
    : PseudoStack()
    , mThreadId(aThreadId)
    , mSleep(AWAKE)
  {
    MOZ_COUNT_CTOR(RacyThreadInfo);
  }

  ~RacyThreadInfo()
  {
    MOZ_COUNT_DTOR(RacyThreadInfo);
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    size_t n = aMallocSizeOf(this);

    // Measurement of the following members may be added later if DMD finds it
    // is worthwhile:
    // - things in the PseudoStack
    // - mPendingMarkers
    //
    // If these measurements are added, the code must be careful to avoid data
    // races. (The current code doesn't have any race issues because the
    // contents of the PseudoStack object aren't accessed; |this| is used only
    // as an address for lookup by aMallocSizeof).

    return n;
  }

  void AddPendingMarker(const char* aMarkerName,
                        mozilla::UniquePtr<ProfilerMarkerPayload> aPayload,
                        double aTime)
  {
    ProfilerMarker* marker =
      new ProfilerMarker(aMarkerName, mThreadId, Move(aPayload), aTime);
    mPendingMarkers.insert(marker);
  }

  // Called within signal. Function must be reentrant.
  ProfilerMarkerLinkedList* GetPendingMarkers()
  {
    // The profiled thread is interrupted, so we can access the list safely.
    // Unless the profiled thread was in the middle of changing the list when
    // we interrupted it - in that case, accessList() will return null.
    return mPendingMarkers.accessList();
  }

  // This is called on every profiler restart. Put things that should happen at
  // that time here.
  void ReinitializeOnResume()
  {
    // This is needed to cause an initial sample to be taken from sleeping
    // threads that had been observed prior to the profiler stopping and
    // restarting. Otherwise sleeping threads would not have any samples to
    // copy forward while sleeping.
    (void)mSleep.compareExchange(SLEEPING_OBSERVED, SLEEPING_NOT_OBSERVED);
  }

  // This returns true for the second and subsequent calls in each sleep cycle.
  bool CanDuplicateLastSampleDueToSleep()
  {
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
  void SetSleeping()
  {
    MOZ_ASSERT(mSleep == AWAKE);
    mSleep = SLEEPING_NOT_OBSERVED;
  }

  // Call this whenever the current thread wakes. Calling it twice in a row
  // without an intervening setSleeping() call is an error.
  void SetAwake()
  {
    MOZ_ASSERT(mSleep != AWAKE);
    mSleep = AWAKE;
  }

  bool IsSleeping() { return mSleep != AWAKE; }

  int ThreadId() const { return mThreadId; }

private:
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
};

// This class contains the info for a single thread.
//
// Note: A thread's ThreadInfo can be held onto after the thread itself exits,
// because we may need to output profiling information about that thread. But
// some of the fields in this class are only relevant while the thread is
// alive. It's possible that this class could be refactored so there is a
// clearer split between those fields and the fields that are still relevant
// after the thread exists.
class ThreadInfo final
{
public:
  ThreadInfo(const char* aName, int aThreadId, bool aIsMainThread,
             void* aStackTop);

  ~ThreadInfo();

  const char* Name() const { return mName.get(); }

  // This is a safe read even when the target thread is not blocked, as this
  // thread id is never mutated.
  int ThreadId() const { return RacyInfo()->ThreadId(); }

  bool IsMainThread() const { return mIsMainThread; }

  mozilla::NotNull<RacyThreadInfo*> RacyInfo() const { return mRacyInfo; }

  void StartProfiling();
  void StopProfiling();
  bool IsBeingProfiled() { return mIsBeingProfiled; }

  void NotifyUnregistered() { mUnregisterTime = TimeStamp::Now(); }

  PlatformData* GetPlatformData() const { return mPlatformData.get(); }
  void* StackTop() const { return mStackTop; }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  ProfileBuffer::LastSample& LastSample() { return mLastSample; }

private:
  mozilla::UniqueFreePtr<char> mName;
  mozilla::TimeStamp mRegisterTime;
  mozilla::TimeStamp mUnregisterTime;
  const bool mIsMainThread;

  // The thread's RacyThreadInfo. This is an owning pointer. It could be an
  // inline member, but we don't do that because RacyThreadInfo is quite large
  // (due to the PseudoStack within it), and we have ThreadInfo vectors and so
  // we'd end up wasting a lot of space in those vectors for excess elements.
  mozilla::NotNull<RacyThreadInfo*> mRacyInfo;

  UniquePlatformData mPlatformData;
  void* mStackTop;

  //
  // The following code is only used for threads that are being profiled, i.e.
  // for which IsBeingProfiled() returns true.
  //

public:
  // Returns the time of the first sample.
  double StreamJSON(const ProfileBuffer& aBuffer, SpliceableJSONWriter& aWriter,
                    const mozilla::TimeStamp& aProcessStartTime,
                    double aSinceTime);

  // Call this method when the JS entries inside the buffer are about to
  // become invalid, i.e., just before JS shutdown.
  void FlushSamplesAndMarkers(const mozilla::TimeStamp& aProcessStartTime,
                              ProfileBuffer& aBuffer);

  // Returns nullptr if this is not the main thread or if this thread is not
  // being profiled.
  ThreadResponsiveness* GetThreadResponsiveness()
  {
    ThreadResponsiveness* responsiveness = mResponsiveness.ptrOr(nullptr);
    MOZ_ASSERT(!!responsiveness == (mIsMainThread && mIsBeingProfiled));
    return responsiveness;
  }

  // Set the JSContext of the thread to be sampled. Sampling cannot begin until
  // this has been set.
  void SetJSContext(JSContext* aContext)
  {
    // This function runs on-thread.

    MOZ_ASSERT(aContext && !mContext);

    mContext = aContext;

    // We give the JS engine a non-owning reference to the RacyInfo (just the
    // PseudoStack, really). It's important that the JS engine doesn't touch
    // this once the thread dies.
    js::SetContextProfilingStack(aContext, RacyInfo());

    PollJSSampling();
  }

  // Request that this thread start JS sampling. JS sampling won't actually
  // start until a subsequent PollJSSampling() call occurs *and* mContext has
  // been set.
  void StartJSSampling()
  {
    // This function runs on-thread or off-thread.

    MOZ_RELEASE_ASSERT(mJSSampling == INACTIVE ||
                       mJSSampling == INACTIVE_REQUESTED);
    mJSSampling = ACTIVE_REQUESTED;
  }

  // Request that this thread stop JS sampling. JS sampling won't actually stop
  // until a subsequent PollJSSampling() call occurs.
  void StopJSSampling()
  {
    // This function runs on-thread or off-thread.

    MOZ_RELEASE_ASSERT(mJSSampling == ACTIVE ||
                       mJSSampling == ACTIVE_REQUESTED);
    mJSSampling = INACTIVE_REQUESTED;
  }

  // Poll to see if JS sampling should be started/stopped.
  void PollJSSampling()
  {
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
        js::RegisterContextProfilingEventMarker(mContext, profiler_add_marker);

      } else if (mJSSampling == INACTIVE_REQUESTED) {
        mJSSampling = INACTIVE;
        js::EnableContextProfilingStack(mContext, false);
      }
    }
  }

private:
  bool mIsBeingProfiled;

  // JS frames in the buffer may require a live JSRuntime to stream (e.g.,
  // stringifying JIT frames). In the case of JSRuntime destruction,
  // FlushSamplesAndMarkers should be called to save them. These are spliced
  // into the final stream.
  mozilla::UniquePtr<char[]> mSavedStreamedSamples;
  double mFirstSavedStreamedSampleTime;
  mozilla::UniquePtr<char[]> mSavedStreamedMarkers;
  mozilla::Maybe<UniqueStacks> mUniqueStacks;

  // This is only used for the main thread.
  mozilla::Maybe<ThreadResponsiveness> mResponsiveness;

public:
  // If this is a JS thread, this is its JSContext, which is required for any
  // JS sampling.
  JSContext* mContext;

private:
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

  // When sampling, this holds the generation number and offset in
  // ActivePS::mBuffer of the most recent sample for this thread.
  ProfileBuffer::LastSample mLastSample;
};

void
StreamSamplesAndMarkers(const char* aName, int aThreadId,
                        const ProfileBuffer& aBuffer,
                        SpliceableJSONWriter& aWriter,
                        const mozilla::TimeStamp& aProcessStartTime,
                        const TimeStamp& aRegisterTime,
                        const TimeStamp& aUnregisterTime,
                        double aSinceTime,
                        double* aOutFirstSampleTime,
                        JSContext* aContext,
                        char* aSavedStreamedSamples,
                        double aFirstSavedStreamedSampleTime,
                        char* aSavedStreamedMarkers,
                        UniqueStacks& aUniqueStacks);

#endif  // ThreadInfo_h
