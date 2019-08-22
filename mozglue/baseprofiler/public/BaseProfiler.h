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

// Everything in here is also safe to include unconditionally, and only defines
// empty macros if MOZ_GECKO_PROFILER or MOZ_BASE_PROFILER is unset.

// MOZ_BASE_PROFILER is #defined (or not) in this header, so it should be
// #included wherever Base Profiler may be used.

#ifdef MOZ_GECKO_PROFILER
// Enable Base Profiler on Mac and Non-Android Linux, which are supported.
// (Android not implemented yet. Windows not working yet when packaged.)
#  if defined(XP_MACOSX) || (defined(XP_LINUX) && !defined(ANDROID))
#    define MOZ_BASE_PROFILER
#  else
// Other platforms are currently not supported. But you may uncomment the
// following line to enable Base Profiler in your build.
//#    define MOZ_BASE_PROFILER
#  endif
#endif  // MOZ_GECKO_PROFILER

// BaseProfilerCounts.h is also safe to include unconditionally, with empty
// macros if MOZ_BASE_PROFILER is unset.
#include "mozilla/BaseProfilerCounts.h"

#ifndef MOZ_BASE_PROFILER

// This file can be #included unconditionally. However, everything within this
// file must be guarded by a #ifdef MOZ_BASE_PROFILER, *except* for the
// following macros, which encapsulate the most common operations and thus
// avoid the need for many #ifdefs.

#  define AUTO_BASE_PROFILER_INIT

#  define BASE_PROFILER_REGISTER_THREAD(name)
#  define BASE_PROFILER_UNREGISTER_THREAD()
#  define AUTO_BASE_PROFILER_REGISTER_THREAD(name)

#  define AUTO_BASE_PROFILER_THREAD_SLEEP
#  define AUTO_BASE_PROFILER_THREAD_WAKE

#  define AUTO_BASE_PROFILER_LABEL(label, categoryPair)
#  define AUTO_BASE_PROFILER_LABEL_CATEGORY_PAIR(categoryPair)
#  define AUTO_BASE_PROFILER_LABEL_DYNAMIC_CSTR(label, categoryPair, cStr)
#  define AUTO_BASE_PROFILER_LABEL_DYNAMIC_STRING(label, categoryPair, str)
#  define AUTO_BASE_PROFILER_LABEL_FAST(label, categoryPair, ctx)
#  define AUTO_BASE_PROFILER_LABEL_DYNAMIC_FAST(label, dynamicString, \
                                                categoryPair, ctx, flags)

#  define BASE_PROFILER_ADD_MARKER(markerName, categoryPair)

#  define MOZDECLARE_DOCSHELL_AND_HISTORY_ID(docShell)
#  define BASE_PROFILER_TRACING(categoryString, markerName, categoryPair, kind)
#  define BASE_PROFILER_TRACING_DOCSHELL(categoryString, markerName, \
                                         categoryPair, kind, docshell)
#  define AUTO_BASE_PROFILER_TRACING(categoryString, markerName, categoryPair)
#  define AUTO_BASE_PROFILER_TRACING_DOCSHELL(categoryString, markerName, \
                                              categoryPair, docShell)
#  define AUTO_BASE_PROFILER_TEXT_MARKER_CAUSE(markerName, text, categoryPair, \
                                               cause)
#  define AUTO_BASE_PROFILER_TEXT_MARKER_DOCSHELL(markerName, text, \
                                                  categoryPair, docShell)
#  define AUTO_BASE_PROFILER_TEXT_MARKER_DOCSHELL_CAUSE( \
      markerName, text, categoryPair, docShell, cause)

#  define AUTO_PROFILER_STATS(name)

#else  // !MOZ_BASE_PROFILER

#  include "BaseProfilingStack.h"

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

#  include <stdint.h>
#  include <string>

