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

// everything in here is also safe to include unconditionally, and only defines
// empty macros if MOZ_GECKO_PROFILER is unset
#include "mozilla/ProfilerCounts.h"

#ifndef MOZ_GECKO_PROFILER

#  include "mozilla/UniquePtr.h"

// This file can be #included unconditionally. However, everything within this
// file must be guarded by a #ifdef MOZ_GECKO_PROFILER, *except* for the
// following macros, which encapsulate the most common operations and thus
// avoid the need for many #ifdefs.

#  define AUTO_PROFILER_INIT

#  define PROFILER_REGISTER_THREAD(name)
#  define PROFILER_UNREGISTER_THREAD()
#  define AUTO_PROFILER_REGISTER_THREAD(name)

#  define AUTO_PROFILER_THREAD_SLEEP
#  define AUTO_PROFILER_THREAD_WAKE

#  define PROFILER_JS_INTERRUPT_CALLBACK()

#  define PROFILER_SET_JS_CONTEXT(cx)
#  define PROFILER_CLEAR_JS_CONTEXT()

#  define AUTO_PROFILER_LABEL(label, categoryPair)
#  define AUTO_PROFILER_LABEL_CATEGORY_PAIR(categoryPair)
#  define AUTO_PROFILER_LABEL_DYNAMIC_CSTR(label, categoryPair, cStr)
#  define AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING(label, categoryPair, nsCStr)
#  define AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(label, categoryPair, nsStr)
#  define AUTO_PROFILER_LABEL_FAST(label, categoryPair, ctx)
#  define AUTO_PROFILER_LABEL_DYNAMIC_FAST(label, dynamicString, categoryPair, \
                                           ctx, flags)

#  define PROFILER_ADD_MARKER(markerName, categoryPair)
#  define PROFILER_ADD_MARKER_WITH_PAYLOAD(markerName, categoryPair, \
                                           PayloadType, payloadArgs)
#  define PROFILER_ADD_NETWORK_MARKER(uri, pri, channel, type, start, end, \
                                      count, cache, timings, redirect, ...)

#  define DECLARE_DOCSHELL_AND_HISTORY_ID(docShell)
#  define PROFILER_TRACING(categoryString, markerName, categoryPair, kind)
#  define PROFILER_TRACING_DOCSHELL(categoryString, markerName, categoryPair, \
                                    kind, docshell)
#  define AUTO_PROFILER_TRACING(categoryString, markerName, categoryPair)
#  define AUTO_PROFILER_TRACING_DOCSHELL(categoryString, markerName, \
                                         categoryPair, docShell)
#  define AUTO_PROFILER_TEXT_MARKER_CAUSE(markerName, text, categoryPair, cause)
#  define AUTO_PROFILER_TEXT_MARKER_DOCSHELL(markerName, text, categoryPair, \
                                             docShell)
#  define AUTO_PROFILER_TEXT_MARKER_DOCSHELL_CAUSE( \
      markerName, text, categoryPair, docShell, cause)

using UniqueProfilerBacktrace = mozilla::UniquePtr<int>;
static inline UniqueProfilerBacktrace profiler_get_backtrace()
{
  return nullptr;
}

#else  // !MOZ_GECKO_PROFILER

#  include "BaseProfiler.h"
#  include "js/AllocationRecording.h"
#  include "js/ProfilingFrameIterator.h"
#  include "js/ProfilingStack.h"
#  include "js/RootingAPI.h"
#  include "js/TypeDecls.h"
#  include "mozilla/Assertions.h"
#  include "mozilla/Atomics.h"
#  include "mozilla/Attributes.h"
#  include "mozilla/GuardObjects.h"
#  include "mozilla/Maybe.h"
#  include "mozilla/PowerOfTwo.h"
#  include "mozilla/Sprintf.h"
#  include "mozilla/ThreadLocal.h"
#  include "mozilla/TimeStamp.h"
#  include "mozilla/UniquePtr.h"
#  include "nscore.h"
#  include "nsID.h"
#  include "nsString.h"

#  include <stdint.h>

