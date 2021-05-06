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

#ifndef GeckoProfiler_h
#define GeckoProfiler_h

// Everything in here is also safe to include unconditionally, and only defines
// empty macros if MOZ_GECKO_PROFILER is unset.
// If your file only uses particular APIs (e.g., only markers), please consider
// including only the needed headers instead of this one, to reduce compilation
// dependencies.
#include "BaseProfiler.h"
#include "mozilla/ProfilerCounts.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/ProfilerState.h"

#ifndef MOZ_GECKO_PROFILER

#  include "mozilla/UniquePtr.h"

// This file can be #included unconditionally. However, everything within this
// file must be guarded by a #ifdef MOZ_GECKO_PROFILER, *except* for the
// following macros and functions, which encapsulate the most common operations
// and thus avoid the need for many #ifdefs.

#  define AUTO_PROFILER_INIT
#  define AUTO_PROFILER_INIT2

#  define PROFILER_REGISTER_THREAD(name)
#  define PROFILER_UNREGISTER_THREAD()
#  define AUTO_PROFILER_REGISTER_THREAD(name)

#  define AUTO_PROFILER_THREAD_SLEEP
#  define AUTO_PROFILER_THREAD_WAKE

#  define PROFILER_JS_INTERRUPT_CALLBACK()

#  define PROFILER_SET_JS_CONTEXT(cx)
#  define PROFILER_CLEAR_JS_CONTEXT()

#  define AUTO_PROFILE_FOLLOWING_RUNNABLE(runnable)

// Function stubs for when MOZ_GECKO_PROFILER is not defined.

// This won't be used, it's just there to allow the empty definition of
// `profiler_get_backtrace`.
struct ProfilerBacktrace {};
using UniqueProfilerBacktrace = mozilla::UniquePtr<ProfilerBacktrace>;

// Get/Capture-backtrace functions can return nullptr or false, the result
// should be fed to another empty macro or stub anyway.

static inline UniqueProfilerBacktrace profiler_get_backtrace() {
  return nullptr;
}

// This won't be used, it's just there to allow the empty definitions of
// `profiler_capture_backtrace_into` and `profiler_capture_backtrace`.
struct ProfileChunkedBuffer {};

static inline bool profiler_capture_backtrace_into(
    mozilla::ProfileChunkedBuffer& aChunkedBuffer,
    mozilla::StackCaptureOptions aCaptureOptions) {
  return false;
}
static inline mozilla::UniquePtr<mozilla::ProfileChunkedBuffer>
profiler_capture_backtrace() {
  return nullptr;
}

#else  // !MOZ_GECKO_PROFILER

#  include "js/ProfilingStack.h"
#  include "mozilla/Assertions.h"
#  include "mozilla/Atomics.h"
#  include "mozilla/Attributes.h"
#  include "mozilla/Maybe.h"
#  include "mozilla/PowerOfTwo.h"
#  include "mozilla/ThreadLocal.h"
#  include "mozilla/TimeStamp.h"
#  include "mozilla/UniquePtr.h"
#  include "nscore.h"
#  include "nsINamed.h"
#  include "nsString.h"
#  include "nsThreadUtils.h"

#  include <functional>
#  include <stdint.h>

class ProfilerBacktrace;
class ProfilerCodeAddressService;
struct JSContext;

namespace JS {
struct RecordAllocationInfo;
}

namespace mozilla {
class ProfileBufferControlledChunkManager;
class ProfileChunkedBuffer;
namespace baseprofiler {
class SpliceableJSONWriter;
}  // namespace baseprofiler
namespace net {
struct TimingStruct;
enum CacheDisposition : uint8_t;
}  // namespace net
}  // namespace mozilla
class nsIURI;

namespace mozilla {
class MallocAllocPolicy;
template <class T, size_t MinInlineCapacity, class AllocPolicy>
class Vector;
}  // namespace mozilla

// Macros used by the AUTO_PROFILER_* macros below.
#  define PROFILER_RAII_PASTE(id, line) id##line
#  define PROFILER_RAII_EXPAND(id, line) PROFILER_RAII_PASTE(id, line)
#  define PROFILER_RAII PROFILER_RAII_EXPAND(raiiObject, __LINE__)

//---------------------------------------------------------------------------
// Start and stop the profiler
//---------------------------------------------------------------------------

static constexpr mozilla::PowerOfTwo32 PROFILER_DEFAULT_ENTRIES =
#  if !defined(GP_PLAT_arm_android)
    mozilla::MakePowerOfTwo32<8 * 1024 * 1024>();  // 8M entries = 64MB
