/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseProfiler.h"

#ifdef MOZ_BASE_PROFILER

#  include "mozilla/Attributes.h"
#  include "mozilla/Vector.h"

#  if defined(_MSC_VER)
#    include <windows.h>
#    include <mmsystem.h>
#    include <process.h>
#  elif defined(__linux__) || (defined(__APPLE__) && defined(__x86_64__))
#    include <time.h>
#    include <unistd.h>
#  else
#    error
#  endif

// Increase the depth, to a maximum (to avoid too-deep recursion).
static constexpr size_t NextDepth(size_t aDepth) {
  constexpr size_t MAX_DEPTH = 128;
  return (aDepth < MAX_DEPTH) ? (aDepth + 1) : aDepth;
}

// Compute fibonacci the hard way (recursively: `f(n)=f(n-1)+f(n-2)`), and
// prevent inlining.
// The template parameter makes each depth be a separate function, to better
// distinguish them in the profiler output.
template <size_t DEPTH = 0>
MOZ_NEVER_INLINE unsigned long long Fibonacci(unsigned long long n) {
  if (n == 0) {
    return 0;
  }
  if (n == 1) {
    return 1;
  }
  unsigned long long f2 = Fibonacci<NextDepth(DEPTH)>(n - 2);
  if (DEPTH == 0) {
    BASE_PROFILER_ADD_MARKER("Half-way through Fibonacci", OTHER);
  }
  unsigned long long f1 = Fibonacci<NextDepth(DEPTH)>(n - 1);
  return f2 + f1;
}

static void SleepMilli(unsigned aMilliseconds) {
#  if defined(_MSC_VER)
  Sleep(aMilliseconds);
#  else
  struct timespec ts;
  ts.tv_sec = aMilliseconds / 1000;
  ts.tv_nsec = long(aMilliseconds % 1000) * 1000000;
  struct timespec tr;
  while (nanosleep(&ts, &tr)) {
    if (errno == EINTR) {
      ts = tr;
    } else {
      printf("nanosleep() -> %s\n", strerror(errno));
      exit(1);
    }
  }
#  endif
}

void TestProfiler() {
  printf("TestProfiler starting -- pid: %d, tid: %d\n",
         profiler_current_process_id(), profiler_current_thread_id());
  // ::Sleep(10000);

  {
    printf("profiler_init()...\n");
    AUTO_BASE_PROFILER_INIT;

    MOZ_RELEASE_ASSERT(!profiler_is_active());
    MOZ_RELEASE_ASSERT(!profiler_thread_is_being_profiled());
    MOZ_RELEASE_ASSERT(!profiler_thread_is_sleeping());

    printf("profiler_start()...\n");
    mozilla::Vector<const char*> filters;
    // Profile all registered threads.
    MOZ_RELEASE_ASSERT(filters.append(""));
    const uint32_t features = ProfilerFeature::Leaf |
                              ProfilerFeature::StackWalk |
                              ProfilerFeature::Threads;
    profiler_start(BASE_PROFILER_DEFAULT_ENTRIES,
                   BASE_PROFILER_DEFAULT_INTERVAL, features, filters.begin(),
                   filters.length());

    MOZ_RELEASE_ASSERT(profiler_is_active());
    MOZ_RELEASE_ASSERT(profiler_thread_is_being_profiled());
    MOZ_RELEASE_ASSERT(!profiler_thread_is_sleeping());

    {
      AUTO_BASE_PROFILER_TEXT_MARKER_CAUSE("fibonacci", "First leaf call",
                                           OTHER, nullptr);
      static const unsigned long long fibStart = 40;
      printf("Fibonacci(%llu)...\n", fibStart);
      AUTO_BASE_PROFILER_LABEL("Label around Fibonacci", OTHER);
      unsigned long long f = Fibonacci(fibStart);
      printf("Fibonacci(%llu) = %llu\n", fibStart, f);
    }

    printf("Sleep 1s...\n");
    {
      AUTO_BASE_PROFILER_THREAD_SLEEP;
      SleepMilli(1000);
    }

    printf("profiler_save_profile_to_file()...\n");
    profiler_save_profile_to_file("TestProfiler_profile.json");

    printf("profiler_stop()...\n");
    profiler_stop();

    MOZ_RELEASE_ASSERT(!profiler_is_active());
    MOZ_RELEASE_ASSERT(!profiler_thread_is_being_profiled());
    MOZ_RELEASE_ASSERT(!profiler_thread_is_sleeping());

    printf("profiler_shutdown()...\n");
  }

  printf("TestProfiler done\n");
}

#else  // MOZ_BASE_PROFILER

// Testing that macros are still #defined (but do nothing) when
// MOZ_BASE_PROFILER is disabled.
void TestProfiler() {
  // These don't need to make sense, we just want to know that they're defined
  // and don't do anything.
  AUTO_BASE_PROFILER_INIT;

  // This wouldn't build if the macro did output its arguments.
  AUTO_BASE_PROFILER_TEXT_MARKER_CAUSE(catch, catch, catch, catch);

  AUTO_BASE_PROFILER_LABEL(catch, catch);

  AUTO_BASE_PROFILER_THREAD_SLEEP;
}

#endif  // MOZ_BASE_PROFILER else

int main() {
  // Note that there are two `TestProfiler` functions above, depending on
  // whether MOZ_BASE_PROFILER is #defined.
  TestProfiler();

  return 0;
}
