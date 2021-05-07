/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The Gecko Profiler is an always-on profiler that takes fast and low overhead
// samples of the program execution using only userspace functionality for
// portability. The goal of this module is to provide performance data in a
// generic cross-platform way without requiring custom tools or kernel support.
//
// Samples are collected to form a timeline with optional timeline event
// (markers) used for filtering. The samples include both native stacks and
// platform-independent "label stack" frames.

#ifndef BaseProfiler_h
#define BaseProfiler_h

// This file is safe to include unconditionally, and only defines
// empty macros if MOZ_GECKO_PROFILER is not set.

// These headers are also safe to include unconditionally, with empty macros if
// MOZ_GECKO_PROFILER is not set.
// If your file only uses particular APIs (e.g., only markers), please consider
// including only the needed headers instead of this one, to reduce compilation
// dependencies.
#include "mozilla/BaseProfilerCounts.h"
#include "mozilla/BaseProfilerLabels.h"
#include "mozilla/BaseProfilerMarkers.h"
#include "mozilla/BaseProfilerState.h"

#ifndef MOZ_GECKO_PROFILER

#  include "mozilla/UniquePtr.h"

// This file can be #included unconditionally. However, everything within this
// file must be guarded by a #ifdef MOZ_GECKO_PROFILER, *except* for the
// following macros and functions, which encapsulate the most common operations
// and thus avoid the need for many #ifdefs.

#  define AUTO_BASE_PROFILER_INIT

#  define BASE_PROFILER_REGISTER_THREAD(name)
#  define BASE_PROFILER_UNREGISTER_THREAD()
#  define AUTO_BASE_PROFILER_REGISTER_THREAD(name)

#  define AUTO_BASE_PROFILER_THREAD_SLEEP
#  define AUTO_BASE_PROFILER_THREAD_WAKE

// Function stubs for when MOZ_GECKO_PROFILER is not defined.

namespace mozilla {
// This won't be used, it's just there to allow the empty definition of
// `profiler_capture_backtrace`.
class ProfileChunkedBuffer {};

namespace baseprofiler {
// This won't be used, it's just there to allow the empty definition of
// `profiler_get_backtrace`.
struct ProfilerBacktrace {};
using UniqueProfilerBacktrace = UniquePtr<ProfilerBacktrace>;

// Get/Capture-backtrace functions can return nullptr or false, the result
// should be fed to another empty macro or stub anyway.

static inline UniqueProfilerBacktrace profiler_get_backtrace() {
  return nullptr;
}

static inline bool profiler_capture_backtrace_into(
    ProfileChunkedBuffer& aChunkedBuffer, StackCaptureOptions aCaptureOptions) {
  return false;
}

static inline UniquePtr<ProfileChunkedBuffer> profiler_capture_backtrace() {
  return nullptr;
}
}  // namespace baseprofiler
}  // namespace mozilla

#else  // !MOZ_GECKO_PROFILER

#  include "BaseProfilingStack.h"

#  include "mozilla/Assertions.h"
#  include "mozilla/Atomics.h"
#  include "mozilla/Attributes.h"
#  include "mozilla/Maybe.h"
#  include "mozilla/PowerOfTwo.h"
#  include "mozilla/TimeStamp.h"
#  include "mozilla/UniquePtr.h"

#  include <functional>
#  include <stdint.h>
#  include <string>

