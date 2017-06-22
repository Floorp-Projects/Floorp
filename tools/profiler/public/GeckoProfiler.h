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
// platform-independent "pseudostacks".

#ifndef GeckoProfiler_h
#define GeckoProfiler_h

#include <functional>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/Sprintf.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/UniquePtr.h"
#include "js/TypeDecls.h"
#include "js/ProfilingStack.h"
#include "nscore.h"

// Make sure that we can use std::min here without the Windows headers messing
// with us.
#ifdef min
# undef min
#endif

class ProfilerBacktrace;
class ProfilerMarkerPayload;
class SpliceableJSONWriter;

namespace mozilla {
class MallocAllocPolicy;
template <class T, size_t MinInlineCapacity, class AllocPolicy> class Vector;
} // namespace mozilla

// When the profiler is disabled functions declared with these macros are
// static inline functions (with a trivial return value if they are non-void)
// that will be optimized away during compilation.
#ifdef MOZ_GECKO_PROFILER
# define PROFILER_FUNC(decl, rv)  decl;
# define PROFILER_FUNC_VOID(decl) void decl;
#else
# define PROFILER_FUNC(decl, rv)  static inline decl { return rv; }
# define PROFILER_FUNC_VOID(decl) static inline void decl {}
#endif

//---------------------------------------------------------------------------
// Profiler features
//---------------------------------------------------------------------------

// Higher-order macro containing all the feature info in one place. Define
// |macro| appropriately to extract the relevant parts. Note that the number
// values are used internally only and so can be changed without consequence.
// Any changes to this list should also be applied to the feature list in
// browser/components/extensions/schemas/geckoProfiler.json.
#define PROFILER_FOR_EACH_FEATURE(macro) \
  /* Dump the display list with the textures. */ \
  macro(0, "displaylistdump", DisplayListDump) \
  \
  /* GPU Profiling (may not be supported by the GL). */ \
  macro(1, "gpu", GPU) \
  \
  /* Profile Java code (Android only). */ \
  macro(2, "java", Java) \
  \
  /* Get the JS engine to emit pseudostack entries in prologues/epilogues */ \
  macro(3, "js", JS) \
  \
  /* Dump the layer tree with the textures. */ \
  macro(4, "layersdump", LayersDump) \
  \
  /* Include the C++ leaf node if not stackwalking. */ \
  /* The DevTools profiler doesn't want the native addresses. */ \
  macro(5, "leaf", Leaf) \
  \
  /* Add main thread I/O to the profile. */ \
  macro(6, "mainthreadio", MainThreadIO) \
  \
  /* Add memory measurements (e.g. RSS). */ \
  macro(7, "memory", Memory) \
  \
  /* Do not include user-identifiable information. */ \
  macro(8, "privacy", Privacy) \
  \
  /* Restyle profiling. */ \
  macro(9, "restyle", Restyle) \
  \
  /* Walk the C++ stack. Not available on all platforms. */ \
  macro(10, "stackwalk", StackWalk) \
  \
  /* Start profiling with feature TaskTracer. */ \
  macro(11, "tasktracer", TaskTracer) \
  \
  /* Profile the registered secondary threads. */ \
  macro(12, "threads", Threads)

struct ProfilerFeature
{
  #define DECLARE(n_, str_, Name_) \
    static const uint32_t Name_ = (1u << n_); \
    static bool Has##Name_(uint32_t aFeatures) { return aFeatures & Name_; } \
    static void Set##Name_(uint32_t& aFeatures) { aFeatures |= Name_; } \
    static void Clear##Name_(uint32_t& aFeatures) { aFeatures &= ~Name_; }

  // Define a bitfield constant, a getter, and two setters for each feature.
  PROFILER_FOR_EACH_FEATURE(DECLARE)

  #undef DECLARE
};

//---------------------------------------------------------------------------
// Start and stop the profiler
//---------------------------------------------------------------------------

#if !defined(ARCH_ARMV6)
# define PROFILER_DEFAULT_ENTRIES 1000000
#else
# define PROFILER_DEFAULT_ENTRIES 100000
#endif

