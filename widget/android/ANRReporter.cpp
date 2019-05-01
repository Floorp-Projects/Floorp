/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ANRReporter.h"
#include "GeckoProfiler.h"

#include <unistd.h>

namespace mozilla {

bool ANRReporter::RequestNativeStack(bool aUnwind) {
#ifdef MOZ_GECKO_PROFILER
  if (profiler_is_active()) {
    // Don't proceed if profiler is already running
    return false;
  }

  // WARNING: we are on the ANR reporter thread at this point and it is
  // generally unsafe to use the profiler from off the main thread. However,
  // the risk here is limited because for most users, the profiler is not run
  // elsewhere. See the discussion in Bug 863777, comment 13
  uint32_t features = ProfilerFeature::Leaf | ProfilerFeature::Privacy |
                      (aUnwind ? ProfilerFeature::StackWalk : 0) |
                      ProfilerFeature::Threads;

  const char* NATIVE_STACK_THREADS[] = {"GeckoMain", "Compositor"};

  // Buffer one sample and let the profiler wait a long time
  profiler_start(/* entries */ 100, /* interval */ 10000, features,
                 NATIVE_STACK_THREADS,
                 sizeof(NATIVE_STACK_THREADS) / sizeof(char*));
#endif
  return true;
}

jni::String::LocalRef ANRReporter::GetNativeStack() {
#ifdef MOZ_GECKO_PROFILER
  // Timeout if we don't get a profiler sample after 5 seconds.
  const PRIntervalTime timeout = PR_SecondsToInterval(5);
  const PRIntervalTime startTime = PR_IntervalNow();

  // Pointer to a profile JSON string
  typedef mozilla::UniquePtr<char[]> ProfilePtr;

  // profiler_get_profile() will return nullptr if the profiler is inactive.
  ProfilePtr profile(profiler_get_profile());
  if (!profile) {
    return nullptr;
  }

  while (profile && !strstr(profile.get(), "\"samples\":[{")) {
    // no sample yet?
    if (PR_IntervalNow() - startTime >= timeout) {
      return nullptr;
    }
    usleep(100000ul);  // Sleep for 100ms
    profile = ProfilePtr(profiler_get_profile());
  }

  if (profile) {
    return jni::String::Param(profile.get());
  }
#endif
  return nullptr;
}

void ANRReporter::ReleaseNativeStack() {
#ifdef MOZ_GECKO_PROFILER
  if (!profiler_is_active()) {
    // Maybe profiler support is disabled?
    return;
  }
  profiler_stop();
#endif
}

}  // namespace mozilla
