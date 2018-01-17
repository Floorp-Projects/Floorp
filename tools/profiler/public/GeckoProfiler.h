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

#ifndef MOZ_GECKO_PROFILER

// This file can be #included unconditionally. However, everything within this
// file must be guarded by a #ifdef MOZ_GECKO_PROFILER, *except* for the
// following macros, which encapsulate the most common operations and thus
// avoid the need for many #ifdefs.

#define AUTO_PROFILER_INIT

#define PROFILER_REGISTER_THREAD(name)
#define PROFILER_UNREGISTER_THREAD()
#define AUTO_PROFILER_REGISTER_THREAD(name)

#define AUTO_PROFILER_THREAD_SLEEP
#define AUTO_PROFILER_THREAD_WAKE

#define PROFILER_JS_INTERRUPT_CALLBACK()

#define PROFILER_SET_JS_CONTEXT(cx)
#define PROFILER_CLEAR_JS_CONTEXT()

#define AUTO_PROFILER_LABEL(label, category)
#define AUTO_PROFILER_LABEL_DYNAMIC_CSTR(label, category, cStr)
#define AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING(label, category, nsCStr)
#define AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(label, category, nsStr)

#define PROFILER_ADD_MARKER(markerName)

#define PROFILER_TRACING(category, markerName, kind)
#define AUTO_PROFILER_TRACING(category, markerName)

#else // !MOZ_GECKO_PROFILER

#include <functional>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/Maybe.h"
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
class TimeStamp;
} // namespace mozilla

// Macros used by the AUTO_PROFILER_* macros below.
#define PROFILER_RAII_PASTE(id, line) id ## line
#define PROFILER_RAII_EXPAND(id, line) PROFILER_RAII_PASTE(id, line)
#define PROFILER_RAII PROFILER_RAII_EXPAND(raiiObject, __LINE__)

//---------------------------------------------------------------------------
// Profiler features
//---------------------------------------------------------------------------

// Higher-order macro containing all the feature info in one place. Define
// |macro| appropriately to extract the relevant parts. Note that the number
// values are used internally only and so can be changed without consequence.
// Any changes to this list should also be applied to the feature list in
// browser/components/extensions/schemas/geckoProfiler.json.
#define PROFILER_FOR_EACH_FEATURE(macro) \
  /* Profile Java code (Android only). */ \
  macro(0, "java", Java) \
  \
  /* Get the JS engine to emit pseudostack entries in prologues/epilogues */ \
  macro(1, "js", JS) \
  \
  /* Include the C++ leaf node if not stackwalking. */ \
  /* The DevTools profiler doesn't want the native addresses. */ \
  macro(2, "leaf", Leaf) \
  \
  /* Add main thread I/O to the profile. */ \
  macro(3, "mainthreadio", MainThreadIO) \
  \
  /* Add memory measurements (e.g. RSS). */ \
  macro(4, "memory", Memory) \
  \
  /* Do not include user-identifiable information. */ \
  macro(5, "privacy", Privacy) \
  \
  /* Restyle profiling. */ \
  macro(6, "restyle", Restyle) \
  \
  /* Walk the C++ stack. Not available on all platforms. */ \
  macro(7, "stackwalk", StackWalk) \
  \
  /* Start profiling with feature TaskTracer. */ \
  macro(8, "tasktracer", TaskTracer) \
  \
  /* Profile the registered secondary threads. */ \
  macro(9, "threads", Threads)

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
void profiler_init(void* stackTop);

#define AUTO_PROFILER_INIT \
  mozilla::AutoProfilerInit PROFILER_RAII

// Clean up the profiler module, stopping it if required. This function may
// also save a shutdown profile if requested. No profiler calls should happen
// after this point and all pseudo labels should have been popped.
void profiler_shutdown();

// Start the profiler -- initializing it first if necessary -- with the
// selected options. Stops and restarts the profiler if it is already active.
// After starting the profiler is "active". The samples will be recorded in a
// circular buffer.
//   "aEntries" is the number of entries in the profiler's circular buffer.
//   "aInterval" the sampling interval, measured in millseconds.
//   "aFeatures" is the feature set. Features unsupported by this
//               platform/configuration are ignored.
void profiler_start(int aEntries, double aInterval, uint32_t aFeatures,
                    const char** aFilters, uint32_t aFilterCount);