#define PROFILER_DEFAULT_INTERVAL 1

// Initialize the profiler. If MOZ_PROFILER_STARTUP is set the profiler will
// also be started. This call must happen before any other profiler calls
// (except profiler_start(), which will call profiler_init() if it hasn't
// already run).
PROFILER_FUNC_VOID(profiler_init(void* stackTop))

// Clean up the profiler module, stopping it if required. This function may
// also save a shutdown profile if requested. No profiler calls should happen
// after this point and all pseudo labels should have been popped.
PROFILER_FUNC_VOID(profiler_shutdown())

// Start the profiler -- initializing it first if necessary -- with the
// selected options. Stops and restarts the profiler if it is already active.
// After starting the profiler is "active". The samples will be recorded in a
// circular buffer.
//   "aEntries" is the number of entries in the profiler's circular buffer.
//   "aInterval" the sampling interval, measured in millseconds.
//   "aFeatures" is the feature set. Features unsupported by this
//               platform/configuration are ignored.
PROFILER_FUNC_VOID(profiler_start(int aEntries, double aInterval,
                                  uint32_t aFeatures,
                                  const char** aFilters, uint32_t aFilterCount))

// Stop the profiler and discard the profile without saving it. A no-op if the
// profiler is inactive. After stopping the profiler is "inactive".
PROFILER_FUNC_VOID(profiler_stop())

//---------------------------------------------------------------------------
// Control the profiler
//---------------------------------------------------------------------------

// Register/unregister threads with the profiler. Both functions operate the
// same whether the profiler is active or inactive.
PROFILER_FUNC_VOID(profiler_register_thread(const char* name,
                                            void* guessStackTop))
PROFILER_FUNC_VOID(profiler_unregister_thread())

// Pause and resume the profiler. No-ops if the profiler is inactive. While
// paused the profile will not take any samples and will not record any data
// into its buffers. The profiler remains fully initialized in this state.
// Timeline markers will still be stored. This feature will keep JavaScript
// profiling enabled, thus allowing toggling the profiler without invalidating
// the JIT.
PROFILER_FUNC_VOID(profiler_pause())
PROFILER_FUNC_VOID(profiler_resume())

// These functions tell the profiler that a thread went to sleep so that we can
// avoid sampling it while it's sleeping. Calling profiler_thread_sleep()
// twice without an intervening profiler_thread_wake() is an error. All three
// functions operate the same whether the profiler is active or inactive.
PROFILER_FUNC_VOID(profiler_thread_sleep())
PROFILER_FUNC_VOID(profiler_thread_wake())

// Called by the JSRuntime's operation callback. This is used to start profiling
// on auxiliary threads. Operates the same whether the profiler is active or
// not.
PROFILER_FUNC_VOID(profiler_js_interrupt_callback())

// Set and clear the current thread's JSContext.
PROFILER_FUNC_VOID(profiler_set_js_context(JSContext* aCx))
PROFILER_FUNC_VOID(profiler_clear_js_context())

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
PROFILER_FUNC(bool profiler_is_active(), false)

// Is the profiler active and paused? Returns false if the profiler is inactive.
PROFILER_FUNC(bool profiler_is_paused(), false)

// Is the current thread sleeping?
PROFILER_FUNC(bool profiler_thread_is_sleeping(), false)

// Get all the features supported by the profiler that are accepted by
// profiler_start(). The result is the same whether the profiler is active or
// not.
PROFILER_FUNC(uint32_t profiler_get_available_features(), 0)

// Check if a profiler feature (specified via the ProfilerFeature type) is
// active. Returns false if the profiler is inactive. Note: the return value
// can become immediately out-of-date, much like the return value of
// profiler_is_active().
PROFILER_FUNC(bool profiler_feature_active(uint32_t aFeature), false)