#  else
    mozilla::MakePowerOfTwo32<2 * 1024 * 1024>();  // 2M entries = 16MB
#  endif

// Startup profiling usually need to capture more data, especially on slow
// systems.
// Note: Keep in sync with GeckoThread.maybeStartGeckoProfiler:
// https://searchfox.org/mozilla-central/source/mobile/android/geckoview/src/main/java/org/mozilla/gecko/GeckoThread.java
static constexpr mozilla::PowerOfTwo32 PROFILER_DEFAULT_STARTUP_ENTRIES =
#  if !defined(GP_PLAT_arm_android)
    mozilla::MakePowerOfTwo32<64 * 1024 * 1024>();  // 64M entries = 512MB
#  else
    mozilla::MakePowerOfTwo32<8 * 1024 * 1024>();  // 8M entries = 64MB
#  endif

#  define PROFILER_DEFAULT_DURATION 20 /* seconds, for tests only */
// Note: Keep in sync with GeckoThread.maybeStartGeckoProfiler:
// https://searchfox.org/mozilla-central/source/mobile/android/geckoview/src/main/java/org/mozilla/gecko/GeckoThread.java
#  define PROFILER_DEFAULT_INTERVAL 1  /* millisecond */
#  define PROFILER_MAX_INTERVAL 5000   /* milliseconds */
#  define PROFILER_DEFAULT_ACTIVE_TAB_ID 0

// Initialize the profiler. If MOZ_PROFILER_STARTUP is set the profiler will
// also be started. This call must happen before any other profiler calls
// (except profiler_start(), which will call profiler_init() if it hasn't
// already run).
void profiler_init(void* stackTop);
void profiler_init_threadmanager();

// Call this as early as possible
#  define AUTO_PROFILER_INIT mozilla::AutoProfilerInit PROFILER_RAII
// Call this after the nsThreadManager is Init()ed
#  define AUTO_PROFILER_INIT2 mozilla::AutoProfilerInit2 PROFILER_RAII

enum class IsFastShutdown {
  No,
  Yes,
};

// Clean up the profiler module, stopping it if required. This function may
// also save a shutdown profile if requested. No profiler calls should happen
// after this point and all profiling stack labels should have been popped.
void profiler_shutdown(IsFastShutdown aIsFastShutdown = IsFastShutdown::No);

// Start the profiler -- initializing it first if necessary -- with the
// selected options. Stops and restarts the profiler if it is already active.
// After starting the profiler is "active". The samples will be recorded in a
// circular buffer.
//   "aCapacity" is the maximum number of 8-bytes entries in the profiler's
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
//   "aActiveTabID" BrowserId of the active browser screen's active tab.
//               It's being used to determine the profiled tab. It's "0" if
//               we failed to get the ID.
//   "aDuration" is the duration of entries in the profiler's circular buffer.
void profiler_start(
    mozilla::PowerOfTwo32 aCapacity, double aInterval, uint32_t aFeatures,
    const char** aFilters, uint32_t aFilterCount, uint64_t aActiveTabID,
    const mozilla::Maybe<double>& aDuration = mozilla::Nothing());

// Stop the profiler and discard the profile without saving it. A no-op if the
// profiler is inactive. After stopping the profiler is "inactive".
void profiler_stop();

// If the profiler is inactive, start it. If it's already active, restart it if
// the requested settings differ from the current settings. Both the check and
// the state change are performed while the profiler state is locked.
// The only difference to profiler_start is that the current buffer contents are
// not discarded if the profiler is already running with the requested settings.
void profiler_ensure_started(
    mozilla::PowerOfTwo32 aCapacity, double aInterval, uint32_t aFeatures,
    const char** aFilters, uint32_t aFilterCount, uint64_t aActiveTabID,
    const mozilla::Maybe<double>& aDuration = mozilla::Nothing());

//---------------------------------------------------------------------------
// Control the profiler
//---------------------------------------------------------------------------

// Register/unregister threads with the profiler. Both functions operate the
// same whether the profiler is active or inactive.
#  define PROFILER_REGISTER_THREAD(name)         \
    do {                                         \
      char stackTop;                             \
      profiler_register_thread(name, &stackTop); \
    } while (0)