// Stop the profiler and discard the profile without saving it. A no-op if the
// profiler is inactive. After stopping the profiler is "inactive".
void profiler_stop();

// If the profiler is inactive, start it. If it's already active, restart it if
// the requested settings differ from the current settings. Both the check and
// the state change are performed while the profiler state is locked.
// The only difference to profiler_start is that the current buffer contents are
// not discarded if the profiler is already running with the requested settings.
void profiler_ensure_started(int aEntries, double aInterval,
                             uint32_t aFeatures, const char** aFilters,
                             uint32_t aFilterCount);

//---------------------------------------------------------------------------
// Control the profiler
//---------------------------------------------------------------------------

// Register/unregister threads with the profiler. Both functions operate the
// same whether the profiler is active or inactive.
#define PROFILER_REGISTER_THREAD(name) \
  do { char stackTop; profiler_register_thread(name, &stackTop); } while (0)
#define PROFILER_UNREGISTER_THREAD() \
  profiler_unregister_thread()
void profiler_register_thread(const char* name, void* guessStackTop);
void profiler_unregister_thread();

// Register and unregister a thread within a scope.
#define AUTO_PROFILER_REGISTER_THREAD(name) \
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
#define AUTO_PROFILER_THREAD_SLEEP \
  mozilla::AutoProfilerThreadSleep PROFILER_RAII
#define AUTO_PROFILER_THREAD_WAKE \
  mozilla::AutoProfilerThreadWake PROFILER_RAII

// Called by the JSRuntime's operation callback. This is used to start profiling
// on auxiliary threads. Operates the same whether the profiler is active or
// not.
#define PROFILER_JS_INTERRUPT_CALLBACK() profiler_js_interrupt_callback()
void profiler_js_interrupt_callback();

// Set and clear the current thread's JSContext.
#define PROFILER_SET_JS_CONTEXT(cx) profiler_set_js_context(cx)
#define PROFILER_CLEAR_JS_CONTEXT() profiler_clear_js_context()
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
bool profiler_is_active();

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
void profiler_get_start_params(int* aEntrySize, double* aInterval,
                               uint32_t* aFeatures,
                               mozilla::Vector<const char*, 0,
                                               mozilla::MallocAllocPolicy>*
                                 aFilters);

// The number of milliseconds since the process started. Operates the same
// whether the profiler is active or inactive.
double profiler_time();

// Get the current thread's ID.
int profiler_current_thread_id();

// An object of this class is passed to profiler_suspend_and_sample_thread().
// For each stack frame, one of the Collect methods will be called.
class ProfilerStackCollector
{
public:
  // Some collectors need to worry about possibly overwriting previous
  // generations of data. If that's not an issue, this can return Nothing,
  // which is the default behaviour.
  virtual mozilla::Maybe<uint32_t> Generation() { return mozilla::Nothing(); }

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

  virtual void CollectPseudoEntry(const js::ProfileEntry& aEntry) = 0;
};

// This method suspends the thread identified by aThreadId, samples its
// pseudo-stack, JS stack, and (optionally) native stack, passing the collected
// frames into aCollector. aFeatures dictates which compiler features are used.
// |Privacy| and |Leaf| are the only relevant ones.
void profiler_suspend_and_sample_thread(int aThreadId, uint32_t aFeatures,
                                        ProfilerStackCollector& aCollector,
                                        bool aSampleNative = true);

struct ProfilerBacktraceDestructor
{
  void operator()(ProfilerBacktrace*);
};

using UniqueProfilerBacktrace =
  mozilla::UniquePtr<ProfilerBacktrace, ProfilerBacktraceDestructor>;

// Immediately capture the current thread's call stack and return it. A no-op
// if the profiler is inactive or in privacy mode.
UniqueProfilerBacktrace profiler_get_backtrace();

// Get information about the current buffer status. A no-op when the profiler
// is inactive. Do not call this function; call profiler_get_buffer_info()
// instead.
void profiler_get_buffer_info_helper(uint32_t* aCurrentPosition,
                                     uint32_t* aEntries,
                                     uint32_t* aGeneration);

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
PseudoStack* profiler_get_pseudo_stack();

//---------------------------------------------------------------------------
// Put profiling data into the profiler (labels and markers)
//---------------------------------------------------------------------------