// Get the params used to start the profiler. Returns 0 and an empty vector
// (via outparams) if the profile is inactive. It's possible that the features
// returned may be slightly different to those requested due to required
// adjustments.
PROFILER_FUNC_VOID(
  profiler_get_start_params(int* aEntrySize, double* aInterval,
                            uint32_t* aFeatures,
                            mozilla::Vector<const char*, 0,
                                            mozilla::MallocAllocPolicy>*
                              aFilters))

// The number of milliseconds since the process started. Operates the same
// whether the profiler is active or inactive.
PROFILER_FUNC(double profiler_time(), 0)

// Get the current thread's ID.
PROFILER_FUNC(int profiler_current_thread_id(), 0)

// This method suspends the thread identified by aThreadId, optionally samples
// it for its native stack, and then calls the callback. The callback is passed
// the native stack's program counters and length as two arguments if
// aSampleNative is true.
//
// WARNING: The target thread is suspended during the callback. Do not try to
// allocate or acquire any locks, or you could deadlock. The target thread will
// have resumed by the time this function returns.
PROFILER_FUNC_VOID(
  profiler_suspend_and_sample_thread(int aThreadId,
                                     const std::function<void(void**, size_t)>&
                                       aCallback,
                                     bool aSampleNative = true))

struct ProfilerBacktraceDestructor
{
#ifdef MOZ_GECKO_PROFILER
  void operator()(ProfilerBacktrace*);
#else
  void operator()(ProfilerBacktrace*) {}
#endif
};

using UniqueProfilerBacktrace =
  mozilla::UniquePtr<ProfilerBacktrace, ProfilerBacktraceDestructor>;

// Immediately capture the current thread's call stack and return it. A no-op
// if the profiler is inactive or in privacy mode.
PROFILER_FUNC(UniqueProfilerBacktrace profiler_get_backtrace(), nullptr)

PROFILER_FUNC_VOID(profiler_get_backtrace_noalloc(char* aOutput,
                                                  size_t aOutputSize))

// Get information about the current buffer status. A no-op when the profiler
// is inactive. Do not call this function; call profiler_get_buffer_info()
// instead.
PROFILER_FUNC_VOID(profiler_get_buffer_info_helper(uint32_t* aCurrentPosition,
                                                   uint32_t* aEntries,
                                                   uint32_t* aGeneration))

// Get information about the current buffer status. Returns (via outparams) the
// current write position in the buffer, the total size of the buffer, and the
// generation of the buffer. Returns zeroes if the profiler is inactive.
//
// This information may be useful to a user-interface displaying the current
// status of the profiler, allowing the user to get a sense for how fast the
// buffer is being written to, and how much data is visible.
static inline void profiler_get_buffer_info(uint32_t* aCurrentPosition,
                                            uint32_t* aEntries,
                                            uint32_t* aGeneration)
{
  *aCurrentPosition = 0;
  *aEntries = 0;
  *aGeneration = 0;

  profiler_get_buffer_info_helper(aCurrentPosition, aEntries, aGeneration);
}

// Get the current thread's PseudoStack.
PROFILER_FUNC(PseudoStack* profiler_get_pseudo_stack(), nullptr)

//---------------------------------------------------------------------------
// Put profiling data into the profiler (labels and markers)
//---------------------------------------------------------------------------

#define PROFILER_APPEND_LINE_NUMBER_PASTE(id, line) id ## line
#define PROFILER_APPEND_LINE_NUMBER_EXPAND(id, line) \
  PROFILER_APPEND_LINE_NUMBER_PASTE(id, line)
#define PROFILER_APPEND_LINE_NUMBER(id) \
  PROFILER_APPEND_LINE_NUMBER_EXPAND(id, __LINE__)

#if defined(__GNUC__) || defined(_MSC_VER)
# define PROFILER_FUNCTION_NAME __FUNCTION__
#else
  // From C99, supported by some C++ compilers. Just the raw function name.
# define PROFILER_FUNCTION_NAME __func__
#endif