class ProfilerBacktrace;
class ProfilerCodeAddressService;
class ProfilerMarkerPayload;
class SpliceableJSONWriter;
namespace mozilla {
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
// Profiler features
//---------------------------------------------------------------------------

// Higher-order macro containing all the feature info in one place. Define
// |MACRO| appropriately to extract the relevant parts. Note that the number
// values are used internally only and so can be changed without consequence.
// Any changes to this list should also be applied to the feature list in
// toolkit/components/extensions/schemas/geckoProfiler.json.
#  define PROFILER_FOR_EACH_FEATURE(MACRO)                                     \
    MACRO(0, "java", Java, "Profile Java code, Android only")                  \
                                                                               \
    MACRO(1, "js", JS,                                                         \
          "Get the JS engine to expose the JS stack to the profiler")          \
                                                                               \
    /* The DevTools profiler doesn't want the native addresses. */             \
    MACRO(2, "leaf", Leaf, "Include the C++ leaf node if not stackwalking")    \
                                                                               \
    MACRO(3, "mainthreadio", MainThreadIO,                                     \
          "Add main thread I/O to the profile")                                \
                                                                               \
    MACRO(4, "privacy", Privacy,                                               \
          "Do not include user-identifiable information")                      \
                                                                               \
    MACRO(5, "responsiveness", Responsiveness,                                 \
          "Collect thread responsiveness information")                         \
                                                                               \
    MACRO(6, "screenshots", Screenshots,                                       \
          "Take a snapshot of the window on every composition")                \
                                                                               \
    MACRO(7, "seqstyle", SequentialStyle,                                      \
          "Disable parallel traversal in styling")                             \
                                                                               \
    MACRO(8, "stackwalk", StackWalk,                                           \
          "Walk the C++ stack, not available on all platforms")                \
                                                                               \
    MACRO(9, "tasktracer", TaskTracer,                                         \
          "Start profiling with feature TaskTracer")                           \
                                                                               \
    MACRO(10, "threads", Threads, "Profile the registered secondary threads")  \
                                                                               \
    MACRO(11, "trackopts", TrackOptimizations,                                 \
          "Have the JavaScript engine track JIT optimizations")                \
                                                                               \
    MACRO(12, "jstracer", JSTracer, "Enable tracing of the JavaScript engine") \
                                                                               \
    MACRO(13, "jsallocations", JSAllocations,                                  \
          "Have the JavaScript engine track allocations")                      \
                                                                               \
    MACRO(15, "preferencereads", PreferenceReads,                              \
          "Track when preferences are read")

struct ProfilerFeature {
#  define DECLARE(n_, str_, Name_, desc_)                     \
    static constexpr uint32_t Name_ = (1u << n_);             \
    static constexpr bool Has##Name_(uint32_t aFeatures) {    \
      return aFeatures & Name_;                               \
    }                                                         \
    static constexpr void Set##Name_(uint32_t& aFeatures) {   \
      aFeatures |= Name_;                                     \
    }                                                         \
    static constexpr void Clear##Name_(uint32_t& aFeatures) { \
      aFeatures &= ~Name_;                                    \
    }

  // Define a bitfield constant, a getter, and two setters for each feature.
  PROFILER_FOR_EACH_FEATURE(DECLARE)

#  undef DECLARE
};

namespace mozilla {
namespace profiler {
namespace detail {

// RacyFeatures is only defined in this header file so that its methods can
// be inlined into profiler_is_active(). Please do not use anything from the
// detail namespace outside the profiler.

// Within the profiler's code, the preferred way to check profiler activeness
// and features is via ActivePS(). However, that requires locking gPSMutex.
// There are some hot operations where absolute precision isn't required, so we
// duplicate the activeness/feature state in a lock-free manner in this class.
class RacyFeatures {
 public:
  static void SetActive(uint32_t aFeatures) {
    sActiveAndFeatures = Active | aFeatures;
  }

  static void SetInactive() { sActiveAndFeatures = 0; }

  static void SetPaused() { sActiveAndFeatures |= Paused; }

  static void SetUnpaused() { sActiveAndFeatures &= ~Paused; }

  static bool IsActive() { return uint32_t(sActiveAndFeatures) & Active; }

  static bool IsActiveWithFeature(uint32_t aFeature) {
    uint32_t af = sActiveAndFeatures;  // copy it first
    return (af & Active) && (af & aFeature);
  }

  static bool IsActiveWithoutPrivacy() {
    uint32_t af = sActiveAndFeatures;  // copy it first
    return (af & Active) && !(af & ProfilerFeature::Privacy);
  }

  static bool IsActiveAndUnpausedWithoutPrivacy() {
    uint32_t af = sActiveAndFeatures;  // copy it first
    return (af & Active) && !(af & (Paused | ProfilerFeature::Privacy));
  }

 private:
  static constexpr uint32_t Active = 1u << 31;
  static constexpr uint32_t Paused = 1u << 30;

// Ensure Active/Paused don't overlap with any of the feature bits.
#  define NO_OVERLAP(n_, str_, Name_, desc_) \
    static_assert(ProfilerFeature::Name_ != Paused, "bad feature value");

  PROFILER_FOR_EACH_FEATURE(NO_OVERLAP);

#  undef NO_OVERLAP

  // We combine the active bit with the feature bits so they can be read or
  // written in a single atomic operation. Accesses to this atomic are not
  // recorded by web replay as they may occur at non-deterministic points.
  static mozilla::Atomic<uint32_t, mozilla::MemoryOrdering::Relaxed,
                         recordreplay::Behavior::DontPreserve>
      sActiveAndFeatures;
};

bool IsThreadBeingProfiled();

}  // namespace detail
}  // namespace profiler
}  // namespace mozilla

//---------------------------------------------------------------------------
// Start and stop the profiler
//---------------------------------------------------------------------------

static constexpr mozilla::PowerOfTwo32 PROFILER_DEFAULT_ENTRIES =
#  if !defined(ARCH_ARMV6)
    mozilla::MakePowerOfTwo32<1u << 20>();  // 1'048'576 entries = 8MB
