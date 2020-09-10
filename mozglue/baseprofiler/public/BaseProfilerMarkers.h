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
// - Use `baseprofiler::AddMarker<ChosenMarkerType>(...)` from
//   mozglue or other libraries that are outside of xul, especially if they may
//   happen outside of xpcom's lifetime (typically startup, shutdown, or tests).
// - Otherwise #include "ProfilerMarkers.h" instead, and use
//   `profiler_add_marker<ChosenMarkerType>(...)`.
// See these functions for more details.

#ifndef BaseProfilerMarkers_h
#define BaseProfilerMarkers_h

#include "mozilla/BaseProfilerMarkersDetail.h"

// TODO: Move common stuff to shared header instead.
#include "BaseProfiler.h"

#ifndef MOZ_GECKO_PROFILER

#  define BASE_PROFILER_MARKER_UNTYPED(markerName, options)
#  define BASE_PROFILER_MARKER(markerName, options, MarkerType, ...)
#  define BASE_PROFILER_MARKER_TEXT(markerName, options, text)

#else  // ndef MOZ_GECKO_PROFILER

#  include "mozilla/ProfileChunkedBuffer.h"
#  include "mozilla/TimeStamp.h"

#  include <functional>
#  include <string>
#  include <utility>

namespace mozilla::baseprofiler {

template <typename MarkerType = ::mozilla::baseprofiler::markers::NoPayload,
          typename... Ts>
ProfileBufferBlockIndex AddMarkerToBuffer(ProfileChunkedBuffer& aBuffer,
                                          const ProfilerString8View& aName,
                                          MarkerOptions&& aOptions,
                                          const Ts&... aTs) {
  return base_profiler_markers_detail::AddMarkerToBuffer<MarkerType>(
      aBuffer, aName, std::move(aOptions),
      ::mozilla::baseprofiler::profiler_capture_backtrace_into, aTs...);
}

template <typename MarkerType = ::mozilla::baseprofiler::markers::NoPayload,
          typename... Ts>
ProfileBufferBlockIndex AddMarker(const ProfilerString8View& aName,
                                  MarkerOptions&& aOptions, const Ts&... aTs) {
  if (!baseprofiler::profiler_can_accept_markers()) {
    return {};
  }
  return ::mozilla::baseprofiler::AddMarkerToBuffer<MarkerType>(
      base_profiler_markers_detail::CachedBaseCoreBuffer(), aName,
      std::move(aOptions), aTs...);
}

// Marker types' StreamJSONMarkerData functions should use this to correctly
// output timestamps as a JSON property.
inline void WritePropertyTime(JSONWriter& aWriter, const char* aName,
                              const TimeStamp& aTime) {
  if (!aTime.IsNull()) {
    aWriter.DoubleProperty(
        aName, (aTime - TimeStamp::ProcessCreation()).ToMilliseconds());
  }
}

}  // namespace mozilla::baseprofiler

#  define BASE_PROFILER_MARKER_UNTYPED(markerName, options)        \
    do {                                                           \
      AUTO_PROFILER_STATS(BASE_PROFILER_MARKER_UNTYPED);           \
      ::mozilla::baseprofiler::AddMarker<>(                        \
          markerName, ::mozilla::baseprofiler::category::options); \
    } while (false)

#  define BASE_PROFILER_MARKER(markerName, options, MarkerType, ...) \
    do {                                                             \
      AUTO_PROFILER_STATS(BASE_PROFILER_MARKER_with_##MarkerType);   \
      ::mozilla::baseprofiler::AddMarker<                            \
          ::mozilla::baseprofiler::markers::MarkerType>(             \
          markerName, ::mozilla::baseprofiler::category::options,    \
          ##__VA_ARGS__);                                            \
    } while (false)

namespace mozilla::baseprofiler::markers {
// Most common marker type. Others are in BaseProfilerMarkerTypes.h.
struct Text {
  static constexpr const char* MarkerTypeName() { return "Text"; }
  static void StreamJSONMarkerData(JSONWriter& aWriter,
                                   const ProfilerString8View& aText) {
    aWriter.StringProperty("name", aText.String().c_str());
  }
};
}  // namespace mozilla::baseprofiler::markers

#  define BASE_PROFILER_MARKER_TEXT(markerName, options, text)           \
    do {                                                                 \
      AUTO_PROFILER_STATS(base_add_marker_v2_with_Text);                 \
      ::mozilla::baseprofiler::AddMarker<                                \
          ::mozilla::baseprofiler::markers::Text>(                       \
          markerName, ::mozilla::baseprofiler::category::options, text); \
    } while (false)

#endif  // nfed MOZ_GECKO_PROFILER else

#endif  // BaseProfilerMarkers_h