namespace mozilla {

class MallocAllocPolicy;
class ProfileChunkedBuffer;
enum class StackCaptureOptions;
template <class T, size_t MinInlineCapacity, class AllocPolicy>
class Vector;

namespace baseprofiler {

class ProfilerBacktrace;
class SpliceableJSONWriter;

//---------------------------------------------------------------------------
// Start and stop the profiler
//---------------------------------------------------------------------------

static constexpr PowerOfTwo32 BASE_PROFILER_DEFAULT_ENTRIES =
#  if !defined(GP_PLAT_arm_android)
    MakePowerOfTwo32<1024 * 1024>();  // 1M entries = 8MB
#  else
    MakePowerOfTwo32<128 * 1024>();  // 128k entries = 1MB
#  endif

// Startup profiling usually need to capture more data, especially on slow
// systems.
static constexpr PowerOfTwo32 BASE_PROFILER_DEFAULT_STARTUP_ENTRIES =
#  if !defined(GP_PLAT_arm_android)
    MakePowerOfTwo32<4 * 1024 * 1024>();  // 4M entries = 32MB
#  else
    MakePowerOfTwo32<256 * 1024>();  // 256k entries = 2MB
#  endif

#  define BASE_PROFILER_DEFAULT_DURATION 20
#  define BASE_PROFILER_DEFAULT_INTERVAL 1

// Initialize the profiler. If MOZ_PROFILER_STARTUP is set the profiler will
// also be started. This call must happen before any other profiler calls
// (except profiler_start(), which will call profiler_init() if it hasn't
// already run).
MFBT_API void profiler_init(void* stackTop);

#  define AUTO_BASE_PROFILER_INIT \
    ::mozilla::baseprofiler::AutoProfilerInit BASE_PROFILER_RAII

// Clean up the profiler module, stopping it if required. This function may
// also save a shutdown profile if requested. No profiler calls should happen
// after this point and all profiling stack labels should have been popped.
MFBT_API void profiler_shutdown();

// Start the profiler -- initializing it first if necessary -- with the
// selected options. Stops and restarts the profiler if it is already active.
// After starting the profiler is "active". The samples will be recorded in a
// circular buffer.
//   "aCapacity" is the maximum number of 8-byte entries in the profiler's
//               circular buffer.
//   "aInterval" the sampling interval, measured in millseconds.
//   "aFeatures" is the feature set. Features unsupported by this
//               platform/configuration are ignored.
//   "aFilters" is the list of thread filters. Threads that do not match any
//              of the filters are not profiled. A filter matches a thread if
//              (a) the thread name contains the filter as a case-insensitive
//                  substring, or
//              (b) the filter is of the form "pid:<n>" where n is the process
//                  id of the process that the thread is running in.
//   "aDuration" is the duration of entries in the profiler's circular buffer.
MFBT_API void profiler_start(PowerOfTwo32 aCapacity, double aInterval,
                             uint32_t aFeatures, const char** aFilters,
                             uint32_t aFilterCount,
                             const Maybe<double>& aDuration = Nothing());

// Stop the profiler and discard the profile without saving it. A no-op if the
// profiler is inactive. After stopping the profiler is "inactive".
MFBT_API void profiler_stop();

// If the profiler is inactive, start it. If it's already active, restart it if
// the requested settings differ from the current settings. Both the check and
// the state change are performed while the profiler state is locked.
// The only difference to profiler_start is that the current buffer contents are
// not discarded if the profiler is already running with the requested settings.
MFBT_API void profiler_ensure_started(
    PowerOfTwo32 aCapacity, double aInterval, uint32_t aFeatures,
    const char** aFilters, uint32_t aFilterCount,
    const Maybe<double>& aDuration = Nothing());

//---------------------------------------------------------------------------
// Control the profiler
//---------------------------------------------------------------------------

// Register/unregister threads with the profiler. Both functions operate the
// same whether the profiler is active or inactive.
#  define BASE_PROFILER_REGISTER_THREAD(name)                             \
    do {                                                                  \
      char stackTop;                                                      \
      ::mozilla::baseprofiler::profiler_register_thread(name, &stackTop); \
    } while (0)
#  define BASE_PROFILER_UNREGISTER_THREAD() \
    ::mozilla::baseprofiler::profiler_unregister_thread()
MFBT_API ProfilingStack* profiler_register_thread(const char* name,
                                                  void* guessStackTop);
MFBT_API void profiler_unregister_thread();

// Registers a DOM Window (the JS global `window`) with the profiler. Each
// Window _roughly_ corresponds to a single document loaded within a
// browsing context. Both the Window Id and Browser Id are recorded to allow
// correlating different Windows loaded within the same tab or frame element.
//
// We register pages for each navigations but we do not register
// history.pushState or history.replaceState since they correspond to the same
// Inner Window ID. When a browsing context is first loaded, the first url
// loaded in it will be about:blank. Because of that, this call keeps the first
// non-about:blank registration of window and discards the previous one.
//
//   "aTabID"                 is the BrowserId of that document belongs to.
//                            That's used to determine the tab of that page.
//   "aInnerWindowID"         is the ID of the `window` global object of that
//                            document.
//   "aUrl"                   is the URL of the page.
//   "aEmbedderInnerWindowID" is the inner window id of embedder. It's used to
//                            determine sub documents of a page.
MFBT_API void profiler_register_page(uint64_t aTabD, uint64_t aInnerWindowID,
                                     const std::string& aUrl,
                                     uint64_t aEmbedderInnerWindowID);
// Unregister page with the profiler.
//
// Take a Inner Window ID and unregister the page entry that has the same ID.
MFBT_API void profiler_unregister_page(uint64_t aRegisteredInnerWindowID);

// Remove all registered and unregistered pages in the profiler.
void profiler_clear_all_pages();

class BaseProfilerCount;
MFBT_API void profiler_add_sampled_counter(BaseProfilerCount* aCounter);
MFBT_API void profiler_remove_sampled_counter(BaseProfilerCount* aCounter);

// Register and unregister a thread within a scope.
#  define AUTO_BASE_PROFILER_REGISTER_THREAD(name) \
    ::mozilla::baseprofiler::AutoProfilerRegisterThread BASE_PROFILER_RAII(name)

// Pause and resume the profiler. No-ops if the profiler is inactive. While
// paused the profile will not take any samples and will not record any data
// into its buffers. The profiler remains fully initialized in this state.
// This feature will keep JavaScript profiling enabled, thus allowing toggling
// the profiler without invalidating the JIT.
MFBT_API void profiler_pause();
MFBT_API void profiler_resume();

// Only pause and resume the periodic sampling loop, including stack sampling,
// counters, and profiling overheads.
MFBT_API void profiler_pause_sampling();
MFBT_API void profiler_resume_sampling();

// These functions tell the profiler that a thread went to sleep so that we can
// avoid sampling it while it's sleeping. Calling profiler_thread_sleep()
// twice without an intervening profiler_thread_wake() is an error. All three
// functions operate the same whether the profiler is active or inactive.
MFBT_API void profiler_thread_sleep();
MFBT_API void profiler_thread_wake();

// Mark a thread as asleep/awake within a scope.
#  define AUTO_BASE_PROFILER_THREAD_SLEEP \
    ::mozilla::baseprofiler::AutoProfilerThreadSleep BASE_PROFILER_RAII
#  define AUTO_BASE_PROFILER_THREAD_WAKE \
    ::mozilla::baseprofiler::AutoProfilerThreadWake BASE_PROFILER_RAII

//---------------------------------------------------------------------------
// Get information from the profiler
//---------------------------------------------------------------------------

// Get the params used to start the profiler. Returns 0 and an empty vector
// (via outparams) if the profile is inactive. It's possible that the features
// returned may be slightly different to those requested due to required
// adjustments.
MFBT_API void profiler_get_start_params(
    int* aEntrySize, Maybe<double>* aDuration, double* aInterval,
    uint32_t* aFeatures, Vector<const char*, 0, MallocAllocPolicy>* aFilters);

// The number of milliseconds since the process started. Operates the same
// whether the profiler is active or inactive.
MFBT_API double profiler_time();

// An object of this class is passed to profiler_suspend_and_sample_thread().
// For each stack frame, one of the Collect methods will be called.
class ProfilerStackCollector {
 public:
  // Some collectors need to worry about possibly overwriting previous
  // generations of data. If that's not an issue, this can return Nothing,
  // which is the default behaviour.
  virtual Maybe<uint64_t> SamplePositionInBuffer() { return Nothing(); }
  virtual Maybe<uint64_t> BufferRangeStart() { return Nothing(); }