namespace mozilla {

class MallocAllocPolicy;
template <class T, size_t MinInlineCapacity, class AllocPolicy>
class Vector;

namespace baseprofiler {

class ProfilerBacktrace;
class ProfilerMarkerPayload;
class SpliceableJSONWriter;

// Macros used by the AUTO_PROFILER_* macros below.
#  define BASE_PROFILER_RAII_PASTE(id, line) id##line
#  define BASE_PROFILER_RAII_EXPAND(id, line) BASE_PROFILER_RAII_PASTE(id, line)
#  define BASE_PROFILER_RAII BASE_PROFILER_RAII_EXPAND(raiiObject, __LINE__)

//---------------------------------------------------------------------------
// Profiler features
//---------------------------------------------------------------------------

// Higher-order macro containing all the feature info in one place. Define
// |MACRO| appropriately to extract the relevant parts. Note that the number
// values are used internally only and so can be changed without consequence.
// Any changes to this list should also be applied to the feature list in
// toolkit/components/extensions/schemas/geckoProfiler.json.
#  define BASE_PROFILER_FOR_EACH_FEATURE(MACRO)                               \
    MACRO(0, "java", Java, "Profile Java code, Android only")                 \
                                                                              \
    MACRO(1, "js", JS,                                                        \
          "Get the JS engine to expose the JS stack to the profiler")         \
                                                                              \
    /* The DevTools profiler doesn't want the native addresses. */            \
    MACRO(2, "leaf", Leaf, "Include the C++ leaf node if not stackwalking")   \
                                                                              \
    MACRO(3, "mainthreadio", MainThreadIO,                                    \
          "Add main thread I/O to the profile")                               \
                                                                              \
    MACRO(4, "privacy", Privacy,                                              \
          "Do not include user-identifiable information")                     \
                                                                              \
    MACRO(5, "responsiveness", Responsiveness,                                \
          "Collect thread responsiveness information")                        \
                                                                              \
    MACRO(6, "screenshots", Screenshots,                                      \
          "Take a snapshot of the window on every composition")               \
                                                                              \
    MACRO(7, "seqstyle", SequentialStyle,                                     \
          "Disable parallel traversal in styling")                            \
                                                                              \
    MACRO(8, "stackwalk", StackWalk,                                          \
          "Walk the C++ stack, not available on all platforms")               \
                                                                              \
    MACRO(9, "tasktracer", TaskTracer,                                        \
          "Start profiling with feature TaskTracer")                          \
                                                                              \
    MACRO(10, "threads", Threads, "Profile the registered secondary threads") \
                                                                              \
    MACRO(11, "trackopts", TrackOptimizations,                                \
          "Have the JavaScript engine track JIT optimizations")               \
                                                                              \
    MACRO(12, "jstracer", JSTracer, "Enable tracing of the JavaScript engine")

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
  BASE_PROFILER_FOR_EACH_FEATURE(DECLARE)

#  undef DECLARE
};

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
  MFBT_API static void SetActive(uint32_t aFeatures);

  MFBT_API static void SetInactive();

  MFBT_API static bool IsActive();

  MFBT_API static bool IsActiveWithFeature(uint32_t aFeature);

  MFBT_API static bool IsActiveWithoutPrivacy();

 private:
  static const uint32_t Active = 1u << 31;

// Ensure Active doesn't overlap with any of the feature bits.
#  define NO_OVERLAP(n_, str_, Name_, desc_) \
    static_assert(ProfilerFeature::Name_ != Active, "bad Active value");

  BASE_PROFILER_FOR_EACH_FEATURE(NO_OVERLAP);

#  undef NO_OVERLAP

  // We combine the active bit with the feature bits so they can be read or
  // written in a single atomic operation. Accesses to this atomic are not
  // recorded by web replay as they may occur at non-deterministic points.
  // TODO: Could this be MFBT_DATA for better inlining optimization?
  static Atomic<uint32_t, MemoryOrdering::Relaxed,
                recordreplay::Behavior::DontPreserve>
      sActiveAndFeatures;
};

MFBT_API bool IsThreadBeingProfiled();

}  // namespace detail

//---------------------------------------------------------------------------
// Start and stop the profiler
//---------------------------------------------------------------------------

static constexpr PowerOfTwo32 BASE_PROFILER_DEFAULT_ENTRIES =
#  if !defined(ARCH_ARMV6)
    MakePowerOfTwo32<1u << 20>();  // 1'048'576
#  else
    MakePowerOfTwo32<1u << 17>();  // 131'072
#  endif