#  define PROFILER_UNREGISTER_THREAD() profiler_unregister_thread()
ProfilingStack* profiler_register_thread(const char* name, void* guessStackTop);
void profiler_unregister_thread();

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
void profiler_register_page(uint64_t aTabID, uint64_t aInnerWindowID,
                            const nsCString& aUrl,
                            uint64_t aEmbedderInnerWindowID);
// Unregister page with the profiler.
//
// Take a Inner Window ID and unregister the page entry that has the same ID.
void profiler_unregister_page(uint64_t aRegisteredInnerWindowID);

// Remove all registered and unregistered pages in the profiler.
void profiler_clear_all_pages();

class BaseProfilerCount;
void profiler_add_sampled_counter(BaseProfilerCount* aCounter);
void profiler_remove_sampled_counter(BaseProfilerCount* aCounter);

// Register and unregister a thread within a scope.
#  define AUTO_PROFILER_REGISTER_THREAD(name) \
    mozilla::AutoProfilerRegisterThread PROFILER_RAII(name)

enum class SamplingState {
  JustStopped,  // Sampling loop has just stopped without sampling, between the
                // callback registration and now.
  SamplingPaused,  // Profiler is active but sampling loop has gone through a
                   // pause.
  NoStackSamplingCompleted,  // A full sampling loop has completed in
                             // no-stack-sampling mode.
  SamplingCompleted          // A full sampling loop has completed.
};

using PostSamplingCallback = std::function<void(SamplingState)>;

// Install a callback to be invoked at the end of the next sampling loop.
// - `false` if profiler is not active, `aCallback` will stay untouched.
// - `true` if `aCallback` was successfully moved-from into internal storage,
//   and *will* be invoked at the end of the next sampling cycle. Note that this
//   will happen on the Sampler thread, and will block further sampling, so
//   please be mindful not to block for a long time (e.g., just dispatch a
//   runnable to another thread.) Calling profiler functions from the callback
//   is allowed.
[[nodiscard]] bool profiler_callback_after_sampling(
    PostSamplingCallback&& aCallback);

// Pause and resume the profiler. No-ops if the profiler is inactive. While
// paused the profile will not take any samples and will not record any data
// into its buffers. The profiler remains fully initialized in this state.
// Timeline markers will still be stored. This feature will keep JavaScript
// profiling enabled, thus allowing toggling the profiler without invalidating
// the JIT.
void profiler_pause();
void profiler_resume();

// Only pause and resume the periodic sampling loop, including stack sampling,
// counters, and profiling overheads.
void profiler_pause_sampling();
void profiler_resume_sampling();

// These functions tell the profiler that a thread went to sleep so that we can
// avoid sampling it while it's sleeping. Calling profiler_thread_sleep()
// twice without an intervening profiler_thread_wake() is an error. All three
// functions operate the same whether the profiler is active or inactive.
void profiler_thread_sleep();
void profiler_thread_wake();

// Mark a thread as asleep/awake within a scope.
#  define AUTO_PROFILER_THREAD_SLEEP \
    mozilla::AutoProfilerThreadSleep PROFILER_RAII
#  define AUTO_PROFILER_THREAD_WAKE \
    mozilla::AutoProfilerThreadWake PROFILER_RAII

// Called by the JSRuntime's operation callback. This is used to start profiling
// on auxiliary threads. Operates the same whether the profiler is active or
// not.
#  define PROFILER_JS_INTERRUPT_CALLBACK() profiler_js_interrupt_callback()
void profiler_js_interrupt_callback();

// Set and clear the current thread's JSContext.
#  define PROFILER_SET_JS_CONTEXT(cx) profiler_set_js_context(cx)
#  define PROFILER_CLEAR_JS_CONTEXT() profiler_clear_js_context()
void profiler_set_js_context(JSContext* aCx);
void profiler_clear_js_context();

//---------------------------------------------------------------------------
// Get information from the profiler
//---------------------------------------------------------------------------

// Get the params used to start the profiler. Returns 0 and an empty vector
// (via outparams) if the profile is inactive. It's possible that the features
// returned may be slightly different to those requested due to required
// adjustments.
void profiler_get_start_params(
    int* aEntrySize, mozilla::Maybe<double>* aDuration, double* aInterval,
    uint32_t* aFeatures,
    mozilla::Vector<const char*, 0, mozilla::MallocAllocPolicy>* aFilters,
    uint64_t* aActiveTabID);

// Get the chunk manager used in the current profiling session, or null.
mozilla::ProfileBufferControlledChunkManager*
profiler_get_controlled_chunk_manager();