#  else
    mozilla::MakePowerOfTwo32<1u << 17>();  // 131'072 entries = 1MB
#  endif

// Startup profiling usually need to capture more data, especially on slow
// systems.
static constexpr mozilla::PowerOfTwo32 PROFILER_DEFAULT_STARTUP_ENTRIES =
#  if !defined(ARCH_ARMV6)
    mozilla::MakePowerOfTwo32<1u << 22>();  // 4'194'304 entries = 32MB
#  else
    mozilla::MakePowerOfTwo32<1u << 17>();  // 131'072 entries = 1MB
#  endif

#  define PROFILER_DEFAULT_DURATION 20
#  define PROFILER_DEFAULT_INTERVAL 1

// Initialize the profiler. If MOZ_PROFILER_STARTUP is set the profiler will
// also be started. This call must happen before any other profiler calls
// (except profiler_start(), which will call profiler_init() if it hasn't
// already run).
void profiler_init(void* stackTop);

#  define AUTO_PROFILER_INIT mozilla::AutoProfilerInit PROFILER_RAII

// Clean up the profiler module, stopping it if required. This function may
// also save a shutdown profile if requested. No profiler calls should happen
// after this point and all profiling stack labels should have been popped.
void profiler_shutdown();

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
//   "aDuration" is the duration of entries in the profiler's circular buffer.
void profiler_start(
    mozilla::PowerOfTwo32 aCapacity, double aInterval, uint32_t aFeatures,
    const char** aFilters, uint32_t aFilterCount,
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
    const char** aFilters, uint32_t aFilterCount,
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

// Register pages with the profiler.
//
// The `page` means every new history entry for docShells.
// DocShellId + HistoryID is a unique pair to identify these pages.
// We also keep these pairs inside markers to associate with the pages.
// That allows us to see which markers belong to a specific page and filter the
// markers by a page.
// We register pages in these cases:
// - If there is a navigation through a link or URL bar.
// - If there is a navigation through `location.replace` or `history.pushState`.
// We do not register pages in these cases:
// - If there is a history navigation through the back and forward buttons.
// - If there is a navigation through `history.replaceState` or anchor scrolls.
//
//   "aDocShellId" is the ID of the docShell that page belongs to.
//   "aHistoryId"  is the ID of the history entry on the given docShell.
//   "aUrl"        is the URL of the page.
//   "aIsSubFrame" is true if the page is a sub frame.
void profiler_register_page(const nsID& aDocShellId, uint32_t aHistoryId,
                            const nsCString& aUrl, bool aIsSubFrame);
// Unregister pages with the profiler.
//
// Take a docShellId and unregister all the page entries that have the given ID.
void profiler_unregister_pages(const nsID& aRegisteredDocShellId);

// Remove all registered and unregistered pages in the profiler.
void profiler_clear_all_pages();

class BaseProfilerCount;
void profiler_add_sampled_counter(BaseProfilerCount* aCounter);
void profiler_remove_sampled_counter(BaseProfilerCount* aCounter);

// Register and unregister a thread within a scope.
#  define AUTO_PROFILER_REGISTER_THREAD(name) \
    mozilla::AutoProfilerRegisterThread PROFILER_RAII(name)

// Pause and resume the profiler. No-ops if the profiler is inactive. While
// paused the profile will not take any samples and will not record any data
// into its buffers. The profiler remains fully initialized in this state.
// Timeline markers will still be stored. This feature will keep JavaScript
// profiling enabled, thus allowing toggling the profiler without invalidating
// the JIT.
void profiler_pause();
void profiler_resume();

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

// Is the profiler active? Note: the return value of this function can become
// immediately out-of-date. E.g. the profile might be active but then
// profiler_stop() is called immediately afterward. One common and reasonable
// pattern of usage is the following:
//
//   if (profiler_is_active()) {
//     ExpensiveData expensiveData = CreateExpensiveData();
//     PROFILER_OPERATION(expensiveData);
//   }
//
// where PROFILER_OPERATION is a no-op if the profiler is inactive. In this
// case the profiler_is_active() check is just an optimization -- it prevents
// us calling CreateExpensiveData() unnecessarily in most cases, but the
// expensive data will end up being created but not used if another thread
// stops the profiler between the CreateExpensiveData() and PROFILER_OPERATION
// calls.
inline bool profiler_is_active() {
  return mozilla::profiler::detail::RacyFeatures::IsActive();
}

// Same as profiler_is_active(), but with the same extra checks that determine
// if the profiler would currently store markers. So this should be used before
// doing some potentially-expensive work that's used in a marker. E.g.:
//
//   if (profiler_can_accept_markers()) {
//     ExpensiveMarkerPayload expensivePayload = CreateExpensivePayload();
//     BASE_PROFILER_ADD_MARKER_WITH_PAYLOAD(name, OTHER, expensivePayload);
//   }
inline bool profiler_can_accept_markers() {
  return mozilla::profiler::detail::RacyFeatures::
      IsActiveAndUnpausedWithoutPrivacy();
}

// Is the profiler active, and is the current thread being profiled?
// (Same caveats and recommented usage as profiler_is_active().)
inline bool profiler_thread_is_being_profiled() {
  return profiler_is_active() &&
         mozilla::profiler::detail::IsThreadBeingProfiled();
}

// Is the profiler active and paused? Returns false if the profiler is inactive.
bool profiler_is_paused();

// Is the current thread sleeping?
bool profiler_thread_is_sleeping();

// Get all the features supported by the profiler that are accepted by
// profiler_start(). The result is the same whether the profiler is active or
// not.
uint32_t profiler_get_available_features();

// Check if a profiler feature (specified via the ProfilerFeature type) is
// active. Returns false if the profiler is inactive. Note: the return value
// can become immediately out-of-date, much like the return value of
// profiler_is_active().
bool profiler_feature_active(uint32_t aFeature);

// Get the params used to start the profiler. Returns 0 and an empty vector
// (via outparams) if the profile is inactive. It's possible that the features
// returned may be slightly different to those requested due to required
// adjustments.
void profiler_get_start_params(
    int* aEntrySize, mozilla::Maybe<double>* aDuration, double* aInterval,
    uint32_t* aFeatures,
    mozilla::Vector<const char*, 0, mozilla::MallocAllocPolicy>* aFilters);

// The number of milliseconds since the process started. Operates the same
// whether the profiler is active or inactive.
double profiler_time();

// Get the current process's ID.
int profiler_current_process_id();

// Get the current thread's ID.
int profiler_current_thread_id();

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
// are used. |Privacy| and |Leaf| are the only relevant ones.
void profiler_suspend_and_sample_thread(int aThreadId, uint32_t aFeatures,
                                        ProfilerStackCollector& aCollector,
                                        bool aSampleNative = true);

struct ProfilerBacktraceDestructor {
  void operator()(ProfilerBacktrace*);
};

using UniqueProfilerBacktrace =
    mozilla::UniquePtr<ProfilerBacktrace, ProfilerBacktraceDestructor>;

// Immediately capture the current thread's call stack and return it. A no-op
// if the profiler is inactive or in privacy mode.
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
  ProfilerStats mIntervalsNs;
  // Sampling stats: Total sampling duration. (Split detail below.)
  ProfilerStats mOverheadsNs;
  // Sampling stats: Time to acquire the lock before sampling.
  ProfilerStats mLockingsNs;
  // Sampling stats: Time to discard expired data.
  ProfilerStats mCleaningsNs;
  // Sampling stats: Time to collect counter data.
  ProfilerStats mCountersNs;
  // Sampling stats: Time to sample thread stacks.
  ProfilerStats mThreadsNs;
};