// Startup profiling usually need to capture more data, especially on slow
// systems.
static constexpr PowerOfTwo32 BASE_PROFILER_DEFAULT_STARTUP_ENTRIES =
#  if !defined(ARCH_ARMV6)
    MakePowerOfTwo32<1u << 22>();  // 4'194'304
#  else
    MakePowerOfTwo32<1u << 17>();  // 131'072
#  endif

#  define BASE_PROFILER_DEFAULT_DURATION 20
#  define BASE_PROFILER_DEFAULT_INTERVAL 1

// Initialize the profiler. If MOZ_BASE_PROFILER_STARTUP is set the profiler
// will also be started. This call must happen before any other profiler calls
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
//   "aCapacity" is the maximum number of entries in the profiler's circular
//               buffer.
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
MFBT_API void profiler_register_page(const std::string& aDocShellId,
                                     uint32_t aHistoryId,
                                     const std::string& aUrl, bool aIsSubFrame);
// Unregister pages with the profiler.
//
// Take a docShellId and unregister all the page entries that have the given ID.
MFBT_API void profiler_unregister_pages(
    const std::string& aRegisteredDocShellId);

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
// Timeline markers will still be stored. This feature will keep JavaScript
// profiling enabled, thus allowing toggling the profiler without invalidating
// the JIT.
MFBT_API void profiler_pause();
MFBT_API void profiler_resume();

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
  return baseprofiler::detail::RacyFeatures::IsActive();
}

// Is the profiler active, and is the current thread being profiled?
// (Same caveats and recommented usage as profiler_is_active().)
inline bool profiler_thread_is_being_profiled() {
  return profiler_is_active() && baseprofiler::detail::IsThreadBeingProfiled();
}

// Is the profiler active and paused? Returns false if the profiler is inactive.
MFBT_API bool profiler_is_paused();

// Is the current thread sleeping?
MFBT_API bool profiler_thread_is_sleeping();

// Get all the features supported by the profiler that are accepted by
// profiler_start(). The result is the same whether the profiler is active or
// not.
MFBT_API uint32_t profiler_get_available_features();

// Check if a profiler feature (specified via the ProfilerFeature type) is
// active. Returns false if the profiler is inactive. Note: the return value
// can become immediately out-of-date, much like the return value of
// profiler_is_active().
MFBT_API bool profiler_feature_active(uint32_t aFeature);

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

// Get the current process's ID.
MFBT_API int profiler_current_process_id();

// Get the current thread's ID.
MFBT_API int profiler_current_thread_id();

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
// are used. |Privacy| and |Leaf| are the only relevant ones.
MFBT_API void profiler_suspend_and_sample_thread(
    int aThreadId, uint32_t aFeatures, ProfilerStackCollector& aCollector,
    bool aSampleNative = true);

struct ProfilerBacktraceDestructor {
  MFBT_API void operator()(ProfilerBacktrace*);
};

using UniqueProfilerBacktrace =
    UniquePtr<ProfilerBacktrace, ProfilerBacktraceDestructor>;

// Immediately capture the current thread's call stack and return it. A no-op
// if the profiler is inactive or in privacy mode.
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
  // Buffer capacity in number of entries.
  uint32_t mEntryCount;
  // Sampling stats: Interval (ns) between successive samplings.
  ProfilerStats mIntervalsNs;
  // Sampling stats: Total duration (ns) of each sampling. (Split detail below.)
  ProfilerStats mOverheadsNs;
  // Sampling stats: Time (ns) to acquire the lock before sampling.
  ProfilerStats mLockingsNs;
  // Sampling stats: Time (ns) to discard expired data.
  ProfilerStats mCleaningsNs;
  // Sampling stats: Time (ns) to collect counter data.
  ProfilerStats mCountersNs;
  // Sampling stats: Time (ns) to sample thread stacks.
  ProfilerStats mThreadsNs;
};

// Get information about the current buffer status.
// Returns Nothing() if the profiler is inactive.
//
// This information may be useful to a user-interface displaying the current
// status of the profiler, allowing the user to get a sense for how fast the
// buffer is being written to, and how much data is visible.
MFBT_API Maybe<ProfilerBufferInfo> profiler_get_buffer_info();

// Uncomment the following line to display profiler runtime statistics at
// shutdown.
// #  define PROFILER_RUNTIME_STATS

