/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Markers are useful to delimit something important happening such as the first
// paint. Unlike labels, which are only recorded in the profile buffer if a
// sample is collected while the label is on the label stack, markers will
// always be recorded in the profile buffer.
//
// This header contains basic definitions necessary to create marker types, and
// to add markers to the profiler buffers.
//
// If basic marker types are needed, #include
// "mozilla/BaseProfilerMarkerTypes.h" instead.
//
// But if you want to create your own marker type locally, you can #include this
// header only; look at mozilla/BaseProfilerMarkerTypes.h for examples of how to
// define types, and mozilla/BaseProfilerMarkerPrerequisites.h for some
// supporting types.
//
// To then record markers:
// - Use `baseprofiler::AddMarker(...)` from  mozglue or other libraries that
//   are outside of xul, especially if they may happen outside of xpcom's
//   lifetime (typically startup, shutdown, or tests).
// - Otherwise #include "ProfilerMarkers.h" instead, and use
//   `profiler_add_marker(...)`.
// See these functions for more details.

#ifndef BaseProfilerMarkers_h
#define BaseProfilerMarkers_h

#include "mozilla/BaseProfilerMarkersDetail.h"
#include "mozilla/BaseProfilerLabels.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Unused.h"

#include <functional>
#include <string>
#include <utility>

namespace mozilla::baseprofiler {

#ifdef MOZ_GECKO_PROFILER
// Forward-declaration. TODO: Move to more common header, see bug 1681416.
MFBT_API bool profiler_capture_backtrace_into(
    ProfileChunkedBuffer& aChunkedBuffer, StackCaptureOptions aCaptureOptions);

// Add a marker to a given buffer. `AddMarker()` and related macros should be
// used in most cases, see below for more information about them and the
// parameters; This function may be useful when markers need to be recorded in a
// local buffer outside of the main profiler buffer.
template <typename MarkerType, typename... PayloadArguments>
ProfileBufferBlockIndex AddMarkerToBuffer(
    ProfileChunkedBuffer& aBuffer, const ProfilerString8View& aName,
    const MarkerCategory& aCategory, MarkerOptions&& aOptions,
    MarkerType aMarkerType, const PayloadArguments&... aPayloadArguments) {
  Unused << aMarkerType;  // Only the empty object type is useful.
  AUTO_BASE_PROFILER_LABEL("baseprofiler::AddMarkerToBuffer", PROFILER);
  return base_profiler_markers_detail::AddMarkerToBuffer<MarkerType>(
      aBuffer, aName, aCategory, std::move(aOptions),
      ::mozilla::baseprofiler::profiler_capture_backtrace_into,
      aPayloadArguments...);
}

// Add a marker (without payload) to a given buffer.
inline ProfileBufferBlockIndex AddMarkerToBuffer(
    ProfileChunkedBuffer& aBuffer, const ProfilerString8View& aName,
    const MarkerCategory& aCategory, MarkerOptions&& aOptions = {}) {
  return AddMarkerToBuffer(aBuffer, aName, aCategory, std::move(aOptions),
                           markers::NoPayload{});
}
#endif  // MOZ_GECKO_PROFILER

// Add a marker to the Base Profiler buffer.
// - aName: Main name of this marker.
// - aCategory: Category for this marker.
// - aOptions: Optional settings (such as timing, inner window id,
//   backtrace...), see `MarkerOptions` for details.
// - aMarkerType: Empty object that specifies the type of marker.
// - aPayloadArguments: Arguments expected by this marker type's
// ` StreamJSONMarkerData` function.
template <typename MarkerType, typename... PayloadArguments>
ProfileBufferBlockIndex AddMarker(
    const ProfilerString8View& aName, const MarkerCategory& aCategory,
    MarkerOptions&& aOptions, MarkerType aMarkerType,
    const PayloadArguments&... aPayloadArguments) {
#ifndef MOZ_GECKO_PROFILER
  return {};
#else
  if ((aOptions.ThreadId().IsUnspecified() ||
       aOptions.ThreadId().ThreadId() == profiler_current_thread_id())
          ? !baseprofiler::profiler_thread_is_being_profiled()
          // If targetting another thread, we can only check if the profiler
          // is active&unpaused.
          : !baseprofiler::detail::RacyFeatures::IsActiveAndUnpaused()) {
    return {};
  }
  return ::mozilla::baseprofiler::AddMarkerToBuffer(
      base_profiler_markers_detail::CachedBaseCoreBuffer(), aName, aCategory,
      std::move(aOptions), aMarkerType, aPayloadArguments...);
#endif
}

// Add a marker (without payload) to the Base Profiler buffer.
inline ProfileBufferBlockIndex AddMarker(const ProfilerString8View& aName,
                                         const MarkerCategory& aCategory,
                                         MarkerOptions&& aOptions = {}) {
  return AddMarker(aName, aCategory, std::move(aOptions), markers::NoPayload{});
}

}  // namespace mozilla::baseprofiler