// Insert an RAII object in this scope to enter a pseudo stack frame. Any
// samples collected in this scope will contain this label in their pseudo
// stack. The label argument must be a static C string. It is usually of the
// form "ClassName::FunctionName". (Ideally we'd use the compiler to provide
// that for us, but __func__ gives us the function name without the class
// name.) If the label applies to only part of a function, you can qualify it
// like this: "ClassName::FunctionName:PartName".
//
// Use AUTO_PROFILER_LABEL_DYNAMIC_* if you want to add additional / dynamic
// information to the pseudo stack frame.
#define AUTO_PROFILER_LABEL(label, category) \
  mozilla::AutoProfilerLabel PROFILER_RAII(label, nullptr, __LINE__, \
                                           js::ProfileEntry::Category::category)

// Similar to AUTO_PROFILER_LABEL, but with an additional string. The inserted
// RAII object stores the cStr pointer in a field; it does not copy the string.
//
// WARNING: This means that the string you pass to this macro needs to live at
// least until the end of the current scope. Be careful using this macro with
// ns[C]String; the other AUTO_PROFILER_LABEL_DYNAMIC_* macros below are
// preferred because they avoid this problem.
//
// If the profiler samples the current thread and walks the pseudo stack while
// this RAII object is on the stack, it will copy the supplied string into the
// profile buffer. So there's one string copy operation, and it happens at
// sample time.
//
// Compare this to the plain AUTO_PROFILER_LABEL macro, which only accepts
// literal strings: When the pseudo stack frames generated by
// AUTO_PROFILER_LABEL are sampled, no string copy needs to be made because the
// profile buffer can just store the raw pointers to the literal strings.
// Consequently, AUTO_PROFILER_LABEL frames take up considerably less space in
// the profile buffer than AUTO_PROFILER_LABEL_DYNAMIC_* frames.
#define AUTO_PROFILER_LABEL_DYNAMIC_CSTR(label, category, cStr) \
  mozilla::AutoProfilerLabel \
    PROFILER_RAII(label, cStr, __LINE__, js::ProfileEntry::Category::category)

// Similar to AUTO_PROFILER_LABEL_DYNAMIC_CSTR, but takes an nsACString.
//
// Note: The use of the Maybe<>s ensures the scopes for the dynamic string and
// the AutoProfilerLabel are appropriate, while also not incurring the runtime
// cost of the string assignment unless the profiler is active. Therefore,
// unlike AUTO_PROFILER_LABEL and AUTO_PROFILER_LABEL_DYNAMIC_CSTR, this macro
// doesn't push/pop a label when the profiler is inactive.
#define AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING(label, category, nsCStr) \
  mozilla::Maybe<nsAutoCString> autoCStr; \
  mozilla::Maybe<AutoProfilerLabel> raiiObjectNsCString; \
  if (profiler_is_active()) { \
    autoCStr.emplace(nsCStr); \
    raiiObjectNsCString.emplace(label, autoCStr->get(), __LINE__, \
                                js::ProfileEntry::Category::category); \
  }

// Similar to AUTO_PROFILER_LABEL_DYNAMIC_CSTR, but takes an nsString that is
// is lossily converted to an ASCII string.
//
// Note: The use of the Maybe<>s ensures the scopes for the converted dynamic
// string and the AutoProfilerLabel are appropriate, while also not incurring
// the runtime cost of the string conversion unless the profiler is active.
// Therefore, unlike AUTO_PROFILER_LABEL and AUTO_PROFILER_LABEL_DYNAMIC_CSTR,
// this macro doesn't push/pop a label when the profiler is inactive.
#define AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(label, category, nsStr) \
  mozilla::Maybe<NS_LossyConvertUTF16toASCII> asciiStr; \
  mozilla::Maybe<AutoProfilerLabel> raiiObjectLossyNsString; \
  if (profiler_is_active()) { \
    asciiStr.emplace(nsStr); \
    raiiObjectLossyNsString.emplace(label, asciiStr->get(), __LINE__, \
                                    js::ProfileEntry::Category::category); \
  }

// Insert a marker in the profile timeline. This is useful to delimit something
// important happening such as the first paint. Unlike labels, which are only
// recorded in the profile buffer if a sample is collected while the label is
// on the pseudostack, markers will always be recorded in the profile buffer.
// aMarkerName is copied, so the caller does not need to ensure it lives for a
// certain length of time. A no-op if the profiler is inactive or in privacy
// mode.
#define PROFILER_ADD_MARKER(markerName) \
  profiler_add_marker(markerName)
