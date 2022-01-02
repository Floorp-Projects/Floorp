/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This header contains classes that hold data related to thread profiling:
// Data members are stored `protected` in `ThreadRegistrationData`.
// Non-virtual sub-classes of ProfilerThreadRegistrationData provide layers of
// public accessors to subsets of the data. Each level builds on the previous
// one and adds further access to more data, but always with the appropriate
// guards where necessary.
// These classes have protected constructors, so only some trusted classes
// `ThreadRegistration` and `ThreadRegistry` will be able to construct them, and
// then give limited access depending on who asks (the owning thread or another
// one), and how much data they actually need.
//
// The hierarchy is, from base to most derived:
// - ThreadRegistrationData
// - ThreadRegistrationUnlockedConstReader
// - ThreadRegistrationUnlockedConstReaderAndAtomicRW
// - ThreadRegistrationUnlockedRWForLockedProfiler
// - ThreadRegistrationUnlockedReaderAndAtomicRWOnThread
// - ThreadRegistrationLockedRWFromAnyThread
// - ThreadRegistrationLockedRWOnThread
// - ThreadRegistration::EmbeddedData (actual data member in ThreadRegistration)
//
// Tech detail: These classes need to be a single hierarchy so that
// `ThreadRegistration` can contain the most-derived class, and from there can
// publish references to base classes without relying on Undefined Behavior.
// (It's not allowed to have some object and give a reference to a sub-class,
// unless that object was *really* constructed as that sub-class at least, even
// if that sub-class only adds member functions!)
// And where appropriate, these references will come along with the required
// lock.

#ifndef ProfilerThreadRegistrationData_h
#define ProfilerThreadRegistrationData_h

#include "js/ProfilingFrameIterator.h"
#include "js/ProfilingStack.h"
#include "mozilla/Atomics.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ProfilerThreadPlatformData.h"
#include "mozilla/ProfilerThreadRegistrationInfo.h"
#include "nsCOMPtr.h"
#include "nsIThread.h"

class ProfiledThreadData;
class PSAutoLock;
struct JSContext;

// Enum listing which profiling features are active for a single thread.
enum class ThreadProfilingFeatures : uint32_t {
  // The thread is not being profiled at all (either the profiler is not
  // running, or this thread is not examined during profiling.)
  NotProfiled = 0u,

  // Single features, binary exclusive. May be `Combine()`d.
  CPUUtilization = 1u << 0,
  Sampling = 1u << 1,
  Markers = 1u << 2,

  // All possible features. Usually used as a mask to see if any feature is
  // active at a given time.
  Any = CPUUtilization | Sampling | Markers
};

// Binary OR of one of more ThreadProfilingFeatures, to mix all arguments.
template <typename... Ts>
[[nodiscard]] constexpr ThreadProfilingFeatures Combine(
    ThreadProfilingFeatures a1, Ts... as) {
  static_assert((true && ... &&
                 std::is_same_v<std::remove_cv_t<std::remove_reference_t<Ts>>,
                                ThreadProfilingFeatures>));
  return static_cast<ThreadProfilingFeatures>(
      (static_cast<std::underlying_type_t<ThreadProfilingFeatures>>(a1) | ... |
       static_cast<std::underlying_type_t<ThreadProfilingFeatures>>(as)));
}

// Binary AND of one of more ThreadProfilingFeatures, to find features common to
// all arguments.
template <typename... Ts>
[[nodiscard]] constexpr ThreadProfilingFeatures Intersect(
    ThreadProfilingFeatures a1, Ts... as) {
  static_assert((true && ... &&
                 std::is_same_v<std::remove_cv_t<std::remove_reference_t<Ts>>,
                                ThreadProfilingFeatures>));
  return static_cast<ThreadProfilingFeatures>(
      (static_cast<std::underlying_type_t<ThreadProfilingFeatures>>(a1) & ... &
       static_cast<std::underlying_type_t<ThreadProfilingFeatures>>(as)));
}

// Are there features in common between the two given sets?
// Mostly useful to test if any of a set of features is present in another set.
template <typename... Ts>
[[nodiscard]] constexpr bool DoFeaturesIntersect(ThreadProfilingFeatures a1,
                                                 ThreadProfilingFeatures a2) {
  return Intersect(a1, a2) != ThreadProfilingFeatures::NotProfiled;
}

namespace mozilla::profiler {

// All data members related to thread profiling are stored here.
// See derived classes below, which give limited unlocked/locked read/write
// access in different situations, and will be available through
// ThreadRegistration and ThreadRegistry.
class ThreadRegistrationData {
 public:
  // No public accessors here. See derived classes for accessors, and
  // Get.../With... functions for who can use these accessors.

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    // Not including data that is not fully owned here.
    return 0;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  static constexpr size_t MAX_JS_FRAMES = 1024;
  using JsFrame = JS::ProfilingFrameIterator::Frame;
  using JsFrameBuffer = JsFrame[MAX_JS_FRAMES];