// Same as `AddMarker()` (without payload). This macro is safe to use even if
// MOZ_GECKO_PROFILER is not #defined.
#define BASE_PROFILER_MARKER_UNTYPED(markerName, categoryName, ...)  \
  do {                                                               \
    AUTO_PROFILER_STATS(BASE_PROFILER_MARKER_UNTYPED);               \
    ::mozilla::baseprofiler::AddMarker(                              \
        markerName, ::mozilla::baseprofiler::category::categoryName, \
        ##__VA_ARGS__);                                              \
  } while (false)

// Same as `AddMarker()` (with payload). This macro is safe to use even if
// MOZ_GECKO_PROFILER is not #defined.
#define BASE_PROFILER_MARKER(markerName, categoryName, options, MarkerType,   \
                             ...)                                             \
  do {                                                                        \
    AUTO_PROFILER_STATS(BASE_PROFILER_MARKER_with_##MarkerType);              \
    ::mozilla::baseprofiler::AddMarker(                                       \
        markerName, ::mozilla::baseprofiler::category::categoryName, options, \
        ::mozilla::baseprofiler::markers::MarkerType{}, ##__VA_ARGS__);       \
  } while (false)

namespace mozilla::baseprofiler::markers {
// Most common marker type. Others are in BaseProfilerMarkerTypes.h.
struct TextMarker {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("Text");
  }
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   const ProfilerString8View& aText) {
    aWriter.StringProperty("name", aText);
  }
  static MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable};
    schema.SetChartLabel("{marker.data.name}");
    schema.SetTableLabel("{marker.name} - {marker.data.name}");
    schema.AddKeyLabelFormat("name", "Details", MS::Format::String);
    return schema;
  }
};

// Keep this struct in sync with the `gecko_profiler::marker::Tracing` Rust
// counterpart.
struct Tracing {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("tracing");
  }
  static void StreamJSONMarkerData(SpliceableJSONWriter& aWriter,
                                   const ProfilerString8View& aCategory) {
    if (aCategory.Length() != 0) {
      aWriter.StringProperty("category", aCategory);
    }
  }
  static MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable,
              MS::Location::TimelineOverview};
    schema.AddKeyLabelFormat("category", "Type", MS::Format::String);
    return schema;
  }
};
}  // namespace mozilla::baseprofiler::markers

// Add a text marker. This macro is safe to use even if MOZ_GECKO_PROFILER is
// not #defined.
#define BASE_PROFILER_MARKER_TEXT(markerName, categoryName, options, text)    \
  do {                                                                        \
    AUTO_PROFILER_STATS(BASE_PROFILER_MARKER_TEXT);                           \
    ::mozilla::baseprofiler::AddMarker(                                       \
        markerName, ::mozilla::baseprofiler::category::categoryName, options, \
        ::mozilla::baseprofiler::markers::TextMarker{}, text);                \
  } while (false)

namespace mozilla::baseprofiler {

// RAII object that adds a BASE_PROFILER_MARKER_TEXT when destroyed; the
// marker's timing will be the interval from construction (unless an instant or
// start time is already specified in the provided options) until destruction.
class MOZ_RAII AutoProfilerTextMarker {
 public:
  AutoProfilerTextMarker(const char* aMarkerName,
                         const MarkerCategory& aCategory,
                         MarkerOptions&& aOptions, const std::string& aText)
      : mMarkerName(aMarkerName),
        mCategory(aCategory),
        mOptions(std::move(aOptions)),
        mText(aText) {
    MOZ_ASSERT(mOptions.Timing().EndTime().IsNull(),
               "AutoProfilerTextMarker options shouldn't have an end time");
    if (mOptions.Timing().StartTime().IsNull()) {
      mOptions.Set(MarkerTiming::InstantNow());
    }
  }

  ~AutoProfilerTextMarker() {
    mOptions.TimingRef().SetIntervalEnd();
    AUTO_PROFILER_STATS(AUTO_BASE_PROFILER_MARKER_TEXT);
    AddMarker(ProfilerString8View::WrapNullTerminatedString(mMarkerName),
              mCategory, std::move(mOptions), markers::TextMarker{}, mText);
  }

 protected:
  const char* mMarkerName;
  MarkerCategory mCategory;
  MarkerOptions mOptions;
  std::string mText;
};

#ifdef MOZ_GECKO_PROFILER
extern template MFBT_API ProfileBufferBlockIndex
AddMarker(const ProfilerString8View&, const MarkerCategory&, MarkerOptions&&,
          markers::TextMarker, const std::string&);

extern template MFBT_API ProfileBufferBlockIndex
AddMarkerToBuffer(ProfileChunkedBuffer&, const ProfilerString8View&,
                  const MarkerCategory&, MarkerOptions&&, markers::NoPayload);

extern template MFBT_API ProfileBufferBlockIndex AddMarkerToBuffer(
    ProfileChunkedBuffer&, const ProfilerString8View&, const MarkerCategory&,
    MarkerOptions&&, markers::TextMarker, const std::string&);
#endif  // MOZ_GECKO_PROFILER

}  // namespace mozilla::baseprofiler

// Creates an AutoProfilerTextMarker RAII object.  This macro is safe to use
// even if MOZ_GECKO_PROFILER is not #defined.
#define AUTO_BASE_PROFILER_MARKER_TEXT(markerName, categoryName, options,   \
                                       text)                                \
  ::mozilla::baseprofiler::AutoProfilerTextMarker BASE_PROFILER_RAII(       \
      markerName, ::mozilla::baseprofiler::category::categoryName, options, \
      text)

#endif  // BaseProfilerMarkers_h
