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
  static void StreamJSONMarkerData(JSONWriter& aWriter,
                                   const ProfilerString8View& aCategory) {
    if (aCategory.Length() != 0) {
      aWriter.StringProperty("category", aCategory);
    }
  }
};

struct FileIO {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("FileIO");
  }
  static void StreamJSONMarkerData(JSONWriter& aWriter,
                                   const ProfilerString8View& aOperation,
                                   const ProfilerString8View& aSource,
                                   const ProfilerString8View& aFilename,
                                   MarkerThreadId aOperationThreadId) {
    aWriter.StringProperty("operation", aOperation);
    aWriter.StringProperty("source", aSource);
    if (aFilename.Length() != 0) {
      aWriter.StringProperty("filename", aFilename);
    }
    if (!aOperationThreadId.IsUnspecified()) {
      aWriter.IntProperty("threadId", aOperationThreadId.ThreadId());
    }
  }
};

struct UserTimingMark {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("UserTiming");
  }
  static void StreamJSONMarkerData(JSONWriter& aWriter,
                                   const ProfilerString8View& aName) {
    aWriter.StringProperty("name", aName);
    aWriter.StringProperty("entryType", "mark");
    aWriter.NullProperty("startMark");
    aWriter.NullProperty("endMark");
  }
};

struct UserTimingMeasure {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("UserTiming");
  }
  static void StreamJSONMarkerData(JSONWriter& aWriter,
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
};

struct Hang {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("BHR-detected hang");
  }
  static void StreamJSONMarkerData(JSONWriter& aWriter) {}
};

struct LongTask {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("MainThreadLongTask");
  }
  static void StreamJSONMarkerData(JSONWriter& aWriter) {
    aWriter.StringProperty("category", "LongTask");
  }
};

struct Log {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("Log");
  }
  static void StreamJSONMarkerData(JSONWriter& aWriter,
                                   const ProfilerString8View& aModule,
                                   const ProfilerString8View& aText) {
    aWriter.StringProperty("module", aModule);
    aWriter.StringProperty("name", aText);
  }
};

struct MediaSample {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("MediaSample");
  }
  static void StreamJSONMarkerData(JSONWriter& aWriter,
                                   int64_t aSampleStartTimeUs,
                                   int64_t aSampleEndTimeUs) {
    aWriter.IntProperty("sampleStartTimeUs", aSampleStartTimeUs);
    aWriter.IntProperty("sampleEndTimeUs", aSampleEndTimeUs);
  }
};

}  // namespace mozilla::baseprofiler::markers

#endif  // MOZ_GECKO_PROFILER

#endif  // BaseProfilerMarkerTypes_h