#  ifdef PROFILER_RUNTIME_STATS
struct StaticBaseProfilerStats {
  explicit StaticBaseProfilerStats(const char* aName) : mName(aName) {}
  ~StaticBaseProfilerStats() {
    long long n = static_cast<long long>(mNumberDurations);
    if (n != 0) {
      long long sumNs = static_cast<long long>(mSumDurationsNs);
      printf("Profiler stats `%s`: %lld ns / %lld = %lld ns\n", mName, sumNs, n,
             sumNs / n);
    } else {
      printf("Profiler stats `%s`: (nothing)\n", mName);
    }
  }
  Atomic<long long> mSumDurationsNs{0};
  Atomic<long long> mNumberDurations{0};

 private:
  const char* mName;
};

class MOZ_RAII AutoProfilerStats {
 public:
  explicit AutoProfilerStats(
      StaticBaseProfilerStats& aStats MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mStats(aStats), mStart(TimeStamp::NowUnfuzzed()) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }

  ~AutoProfilerStats() {
    mStats.mSumDurationsNs += int64_t(
        (TimeStamp::NowUnfuzzed() - mStart).ToMicroseconds() * 1000 + 0.5);
    ++mStats.mNumberDurations;
  }

 private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  StaticBaseProfilerStats& mStats;
  TimeStamp mStart;
};

#    define AUTO_PROFILER_STATS(name)                                      \
      static ::mozilla::baseprofiler::StaticBaseProfilerStats sStat##name( \
          #name);                                                          \
      ::mozilla::baseprofiler::AutoProfilerStats autoStat##name(sStat##name);

#  else  // PROFILER_RUNTIME_STATS

#    define AUTO_PROFILER_STATS(name)

#  endif  // PROFILER_RUNTIME_STATS else

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
// Use AUTO_BASE_PROFILER_LABEL_DYNAMIC_* if you want to add additional /
// dynamic information to the label stack frame.
#  define AUTO_BASE_PROFILER_LABEL(label, categoryPair)            \
    ::mozilla::baseprofiler::AutoProfilerLabel BASE_PROFILER_RAII( \
        label, nullptr,                                            \
        ::mozilla::baseprofiler::ProfilingCategoryPair::categoryPair)

// Similar to AUTO_BASE_PROFILER_LABEL, but with only one argument: the category
// pair. The label string is taken from the category pair. This is convenient
// for labels like
// AUTO_BASE_PROFILER_LABEL_CATEGORY_PAIR(GRAPHICS_LayerBuilding) which would
// otherwise just repeat the string.
#  define AUTO_BASE_PROFILER_LABEL_CATEGORY_PAIR(categoryPair)         \
    ::mozilla::baseprofiler::AutoProfilerLabel BASE_PROFILER_RAII(     \
        "", nullptr,                                                   \
        ::mozilla::baseprofiler::ProfilingCategoryPair::categoryPair,  \
        uint32_t(::mozilla::baseprofiler::ProfilingStackFrame::Flags:: \
                     LABEL_DETERMINED_BY_CATEGORY_PAIR))

// Similar to AUTO_BASE_PROFILER_LABEL, but with an additional string. The
// inserted RAII object stores the cStr pointer in a field; it does not copy the
// string.
//
// WARNING: This means that the string you pass to this macro needs to live at
// least until the end of the current scope. Be careful using this macro with
// ns[C]String; the other AUTO_BASE_PROFILER_LABEL_DYNAMIC_* macros below are
// preferred because they avoid this problem.
//
// If the profiler samples the current thread and walks the label stack while
// this RAII object is on the stack, it will copy the supplied string into the
// profile buffer. So there's one string copy operation, and it happens at
// sample time.
//
// Compare this to the plain AUTO_BASE_PROFILER_LABEL macro, which only accepts
// literal strings: When the label stack frames generated by
// AUTO_BASE_PROFILER_LABEL are sampled, no string copy needs to be made because
// the profile buffer can just store the raw pointers to the literal strings.
// Consequently, AUTO_BASE_PROFILER_LABEL frames take up considerably less space
// in the profile buffer than AUTO_BASE_PROFILER_LABEL_DYNAMIC_* frames.
#  define AUTO_BASE_PROFILER_LABEL_DYNAMIC_CSTR(label, categoryPair, cStr) \
    ::mozilla::baseprofiler::AutoProfilerLabel BASE_PROFILER_RAII(         \
        label, cStr,                                                       \
        ::mozilla::baseprofiler::ProfilingCategoryPair::categoryPair)

