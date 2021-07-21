/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BaseProfilerUtils_h
#define BaseProfilerUtils_h

// This header contains most process- and thread-related functions.
// It is safe to include unconditionally.

#ifndef MOZ_GECKO_PROFILER

namespace mozilla::baseprofiler {

[[nodiscard]] inline int profiler_current_process_id() { return 0; }
[[nodiscard]] inline int profiler_current_thread_id() { return 0; }
[[nodiscard]] inline int profiler_main_thread_id() { return 0; }
[[nodiscard]] inline bool profiler_is_main_thread() { return false; }

}  // namespace mozilla::baseprofiler

#else  // !MOZ_GECKO_PROFILER

#  include "mozilla/Types.h"

namespace mozilla::baseprofiler {

// Get the current process's ID.
[[nodiscard]] MFBT_API int profiler_current_process_id();

// Get the current thread's ID.
[[nodiscard]] MFBT_API int profiler_current_thread_id();

namespace detail {
// Statically initialized to 0, then set once from profiler_init(), which should
// be called from the main thread before any other use of the profiler.
extern MFBT_DATA int scProfilerMainThreadId;
}  // namespace detail

[[nodiscard]] inline int profiler_main_thread_id() {
  return detail::scProfilerMainThreadId;
}

[[nodiscard]] inline bool profiler_is_main_thread() {
  return profiler_current_thread_id() == profiler_main_thread_id();
}

}  // namespace mozilla::baseprofiler

#endif  // !MOZ_GECKO_PROFILER

#endif  // BaseProfilerUtils_h
