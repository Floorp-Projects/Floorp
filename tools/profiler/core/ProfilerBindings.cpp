/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* FFI functions for Profiler Rust API to call into profiler */

#include "ProfilerBindings.h"

#include "GeckoProfiler.h"

#include <set>
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

void gecko_profiler_clone_timestamp(const mozilla::TimeStamp* aSrcTimeStamp,
                                    mozilla::TimeStamp* aDestTimeStamp) {
  new (aDestTimeStamp) mozilla::TimeStamp(*aSrcTimeStamp);
}

void gecko_profiler_destruct_timestamp(mozilla::TimeStamp* aTimeStamp) {
  aTimeStamp->~TimeStamp();
}

void gecko_profiler_add_timestamp(const mozilla::TimeStamp* aTimeStamp,
                                  mozilla::TimeStamp* aDestTimeStamp,
                                  double aMicroseconds) {
  new (aDestTimeStamp) mozilla::TimeStamp(
      *aTimeStamp + mozilla::TimeDuration::FromMicroseconds(aMicroseconds));
}

void gecko_profiler_subtract_timestamp(const mozilla::TimeStamp* aTimeStamp,
                                       mozilla::TimeStamp* aDestTimeStamp,
                                       double aMicroseconds) {
  new (aDestTimeStamp) mozilla::TimeStamp(
      *aTimeStamp - mozilla::TimeDuration::FromMicroseconds(aMicroseconds));
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

void gecko_profiler_construct_marker_schema(
    mozilla::MarkerSchema* aMarkerSchema,
    const mozilla::MarkerSchema::Location* aLocations, size_t aLength) {
#ifdef MOZ_GECKO_PROFILER
  new (aMarkerSchema) mozilla::MarkerSchema(aLocations, aLength);
#endif
}

void gecko_profiler_construct_marker_schema_with_special_front_end_location(
    mozilla::MarkerSchema* aMarkerSchema) {
#ifdef MOZ_GECKO_PROFILER
  new (aMarkerSchema)
      mozilla::MarkerSchema(mozilla::MarkerSchema::SpecialFrontendLocation{});
#endif
}

void gecko_profiler_destruct_marker_schema(
    mozilla::MarkerSchema* aMarkerSchema) {
#ifdef MOZ_GECKO_PROFILER
  aMarkerSchema->~MarkerSchema();
#endif
}

void gecko_profiler_marker_schema_set_chart_label(
    mozilla::MarkerSchema* aSchema, const char* aLabel, size_t aLabelLength) {
#ifdef MOZ_GECKO_PROFILER
  aSchema->SetChartLabel(std::string(aLabel, aLabelLength));
#endif
}

void gecko_profiler_marker_schema_set_tooltip_label(
    mozilla::MarkerSchema* aSchema, const char* aLabel, size_t aLabelLength) {
#ifdef MOZ_GECKO_PROFILER
  aSchema->SetTooltipLabel(std::string(aLabel, aLabelLength));
#endif
}

void gecko_profiler_marker_schema_set_table_label(
    mozilla::MarkerSchema* aSchema, const char* aLabel, size_t aLabelLength) {
#ifdef MOZ_GECKO_PROFILER
  aSchema->SetTableLabel(std::string(aLabel, aLabelLength));
#endif
}

void gecko_profiler_marker_schema_set_all_labels(mozilla::MarkerSchema* aSchema,
                                                 const char* aLabel,
                                                 size_t aLabelLength) {
#ifdef MOZ_GECKO_PROFILER
  aSchema->SetAllLabels(std::string(aLabel, aLabelLength));
#endif
}

void gecko_profiler_marker_schema_add_key_format(
    mozilla::MarkerSchema* aSchema, const char* aKey, size_t aKeyLength,
    mozilla::MarkerSchema::Format aFormat) {
#ifdef MOZ_GECKO_PROFILER
  aSchema->AddKeyFormat(std::string(aKey, aKeyLength), aFormat);
#endif
}

void gecko_profiler_marker_schema_add_key_label_format(
    mozilla::MarkerSchema* aSchema, const char* aKey, size_t aKeyLength,
    const char* aLabel, size_t aLabelLength,
    mozilla::MarkerSchema::Format aFormat) {
#ifdef MOZ_GECKO_PROFILER
  aSchema->AddKeyLabelFormat(std::string(aKey, aKeyLength),
                             std::string(aLabel, aLabelLength), aFormat);
#endif
}

void gecko_profiler_marker_schema_add_key_format_searchable(
    mozilla::MarkerSchema* aSchema, const char* aKey, size_t aKeyLength,
    mozilla::MarkerSchema::Format aFormat,
    mozilla::MarkerSchema::Searchable aSearchable) {
#ifdef MOZ_GECKO_PROFILER
  aSchema->AddKeyFormatSearchable(std::string(aKey, aKeyLength), aFormat,
                                  aSearchable);
#endif
}

void gecko_profiler_marker_schema_add_key_label_format_searchable(
    mozilla::MarkerSchema* aSchema, const char* aKey, size_t aKeyLength,
    const char* aLabel, size_t aLabelLength,
    mozilla::MarkerSchema::Format aFormat,
    mozilla::MarkerSchema::Searchable aSearchable) {
#ifdef MOZ_GECKO_PROFILER
  aSchema->AddKeyLabelFormatSearchable(std::string(aKey, aKeyLength),
                                       std::string(aLabel, aLabelLength),
                                       aFormat, aSearchable);
#endif
}

void gecko_profiler_marker_schema_add_static_label_value(
    mozilla::MarkerSchema* aSchema, const char* aLabel, size_t aLabelLength,
    const char* aValue, size_t aValueLength) {
#ifdef MOZ_GECKO_PROFILER
  aSchema->AddStaticLabelValue(std::string(aLabel, aLabelLength),
                               std::string(aValue, aValueLength));
#endif
}

void gecko_profiler_marker_schema_stream(
    mozilla::baseprofiler::SpliceableJSONWriter* aWriter, const char* aName,
    size_t aNameLength, mozilla::MarkerSchema* aMarkerSchema,
    void* aStreamedNamesSet) {
#ifdef MOZ_GECKO_PROFILER
  auto* streamedNames = static_cast<std::set<std::string>*>(aStreamedNamesSet);
  // std::set.insert(T&&) returns a pair, its `second` is true if the element
  // was actually inserted (i.e., it was not there yet.).
  const bool didInsert =
      streamedNames->insert(std::string(aName, aNameLength)).second;
  if (didInsert) {
    std::move(*aMarkerSchema)
        .Stream(*aWriter, mozilla::Span(aName, aNameLength));
  }
#endif
}

void gecko_profiler_json_writer_int_property(
    mozilla::baseprofiler::SpliceableJSONWriter* aWriter, const char* aName,
    size_t aNameLength, int64_t aValue) {
#ifdef MOZ_GECKO_PROFILER
  aWriter->IntProperty(mozilla::Span(aName, aNameLength), aValue);
#endif
}

void gecko_profiler_json_writer_float_property(
    mozilla::baseprofiler::SpliceableJSONWriter* aWriter, const char* aName,
    size_t aNameLength, double aValue) {
#ifdef MOZ_GECKO_PROFILER
  aWriter->DoubleProperty(mozilla::Span(aName, aNameLength), aValue);
#endif
}

void gecko_profiler_json_writer_bool_property(
    mozilla::baseprofiler::SpliceableJSONWriter* aWriter, const char* aName,
    size_t aNameLength, bool aValue) {
#ifdef MOZ_GECKO_PROFILER
  aWriter->BoolProperty(mozilla::Span(aName, aNameLength), aValue);
#endif
}
void gecko_profiler_json_writer_string_property(
    mozilla::baseprofiler::SpliceableJSONWriter* aWriter, const char* aName,
    size_t aNameLength, const char* aValue, size_t aValueLength) {
#ifdef MOZ_GECKO_PROFILER
  aWriter->StringProperty(mozilla::Span(aName, aNameLength),
                          mozilla::Span(aValue, aValueLength));
#endif
}

void gecko_profiler_json_writer_unique_string_property(
    mozilla::baseprofiler::SpliceableJSONWriter* aWriter, const char* aName,
    size_t aNameLength, const char* aValue, size_t aValueLength) {
#ifdef MOZ_GECKO_PROFILER
  aWriter->UniqueStringProperty(mozilla::Span(aName, aNameLength),
                                mozilla::Span(aValue, aValueLength));
#endif
}

void gecko_profiler_json_writer_null_property(
    mozilla::baseprofiler::SpliceableJSONWriter* aWriter, const char* aName,
    size_t aNameLength) {
#ifdef MOZ_GECKO_PROFILER
  aWriter->NullProperty(mozilla::Span(aName, aNameLength));
#endif
}

void gecko_profiler_add_marker_untyped(
    const char* aName, size_t aNameLength,
    mozilla::baseprofiler::ProfilingCategoryPair aCategoryPair,
    mozilla::MarkerTiming* aMarkerTiming,
    mozilla::StackCaptureOptions aStackCaptureOptions) {
#ifdef MOZ_GECKO_PROFILER
  profiler_add_marker(
      mozilla::ProfilerString8View(aName, aNameLength),
      mozilla::MarkerCategory{aCategoryPair},
      mozilla::MarkerOptions(
          std::move(*aMarkerTiming),
          mozilla::MarkerStack::WithCaptureOptions(aStackCaptureOptions)));
#endif
}

void gecko_profiler_add_marker_text(
    const char* aName, size_t aNameLength,
    mozilla::baseprofiler::ProfilingCategoryPair aCategoryPair,
    mozilla::MarkerTiming* aMarkerTiming,
    mozilla::StackCaptureOptions aStackCaptureOptions, const char* aText,
    size_t aTextLength) {
#ifdef MOZ_GECKO_PROFILER
  profiler_add_marker(
      mozilla::ProfilerString8View(aName, aNameLength),
      mozilla::MarkerCategory{aCategoryPair},
      mozilla::MarkerOptions(
          std::move(*aMarkerTiming),
          mozilla::MarkerStack::WithCaptureOptions(aStackCaptureOptions)),
      geckoprofiler::markers::TextMarker{},
      mozilla::ProfilerString8View(aText, aTextLength));
#endif
}

void gecko_profiler_add_marker(
    const char* aName, size_t aNameLength,
    mozilla::baseprofiler::ProfilingCategoryPair aCategoryPair,
    mozilla::MarkerTiming* aMarkerTiming,
    mozilla::StackCaptureOptions aStackCaptureOptions, uint8_t aMarkerTag,
    const uint8_t* aPayload, size_t aPayloadSize) {
#ifdef MOZ_GECKO_PROFILER
  // Copy the marker timing and create the marker option.
  mozilla::MarkerOptions markerOptions(
      std::move(*aMarkerTiming),
      mozilla::MarkerStack::WithCaptureOptions(aStackCaptureOptions));

  // Currently it's not possible to add a threadId option, but we will
  // have it soon.
  if (markerOptions.ThreadId().IsUnspecified()) {
    // If yet unspecified, set thread to this thread where the marker is added.
    markerOptions.Set(mozilla::MarkerThreadId::CurrentThread());
  }

  auto& buffer = profiler_get_core_buffer();
  mozilla::Span payload(aPayload, aPayloadSize);

  mozilla::StackCaptureOptions captureOptions =
      markerOptions.Stack().CaptureOptions();
  if (captureOptions != mozilla::StackCaptureOptions::NoStack &&
      // Do not capture a stack if the NoMarkerStacks feature is set.
      profiler_active_without_feature(ProfilerFeature::NoMarkerStacks)) {
    // A capture was requested, let's attempt to do it here&now. This avoids a
    // lot of allocations that would be necessary if capturing a backtrace
    // separately.
    // TODO use a local on-stack byte buffer to remove last allocation.
    // TODO reduce internal profiler stack levels, see bug 1659872.
    mozilla::ProfileBufferChunkManagerSingle chunkManager(
        mozilla::ProfileBufferChunkManager::scExpectedMaximumStackSize);
    mozilla::ProfileChunkedBuffer chunkedBuffer(
        mozilla::ProfileChunkedBuffer::ThreadSafety::WithoutMutex,
        chunkManager);
    markerOptions.StackRef().UseRequestedBacktrace(
        profiler_capture_backtrace_into(chunkedBuffer, captureOptions)
            ? &chunkedBuffer
            : nullptr);

    // This call must be made from here, while chunkedBuffer is in scope.
    buffer.PutObjects(
        mozilla::ProfileBufferEntryKind::Marker, markerOptions,
        mozilla::ProfilerString8View(aName, aNameLength),
        mozilla::MarkerCategory{aCategoryPair},
        mozilla::base_profiler_markers_detail::Streaming::DeserializerTag(
            aMarkerTag),
        mozilla::MarkerPayloadType::Rust, payload);
    return;
  }

  buffer.PutObjects(
      mozilla::ProfileBufferEntryKind::Marker, markerOptions,
      mozilla::ProfilerString8View(aName, aNameLength),
      mozilla::MarkerCategory{aCategoryPair},
      mozilla::base_profiler_markers_detail::Streaming::DeserializerTag(
          aMarkerTag),
      mozilla::MarkerPayloadType::Rust, payload);
#endif
}