// Similar to AUTO_BASE_PROFILER_LABEL_DYNAMIC_CSTR, but takes an std::string.
//
// Note: The use of the Maybe<>s ensures the scopes for the dynamic string and
// the AutoProfilerLabel are appropriate, while also not incurring the runtime
// cost of the string assignment unless the profiler is active. Therefore,
// unlike AUTO_BASE_PROFILER_LABEL and AUTO_BASE_PROFILER_LABEL_DYNAMIC_CSTR,
// this macro doesn't push/pop a label when the profiler is inactive.
#  define AUTO_BASE_PROFILER_LABEL_DYNAMIC_STRING(label, categoryPair, str) \
    Maybe<std::string> autoStr;                                             \
    Maybe<::mozilla::baseprofiler::AutoProfilerLabel> raiiObjectString;     \
    if (::mozilla::baseprofiler::profiler_is_active()) {                    \
      autoStr.emplace(str);                                                 \
      raiiObjectString.emplace(                                             \
          label, autoStr->c_str(),                                          \
          ::mozilla::baseprofiler::ProfilingCategoryPair::categoryPair);    \
    }

// Similar to AUTO_BASE_PROFILER_LABEL, but accepting a JSContext* parameter,
// and a no-op if the profiler is disabled. Used to annotate functions for which
// overhead in the range of nanoseconds is noticeable. It avoids overhead from
// the TLS lookup because it can get the ProfilingStack from the JS context, and
// avoids almost all overhead in the case where the profiler is disabled.
#  define AUTO_BASE_PROFILER_LABEL_FAST(label, categoryPair, ctx)  \
    ::mozilla::baseprofiler::AutoProfilerLabel BASE_PROFILER_RAII( \
        ctx, label, nullptr,                                       \
        ::mozilla::baseprofiler::ProfilingCategoryPair::categoryPair)

// Similar to AUTO_BASE_PROFILER_LABEL_FAST, but also takes an extra string and
// an additional set of flags. The flags parameter should carry values from the
// ProfilingStackFrame::Flags enum.
#  define AUTO_BASE_PROFILER_LABEL_DYNAMIC_FAST(label, dynamicString,     \
                                                categoryPair, ctx, flags) \
    ::mozilla::baseprofiler::AutoProfilerLabel BASE_PROFILER_RAII(        \
        ctx, label, dynamicString,                                        \
        ::mozilla::baseprofiler::ProfilingCategoryPair::categoryPair, flags)

// Insert a marker in the profile timeline. This is useful to delimit something
// important happening such as the first paint. Unlike labels, which are only
// recorded in the profile buffer if a sample is collected while the label is
// on the label stack, markers will always be recorded in the profile buffer.
// aMarkerName is copied, so the caller does not need to ensure it lives for a
// certain length of time. A no-op if the profiler is inactive or in privacy
// mode.

#  define BASE_PROFILER_ADD_MARKER(markerName, categoryPair) \
    ::mozilla::baseprofiler::profiler_add_marker(            \
        markerName,                                          \
        ::mozilla::baseprofiler::ProfilingCategoryPair::categoryPair)

MFBT_API void profiler_add_marker(const char* aMarkerName,
                                  ProfilingCategoryPair aCategoryPair);
MFBT_API void profiler_add_marker(const char* aMarkerName,
                                  ProfilingCategoryPair aCategoryPair,
                                  UniquePtr<ProfilerMarkerPayload> aPayload);
MFBT_API void profiler_add_js_marker(const char* aMarkerName);

// Insert a marker in the profile timeline for a specified thread.
MFBT_API void profiler_add_marker_for_thread(
    int aThreadId, ProfilingCategoryPair aCategoryPair, const char* aMarkerName,
    UniquePtr<ProfilerMarkerPayload> aPayload);

enum TracingKind {
  TRACING_EVENT,
  TRACING_INTERVAL_START,
  TRACING_INTERVAL_END,
};

