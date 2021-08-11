/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerUtils_h
#define ProfilerUtils_h

// This header contains most process- and thread-related functions.
// It is safe to include unconditionally.

#include "mozilla/BaseProfilerUtils.h"

using ProfilerProcessId = mozilla::baseprofiler::BaseProfilerProcessId;
using ProfilerThreadId = mozilla::baseprofiler::BaseProfilerThreadId;

// Get the current process's ID.
[[nodiscard]] ProfilerProcessId profiler_current_process_id();

// Get the current thread's ID.
[[nodiscard]] ProfilerThreadId profiler_current_thread_id();

namespace mozilla::profiler::detail {
// Statically initialized to 0, then set once from profiler_init(), which should
// be called from the main thread before any other use of the profiler.
extern ProfilerThreadId scProfilerMainThreadId;
}  // namespace mozilla::profiler::detail

[[nodiscard]] inline ProfilerThreadId profiler_main_thread_id() {
  return mozilla::profiler::detail::scProfilerMainThreadId;
}

[[nodiscard]] inline bool profiler_is_main_thread() {
  return profiler_current_thread_id() == profiler_main_thread_id();
}

#endif  // ProfilerUtils_h