  // `protected` to allow derived classes to read all data members.
 protected:
  ThreadRegistrationData(const char* aName, const void* aStackTop);

#ifdef DEBUG
  // Destructor only used to check invariants.
  ~ThreadRegistrationData() {
    MOZ_ASSERT((mProfilingFeatures != ThreadProfilingFeatures::NotProfiled) ==
               !!mProfiledThreadData);
    MOZ_ASSERT(!mProfiledThreadData,
               "mProfiledThreadData pointer should have been reset before "
               "~ThreadRegistrationData");
  }
#endif  // DEBUG

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

  // If mJSContext is not null AND the thread is being profiled, this points at
  // the start of a JsFrameBuffer to be used for on-thread synchronous sampling.
  JsFrame* mJsFrameBuffer = nullptr;

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

  // Is this thread currently being profiled, and with which features?
  // Written from profiler, read from any thread.
  // Invariant: `!!mProfilingFeatures == !!mProfiledThreadData` (set together.)
  Atomic<ThreadProfilingFeatures, MemoryOrdering::Relaxed> mProfilingFeatures{
      ThreadProfilingFeatures::NotProfiled};

  // If the profiler is active and this thread is selected for profiling, this
  // points at the relevant ProfiledThreadData.
  // Fully controlled by the profiler.
  // Invariant: `!!mProfilingFeatures == !!mProfiledThreadData` (set together).
  ProfiledThreadData* mProfiledThreadData = nullptr;
};

// Accessing const data from any thread.
class ThreadRegistrationUnlockedConstReader : public ThreadRegistrationData {
 public:
  [[nodiscard]] const ThreadRegistrationInfo& Info() const { return mInfo; }

  [[nodiscard]] const PlatformData& PlatformDataCRef() const {
    return mPlatformData;
  }

  [[nodiscard]] const void* StackTop() const { return mStackTop; }

