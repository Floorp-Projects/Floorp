/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// APIs that inform the profiler when a thread is effectively asleep so that we
// can avoid sampling it more than once.

#ifndef ProfilerThreadSleep_h
#define ProfilerThreadSleep_h

#ifndef MOZ_GECKO_PROFILER

// This file can be #included unconditionally. However, everything within this
// file must be guarded by a #ifdef MOZ_GECKO_PROFILER, *except* for the
// following macros and functions, which encapsulate the most common operations
// and thus avoid the need for many #ifdefs.

#  define AUTO_PROFILER_THREAD_SLEEP

static inline void profiler_thread_sleep() {}

static inline void profiler_thread_wake() {}

#else  // !MOZ_GECKO_PROFILER

#  include "mozilla/Attributes.h"
#  include "mozilla/BaseProfilerRAIIMacro.h"

// These functions tell the profiler that a thread went to sleep so that we can
// avoid sampling it more than once while it's sleeping. Calling
// profiler_thread_sleep() twice without an intervening profiler_thread_wake()
// is an error. All three functions operate the same whether the profiler is
// active or inactive.
void profiler_thread_sleep();
void profiler_thread_wake();

// Mark a thread as asleep within a scope.
// (See also AUTO_PROFILER_THREAD_WAKE in ProfilerThreadState.h)
#  define AUTO_PROFILER_THREAD_SLEEP \
    mozilla::AutoProfilerThreadSleep PROFILER_RAII

namespace mozilla {

// (See also AutoProfilerThreadWake in ProfilerThreadState.h)
class MOZ_RAII AutoProfilerThreadSleep {
 public:
  explicit AutoProfilerThreadSleep() { profiler_thread_sleep(); }

  ~AutoProfilerThreadSleep() { profiler_thread_wake(); }
};

}  // namespace mozilla

#endif  // !MOZ_GECKO_PROFILER

#endif  // ProfilerThreadSleep_h