// Get information about the current buffer status.
// Returns Nothing() if the profiler is inactive.
//
// This information may be useful to a user-interface displaying the current
// status of the profiler, allowing the user to get a sense for how fast the
// buffer is being written to, and how much data is visible.
mozilla::Maybe<ProfilerBufferInfo> profiler_get_buffer_info();

//---------------------------------------------------------------------------
// Put profiling data into the profiler (labels and markers)
//---------------------------------------------------------------------------

// Insert an RAII object in this scope to enter a label stack frame. Any
// samples collected in this scope will contain this label in their stack.
// The label argument must be a static C string. It is usually of the
// form "ClassName::FunctionName". (Ideally we'd use the compiler to provide
// that for us, but __func__ gives us the function name without the class
// name.) If the label applies to only part of a function, you can qualify it
// like this: "ClassName::FunctionName:PartName".
//
// Use AUTO_PROFILER_LABEL_DYNAMIC_* if you want to add additional / dynamic
// information to the label stack frame.
#  define AUTO_PROFILER_LABEL(label, categoryPair) \
    mozilla::AutoProfilerLabel PROFILER_RAII(      \
        label, nullptr, JS::ProfilingCategoryPair::categoryPair)

// Similar to AUTO_PROFILER_LABEL, but with only one argument: the category
// pair. The label string is taken from the category pair. This is convenient
// for labels like AUTO_PROFILER_LABEL_CATEGORY_PAIR(GRAPHICS_LayerBuilding)
// which would otherwise just repeat the string.
#  define AUTO_PROFILER_LABEL_CATEGORY_PAIR(categoryPair)     \
    mozilla::AutoProfilerLabel PROFILER_RAII(                 \
        "", nullptr, JS::ProfilingCategoryPair::categoryPair, \
        uint32_t(js::ProfilingStackFrame::Flags::             \
                     LABEL_DETERMINED_BY_CATEGORY_PAIR))

