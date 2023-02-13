/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RegisteredThread_h
#define RegisteredThread_h

#include "platform.h"
#include "ThreadInfo.h"

#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace baseprofiler {

// This class contains the state for a single thread that is accessible without
// protection from gPSMutex in platform.cpp. Because there is no external
// protection against data races, it must provide internal protection. Hence
// the "Racy" prefix.
//
class RacyRegisteredThread final {
 public:
  explicit RacyRegisteredThread(BaseProfilerThreadId aThreadId)
      : mThreadId(aThreadId), mSleep(AWAKE), mIsBeingProfiled(false) {}

  ~RacyRegisteredThread() {}

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

  BaseProfilerThreadId ThreadId() const { return mThreadId; }

  class ProfilingStack& ProfilingStack() { return mProfilingStack; }
  const class ProfilingStack& ProfilingStack() const { return mProfilingStack; }

 private:
  class ProfilingStack mProfilingStack;

  // mThreadId contains the thread ID of the current thread. It is safe to read
  // this from multiple threads concurrently, as it will never be mutated.
  const BaseProfilerThreadId mThreadId;

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
  Atomic<int> mSleep;

  // Is this thread being profiled? (e.g., should markers be recorded?)
  Atomic<bool, MemoryOrdering::Relaxed> mIsBeingProfiled;
};

// This class contains information that's relevant to a single thread only
// while that thread is running and registered with the profiler, but
// regardless of whether the profiler is running. All accesses to it are
// protected by the profiler state lock.
class RegisteredThread final {
 public:
  RegisteredThread(ThreadInfo* aInfo, void* aStackTop);
  ~RegisteredThread();

  class RacyRegisteredThread& RacyRegisteredThread() {
    return mRacyRegisteredThread;
  }
  const class RacyRegisteredThread& RacyRegisteredThread() const {
    return mRacyRegisteredThread;
  }

  PlatformData* GetPlatformData() const { return mPlatformData.get(); }
  const void* StackTop() const { return mStackTop; }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  const RefPtr<ThreadInfo> Info() const { return mThreadInfo; }

 private:
  class RacyRegisteredThread mRacyRegisteredThread;

  const UniquePlatformData mPlatformData;
  const void* mStackTop;

  const RefPtr<ThreadInfo> mThreadInfo;
};

}  // namespace baseprofiler
}  // namespace mozilla

#endif  // RegisteredThread_h