void profiler_add_marker(const char* aMarkerName);
void profiler_add_marker(const char* aMarkerName,
                         mozilla::UniquePtr<ProfilerMarkerPayload> aPayload);

// Insert a marker in the profile timeline for a specified thread.
void profiler_add_marker_for_thread(int aThreadId,
                                    const char* aMarkerName,
                                    mozilla::UniquePtr<ProfilerMarkerPayload> aPayload);

enum TracingKind {
  TRACING_EVENT,
  TRACING_INTERVAL_START,
  TRACING_INTERVAL_END,
};

// Adds a tracing marker to the profile. A no-op if the profiler is inactive or
// in privacy mode.
#define PROFILER_TRACING(category, markerName, kind) \
  profiler_tracing(category, markerName, kind)
void profiler_tracing(const char* aCategory, const char* aMarkerName,
                      TracingKind aKind);
void profiler_tracing(const char* aCategory, const char* aMarkerName,
                      TracingKind aKind, UniqueProfilerBacktrace aCause);

// Adds a START/END pair of tracing markers.
#define AUTO_PROFILER_TRACING(category, markerName) \
  mozilla::AutoProfilerTracing PROFILER_RAII(category, markerName)

//---------------------------------------------------------------------------
// Output profiles
//---------------------------------------------------------------------------

// Get the profile encoded as a JSON string. A no-op (returning nullptr) if the
// profiler is inactive.
// If aIsShuttingDown is true, the current time is included as the process
// shutdown time in the JSON's "meta" object.
mozilla::UniquePtr<char[]> profiler_get_profile(double aSinceTime = 0,
                                                bool aIsShuttingDown = false);

// Write the profile for this process (excluding subprocesses) into aWriter.
// Returns false if the profiler is inactive.
bool profiler_stream_json_for_this_process(SpliceableJSONWriter& aWriter,
                                           double aSinceTime = 0,
                                           bool aIsShuttingDown = false,
                                           mozilla::TimeStamp* aOutFirstSampleTime = nullptr);

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

class MOZ_RAII AutoProfilerInit
{
public:
  explicit AutoProfilerInit(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_init(this);
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

    mPseudoStack = sPseudoStack.get();
    if (mPseudoStack) {
      mPseudoStack->pushCppFrame(aLabel, aDynamicString, this, aLine,
                                 js::ProfileEntry::Kind::CPP_NORMAL, aCategory);
    }
  }

  ~AutoProfilerLabel()
  {
    // This function runs both on and off the main thread.

    if (mPseudoStack) {
      mPseudoStack->pop();
    }
  }

private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  // We save a PseudoStack pointer in the ctor so we don't have to redo the TLS
  // lookup in the dtor.
  PseudoStack* mPseudoStack;

public:
  // See the comment on the definition in platform.cpp for details about this.
  static MOZ_THREAD_LOCAL(PseudoStack*) sPseudoStack;
};

class MOZ_RAII AutoProfilerTracing
{
public:
  AutoProfilerTracing(const char* aCategory, const char* aMarkerName
                      MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mCategory(aCategory)
    , mMarkerName(aMarkerName)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_tracing(mCategory, mMarkerName, TRACING_INTERVAL_START);
  }

  AutoProfilerTracing(const char* aCategory, const char* aMarkerName,
                      UniqueProfilerBacktrace aBacktrace
                      MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mCategory(aCategory)
    , mMarkerName(aMarkerName)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_tracing(mCategory, mMarkerName, TRACING_INTERVAL_START,
                     Move(aBacktrace));
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

// Set MOZ_PROFILER_STARTUP* environment variables that will be inherited into
// a child process that is about to be launched, in order to make that child
// process start with the same profiler settings as in the current process.
class MOZ_RAII AutoSetProfilerEnvVarsForChildProcess
{
public:
  explicit AutoSetProfilerEnvVarsForChildProcess(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
  ~AutoSetProfilerEnvVarsForChildProcess();

private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  char mSetEntries[64];
  char mSetInterval[64];
  char mSetFeaturesBitfield[64];
  char mSetFilters[1024];
};

} // namespace mozilla

#endif // !MOZ_GECKO_PROFILER

#endif  // GeckoProfiler_h