// Similar to AUTO_PROFILER_LABEL, but with an additional string. The inserted
// RAII object stores the cStr pointer in a field; it does not copy the string.
//
// WARNING: This means that the string you pass to this macro needs to live at
// least until the end of the current scope. Be careful using this macro with
// ns[C]String; the other AUTO_PROFILER_LABEL_DYNAMIC_* macros below are
// preferred because they avoid this problem.
//
// If the profiler samples the current thread and walks the label stack while
// this RAII object is on the stack, it will copy the supplied string into the
// profile buffer. So there's one string copy operation, and it happens at
// sample time.
//
// Compare this to the plain AUTO_PROFILER_LABEL macro, which only accepts
// literal strings: When the label stack frames generated by
// AUTO_PROFILER_LABEL are sampled, no string copy needs to be made because the
// profile buffer can just store the raw pointers to the literal strings.
// Consequently, AUTO_PROFILER_LABEL frames take up considerably less space in
// the profile buffer than AUTO_PROFILER_LABEL_DYNAMIC_* frames.
#  define AUTO_PROFILER_LABEL_DYNAMIC_CSTR(label, categoryPair, cStr) \
    mozilla::AutoProfilerLabel PROFILER_RAII(                         \
        label, cStr, JS::ProfilingCategoryPair::categoryPair)

// Similar to AUTO_PROFILER_LABEL_DYNAMIC_CSTR, but takes an nsACString.
//
// Note: The use of the Maybe<>s ensures the scopes for the dynamic string and
// the AutoProfilerLabel are appropriate, while also not incurring the runtime
// cost of the string assignment unless the profiler is active. Therefore,
// unlike AUTO_PROFILER_LABEL and AUTO_PROFILER_LABEL_DYNAMIC_CSTR, this macro
// doesn't push/pop a label when the profiler is inactive.
#  define AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING(label, categoryPair, nsCStr) \
    mozilla::Maybe<nsAutoCString> autoCStr;                                  \
    mozilla::Maybe<mozilla::AutoProfilerLabel> raiiObjectNsCString;          \
    if (profiler_is_active()) {                                              \
      autoCStr.emplace(nsCStr);                                              \
      raiiObjectNsCString.emplace(label, autoCStr->get(),                    \
                                  JS::ProfilingCategoryPair::categoryPair);  \
    }

// Similar to AUTO_PROFILER_LABEL_DYNAMIC_CSTR, but takes an nsString that is
// is lossily converted to an ASCII string.
//
// Note: The use of the Maybe<>s ensures the scopes for the converted dynamic
// string and the AutoProfilerLabel are appropriate, while also not incurring
// the runtime cost of the string conversion unless the profiler is active.
// Therefore, unlike AUTO_PROFILER_LABEL and AUTO_PROFILER_LABEL_DYNAMIC_CSTR,
// this macro doesn't push/pop a label when the profiler is inactive.
#  define AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(label, categoryPair,   \
                                                     nsStr)                 \
    mozilla::Maybe<NS_LossyConvertUTF16toASCII> asciiStr;                   \
    mozilla::Maybe<mozilla::AutoProfilerLabel> raiiObjectLossyNsString;     \
    if (profiler_is_active()) {                                             \
      asciiStr.emplace(nsStr);                                              \
      raiiObjectLossyNsString.emplace(                                      \
          label, asciiStr->get(), JS::ProfilingCategoryPair::categoryPair); \
    }

// Similar to AUTO_PROFILER_LABEL, but accepting a JSContext* parameter, and a
// no-op if the profiler is disabled.
// Used to annotate functions for which overhead in the range of nanoseconds is
// noticeable. It avoids overhead from the TLS lookup because it can get the
// ProfilingStack from the JS context, and avoids almost all overhead in the
// case where the profiler is disabled.
#  define AUTO_PROFILER_LABEL_FAST(label, categoryPair, ctx) \
    mozilla::AutoProfilerLabel PROFILER_RAII(                \
        ctx, label, nullptr, JS::ProfilingCategoryPair::categoryPair)

// Similar to AUTO_PROFILER_LABEL_FAST, but also takes an extra string and an
// additional set of flags. The flags parameter should carry values from the
// js::ProfilingStackFrame::Flags enum.
#  define AUTO_PROFILER_LABEL_DYNAMIC_FAST(label, dynamicString, categoryPair, \
                                           ctx, flags)                         \
    mozilla::AutoProfilerLabel PROFILER_RAII(                                  \
        ctx, label, dynamicString, JS::ProfilingCategoryPair::categoryPair,    \
        flags)

// Insert a marker in the profile timeline. This is useful to delimit something
// important happening such as the first paint. Unlike labels, which are only
// recorded in the profile buffer if a sample is collected while the label is
// on the label stack, markers will always be recorded in the profile buffer.
// aMarkerName is copied, so the caller does not need to ensure it lives for a
// certain length of time. A no-op if the profiler is inactive or in privacy
// mode.

#  define PROFILER_ADD_MARKER(markerName, categoryPair)                 \
    do {                                                                \
      AUTO_PROFILER_STATS(add_marker);                                  \
      ::profiler_add_marker(markerName,                                 \
                            ::JS::ProfilingCategoryPair::categoryPair); \
    } while (false)

void profiler_add_marker(const char* aMarkerName,
                         JS::ProfilingCategoryPair aCategoryPair);

