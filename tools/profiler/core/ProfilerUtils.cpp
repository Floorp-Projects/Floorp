/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file implements functions from ProfilerUtils.h on all platforms.
// Functions with platform-specific implementations are separated in #if blocks
// below, with each block being self-contained with all the #includes and
// definitions it needs, to keep platform code easier to maintain in isolation.

#include "mozilla/ProfilerUtils.h"

// --------------------------------------------- Windows process & thread ids
#if defined(XP_WIN)

#  include "mozilla/Assertions.h"

#  include <process.h>
#  include <processthreadsapi.h>

ProfilerProcessId profiler_current_process_id() {
  return ProfilerProcessId::FromNumber(_getpid());
}

ProfilerThreadId profiler_current_thread_id() {
  DWORD threadId = GetCurrentThreadId();
  MOZ_ASSERT(threadId <= INT32_MAX, "native thread ID is > INT32_MAX");
  return ProfilerThreadId::FromNumber(
      static_cast<ProfilerThreadId::NumberType>(threadId));
}

// --------------------------------------------- Non-Windows process id
#else
// All non-Windows platforms are assumed to be POSIX, which has getpid().

#  include <unistd.h>

ProfilerProcessId profiler_current_process_id() {
  return ProfilerProcessId::FromNumber(getpid());
}

// --------------------------------------------- Non-Windows thread id
// ------------------------------------------------------- macOS
#  if defined(XP_MACOSX)

#    include <pthread.h>

ProfilerThreadId profiler_current_thread_id() {
  uint64_t tid;
  pthread_threadid_np(nullptr, &tid);
  // Cast the uint64_t value to NumberType, which is an int.
  // In theory, this risks truncating the value. It's unknown if such large
  // values occur in reality.
  // It may be worth changing our cross-platform tid type to 64 bits.
  return ProfilerThreadId::FromNumber(
      static_cast<ProfilerThreadId::NumberType>(tid));
}

// ------------------------------------------------------- Android
// Test Android before Linux, because Linux includes Android.
#  elif defined(__ANDROID__) || defined(ANDROID)

#    include <sys/types.h>

ProfilerThreadId profiler_current_thread_id() {
  return ProfilerThreadId::FromNumber(
      static_cast<ProfilerThreadId::NumberType>(gettid()));
}

// ------------------------------------------------------- Linux
#  elif defined(XP_LINUX)

#    include <sys/syscall.h>

ProfilerThreadId profiler_current_thread_id() {
  // glibc doesn't provide a wrapper for gettid() until 2.30
  return ProfilerThreadId::FromNumber(
      static_cast<ProfilerThreadId::NumberType>(syscall(SYS_gettid)));
}

// ------------------------------------------------------- FreeBSD
#  elif defined(XP_FREEBSD)

#    include <sys/thr.h>

ProfilerThreadId profiler_current_thread_id() {
  long id;
  (void)thr_self(&id);
  return ProfilerThreadId::FromNumber(
      static_cast<ProfilerThreadId::NumberType>(id));
}

// ------------------------------------------------------- Others
#  else  // Unsupported platforms.

ProfilerThreadId profiler_current_thread_id() { return ProfilerThreadId{}; }

#  endif  // End of unsupported platforms.
#endif    // End of non-XP_WIN.

// --------------------------------------------- Platform-agnostic definitions

#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"

static ProfilerThreadId scProfilerMainThreadId;

void profiler_init_main_thread_id() {
  MOZ_ASSERT(NS_IsMainThread());
  mozilla::baseprofiler::profiler_init_main_thread_id();
  if (!scProfilerMainThreadId.IsSpecified()) {
    scProfilerMainThreadId = profiler_current_thread_id();
  }
}

[[nodiscard]] ProfilerThreadId profiler_main_thread_id() {
  return scProfilerMainThreadId;
}

[[nodiscard]] bool profiler_is_main_thread() {
  return profiler_current_thread_id() == scProfilerMainThreadId;
}