 protected:
  ThreadRegistrationUnlockedConstReader(const char* aName,
                                        const void* aStackTop)
      : ThreadRegistrationData(aName, aStackTop) {}
};

// Accessing atomic data from any thread.
class ThreadRegistrationUnlockedConstReaderAndAtomicRW
    : public ThreadRegistrationUnlockedConstReader {
 public:
  [[nodiscard]] const ProfilingStack& ProfilingStackCRef() const {
    return mProfilingStack;
  }
  [[nodiscard]] ProfilingStack& ProfilingStackRef() { return mProfilingStack; }

  // Similar to `profiler_is_active()`, this atomic flag may become out-of-date.
  // It should only be used as an indication to know whether this thread is
  // probably being profiled (with some specific features), to avoid doing
  // expensive operations otherwise. Edge cases:
  // - This thread could get `NotProfiled`, but the profiler has just started,
  //   so some very early data may be missing. No real impact on profiling.
  // - This thread could see profiled features, but the profiled has just
  //   stopped, so some some work will be done and then discarded when finally
  //   attempting to write to the buffer. No impact on profiling.
  // - This thread could see profiled features, but the profiler will quickly
  //   stop and restart, so this thread will write information relevant to the
  //   previous profiling session. Very rare, and little impact on profiling.
  [[nodiscard]] ThreadProfilingFeatures ProfilingFeatures() const {
    return mProfilingFeatures;
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

  // This is called on every profiler restart. Put things that should happen
  // at that time here.
  void ReinitializeOnResume() {
    // This is needed to cause an initial sample to be taken from sleeping
    // threads that had been observed prior to the profiler stopping and
    // restarting. Otherwise sleeping threads would not have any samples to
    // copy forward while sleeping.
    (void)mSleep.compareExchange(SLEEPING_OBSERVED, SLEEPING_NOT_OBSERVED);
  }

  // This returns true for the second and subsequent calls in each sleep
  // cycle, so that the sampler can skip its full sampling and reuse the first
  // asleep sample instead.
  [[nodiscard]] bool CanDuplicateLastSampleDueToSleep() {
    if (mSleep == AWAKE) {
      return false;
    }
    if (mSleep.compareExchange(SLEEPING_NOT_OBSERVED, SLEEPING_OBSERVED)) {
      return false;
    }
    return true;
  }

  [[nodiscard]] bool IsSleeping() const { return mSleep != AWAKE; }

 protected:
  ThreadRegistrationUnlockedConstReaderAndAtomicRW(const char* aName,
                                                   const void* aStackTop)
      : ThreadRegistrationUnlockedConstReader(aName, aStackTop) {}
};

// Like above, with special PSAutoLock-guarded accessors.
class ThreadRegistrationUnlockedRWForLockedProfiler
    : public ThreadRegistrationUnlockedConstReaderAndAtomicRW {
 public:
  // IMPORTANT! IMPORTANT! IMPORTANT! IMPORTANT! IMPORTANT! IMPORTANT!
  // Only add functions that take a `const PSAutoLock&` proof-of-lock.
  // (Because there is no other lock.)

  [[nodiscard]] const ProfiledThreadData* GetProfiledThreadData(
      const PSAutoLock&) const {
    return mProfiledThreadData;
  }

  [[nodiscard]] ProfiledThreadData* GetProfiledThreadData(const PSAutoLock&) {
    return mProfiledThreadData;
  }

 protected:
  ThreadRegistrationUnlockedRWForLockedProfiler(const char* aName,
                                                const void* aStackTop)
      : ThreadRegistrationUnlockedConstReaderAndAtomicRW(aName, aStackTop) {}
};

// Reading data, unlocked from the thread, or locked otherwise.
// This data MUST only be written from the thread with lock (i.e., in
// LockedRWOnThread through RWOnThreadWithLock.)
class ThreadRegistrationUnlockedReaderAndAtomicRWOnThread
    : public ThreadRegistrationUnlockedRWForLockedProfiler {
 public:
  // IMPORTANT! IMPORTANT! IMPORTANT! IMPORTANT! IMPORTANT! IMPORTANT!
  // Non-atomic members read here MUST be written from LockedRWOnThread (to
  // guarantee that they are only modified on this thread.)

  [[nodiscard]] JSContext* GetJSContext() const { return mJSContext; }

 protected:
  ThreadRegistrationUnlockedReaderAndAtomicRWOnThread(const char* aName,
                                                      const void* aStackTop)
      : ThreadRegistrationUnlockedRWForLockedProfiler(aName, aStackTop) {}
};

// Accessing locked data from the thread, or from any thread through the locked
// profiler:

// Like above, and profiler can also read&write mutex-protected members.
class ThreadRegistrationLockedRWFromAnyThread
    : public ThreadRegistrationUnlockedReaderAndAtomicRWOnThread {
 public:
  void SetProfilingFeaturesAndData(ThreadProfilingFeatures aProfilingFeatures,
                                   ProfiledThreadData* aProfiledThreadData,
                                   const PSAutoLock&);
  void ClearProfilingFeaturesAndData(const PSAutoLock&);

  // Not null when JSContext is not null AND this thread is being profiled.
  // Points at the start of JsFrameBuffer.
  [[nodiscard]] JsFrame* GetJsFrameBuffer() const { return mJsFrameBuffer; }

  [[nodiscard]] const nsCOMPtr<nsIEventTarget> GetEventTarget() const {
    return mThread;
  }

  void ResetMainThread(nsIThread* aThread) { mThread = aThread; }

  // aDelay is the time the event that is currently running on the thread was
  // queued before starting to run (if a PrioritizedEventQueue
  // (i.e. MainThread), this will be 0 for any event at a lower priority
  // than Input).
  // aRunning is the time the event has been running. If no event is running
  // these will both be TimeDuration() (i.e. 0). Both are out params, and are
  // always set. Their initial value is discarded.
  void GetRunningEventDelay(const TimeStamp& aNow, TimeDuration& aDelay,
                            TimeDuration& aRunning) {
    if (mThread) {  // can be null right at the start of a process
      TimeStamp start;
      mThread->GetRunningEventDelay(&aDelay, &start);
      if (!start.IsNull()) {
        // Note: the timestamp used here will be from when we started to
        // suspend and sample the thread; which is also the timestamp
        // associated with the sample.
        aRunning = aNow - start;
        return;
      }
    }
    aDelay = TimeDuration();
    aRunning = TimeDuration();
  }

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

  // Request that this thread stop JS sampling. JS sampling won't actually
  // stop until a subsequent PollJSSampling() call occurs.
  void StopJSSampling() {
    // This function runs on-thread or off-thread.

    MOZ_RELEASE_ASSERT(mJSSampling == ACTIVE ||
                       mJSSampling == ACTIVE_REQUESTED);
    mJSSampling = INACTIVE_REQUESTED;
  }

 protected:
  ThreadRegistrationLockedRWFromAnyThread(const char* aName,
                                          const void* aStackTop)
      : ThreadRegistrationUnlockedReaderAndAtomicRWOnThread(aName, aStackTop) {}
};

// Accessing data, locked, from the thread.
// If any non-atomic data is readable from UnlockedReaderAndAtomicRWOnThread,
// it must be written from here, and not in base classes: Since this data is
// only written on the thread, it can be read from the same thread without
// lock; but writing must be locked so that other threads can safely read it,
// typically from LockedRWFromAnyThread.
class ThreadRegistrationLockedRWOnThread
    : public ThreadRegistrationLockedRWFromAnyThread {
 public:
  void SetJSContext(JSContext* aJSContext);
  void ClearJSContext();

  // Poll to see if JS sampling should be started/stopped.
  void PollJSSampling();

 public:
  ThreadRegistrationLockedRWOnThread(const char* aName, const void* aStackTop)
      : ThreadRegistrationLockedRWFromAnyThread(aName, aStackTop) {}
};

}  // namespace mozilla::profiler

#endif  // ProfilerThreadRegistrationData_h