// `PayloadType` is a sub-class of MarkerPayload, `parenthesizedPayloadArgs` is
// the argument list used to construct that `PayloadType`. E.g.:
// `PROFILER_ADD_MARKER_WITH_PAYLOAD("Load", DOM, TextMarkerPayload,
//                                   ("text", start, end, ds, dsh))`
#  define PROFILER_ADD_MARKER_WITH_PAYLOAD(                            \
      markerName, categoryPair, PayloadType, parenthesizedPayloadArgs) \
    do {                                                               \
      AUTO_PROFILER_STATS(add_marker_with_##PayloadType);              \
      ::profiler_add_marker(markerName,                                \
                            ::JS::ProfilingCategoryPair::categoryPair, \
                            PayloadType parenthesizedPayloadArgs);     \
    } while (false)

void profiler_add_marker(const char* aMarkerName,
                         JS::ProfilingCategoryPair aCategoryPair,
                         const ProfilerMarkerPayload& aPayload);

void profiler_add_js_marker(const char* aMarkerName);
void profiler_add_js_allocation_marker(JS::RecordAllocationInfo&& info);

// Insert a marker in the profile timeline for a specified thread.
void profiler_add_marker_for_thread(
    int aThreadId, JS::ProfilingCategoryPair aCategoryPair,
    const char* aMarkerName,
    mozilla::UniquePtr<ProfilerMarkerPayload> aPayload);

enum class NetworkLoadType { LOAD_START, LOAD_STOP, LOAD_REDIRECT };

#  define PROFILER_ADD_NETWORK_MARKER(uri, pri, channel, type, start, end,  \
                                      count, cache, timings, redirect, ...) \
    profiler_add_network_marker(uri, pri, channel, type, start, end, count, \
                                cache, timings, redirect, ##__VA_ARGS__)

void profiler_add_network_marker(
    nsIURI* aURI, int32_t aPriority, uint64_t aChannelId, NetworkLoadType aType,
    mozilla::TimeStamp aStart, mozilla::TimeStamp aEnd, int64_t aCount,
    mozilla::net::CacheDisposition aCacheDisposition,
    const mozilla::net::TimingStruct* aTimings = nullptr,
    nsIURI* aRedirectURI = nullptr, UniqueProfilerBacktrace aSource = nullptr);

enum TracingKind {
  TRACING_EVENT,
  TRACING_INTERVAL_START,
  TRACING_INTERVAL_END,
};

// Helper macro to retrieve DocShellId and DocShellHistoryId from docShell
#  define DECLARE_DOCSHELL_AND_HISTORY_ID(docShell)      \
    mozilla::Maybe<nsID> docShellId;                     \
    mozilla::Maybe<uint32_t> docShellHistoryId;          \
    if (docShell) {                                      \
      docShellId = mozilla::Some(docShell->HistoryID()); \
      uint32_t id;                                       \
      nsresult rv = docShell->GetOSHEId(&id);            \
      if (NS_SUCCEEDED(rv)) {                            \
        docShellHistoryId = mozilla::Some(id);           \
      } else {                                           \
        docShellHistoryId = mozilla::Nothing();          \
      }                                                  \
    } else {                                             \
      docShellId = mozilla::Nothing();                   \
      docShellHistoryId = mozilla::Nothing();            \
    }

// Adds a tracing marker to the profile. A no-op if the profiler is inactive or
// in privacy mode.

#  define PROFILER_TRACING(categoryString, markerName, categoryPair, kind) \
    profiler_tracing(categoryString, markerName,                           \
                     JS::ProfilingCategoryPair::categoryPair, kind)
#  define PROFILER_TRACING_DOCSHELL(categoryString, markerName, categoryPair, \
                                    kind, docShell)                           \
    DECLARE_DOCSHELL_AND_HISTORY_ID(docShell);                                \
    profiler_tracing(categoryString, markerName,                              \
                     JS::ProfilingCategoryPair::categoryPair, kind,           \
                     docShellId, docShellHistoryId)

void profiler_tracing(
    const char* aCategoryString, const char* aMarkerName,
    JS::ProfilingCategoryPair aCategoryPair, TracingKind aKind,
    const mozilla::Maybe<nsID>& aDocShellId = mozilla::Nothing(),
    const mozilla::Maybe<uint32_t>& aDocShellHistoryId = mozilla::Nothing());
void profiler_tracing(
    const char* aCategoryString, const char* aMarkerName,
    JS::ProfilingCategoryPair aCategoryPair, TracingKind aKind,
    UniqueProfilerBacktrace aCause,
    const mozilla::Maybe<nsID>& aDocShellId = mozilla::Nothing(),
    const mozilla::Maybe<uint32_t>& aDocShellHistoryId = mozilla::Nothing());

// Adds a START/END pair of tracing markers.
#  define AUTO_PROFILER_TRACING(categoryString, markerName, categoryPair)    \
    mozilla::AutoProfilerTracing PROFILER_RAII(                              \
        categoryString, markerName, JS::ProfilingCategoryPair::categoryPair, \
        mozilla::Nothing(), mozilla::Nothing())
