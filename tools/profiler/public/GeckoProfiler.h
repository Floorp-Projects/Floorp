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
#include "mozilla/ProfilerThreadSleep.h"
#include "mozilla/ProfilerThreadState.h"
#include "mozilla/ProgressLogger.h"

#ifndef MOZ_GECKO_PROFILER

#  include "mozilla/UniquePtr.h"

// This file can be #included unconditionally. However, everything within this
// file must be guarded by a #ifdef MOZ_GECKO_PROFILER, *except* for the
// following macros and functions, which encapsulate the most common operations
// and thus avoid the need for many #ifdefs.

#  define PROFILER_REGISTER_THREAD(name)
#  define PROFILER_UNREGISTER_THREAD()
#  define AUTO_PROFILER_REGISTER_THREAD(name)

#  define PROFILER_JS_INTERRUPT_CALLBACK()

#  define PROFILER_SET_JS_CONTEXT(cx)
#  define PROFILER_CLEAR_JS_CONTEXT()

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

static inline void profiler_set_process_name(
    const nsACString& aProcessName, const nsACString* aETLDplus1 = nullptr) {}

static inline void profiler_received_exit_profile(
    const nsACString& aExitProfile) {}

static inline void profiler_register_page(uint64_t aTabID,
                                          uint64_t aInnerWindowID,
                                          const nsCString& aUrl,
                                          uint64_t aEmbedderInnerWindowID,
                                          bool aIsPrivateBrowsing) {}
static inline void profiler_unregister_page(uint64_t aRegisteredInnerWindowID) {
}

static inline void GetProfilerEnvVarsForChildProcess(
    std::function<void(const char* key, const char* value)>&& aSetEnv) {}

static inline void profiler_record_wakeup_count(
    const nsACString& aProcessType) {}

#else  // !MOZ_GECKO_PROFILER

#  include "js/ProfilingStack.h"
#  include "mozilla/Assertions.h"
#  include "mozilla/Atomics.h"
#  include "mozilla/Attributes.h"
#  include "mozilla/BaseProfilerRAIIMacro.h"
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

namespace mozilla {
class ProfileBufferControlledChunkManager;
class ProfileChunkedBuffer;
namespace baseprofiler {
class SpliceableJSONWriter;
}  // namespace baseprofiler
}  // namespace mozilla
class nsIURI;

//---------------------------------------------------------------------------
// Give information to the profiler
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
//   "aIsPrivateBrowsing"     is true if this browsing context happens in a
//                            private browsing context.
void profiler_register_page(uint64_t aTabID, uint64_t aInnerWindowID,
                            const nsCString& aUrl,
                            uint64_t aEmbedderInnerWindowID,
                            bool aIsPrivateBrowsing);
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
// Use `ProfilerThreadId{}` (unspecified) to sample the current thread.
void profiler_suspend_and_sample_thread(ProfilerThreadId aThreadId,
                                        uint32_t aFeatures,
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

// Record through glean how many times profiler_thread_wake has been
// called.
void profiler_record_wakeup_count(const nsACString& aProcessType);

//---------------------------------------------------------------------------
// Output profiles
//---------------------------------------------------------------------------

// Set a user-friendly process name, used in JSON stream.  Allows an optional
// detailed name which may include private info (eTLD+1 in fission)
void profiler_set_process_name(const nsACString& aProcessName,
                               const nsACString* aETLDplus1 = nullptr);

// Record an exit profile from a child process.
void profiler_received_exit_profile(const nsACString& aExitProfile);

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
    ProfilerCodeAddressService* aService = nullptr,
    mozilla::ProgressLogger aProgressLogger = {});

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

// Get the MOZ_PROFILER_STARTUP* environment variables that should be
// supplied to a child process that is about to be launched, in order
// to make that child process start with the same profiler settings as
// in the current process.  The given function is invoked once for
// each variable to be set.
void GetProfilerEnvVarsForChildProcess(
    std::function<void(const char* key, const char* value)>&& aSetEnv);

}  // namespace mozilla

#endif  // !MOZ_GECKO_PROFILER

#endif  // GeckoProfiler_h
