/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* FFI functions for Profiler Rust API to call into profiler */

#ifndef ProfilerBindings_h
#define ProfilerBindings_h

#include "mozilla/BaseProfilerMarkersPrerequisites.h"

#include <cstddef>
#include <stdint.h>

namespace mozilla {
class AutoProfilerLabel;
class MarkerSchema;
class MarkerTiming;
class TimeStamp;
enum class StackCaptureOptions;

namespace baseprofiler {
enum class ProfilingCategoryPair : uint32_t;
class SpliceableJSONWriter;
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

// Construct, clone and destruct the timestamp for profiler time.
void gecko_profiler_construct_timestamp_now(mozilla::TimeStamp* aTimeStamp);
void gecko_profiler_clone_timestamp(const mozilla::TimeStamp* aSrcTimeStamp,
                                    mozilla::TimeStamp* aDestTimeStamp);
void gecko_profiler_destruct_timestamp(mozilla::TimeStamp* aTimeStamp);

// Addition and subtraction for timestamp.
void gecko_profiler_add_timestamp(const mozilla::TimeStamp* aTimeStamp,
                                  mozilla::TimeStamp* aDestTimeStamp,
                                  double aMicroseconds);
void gecko_profiler_subtract_timestamp(const mozilla::TimeStamp* aTimeStamp,
                                       mozilla::TimeStamp* aDestTimeStamp,
                                       double aMicroseconds);

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

// MarkerSchema constructors and destructor.
void gecko_profiler_construct_marker_schema(
    mozilla::MarkerSchema* aMarkerSchema,
    const mozilla::MarkerSchema::Location* aLocations, size_t aLength);
void gecko_profiler_construct_marker_schema_with_special_front_end_location(
    mozilla::MarkerSchema* aMarkerSchema);
void gecko_profiler_destruct_marker_schema(
    mozilla::MarkerSchema* aMarkerSchema);

// MarkerSchema methods for adding labels.
void gecko_profiler_marker_schema_set_chart_label(
    mozilla::MarkerSchema* aSchema, const char* aLabel, size_t aLabelLength);
void gecko_profiler_marker_schema_set_tooltip_label(
    mozilla::MarkerSchema* aSchema, const char* aLabel, size_t aLabelLength);
void gecko_profiler_marker_schema_set_table_label(
    mozilla::MarkerSchema* aSchema, const char* aLabel, size_t aLabelLength);
void gecko_profiler_marker_schema_set_all_labels(mozilla::MarkerSchema* aSchema,
                                                 const char* aLabel,
                                                 size_t aLabelLength);

// MarkerSchema methods for adding key/key-label values.
void gecko_profiler_marker_schema_add_key_format(
    mozilla::MarkerSchema* aSchema, const char* aKey, size_t aKeyLength,
    mozilla::MarkerSchema::Format aFormat);
void gecko_profiler_marker_schema_add_key_label_format(
    mozilla::MarkerSchema* aSchema, const char* aKey, size_t aKeyLength,
    const char* aLabel, size_t aLabelLength,
    mozilla::MarkerSchema::Format aFormat);
void gecko_profiler_marker_schema_add_key_format_searchable(
    mozilla::MarkerSchema* aSchema, const char* aKey, size_t aKeyLength,
    mozilla::MarkerSchema::Format aFormat,
    mozilla::MarkerSchema::Searchable aSearchable);
void gecko_profiler_marker_schema_add_key_label_format_searchable(
    mozilla::MarkerSchema* aSchema, const char* aKey, size_t aKeyLength,
    const char* aLabel, size_t aLabelLength,
    mozilla::MarkerSchema::Format aFormat,
    mozilla::MarkerSchema::Searchable aSearchable);
void gecko_profiler_marker_schema_add_static_label_value(
    mozilla::MarkerSchema* aSchema, const char* aLabel, size_t aLabelLength,
    const char* aValue, size_t aValueLength);

// Stream MarkerSchema to SpliceableJSONWriter.
void gecko_profiler_marker_schema_stream(
    mozilla::baseprofiler::SpliceableJSONWriter* aWriter, const char* aName,
    size_t aNameLength, mozilla::MarkerSchema* aMarkerSchema,
    void* aStreamedNamesSet);

// Various SpliceableJSONWriter methods to add properties.
void gecko_profiler_json_writer_int_property(
    mozilla::baseprofiler::SpliceableJSONWriter* aWriter, const char* aName,
    size_t aNameLength, int64_t aValue);
void gecko_profiler_json_writer_float_property(
    mozilla::baseprofiler::SpliceableJSONWriter* aWriter, const char* aName,
    size_t aNameLength, double aValue);
void gecko_profiler_json_writer_bool_property(
    mozilla::baseprofiler::SpliceableJSONWriter* aWriter, const char* aName,
    size_t aNameLength, bool aValue);
void gecko_profiler_json_writer_string_property(
    mozilla::baseprofiler::SpliceableJSONWriter* aWriter, const char* aName,
    size_t aNameLength, const char* aValue, size_t aValueLength);
void gecko_profiler_json_writer_unique_string_property(
    mozilla::baseprofiler::SpliceableJSONWriter* aWriter, const char* aName,
    size_t aNameLength, const char* aValue, size_t aValueLength);
void gecko_profiler_json_writer_null_property(
    mozilla::baseprofiler::SpliceableJSONWriter* aWriter, const char* aName,
    size_t aNameLength);

// Marker APIs.
void gecko_profiler_add_marker_untyped(
    const char* aName, size_t aNameLength,
    mozilla::baseprofiler::ProfilingCategoryPair aCategoryPair,
    mozilla::MarkerTiming* aMarkerTiming,
    mozilla::StackCaptureOptions aStackCaptureOptions);
void gecko_profiler_add_marker_text(
    const char* aName, size_t aNameLength,
    mozilla::baseprofiler::ProfilingCategoryPair aCategoryPair,
    mozilla::MarkerTiming* aMarkerTiming,
    mozilla::StackCaptureOptions aStackCaptureOptions, const char* aText,
    size_t aTextLength);
void gecko_profiler_add_marker(
    const char* aName, size_t aNameLength,
    mozilla::baseprofiler::ProfilingCategoryPair aCategoryPair,
    mozilla::MarkerTiming* aMarkerTiming,
    mozilla::StackCaptureOptions aStackCaptureOptions, uint8_t aMarkerTag,
    const uint8_t* aPayload, size_t aPayloadSize);

}  // extern "C"

#endif  // ProfilerBindings_h