#  define AUTO_PROFILER_TRACING_DOCSHELL(categoryString, markerName,         \
                                         categoryPair, docShell)             \
    DECLARE_DOCSHELL_AND_HISTORY_ID(docShell);                               \
    mozilla::AutoProfilerTracing PROFILER_RAII(                              \
        categoryString, markerName, JS::ProfilingCategoryPair::categoryPair, \
        docShellId, docShellHistoryId)

// Add a text marker. Text markers are similar to tracing markers, with the
// difference that text markers have their "text" separate from the marker name;
// multiple text markers with the same name can have different text, and these
// markers will still be displayed in the same "row" in the UI.
// Another difference is that text markers combine the start and end markers
// into one marker.
void profiler_add_text_marker(
    const char* aMarkerName, const nsACString& aText,
    JS::ProfilingCategoryPair aCategoryPair,
    const mozilla::TimeStamp& aStartTime, const mozilla::TimeStamp& aEndTime,
    const mozilla::Maybe<nsID>& aDocShellId = mozilla::Nothing(),
    const mozilla::Maybe<uint32_t>& aDocShellHistoryId = mozilla::Nothing(),
    UniqueProfilerBacktrace aCause = nullptr);

class MOZ_RAII AutoProfilerTextMarker {
 public:
  AutoProfilerTextMarker(const char* aMarkerName, const nsACString& aText,
                         JS::ProfilingCategoryPair aCategoryPair,
                         const mozilla::Maybe<nsID>& aDocShellId,
                         const mozilla::Maybe<uint32_t>& aDocShellHistoryId,
                         UniqueProfilerBacktrace&& aCause =
                             nullptr MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mMarkerName(aMarkerName),
        mText(aText),
        mCategoryPair(aCategoryPair),
        mStartTime(mozilla::TimeStamp::NowUnfuzzed()),
        mCause(std::move(aCause)),
        mDocShellId(aDocShellId),
        mDocShellHistoryId(aDocShellHistoryId) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }

  ~AutoProfilerTextMarker() {
    profiler_add_text_marker(mMarkerName, mText, mCategoryPair, mStartTime,
                             mozilla::TimeStamp::NowUnfuzzed(), mDocShellId,
                             mDocShellHistoryId, std::move(mCause));
  }

 protected:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  const char* mMarkerName;
  nsCString mText;
  const JS::ProfilingCategoryPair mCategoryPair;
  mozilla::TimeStamp mStartTime;
  UniqueProfilerBacktrace mCause;
  const mozilla::Maybe<nsID> mDocShellId;
  const mozilla::Maybe<uint32_t> mDocShellHistoryId;
};

#  define AUTO_PROFILER_TEXT_MARKER_CAUSE(markerName, text, categoryPair, \
                                          cause)                          \
    AutoProfilerTextMarker PROFILER_RAII(                                 \
        markerName, text, JS::ProfilingCategoryPair::categoryPair,        \
        mozilla::Nothing(), mozilla::Nothing(), cause)

#  define AUTO_PROFILER_TEXT_MARKER_DOCSHELL(markerName, text, categoryPair,   \
                                             docShell)                         \
    DECLARE_DOCSHELL_AND_HISTORY_ID(docShell);                                 \
    AutoProfilerTextMarker PROFILER_RAII(                                      \
        markerName, text, JS::ProfilingCategoryPair::categoryPair, docShellId, \
        docShellHistoryId)

#  define AUTO_PROFILER_TEXT_MARKER_DOCSHELL_CAUSE(                            \
      markerName, text, categoryPair, docShell, cause)                         \
    DECLARE_DOCSHELL_AND_HISTORY_ID(docShell);                                 \
    AutoProfilerTextMarker PROFILER_RAII(                                      \
        markerName, text, JS::ProfilingCategoryPair::categoryPair, docShellId, \
        docShellHistoryId, cause)

//---------------------------------------------------------------------------
// Output profiles
//---------------------------------------------------------------------------

// Set a user-friendly process name, used in JSON stream.
void profiler_set_process_name(const nsACString& aProcessName);

// Get the profile encoded as a JSON string. A no-op (returning nullptr) if the
// profiler is inactive.
// If aIsShuttingDown is true, the current time is included as the process
// shutdown time in the JSON's "meta" object.
mozilla::UniquePtr<char[]> profiler_get_profile(double aSinceTime = 0,
                                                bool aIsShuttingDown = false);

