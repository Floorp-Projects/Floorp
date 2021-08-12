/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerThreadRegistrationData_h
#define ProfilerThreadRegistrationData_h

#include "js/ProfilingStack.h"
#include "mozilla/Atomics.h"
#include "mozilla/ProfilerThreadPlatformData.h"
#include "mozilla/ProfilerThreadRegistrationInfo.h"
#include "nsCOMPtr.h"
#include "nsIThread.h"

struct JSContext;

namespace mozilla::profiler {

// All data members related to thread profiling are stored here.
// See derived classes below, which give limited unlocked/locked read/write
// access in different situations, and will be available through
// ThreadRegistration and ThreadRegistry.
class ThreadRegistrationData {
 public:
  // No public accessors here. See derived classes for accessors, and
  // Get.../With... functions for who can uses these accessors.

  // `protected` to allow derived classes to read all data members.
 protected:
  ThreadRegistrationData(const char* aName, const void* aStackTop);

  // Permanent thread information.
  // Set at construction, read from anywhere, moved-from at destruction.
  ThreadRegistrationInfo mInfo;

  // Contains profiler labels and JS frames.
  // Deep-written on thread only, deep-read from thread and suspended thread.
  ProfilingStack mProfilingStack;

  // In practice, only read from thread and suspended thread.
  PlatformData mPlatformData;

  // Only read from thread and suspended thread.
  const void* const mStackTop;

  // Written from thread, read from thread and suspended thread.
  nsCOMPtr<nsIThread> mThread;

  // If this is a JS thread, this is its JSContext, which is required for any
  // JS sampling.
  // Written from thread, read from thread and suspended thread.
  JSContext* mJSContext = nullptr;

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
  } mJSSampling = INACTIVE;

  uint32_t mJSFlags = 0;

  // Flags to conveniently track various JS instrumentations.
  enum class JSInstrumentationFlags {
    StackSampling = 0x1,
    TraceLogging = 0x2,
    Allocations = 0x4,
  };

  [[nodiscard]] bool JSTracerEnabled() const {
    return mJSFlags & uint32_t(JSInstrumentationFlags::TraceLogging);
  }

  [[nodiscard]] bool JSAllocationsEnabled() const {
    return mJSFlags & uint32_t(JSInstrumentationFlags::Allocations);
  }

  // The following members may be modified from another thread.
  // They need to be atomic, because LockData() does not prevent reads from
  // the owning thread.

  // Is this thread being profiled? (e.g., should markers be recorded?)
  // Written from profiler, read from thread.
  Atomic<bool, MemoryOrdering::Relaxed> mIsBeingProfiled{false};

  // mSleep tracks whether the thread is sleeping, and if so, whether it has
  // been previously observed. This is used for an optimization: in some
  // cases, when a thread is asleep, we duplicate the previous sample, which
  // is cheaper than taking a new sample.
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
  // Read&written from thread and suspended thread.
  Atomic<int> mSleep{AWAKE};
};

}  // namespace mozilla::profiler

#endif  // ProfilerThreadRegistrationData_h