// Helper macro to retrieve DocShellId and DocShellHistoryId from docShell
#  define MOZDECLARE_DOCSHELL_AND_HISTORY_ID(docShell)   \
    Maybe<std::string> docShellId;                       \
    Maybe<uint32_t> docShellHistoryId;                   \
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

#  define BASE_PROFILER_TRACING(categoryString, markerName, categoryPair, \
                                kind)                                     \
    ::mozilla::baseprofiler::profiler_tracing(                            \
        categoryString, markerName,                                       \
        ::mozilla::baseprofiler::ProfilingCategoryPair::categoryPair, kind)
#  define BASE_PROFILER_TRACING_DOCSHELL(categoryString, markerName,        \
                                         categoryPair, kind, docShell)      \
    MOZDECLARE_DOCSHELL_AND_HISTORY_ID(docShell);                           \
    ::mozilla::baseprofiler::profiler_tracing(                              \
        categoryString, markerName,                                         \
        ::mozilla::baseprofiler::ProfilingCategoryPair::categoryPair, kind, \
        docShellId, docShellHistoryId)

MFBT_API void profiler_tracing(
    const char* aCategoryString, const char* aMarkerName,
    ProfilingCategoryPair aCategoryPair, TracingKind aKind,
    const Maybe<std::string>& aDocShellId = Nothing(),
    const Maybe<uint32_t>& aDocShellHistoryId = Nothing());
MFBT_API void profiler_tracing(
    const char* aCategoryString, const char* aMarkerName,
    ProfilingCategoryPair aCategoryPair, TracingKind aKind,
    UniqueProfilerBacktrace aCause,
    const Maybe<std::string>& aDocShellId = Nothing(),
    const Maybe<uint32_t>& aDocShellHistoryId = Nothing());

// Adds a START/END pair of tracing markers.
#  define AUTO_BASE_PROFILER_TRACING(categoryString, markerName, categoryPair) \
    ::mozilla::baseprofiler::AutoProfilerTracing BASE_PROFILER_RAII(           \
        categoryString, markerName,                                            \
        ::mozilla::baseprofiler::ProfilingCategoryPair::categoryPair,          \
        Nothing(), Nothing())
#  define AUTO_BASE_PROFILER_TRACING_DOCSHELL(categoryString, markerName, \
                                              categoryPair, docShell)     \
    MOZDECLARE_DOCSHELL_AND_HISTORY_ID(docShell);                         \
    ::mozilla::baseprofiler::AutoProfilerTracing BASE_PROFILER_RAII(      \
        categoryString, markerName,                                       \
        ::mozilla::baseprofiler::ProfilingCategoryPair::categoryPair,     \
        docShellId, docShellHistoryId)

// Add a text marker. Text markers are similar to tracing markers, with the
// difference that text markers have their "text" separate from the marker name;
// multiple text markers with the same name can have different text, and these
// markers will still be displayed in the same "row" in the UI.
// Another difference is that text markers combine the start and end markers
// into one marker.
MFBT_API void profiler_add_text_marker(
    const char* aMarkerName, const std::string& aText,
    ProfilingCategoryPair aCategoryPair, const TimeStamp& aStartTime,
    const TimeStamp& aEndTime,
    const Maybe<std::string>& aDocShellId = Nothing(),
    const Maybe<uint32_t>& aDocShellHistoryId = Nothing(),
    UniqueProfilerBacktrace aCause = nullptr);

class MOZ_RAII AutoProfilerTextMarker {
 public:
  AutoProfilerTextMarker(const char* aMarkerName, const std::string& aText,
                         ProfilingCategoryPair aCategoryPair,
                         const Maybe<std::string>& aDocShellId,
                         const Maybe<uint32_t>& aDocShellHistoryId,
                         UniqueProfilerBacktrace&& aCause =
                             nullptr MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mMarkerName(aMarkerName),
        mText(aText),
        mCategoryPair(aCategoryPair),
        mStartTime(TimeStamp::NowUnfuzzed()),
        mCause(std::move(aCause)),
        mDocShellId(aDocShellId),
        mDocShellHistoryId(aDocShellHistoryId) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }

