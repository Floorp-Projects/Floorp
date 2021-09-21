/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* FFI functions for Profiler Rust API to call into profiler */

#include "ProfilerBindings.h"

#include "GeckoProfiler.h"

#include <type_traits>

void gecko_profiler_register_thread(const char* aName) {
  PROFILER_REGISTER_THREAD(aName);
}

void gecko_profiler_unregister_thread() { PROFILER_UNREGISTER_THREAD(); }

void gecko_profiler_construct_label(mozilla::AutoProfilerLabel* aAutoLabel,
                                    JS::ProfilingCategoryPair aCategoryPair) {
#ifdef MOZ_GECKO_PROFILER
  new (aAutoLabel) mozilla::AutoProfilerLabel(
      "", nullptr, aCategoryPair,
      uint32_t(
          js::ProfilingStackFrame::Flags::LABEL_DETERMINED_BY_CATEGORY_PAIR));
#endif
}

void gecko_profiler_destruct_label(mozilla::AutoProfilerLabel* aAutoLabel) {
#ifdef MOZ_GECKO_PROFILER
  aAutoLabel->~AutoProfilerLabel();
#endif
}

void gecko_profiler_construct_timestamp_now(mozilla::TimeStamp* aTimeStamp) {
  new (aTimeStamp) mozilla::TimeStamp(mozilla::TimeStamp::Now());
}

void gecko_profiler_destruct_timestamp(mozilla::TimeStamp* aTimeStamp) {
  aTimeStamp->~TimeStamp();
}

void gecko_profiler_construct_marker_timing_instant_at(
    mozilla::MarkerTiming* aMarkerTiming, const mozilla::TimeStamp* aTime) {
#ifdef MOZ_GECKO_PROFILER
  static_assert(std::is_trivially_copyable_v<mozilla::MarkerTiming>);
  mozilla::MarkerTiming::UnsafeConstruct(aMarkerTiming, *aTime,
                                         mozilla::TimeStamp{},
                                         mozilla::MarkerTiming::Phase::Instant);
#endif
}

void gecko_profiler_construct_marker_timing_instant_now(
    mozilla::MarkerTiming* aMarkerTiming) {
#ifdef MOZ_GECKO_PROFILER
  static_assert(std::is_trivially_copyable_v<mozilla::MarkerTiming>);
  mozilla::MarkerTiming::UnsafeConstruct(
      aMarkerTiming, mozilla::TimeStamp::Now(), mozilla::TimeStamp{},
      mozilla::MarkerTiming::Phase::Instant);
#endif
}

void gecko_profiler_construct_marker_timing_interval(
    mozilla::MarkerTiming* aMarkerTiming, const mozilla::TimeStamp* aStartTime,
    const mozilla::TimeStamp* aEndTime) {
#ifdef MOZ_GECKO_PROFILER
  static_assert(std::is_trivially_copyable_v<mozilla::MarkerTiming>);
  mozilla::MarkerTiming::UnsafeConstruct(
      aMarkerTiming, *aStartTime, *aEndTime,
      mozilla::MarkerTiming::Phase::Interval);
#endif
}

void gecko_profiler_construct_marker_timing_interval_until_now_from(
    mozilla::MarkerTiming* aMarkerTiming,
    const mozilla::TimeStamp* aStartTime) {
#ifdef MOZ_GECKO_PROFILER
  static_assert(std::is_trivially_copyable_v<mozilla::MarkerTiming>);
  mozilla::MarkerTiming::UnsafeConstruct(
      aMarkerTiming, *aStartTime, mozilla::TimeStamp::Now(),
      mozilla::MarkerTiming::Phase::Interval);
#endif
}

void gecko_profiler_construct_marker_timing_interval_start(
    mozilla::MarkerTiming* aMarkerTiming, const mozilla::TimeStamp* aTime) {
#ifdef MOZ_GECKO_PROFILER
  static_assert(std::is_trivially_copyable_v<mozilla::MarkerTiming>);
  mozilla::MarkerTiming::UnsafeConstruct(
      aMarkerTiming, *aTime, mozilla::TimeStamp{},
      mozilla::MarkerTiming::Phase::IntervalStart);
#endif
}

void gecko_profiler_construct_marker_timing_interval_end(
    mozilla::MarkerTiming* aMarkerTiming, const mozilla::TimeStamp* aTime) {
#ifdef MOZ_GECKO_PROFILER
  static_assert(std::is_trivially_copyable_v<mozilla::MarkerTiming>);
  mozilla::MarkerTiming::UnsafeConstruct(
      aMarkerTiming, mozilla::TimeStamp{}, *aTime,
      mozilla::MarkerTiming::Phase::IntervalEnd);
#endif
}

void gecko_profiler_destruct_marker_timing(
    mozilla::MarkerTiming* aMarkerTiming) {
#ifdef MOZ_GECKO_PROFILER
  aMarkerTiming->~MarkerTiming();
#endif
}