  // This method will be called once if the thread being suspended is the main
  // thread. Default behaviour is to do nothing.
  virtual void SetIsMainThread() {}

  // WARNING: The target thread is suspended when the Collect methods are
  // called. Do not try to allocate or acquire any locks, or you could
  // deadlock. The target thread will have resumed by the time this function
  // returns.

  virtual void CollectNativeLeafAddr(void* aAddr) = 0;

  virtual void CollectProfilingStackFrame(
      const ProfilingStackFrame& aFrame) = 0;
};

// This method suspends the thread identified by aThreadId, samples its
// profiling stack, JS stack, and (optionally) native stack, passing the
// collected frames into aCollector. aFeatures dictates which compiler features
// are used. |Leaf| is the only relevant one.
// Use `aThreadId`=0 to sample the current thread.
MFBT_API void profiler_suspend_and_sample_thread(
    int aThreadId, uint32_t aFeatures, ProfilerStackCollector& aCollector,
    bool aSampleNative = true);

struct ProfilerBacktraceDestructor {
  MFBT_API void operator()(ProfilerBacktrace*);
};

using UniqueProfilerBacktrace =
    UniquePtr<ProfilerBacktrace, ProfilerBacktraceDestructor>;

// Immediately capture the current thread's call stack, store it in the provided
// buffer (usually to avoid allocations if you can construct the buffer on the
// stack). Returns false if unsuccessful, if the profiler is inactive, or if
// aCaptureOptions is NoStack.
MFBT_API bool profiler_capture_backtrace_into(
    ProfileChunkedBuffer& aChunkedBuffer, StackCaptureOptions aCaptureOptions);

// Immediately capture the current thread's call stack, and return it in a
// ProfileChunkedBuffer (usually for later use in MarkerStack::TakeBacktrace()).
// May be null if unsuccessful, or if the profiler is inactive.
MFBT_API UniquePtr<ProfileChunkedBuffer> profiler_capture_backtrace();

// Immediately capture the current thread's call stack, and return it in a
// ProfilerBacktrace (usually for later use in marker function that take a
// ProfilerBacktrace). May be null if unsuccessful, or if the profiler is
// inactive.
MFBT_API UniqueProfilerBacktrace profiler_get_backtrace();

struct ProfilerStats {
  unsigned n = 0;
  double sum = 0;
  double min = std::numeric_limits<double>::max();
  double max = 0;
  void Count(double v) {
    ++n;
    sum += v;
    if (v < min) {
      min = v;
    }
    if (v > max) {
      max = v;
    }
  }
};

struct ProfilerBufferInfo {
  // Index of the oldest entry.
  uint64_t mRangeStart;
  // Index of the newest entry.
  uint64_t mRangeEnd;
  // Buffer capacity in number of 8-byte entries.
  uint32_t mEntryCount;
  // Sampling stats: Interval (us) between successive samplings.
  ProfilerStats mIntervalsUs;
  // Sampling stats: Total duration (us) of each sampling. (Split detail below.)
  ProfilerStats mOverheadsUs;
  // Sampling stats: Time (us) to acquire the lock before sampling.
  ProfilerStats mLockingsUs;
  // Sampling stats: Time (us) to discard expired data.
  ProfilerStats mCleaningsUs;
  // Sampling stats: Time (us) to collect counter data.
  ProfilerStats mCountersUs;
  // Sampling stats: Time (us) to sample thread stacks.
  ProfilerStats mThreadsUs;
};

// Get information about the current buffer status.
// Returns Nothing() if the profiler is inactive.
//
// This information may be useful to a user-interface displaying the current
// status of the profiler, allowing the user to get a sense for how fast the
// buffer is being written to, and how much data is visible.
MFBT_API Maybe<ProfilerBufferInfo> profiler_get_buffer_info();

}  // namespace baseprofiler
}  // namespace mozilla

