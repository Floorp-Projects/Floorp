/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// APIs that control the lifetime of the profiler: Initialization, start, pause,
// resume, stop, and shutdown.

#ifndef ProfilerControl_h
#define ProfilerControl_h

#include "mozilla/BaseProfilerRAIIMacro.h"

// Everything in here is also safe to include unconditionally, and only defines
// empty macros if MOZ_GECKO_PROFILER is unset.
// If your file only uses particular APIs (e.g., only markers), please consider
// including only the needed headers instead of this one, to reduce compilation
// dependencies.

enum class IsFastShutdown {
  No,
  Yes,
};

#ifndef MOZ_GECKO_PROFILER

// This file can be #included unconditionally. However, everything within this
// file must be guarded by a #ifdef MOZ_GECKO_PROFILER, *except* for the
// following macros and functions, which encapsulate the most common operations
// and thus avoid the need for many #ifdefs.

#  define AUTO_PROFILER_INIT ::profiler_init_main_thread_id()
#  define AUTO_PROFILER_INIT2

// Function stubs for when MOZ_GECKO_PROFILER is not defined.

static inline void profiler_init(void* stackTop) {}

static inline void profiler_shutdown(
    IsFastShutdown aIsFastShutdown = IsFastShutdown::No) {}

#else  // !MOZ_GECKO_PROFILER

#  include "BaseProfiler.h"
#  include "mozilla/Attributes.h"
#  include "mozilla/Maybe.h"
#  include "mozilla/MozPromise.h"
#  include "mozilla/PowerOfTwo.h"
#  include "mozilla/Vector.h"

//---------------------------------------------------------------------------
// Start and stop the profiler
//---------------------------------------------------------------------------

static constexpr mozilla::PowerOfTwo32 PROFILER_DEFAULT_ENTRIES =
    mozilla::baseprofiler::BASE_PROFILER_DEFAULT_ENTRIES;

static constexpr mozilla::PowerOfTwo32 PROFILER_DEFAULT_STARTUP_ENTRIES =
    mozilla::baseprofiler::BASE_PROFILER_DEFAULT_STARTUP_ENTRIES;

#  define PROFILER_DEFAULT_INTERVAL BASE_PROFILER_DEFAULT_INTERVAL
#  define PROFILER_MAX_INTERVAL BASE_PROFILER_MAX_INTERVAL

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
// Returns as soon as this process' profiler has started, the returned promise
// gets resolved when profilers in sub-processes (if any) have started.
RefPtr<mozilla::GenericPromise> profiler_start(
    mozilla::PowerOfTwo32 aCapacity, double aInterval, uint32_t aFeatures,
    const char** aFilters, uint32_t aFilterCount, uint64_t aActiveTabID,
    const mozilla::Maybe<double>& aDuration = mozilla::Nothing());

// Stop the profiler and discard the profile without saving it. A no-op if the
// profiler is inactive. After stopping the profiler is "inactive".
// Returns as soon as this process' profiler has stopped, the returned promise
// gets resolved when profilers in sub-processes (if any) have stopped.
RefPtr<mozilla::GenericPromise> profiler_stop();

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

// Pause and resume the profiler. No-ops if the profiler is inactive. While
// paused the profile will not take any samples and will not record any data
// into its buffers. The profiler remains fully initialized in this state.
// Timeline markers will still be stored. This feature will keep JavaScript
// profiling enabled, thus allowing toggling the profiler without invalidating
// the JIT.
// Returns as soon as this process' profiler has paused/resumed, the returned
// promise gets resolved when profilers in sub-processes (if any) have
// paused/resumed.
RefPtr<mozilla::GenericPromise> profiler_pause();
RefPtr<mozilla::GenericPromise> profiler_resume();

// Only pause and resume the periodic sampling loop, including stack sampling,
// counters, and profiling overheads.
// Returns as soon as this process' profiler has paused/resumed sampling, the
// returned promise gets resolved when profilers in sub-processes (if any) have
// paused/resumed sampling.
RefPtr<mozilla::GenericPromise> profiler_pause_sampling();
RefPtr<mozilla::GenericPromise> profiler_resume_sampling();

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

}  // namespace mozilla

#endif  // !MOZ_GECKO_PROFILER

#endif  // ProfilerControl_h
