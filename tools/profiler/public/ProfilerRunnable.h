/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerRunnable_h
#define ProfilerRunnable_h

#include "GeckoProfiler.h"
#include "nsIThreadPool.h"

#if !defined(MOZ_GECKO_PROFILER) || !defined(MOZ_COLLECTING_RUNNABLE_TELEMETRY)
#  define AUTO_PROFILE_FOLLOWING_RUNNABLE(runnable)
#else
#  define AUTO_PROFILE_FOLLOWING_RUNNABLE(runnable) \
    mozilla::AutoProfileRunnable PROFILER_RAII(runnable)

namespace mozilla {

class MOZ_RAII AutoProfileRunnable {
 public:
  explicit AutoProfileRunnable(Runnable* aRunnable)
      : mStartTime(TimeStamp::Now()) {
    if (!profiler_thread_is_being_profiled_for_markers()) {
      return;
    }

    aRunnable->GetName(mName);
  }
  explicit AutoProfileRunnable(nsIRunnable* aRunnable)
      : mStartTime(TimeStamp::Now()) {
    if (!profiler_thread_is_being_profiled_for_markers()) {
      return;
    }

    nsCOMPtr<nsIThreadPool> threadPool = do_QueryInterface(aRunnable);
    if (threadPool) {
      // nsThreadPool::Run has its own call to AUTO_PROFILE_FOLLOWING_RUNNABLE,
      // avoid nesting runnable markers.
      return;
    }

    nsCOMPtr<nsINamed> named = do_QueryInterface(aRunnable);
    if (named) {
      named->GetName(mName);
    }
  }
  explicit AutoProfileRunnable(nsACString& aName)
      : mStartTime(TimeStamp::Now()), mName(aName) {}

  ~AutoProfileRunnable() {
    if (!profiler_thread_is_being_profiled_for_markers() || mName.IsEmpty()) {
      return;
    }

    AUTO_PROFILER_LABEL("AutoProfileRunnable", PROFILER);
    AUTO_PROFILER_STATS(AUTO_PROFILE_RUNNABLE);
    profiler_add_marker("Runnable", ::mozilla::baseprofiler::category::OTHER,
                        MarkerTiming::IntervalUntilNowFrom(mStartTime),
                        geckoprofiler::markers::TextMarker{}, mName);
  }

 protected:
  TimeStamp mStartTime;
  nsAutoCString mName;
};

}  // namespace mozilla

#endif

#endif  // ProfilerRunnable_h