// Write the profile for this process (excluding subprocesses) into aWriter.
// Returns false if the profiler is inactive.
bool profiler_stream_json_for_this_process(
    SpliceableJSONWriter& aWriter, double aSinceTime = 0,
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
  explicit AutoProfilerInit(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_init(this);
  }

  ~AutoProfilerInit() { profiler_shutdown(); }

 private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

// Convenience class to register and unregister a thread with the profiler.
// Needs to be the first object on the stack of the thread.
class MOZ_RAII AutoProfilerRegisterThread final {
 public:
  explicit AutoProfilerRegisterThread(
      const char* aName MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_register_thread(aName, this);
  }

  ~AutoProfilerRegisterThread() { profiler_unregister_thread(); }

 private:
  AutoProfilerRegisterThread(const AutoProfilerRegisterThread&) = delete;
  AutoProfilerRegisterThread& operator=(const AutoProfilerRegisterThread&) =
      delete;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class MOZ_RAII AutoProfilerThreadSleep {
 public:
  explicit AutoProfilerThreadSleep(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_thread_sleep();
  }

  ~AutoProfilerThreadSleep() { profiler_thread_wake(); }

 private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

// Temporarily wake up the profiling of a thread while servicing events such as
// Asynchronous Procedure Calls (APCs).
class MOZ_RAII AutoProfilerThreadWake {
 public:
  explicit AutoProfilerThreadWake(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM)
      : mIssuedWake(profiler_thread_is_sleeping()) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
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
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  bool mIssuedWake;
};

// This class creates a non-owning ProfilingStack reference. Objects of this
// class are stack-allocated, and so exist within a thread, and are thus bounded
// by the lifetime of the thread, which ensures that the references held can't
// be used after the ProfilingStack is destroyed.
class MOZ_RAII AutoProfilerLabel {
 public:
  // This is the AUTO_PROFILER_LABEL and AUTO_PROFILER_LABEL_DYNAMIC variant.
  AutoProfilerLabel(const char* aLabel, const char* aDynamicString,
                    JS::ProfilingCategoryPair aCategoryPair,
                    uint32_t aFlags = 0 MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;

    // Get the ProfilingStack from TLS.
    Push(sProfilingStack.get(), aLabel, aDynamicString, aCategoryPair, aFlags);
  }

  // This is the AUTO_PROFILER_LABEL_FAST variant. It retrieves the
  // ProfilingStack from the JSContext and does nothing if the profiler is
  // inactive.
  AutoProfilerLabel(JSContext* aJSContext, const char* aLabel,
                    const char* aDynamicString,
                    JS::ProfilingCategoryPair aCategoryPair,
                    uint32_t aFlags MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    Push(js::GetContextProfilingStackIfEnabled(aJSContext), aLabel,
         aDynamicString, aCategoryPair, aFlags);
  }

  void Push(ProfilingStack* aProfilingStack, const char* aLabel,
            const char* aDynamicString, JS::ProfilingCategoryPair aCategoryPair,
            uint32_t aFlags = 0) {
    // This function runs both on and off the main thread.

    mProfilingStack = aProfilingStack;
    if (mProfilingStack) {
      mProfilingStack->pushLabelFrame(aLabel, aDynamicString, this,
                                      aCategoryPair, aFlags);
    }
  }

  ~AutoProfilerLabel() {
    // This function runs both on and off the main thread.

    if (mProfilingStack) {
      mProfilingStack->pop();
    }
  }

 private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  // We save a ProfilingStack pointer in the ctor so we don't have to redo the
  // TLS lookup in the dtor.
  ProfilingStack* mProfilingStack;

 public:
  // See the comment on the definition in platform.cpp for details about this.
  static MOZ_THREAD_LOCAL(ProfilingStack*) sProfilingStack;
};

class MOZ_RAII AutoProfilerTracing {
 public:
  AutoProfilerTracing(const char* aCategoryString, const char* aMarkerName,
                      JS::ProfilingCategoryPair aCategoryPair,
                      const mozilla::Maybe<nsID>& aDocShellId,
                      const mozilla::Maybe<uint32_t>& aDocShellHistoryId
                          MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mCategoryString(aCategoryString),
        mMarkerName(aMarkerName),
        mCategoryPair(aCategoryPair),
        mDocShellId(aDocShellId),
        mDocShellHistoryId(aDocShellHistoryId) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_tracing(mCategoryString, mMarkerName, aCategoryPair,
                     TRACING_INTERVAL_START, mDocShellId, mDocShellHistoryId);
  }

  AutoProfilerTracing(const char* aCategoryString, const char* aMarkerName,
                      JS::ProfilingCategoryPair aCategoryPair,
                      UniqueProfilerBacktrace aBacktrace,
                      const mozilla::Maybe<nsID>& aDocShellId,
                      const mozilla::Maybe<uint32_t>& aDocShellHistoryId
                          MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mCategoryString(aCategoryString),
        mMarkerName(aMarkerName),
        mCategoryPair(aCategoryPair),
        mDocShellId(aDocShellId),
        mDocShellHistoryId(aDocShellHistoryId) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_tracing(mCategoryString, mMarkerName, aCategoryPair,
                     TRACING_INTERVAL_START, std::move(aBacktrace), mDocShellId,
                     mDocShellHistoryId);
  }

  ~AutoProfilerTracing() {
    profiler_tracing(mCategoryString, mMarkerName, mCategoryPair,
                     TRACING_INTERVAL_END, mDocShellId, mDocShellHistoryId);
  }

 protected:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  const char* mCategoryString;
  const char* mMarkerName;
  const JS::ProfilingCategoryPair mCategoryPair;
  const mozilla::Maybe<nsID> mDocShellId;
  const mozilla::Maybe<uint32_t> mDocShellHistoryId;
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