namespace mozilla {
namespace baseprofiler {

//---------------------------------------------------------------------------
// Put profiling data into the profiler (markers)
//---------------------------------------------------------------------------

MFBT_API void profiler_add_js_marker(const char* aMarkerName,
                                     const char* aMarkerText);

//---------------------------------------------------------------------------
// Output profiles
//---------------------------------------------------------------------------

// Set a user-friendly process name, used in JSON stream.
MFBT_API void profiler_set_process_name(const std::string& aProcessName,
                                        const std::string* aETLDplus1);

// Get the profile encoded as a JSON string. A no-op (returning nullptr) if the
// profiler is inactive.
// If aIsShuttingDown is true, the current time is included as the process
// shutdown time in the JSON's "meta" object.
MFBT_API UniquePtr<char[]> profiler_get_profile(double aSinceTime = 0,
                                                bool aIsShuttingDown = false,
                                                bool aOnlyThreads = false);

// Write the profile for this process (excluding subprocesses) into aWriter.
// Returns false if the profiler is inactive.
MFBT_API bool profiler_stream_json_for_this_process(
    SpliceableJSONWriter& aWriter, double aSinceTime = 0,
    bool aIsShuttingDown = false, bool aOnlyThreads = false);

// Get the profile and write it into a file. A no-op if the profile is
// inactive.
MFBT_API void profiler_save_profile_to_file(const char* aFilename);

//---------------------------------------------------------------------------
// RAII classes
//---------------------------------------------------------------------------

class MOZ_RAII AutoProfilerInit {
 public:
  explicit AutoProfilerInit() { profiler_init(this); }