// The number of milliseconds since the process started. Operates the same
// whether the profiler is active or inactive.
double profiler_time();

// An object of this class is passed to profiler_suspend_and_sample_thread().
// For each stack frame, one of the Collect methods will be called.
class ProfilerStackCollector {
 public:
  // Some collectors need to worry about possibly overwriting previous
  // generations of data. If that's not an issue, this can return Nothing,
  // which is the default behaviour.
  virtual mozilla::Maybe<uint64_t> SamplePositionInBuffer() {
    return mozilla::Nothing();
  }
  virtual mozilla::Maybe<uint64_t> BufferRangeStart() {
    return mozilla::Nothing();
  }

  // This method will be called once if the thread being suspended is the main
  // thread. Default behaviour is to do nothing.
  virtual void SetIsMainThread() {}

  // WARNING: The target thread is suspended when the Collect methods are
  // called. Do not try to allocate or acquire any locks, or you could
  // deadlock. The target thread will have resumed by the time this function
  // returns.

  virtual void CollectNativeLeafAddr(void* aAddr) = 0;

  virtual void CollectJitReturnAddr(void* aAddr) = 0;

  virtual void CollectWasmFrame(const char* aLabel) = 0;

  virtual void CollectProfilingStackFrame(
      const js::ProfilingStackFrame& aFrame) = 0;
};

// This method suspends the thread identified by aThreadId, samples its
// profiling stack, JS stack, and (optionally) native stack, passing the
// collected frames into aCollector. aFeatures dictates which compiler features
// are used. |Leaf| is the only relevant one.
// Use `aThreadId`=0 to sample the current thread.
void profiler_suspend_and_sample_thread(int aThreadId, uint32_t aFeatures,
                                        ProfilerStackCollector& aCollector,
                                        bool aSampleNative = true);

struct ProfilerBacktraceDestructor {
  void operator()(ProfilerBacktrace*);
};

using UniqueProfilerBacktrace =
    mozilla::UniquePtr<ProfilerBacktrace, ProfilerBacktraceDestructor>;

// Immediately capture the current thread's call stack, store it in the provided
// buffer (usually to avoid allocations if you can construct the buffer on the
// stack). Returns false if unsuccessful, or if the profiler is inactive.
bool profiler_capture_backtrace_into(
    mozilla::ProfileChunkedBuffer& aChunkedBuffer,
    mozilla::StackCaptureOptions aCaptureOptions);

// Immediately capture the current thread's call stack, and return it in a
// ProfileChunkedBuffer (usually for later use in MarkerStack::TakeBacktrace()).
// May be null if unsuccessful, or if the profiler is inactive.
mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> profiler_capture_backtrace();

// Immediately capture the current thread's call stack, and return it in a
// ProfilerBacktrace (usually for later use in marker function that take a
// ProfilerBacktrace). May be null if unsuccessful, or if the profiler is
// inactive.
UniqueProfilerBacktrace profiler_get_backtrace();

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
  // Sampling stats: Interval between successive samplings.
  ProfilerStats mIntervalsUs;
  // Sampling stats: Total sampling duration. (Split detail below.)
  ProfilerStats mOverheadsUs;
  // Sampling stats: Time to acquire the lock before sampling.
  ProfilerStats mLockingsUs;
  // Sampling stats: Time to discard expired data.
  ProfilerStats mCleaningsUs;
  // Sampling stats: Time to collect counter data.
  ProfilerStats mCountersUs;
  // Sampling stats: Time to sample thread stacks.
  ProfilerStats mThreadsUs;
};

// Get information about the current buffer status.
// Returns Nothing() if the profiler is inactive.
//
// This information may be useful to a user-interface displaying the current
// status of the profiler, allowing the user to get a sense for how fast the
// buffer is being written to, and how much data is visible.
mozilla::Maybe<ProfilerBufferInfo> profiler_get_buffer_info();

//---------------------------------------------------------------------------
// Put profiling data into the profiler (markers)
//---------------------------------------------------------------------------

void profiler_add_js_marker(const char* aMarkerName, const char* aMarkerText);
void profiler_add_js_allocation_marker(JS::RecordAllocationInfo&& info);

// Returns true or or false depending on whether the marker was actually added
// or not.
bool profiler_add_native_allocation_marker(int64_t aSize,
                                           uintptr_t aMemoryAddress);

enum class NetworkLoadType { LOAD_START, LOAD_STOP, LOAD_REDIRECT };