  ~AutoProfilerTextMarker() {
    profiler_add_text_marker(mMarkerName, mText, mCategoryPair, mStartTime,
                             TimeStamp::NowUnfuzzed(), mDocShellId,
                             mDocShellHistoryId, std::move(mCause));
  }

 protected:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  const char* mMarkerName;
  std::string mText;
  const ProfilingCategoryPair mCategoryPair;
  TimeStamp mStartTime;
  UniqueProfilerBacktrace mCause;
  const Maybe<std::string> mDocShellId;
  const Maybe<uint32_t> mDocShellHistoryId;
};

#  define AUTO_BASE_PROFILER_TEXT_MARKER_CAUSE(markerName, text, categoryPair, \
                                               cause)                          \
    ::mozilla::baseprofiler::AutoProfilerTextMarker BASE_PROFILER_RAII(        \
        markerName, text,                                                      \
        ::mozilla::baseprofiler::ProfilingCategoryPair::categoryPair,          \
        mozilla::Nothing(), mozilla::Nothing(), cause)

#  define AUTO_BASE_PROFILER_TEXT_MARKER_DOCSHELL(markerName, text,       \
                                                  categoryPair, docShell) \
    MOZDECLARE_DOCSHELL_AND_HISTORY_ID(docShell);                         \
    ::mozilla::baseprofiler::AutoProfilerTextMarker BASE_PROFILER_RAII(   \
        markerName, text,                                                 \
        ::mozilla::baseprofiler::ProfilingCategoryPair::categoryPair,     \
        docShellId, docShellHistoryId)

#  define AUTO_BASE_PROFILER_TEXT_MARKER_DOCSHELL_CAUSE(                \
      markerName, text, categoryPair, docShell, cause)                  \
    MOZDECLARE_DOCSHELL_AND_HISTORY_ID(docShell);                       \
    ::mozilla::baseprofiler::AutoProfilerTextMarker BASE_PROFILER_RAII( \
        markerName, text,                                               \
        ::mozilla::baseprofiler::ProfilingCategoryPair::categoryPair,   \
        docShellId, docShellHistoryId, cause)

//---------------------------------------------------------------------------
// Output profiles
//---------------------------------------------------------------------------

// Set a user-friendly process name, used in JSON stream.
MFBT_API void profiler_set_process_name(const std::string& aProcessName);

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
  // This is the AUTO_BASE_PROFILER_LABEL and AUTO_BASE_PROFILER_LABEL_DYNAMIC
  // variant.
  AutoProfilerLabel(const char* aLabel, const char* aDynamicString,
                    ProfilingCategoryPair aCategoryPair,
                    uint32_t aFlags = 0 MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;

    // Get the ProfilingStack from TLS.
    Push(GetProfilingStack(), aLabel, aDynamicString, aCategoryPair, aFlags);
  }

  void Push(ProfilingStack* aProfilingStack, const char* aLabel,
            const char* aDynamicString, ProfilingCategoryPair aCategoryPair,
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

  MFBT_API static ProfilingStack* GetProfilingStack();

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
                      ProfilingCategoryPair aCategoryPair,
                      const Maybe<std::string>& aDocShellId,
                      const Maybe<uint32_t>& aDocShellHistoryId
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

  AutoProfilerTracing(
      const char* aCategoryString, const char* aMarkerName,
      ProfilingCategoryPair aCategoryPair, UniqueProfilerBacktrace aBacktrace,
      const Maybe<std::string>& aDocShellId,
      const Maybe<uint32_t>& aDocShellHistoryId MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
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
  const ProfilingCategoryPair mCategoryPair;
  const Maybe<std::string> mDocShellId;
  const Maybe<uint32_t> mDocShellHistoryId;
};

// Get the MOZ_BASE_PROFILER_STARTUP* environment variables that should be
// supplied to a child process that is about to be launched, in order
// to make that child process start with the same profiler settings as
// in the current process.  The given function is invoked once for
// each variable to be set.
MFBT_API void GetProfilerEnvVarsForChildProcess(
    std::function<void(const char* key, const char* value)>&& aSetEnv);

}  // namespace baseprofiler
}  // namespace mozilla

#endif  // !MOZ_BASE_PROFILER

#endif  // BaseProfiler_h
