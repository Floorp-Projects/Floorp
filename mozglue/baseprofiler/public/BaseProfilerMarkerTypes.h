/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BaseProfilerMarkerTypes_h
#define BaseProfilerMarkerTypes_h

// This header contains common marker type definitions.
//
// It #include's "mozilla/BaseProfilerMarkers.h", see that file for how to
// define other marker types, and how to add markers to the profiler buffers.
//
// If you don't need to use these common types, #include
// "mozilla/BaseProfilerMarkers.h" instead.
//
// Types in this files can be defined without relying on xpcom.
// Others are defined in "ProfilerMarkerTypes.h".

// !!!                       /!\ WORK IN PROGRESS /!\                       !!!
// This file contains draft marker definitions, but most are not used yet.
// Further work is needed to complete these definitions, and use them to convert
// existing PROFILER_ADD_MARKER calls. See meta bug 1661394.

#include "mozilla/BaseProfilerMarkers.h"

#ifdef MOZ_GECKO_PROFILER

namespace mozilla::baseprofiler::markers {

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
    MS schema{MS::Location::markerChart, MS::Location::markerTable,
              MS::Location::timelineOverview};
    schema.AddKeyLabelFormat("category", "Type", MS::Format::string);
    return schema;
  }
};

struct UserTimingMark {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("UserTimingMark");
  }
  static void StreamJSONMarkerData(SpliceableJSONWriter& aWriter,
                                   const ProfilerString8View& aName) {
    aWriter.StringProperty("name", aName);
    aWriter.StringProperty("entryType", "mark");
    aWriter.NullProperty("startMark");
    aWriter.NullProperty("endMark");
  }
  static MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::markerChart, MS::Location::markerTable};
    schema.SetAllLabels("{marker.data.name}");
    schema.AddStaticLabelValue("Marker", "UserTiming");
    schema.AddKeyLabelFormat("entryType", "Entry Type", MS::Format::string);
    schema.AddKeyLabelFormat("name", "Name", MS::Format::string);
    schema.AddStaticLabelValue(
        "Description",
        "UserTimingMark is created using the DOM API performance.mark().");
    return schema;
  }
};

struct UserTimingMeasure {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("UserTimingMeasure");
  }
  static void StreamJSONMarkerData(SpliceableJSONWriter& aWriter,
                                   const ProfilerString8View& aName,
                                   const Maybe<ProfilerString8View>& aStartMark,
                                   const Maybe<ProfilerString8View>& aEndMark) {
    aWriter.StringProperty("name", aName);
    aWriter.StringProperty("entryType", "measure");

    if (aStartMark.isSome()) {
      aWriter.StringProperty("startMark", *aStartMark);
    } else {
      aWriter.NullProperty("startMark");
    }
    if (aEndMark.isSome()) {
      aWriter.StringProperty("endMark", *aEndMark);
    } else {
      aWriter.NullProperty("endMark");
    }
  }
  static MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::markerChart, MS::Location::markerTable};
    schema.SetAllLabels("{marker.data.name}");
    schema.AddStaticLabelValue("Marker", "UserTiming");
    schema.AddKeyLabelFormat("entryType", "Entry Type", MS::Format::string);
    schema.AddKeyLabelFormat("name", "Name", MS::Format::string);
    schema.AddKeyLabelFormat("startMark", "Start Mark", MS::Format::string);
    schema.AddKeyLabelFormat("endMark", "End Mark", MS::Format::string);
    schema.AddStaticLabelValue("Description",
                               "UserTimingMeasure is created using the DOM API "
                               "performance.measure().");
    return schema;
  }
};

struct Log {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("Log");
  }
  static void StreamJSONMarkerData(SpliceableJSONWriter& aWriter,
                                   const ProfilerString8View& aModule,
                                   const ProfilerString8View& aText) {
    aWriter.StringProperty("module", aModule);
    aWriter.StringProperty("name", aText);
  }
  static MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::markerTable};
    schema.SetTableLabel("({marker.data.module}) {marker.data.name}");
    schema.AddKeyLabelFormat("module", "Module", MS::Format::string);
    schema.AddKeyLabelFormat("name", "Name", MS::Format::string);
    return schema;
  }
};

struct MediaSample {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("MediaSample");
  }
  static void StreamJSONMarkerData(SpliceableJSONWriter& aWriter,
                                   int64_t aSampleStartTimeUs,
                                   int64_t aSampleEndTimeUs) {
    aWriter.IntProperty("sampleStartTimeUs", aSampleStartTimeUs);
    aWriter.IntProperty("sampleEndTimeUs", aSampleEndTimeUs);
  }
  static MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::markerChart, MS::Location::markerTable};
    schema.AddKeyLabelFormat("sampleStartTimeUs", "Sample start time",
                             MS::Format::microseconds);
    schema.AddKeyLabelFormat("sampleEndTimeUs", "Sample end time",
                             MS::Format::microseconds);
    return schema;
  }
};

}  // namespace mozilla::baseprofiler::markers

#endif  // MOZ_GECKO_PROFILER

#endif  // BaseProfilerMarkerTypes_h