void profiler_add_network_marker(
    nsIURI* aURI, const nsACString& aRequestMethod, int32_t aPriority,
    uint64_t aChannelId, NetworkLoadType aType, mozilla::TimeStamp aStart,
    mozilla::TimeStamp aEnd, int64_t aCount,
    mozilla::net::CacheDisposition aCacheDisposition, uint64_t aInnerWindowID,
    const mozilla::net::TimingStruct* aTimings = nullptr,
    nsIURI* aRedirectURI = nullptr,
    mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> aSource = nullptr,
    const mozilla::Maybe<nsDependentCString>& aContentType =
        mozilla::Nothing());

enum TracingKind {
  TRACING_EVENT,
  TRACING_INTERVAL_START,
  TRACING_INTERVAL_END,
};

//---------------------------------------------------------------------------
// Output profiles
//---------------------------------------------------------------------------

// Set a user-friendly process name, used in JSON stream.  Allows an optional
// detailed name which may include private info (eTLD+1 in fission)
void profiler_set_process_name(const nsACString& aProcessName,
                               const nsACString* aETLDplus1 = nullptr);

// Get the profile encoded as a JSON string. A no-op (returning nullptr) if the
// profiler is inactive.
// If aIsShuttingDown is true, the current time is included as the process
// shutdown time in the JSON's "meta" object.
mozilla::UniquePtr<char[]> profiler_get_profile(double aSinceTime = 0,
                                                bool aIsShuttingDown = false);

// Write the profile for this process (excluding subprocesses) into aWriter.
// Returns false if the profiler is inactive.
bool profiler_stream_json_for_this_process(
    mozilla::baseprofiler::SpliceableJSONWriter& aWriter, double aSinceTime = 0,
    bool aIsShuttingDown = false,
    ProfilerCodeAddressService* aService = nullptr);

// Get the profile and write it into a file. A no-op if the profile is
// inactive.
//
// This function is 'extern "C"' so that it is easily callable from a debugger
// in a build without debugging information (a workaround for
// http://llvm.org/bugs/show_bug.cgi?id=22211).
extern "C" {
void profiler_save_profile_to_file(const char* aFilename);
}

//---------------------------------------------------------------------------
// RAII classes
//---------------------------------------------------------------------------

namespace mozilla {

class MOZ_RAII AutoProfilerInit {
 public:
  explicit AutoProfilerInit() { profiler_init(this); }

  ~AutoProfilerInit() { profiler_shutdown(); }

 private:
};

class MOZ_RAII AutoProfilerInit2 {
 public:
  explicit AutoProfilerInit2() { profiler_init_threadmanager(); }

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
void GetProfilerEnvVarsForChildProcess(
    std::function<void(const char* key, const char* value)>&& aSetEnv);

#  ifndef MOZ_COLLECTING_RUNNABLE_TELEMETRY
#    define AUTO_PROFILE_FOLLOWING_RUNNABLE(runnable)
#  else
#    define AUTO_PROFILE_FOLLOWING_RUNNABLE(runnable) \
      mozilla::AutoProfileRunnable PROFILER_RAII(runnable)

class MOZ_RAII AutoProfileRunnable {
 public:
  explicit AutoProfileRunnable(Runnable* aRunnable)
      : mStartTime(TimeStamp::Now()) {
    if (!profiler_thread_is_being_profiled()) {
      return;
    }

    aRunnable->GetName(mName);
  }
  explicit AutoProfileRunnable(nsIRunnable* aRunnable)
      : mStartTime(TimeStamp::Now()) {
    if (!profiler_thread_is_being_profiled()) {
      return;
    }

    nsCOMPtr<nsINamed> named = do_QueryInterface(aRunnable);
    if (named) {
      named->GetName(mName);
    }
  }

  ~AutoProfileRunnable() {
    if (!profiler_thread_is_being_profiled()) {
      return;
    }

    AUTO_PROFILER_LABEL("AutoProfileRunnable", PROFILER);
    AUTO_PROFILER_STATS(AUTO_PROFILE_RUNNABLE);
    profiler_add_marker("Runnable", ::mozilla::baseprofiler::category::OTHER,
                        MarkerTiming::IntervalUntilNowFrom(mStartTime),
                        geckoprofiler::markers::TextMarker{}, mName);
  }

 protected:
  TimeStamp mStartTime;
  nsAutoCString mName;
};
#  endif

}  // namespace mozilla

#endif  // !MOZ_GECKO_PROFILER

#endif  // GeckoProfiler_h
