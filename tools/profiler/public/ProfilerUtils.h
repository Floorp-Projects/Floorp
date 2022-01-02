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

// Must be called at least once from the main thread, before any other main-
// thread id function.
void profiler_init_main_thread_id();

[[nodiscard]] ProfilerThreadId profiler_main_thread_id();

[[nodiscard]] bool profiler_is_main_thread();

#endif  // ProfilerUtils_h