// Insert an RAII object in this scope to enter a pseudo stack frame. Any
// samples collected in this scope will contain this label in their pseudo
// stack. The name_space and info arguments must be string literals.
// Use PROFILER_LABEL_DYNAMIC if you want to add additional / dynamic
// information to the pseudo stack frame.
#define PROFILER_LABEL(name_space, info, category) \
  mozilla::AutoProfilerLabel \
  PROFILER_APPEND_LINE_NUMBER(profiler_raii)(name_space "::" info, nullptr, \
                                             __LINE__, category)

// Similar to PROFILER_LABEL, PROFILER_LABEL_FUNC will push/pop the enclosing
// functon name as the pseudostack label.
#define PROFILER_LABEL_FUNC(category) \
  mozilla::AutoProfilerLabel \
  PROFILER_APPEND_LINE_NUMBER(profiler_raii)(PROFILER_FUNCTION_NAME, nullptr, \
                                             __LINE__, category)

// Similar to PROFILER_LABEL, but with an additional string. The inserted RAII
// object stores the dynamicStr pointer in a field; it does not copy the string.
// This means that the string you pass to this macro needs to live at least
// until the end of the current scope.
//
// If the profiler samples the current thread and walks the pseudo stack while
// this RAII object is on the stack, it will copy the supplied string into the
// profile buffer. So there's one string copy operation, and it happens at
// sample time.
//
// Compare this to the plain PROFILER_LABEL macro, which only accepts literal
// strings: When the pseudo stack frames generated by PROFILER_LABEL are
// sampled, no string copy needs to be made because the profile buffer can
// just store the raw pointers to the literal strings. Consequently,
// PROFILER_LABEL frames take up considerably less space in the profile buffer
// than PROFILER_LABEL_DYNAMIC frames.
#define PROFILER_LABEL_DYNAMIC(name_space, info, category, dynamicStr) \
  mozilla::AutoProfilerLabel \
  PROFILER_APPEND_LINE_NUMBER(profiler_raii)(name_space "::" info, dynamicStr, \
                                             __LINE__, category)

// Insert a marker in the profile timeline. This is useful to delimit something
// important happening such as the first paint. Unlike labels, which are only
// recorded in the profile buffer if a sample is collected while the label is
// on the pseudostack, markers will always be recorded in the profile buffer.
// A no-op if the profiler is inactive or in privacy mode.
PROFILER_FUNC_VOID(profiler_add_marker(const char* aMarkerName))
PROFILER_FUNC_VOID(
  profiler_add_marker(const char* aMarkerName,
                      mozilla::UniquePtr<ProfilerMarkerPayload> aPayload))

enum TracingKind {
  TRACING_EVENT,
  TRACING_INTERVAL_START,
  TRACING_INTERVAL_END,
};


// Adds a tracing marker to the PseudoStack. A no-op if the profiler is
// inactive or in privacy mode.
PROFILER_FUNC_VOID(profiler_tracing(const char* aCategory,
                                    const char* aMarkerName,
                                    TracingKind aKind = TRACING_EVENT))
PROFILER_FUNC_VOID(profiler_tracing(const char* aCategory,
                                    const char* aMarkerName,
                                    UniqueProfilerBacktrace aCause,
                                    TracingKind aKind = TRACING_EVENT))

//---------------------------------------------------------------------------
// Output profiles
//---------------------------------------------------------------------------

// Get the profile encoded as a JSON string. A no-op (returning nullptr) if the
// profiler is inactive.
PROFILER_FUNC(
  mozilla::UniquePtr<char[]> profiler_get_profile(double aSinceTime = 0),
  nullptr)

// Write the profile for this process (excluding subprocesses) into aWriter.
// Returns false if the profiler is inactive.
PROFILER_FUNC(
  bool profiler_stream_json_for_this_process(SpliceableJSONWriter& aWriter,
                                             double aSinceTime = 0),
  false)

// Get the profile and write it into a file. A no-op if the profile is
// inactive.
//
// This function is 'extern "C"' so that it is easily callable from a debugger
// in a build without debugging information (a workaround for
// http://llvm.org/bugs/show_bug.cgi?id=22211).
extern "C" {
PROFILER_FUNC_VOID(profiler_save_profile_to_file(const char* aFilename))
}