  ~AutoProfilerInit() { profiler_shutdown(); }

 private:
};

// Convenience class to register and unregister a thread with the profiler.
// Needs to be the first object on the stack of the thread.
class MOZ_RAII AutoProfilerRegisterThread final {
 public:
  explicit AutoProfilerRegisterThread(const char* aName) {
    profiler_register_thread(aName, this);
  }

  ~AutoProfilerRegisterThread() { profiler_unregister_thread(); }

 private:
  AutoProfilerRegisterThread(const AutoProfilerRegisterThread&) = delete;
  AutoProfilerRegisterThread& operator=(const AutoProfilerRegisterThread&) =
      delete;
};

class MOZ_RAII AutoProfilerThreadSleep {
 public:
  explicit AutoProfilerThreadSleep() { profiler_thread_sleep(); }

  ~AutoProfilerThreadSleep() { profiler_thread_wake(); }

 private:
};

// Temporarily wake up the profiling of a thread while servicing events such as
// Asynchronous Procedure Calls (APCs).
class MOZ_RAII AutoProfilerThreadWake {
 public:
  explicit AutoProfilerThreadWake()
      : mIssuedWake(profiler_thread_is_sleeping()) {
    if (mIssuedWake) {
      profiler_thread_wake();
    }
  }

  ~AutoProfilerThreadWake() {
    if (mIssuedWake) {
      MOZ_ASSERT(!profiler_thread_is_sleeping());
      profiler_thread_sleep();
    }
  }

 private:
  bool mIssuedWake;
};

// Get the MOZ_PROFILER_STARTUP* environment variables that should be
// supplied to a child process that is about to be launched, in order
// to make that child process start with the same profiler settings as
// in the current process.  The given function is invoked once for
// each variable to be set.
MFBT_API void GetProfilerEnvVarsForChildProcess(
    std::function<void(const char* key, const char* value)>&& aSetEnv);

}  // namespace baseprofiler
}  // namespace mozilla

#endif  // !MOZ_GECKO_PROFILER

#endif  // BaseProfiler_h
