/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* FFI functions for Profiler Rust API to call into profiler */

#ifndef ProfilerBindings_h
#define ProfilerBindings_h

#include <cstddef>
#include <stdint.h>

namespace mozilla {
class AutoProfilerLabel;
class MarkerTiming;
class TimeStamp;
enum class StackCaptureOptions;

namespace baseprofiler {
enum class ProfilingCategoryPair : uint32_t;
}  // namespace baseprofiler

}  // namespace mozilla

namespace JS {
enum class ProfilingCategoryPair : uint32_t;
}  // namespace JS

// Everything in here is safe to include unconditionally, implementations must
// take !MOZ_GECKO_PROFILER into account.
extern "C" {

void gecko_profiler_register_thread(const char* aName);
void gecko_profiler_unregister_thread();

void gecko_profiler_construct_label(mozilla::AutoProfilerLabel* aAutoLabel,
                                    JS::ProfilingCategoryPair aCategoryPair);
void gecko_profiler_destruct_label(mozilla::AutoProfilerLabel* aAutoLabel);

// Construct and destruct the timestamp for profiler time.
void gecko_profiler_construct_timestamp_now(mozilla::TimeStamp* aTimeStamp);
void gecko_profiler_destruct_timestamp(mozilla::TimeStamp* aTimeStamp);

// Various MarkerTiming constructors and a destructor.
void gecko_profiler_construct_marker_timing_instant_at(
    mozilla::MarkerTiming* aMarkerTiming, const mozilla::TimeStamp* aTime);
void gecko_profiler_construct_marker_timing_instant_now(
    mozilla::MarkerTiming* aMarkerTiming);
void gecko_profiler_construct_marker_timing_interval(
    mozilla::MarkerTiming* aMarkerTiming, const mozilla::TimeStamp* aStartTime,
    const mozilla::TimeStamp* aEndTime);
void gecko_profiler_construct_marker_timing_interval_until_now_from(
    mozilla::MarkerTiming* aMarkerTiming, const mozilla::TimeStamp* aStartTime);
void gecko_profiler_construct_marker_timing_interval_start(
    mozilla::MarkerTiming* aMarkerTiming, const mozilla::TimeStamp* aTime);
void gecko_profiler_construct_marker_timing_interval_end(
    mozilla::MarkerTiming* aMarkerTiming, const mozilla::TimeStamp* aTime);
void gecko_profiler_destruct_marker_timing(
    mozilla::MarkerTiming* aMarkerTiming);

// Marker APIs.
void gecko_profiler_add_marker_untyped(
    const char* aName, size_t aNameLength,
    mozilla::baseprofiler::ProfilingCategoryPair aCategoryPair,
    mozilla::MarkerTiming* aMarkerTiming,
    mozilla::StackCaptureOptions aStackCaptureOptions);

}  // extern "C"

#endif  // ProfilerBindings_h