//---------------------------------------------------------------------------
// RAII classes
//---------------------------------------------------------------------------

namespace mozilla {

class MOZ_RAII AutoProfilerInit
{
public:
  explicit AutoProfilerInit(void* stackTop
                            MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_init(stackTop);
  }

  ~AutoProfilerInit() { profiler_shutdown(); }

private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

// Convenience class to register and unregister a thread with the profiler.
// Needs to be the first object on the stack of the thread.
class MOZ_RAII AutoProfilerRegisterThread final
{
public:
  explicit AutoProfilerRegisterThread(const char* aName
                                MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
  {
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

class MOZ_RAII AutoProfilerThreadSleep
{
public:
  explicit AutoProfilerThreadSleep(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_thread_sleep();
  }

  ~AutoProfilerThreadSleep() { profiler_thread_wake(); }

private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

// Temporarily wake up the profiling of a thread while servicing events such as
// Asynchronous Procedure Calls (APCs).
class MOZ_RAII AutoProfilerThreadWake
{
public:
  explicit AutoProfilerThreadWake(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM)
    : mIssuedWake(profiler_thread_is_sleeping())
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    if (mIssuedWake) {
      profiler_thread_wake();
    }
  }

  ~AutoProfilerThreadWake()
  {
    if (mIssuedWake) {
      MOZ_ASSERT(!profiler_thread_is_sleeping());
      profiler_thread_sleep();
    }
  }

private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  bool mIssuedWake;
};

// This class creates a non-owning PseudoStack reference. Objects of this class
// are stack-allocated, and so exist within a thread, and are thus bounded by
// the lifetime of the thread, which ensures that the references held can't be
// used after the PseudoStack is destroyed.
class MOZ_RAII AutoProfilerLabel
{
public:
  AutoProfilerLabel(const char* aLabel, const char* aDynamicString,
                    uint32_t aLine, js::ProfileEntry::Category aCategory
                    MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;

    // This function runs both on and off the main thread.

#ifdef MOZ_GECKO_PROFILER
    mPseudoStack = sPseudoStack.get();
    if (mPseudoStack) {
      mPseudoStack->pushCppFrame(aLabel, aDynamicString, this, aLine,
                                 js::ProfileEntry::Kind::CPP_NORMAL, aCategory);
    }
#endif
  }

  ~AutoProfilerLabel()
  {
    // This function runs both on and off the main thread.

#ifdef MOZ_GECKO_PROFILER
    if (mPseudoStack) {
      mPseudoStack->pop();
    }
#endif
  }

private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

#ifdef MOZ_GECKO_PROFILER
  // We save a PseudoStack pointer in the ctor so we don't have to redo the TLS
  // lookup in the dtor.
  PseudoStack* mPseudoStack;

public:
  // See the comment on the definition in platform.cpp for details about this.
  static MOZ_THREAD_LOCAL(PseudoStack*) sPseudoStack;
#endif
};

class MOZ_RAII AutoProfilerTracing
{
public:
  AutoProfilerTracing(const char* aCategory, const char* aMarkerName,
                      UniqueProfilerBacktrace aBacktrace
                      MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mCategory(aCategory)
    , mMarkerName(aMarkerName)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_tracing(mCategory, mMarkerName, Move(aBacktrace),
                     TRACING_INTERVAL_START);
  }

  AutoProfilerTracing(const char* aCategory, const char* aMarkerName
                      MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mCategory(aCategory)
    , mMarkerName(aMarkerName)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_tracing(mCategory, mMarkerName, TRACING_INTERVAL_START);
  }

  ~AutoProfilerTracing()
  {
    profiler_tracing(mCategory, mMarkerName, TRACING_INTERVAL_END);
  }

protected:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  const char* mCategory;
  const char* mMarkerName;
};

} // namespace mozilla

#endif  // GeckoProfiler_h
