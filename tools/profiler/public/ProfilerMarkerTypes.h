/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerMarkerTypes_h
#define ProfilerMarkerTypes_h

// This header contains common marker type definitions that rely on xpcom.
//
// It #include's "mozilla/BaseProfilerMarkerTypess.h" and "ProfilerMarkers.h",
// see these files for more marker types, how to define other marker types, and
// how to add markers to the profiler buffers.

// !!!                       /!\ WORK IN PROGRESS /!\                       !!!
// This file contains draft marker definitions, but most are not used yet.
// Further work is needed to complete these definitions, and use them to convert
// existing PROFILER_ADD_MARKER calls. See meta bug 1661394.

#include "mozilla/BaseProfilerMarkerTypes.h"
#include "mozilla/ProfilerMarkers.h"

#ifdef MOZ_GECKO_PROFILER

#  include "gfxASurface.h"
#  include "js/AllocationRecording.h"
#  include "js/ProfilingFrameIterator.h"
#  include "js/Utility.h"
#  include "Layers.h"
#  include "mozilla/ipc/ProtocolUtils.h"
#  include "mozilla/net/HttpBaseChannel.h"
#  include "mozilla/Preferences.h"
#  include "mozilla/ServoTraversalStatistics.h"

namespace geckoprofiler::markers {

// Import some common markers from mozilla::baseprofiler::markers.
using Tracing = mozilla::baseprofiler::markers::Tracing;
using FileIO = mozilla::baseprofiler::markers::FileIO;
using UserTimingMark = mozilla::baseprofiler::markers::UserTimingMark;
using UserTimingMeasure = mozilla::baseprofiler::markers::UserTimingMeasure;
using Hang = mozilla::baseprofiler::markers::Hang;
using LongTask = mozilla::baseprofiler::markers::LongTask;
using Log = mozilla::baseprofiler::markers::Log;
using MediaSample = mozilla::baseprofiler::markers::MediaSample;

struct Budget {
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return mozilla::MakeStringSpan("Budget");
  }
  static void StreamJSONMarkerData(mozilla::JSONWriter& aWriter) {}
};

struct Pref {
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return mozilla::MakeStringSpan("PreferenceRead");
  }
  static void StreamJSONMarkerData(
      mozilla::JSONWriter& aWriter,
      const mozilla::ProfilerString8View& aPrefName,
      const mozilla::Maybe<mozilla::PrefValueKind>& aPrefKind,
      const mozilla::Maybe<mozilla::PrefType>& aPrefType,
      const mozilla::ProfilerString8View& aPrefValue,
      const mozilla::TimeStamp& aPrefAccessTime) {
    // TODO: This looks like it's always `Now()`, so it could probably be
    // removed; but the frontend may need updating first.
    mozilla::baseprofiler::WritePropertyTime(aWriter, "prefAccessTime",
                                             aPrefAccessTime);
    aWriter.StringProperty("prefName", aPrefName);
    aWriter.StringProperty("prefKind", PrefValueKindToString(aPrefKind));
    aWriter.StringProperty("prefType", PrefTypeToString(aPrefType));
    aWriter.StringProperty("prefValue", aPrefValue);
  }

 private:
  static mozilla::Span<const char> PrefValueKindToString(
      const mozilla::Maybe<mozilla::PrefValueKind>& aKind) {
    if (aKind) {
      return *aKind == mozilla::PrefValueKind::Default
                 ? mozilla::MakeStringSpan("Default")
                 : mozilla::MakeStringSpan("User");
    }
    return "Shared";
  }

  static mozilla::Span<const char> PrefTypeToString(
      const mozilla::Maybe<mozilla::PrefType>& type) {
    if (type) {
      switch (*type) {
        case mozilla::PrefType::None:
          return "None";
        case mozilla::PrefType::Int:
          return "Int";
        case mozilla::PrefType::Bool:
          return "Bool";
        case mozilla::PrefType::String:
          return "String";
        default:
          MOZ_ASSERT_UNREACHABLE("Unknown preference type.");
      }
    }
    return "Preference not found";
  }
};

// Contains the translation applied to a 2d layer so we can track the layer
// position at each frame.
struct LayerTranslation {
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return mozilla::MakeStringSpan("LayerTranslation");
  }
  static void StreamJSONMarkerData(mozilla::JSONWriter& aWriter,
                                   mozilla::layers::Layer* aLayer,
                                   mozilla::gfx::Point aPoint) {
    const size_t bufferSize = 32;
    char buffer[bufferSize];
    SprintfLiteral(buffer, "%p", aLayer);

    aWriter.StringProperty("layer", buffer);
    aWriter.IntProperty("x", aPoint.x);
    aWriter.IntProperty("y", aPoint.y);
  }
};

// Tracks when a vsync occurs according to the HardwareComposer.
struct Vsync {
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return mozilla::MakeStringSpan("VsyncTimestamp");
  }
  static void StreamJSONMarkerData(mozilla::JSONWriter& aWriter) {}
};

struct Network {
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return mozilla::MakeStringSpan("Network");
  }
  static void StreamJSONMarkerData(
      mozilla::JSONWriter& aWriter, int64_t aID,
      const mozilla::ProfilerString8View& aURI, NetworkLoadType aType,
      const mozilla::TimeStamp& aStartTime, const mozilla::TimeStamp& aEndTime,
      int32_t aPri, int64_t aCount,
      mozilla::net::CacheDisposition aCacheDisposition, uint64_t aInnerWindowID,
      const mozilla::net::TimingStruct& aTimings,
      const mozilla::ProfilerString8View& aRedirectURI,
      UniqueProfilerBacktrace aSource,
      const mozilla::ProfilerString8View& aContentType) {
    // TODO: Remove these Legacy start&end times when frontend is updated.
    mozilla::baseprofiler::WritePropertyTime(aWriter, "startTime", aStartTime);
    mozilla::baseprofiler::WritePropertyTime(aWriter, "endTime", aEndTime);

    aWriter.IntProperty("id", aID);
    mozilla::Span<const char> typeString = GetNetworkState(aType);
    mozilla::Span<const char> cacheString = GetCacheState(aCacheDisposition);
    // want to use aUniqueStacks.mUniqueStrings->WriteElement(aWriter,
    // typeString);
    aWriter.StringProperty("status", typeString);
    if (!cacheString.IsEmpty()) {
      aWriter.StringProperty("cache", cacheString);
    }
    aWriter.IntProperty("pri", aPri);
    if (aCount > 0) {
      aWriter.IntProperty("count", aCount);
    }
    if (aURI.Length() != 0) {
      aWriter.StringProperty("URI", aURI);
    }
    if (aRedirectURI.Length() != 0) {
      aWriter.StringProperty("RedirectURI", aRedirectURI);
    }

    if (aContentType.Length() != 0) {
      aWriter.StringProperty("contentType", aContentType);
    } else {
      aWriter.NullProperty("contentType");
    }

    if (aType != NetworkLoadType::LOAD_START) {
      mozilla::baseprofiler::WritePropertyTime(aWriter, "domainLookupStart",
                                               aTimings.domainLookupStart);
      mozilla::baseprofiler::WritePropertyTime(aWriter, "domainLookupEnd",
                                               aTimings.domainLookupEnd);
      mozilla::baseprofiler::WritePropertyTime(aWriter, "connectStart",
                                               aTimings.connectStart);
      mozilla::baseprofiler::WritePropertyTime(aWriter, "tcpConnectEnd",
                                               aTimings.tcpConnectEnd);
      mozilla::baseprofiler::WritePropertyTime(aWriter, "secureConnectionStart",
                                               aTimings.secureConnectionStart);
      mozilla::baseprofiler::WritePropertyTime(aWriter, "connectEnd",
                                               aTimings.connectEnd);
      mozilla::baseprofiler::WritePropertyTime(aWriter, "requestStart",
                                               aTimings.requestStart);
      mozilla::baseprofiler::WritePropertyTime(aWriter, "responseStart",
                                               aTimings.responseStart);
      mozilla::baseprofiler::WritePropertyTime(aWriter, "responseEnd",
                                               aTimings.responseEnd);
    }
  }

 private:
  static mozilla::Span<const char> GetNetworkState(NetworkLoadType aType) {
    switch (aType) {
      case NetworkLoadType::LOAD_START:
        return mozilla::MakeStringSpan("STATUS_START");
      case NetworkLoadType::LOAD_STOP:
        return mozilla::MakeStringSpan("STATUS_STOP");
      case NetworkLoadType::LOAD_REDIRECT:
        return mozilla::MakeStringSpan("STATUS_REDIRECT");
    }
    return mozilla::MakeStringSpan("");
  }

  static mozilla::Span<const char> GetCacheState(
      mozilla::net::CacheDisposition aCacheDisposition) {
    switch (aCacheDisposition) {
      case mozilla::net::kCacheUnresolved:
        return mozilla::MakeStringSpan("Unresolved");
      case mozilla::net::kCacheHit:
        return mozilla::MakeStringSpan("Hit");
      case mozilla::net::kCacheHitViaReval:
        return mozilla::MakeStringSpan("HitViaReval");
      case mozilla::net::kCacheMissedViaReval:
        return mozilla::MakeStringSpan("MissedViaReval");
      case mozilla::net::kCacheMissed:
        return mozilla::MakeStringSpan("Missed");
      case mozilla::net::kCacheUnknown:
      default:
        return mozilla::MakeStringSpan("");
    }
  }
};

struct ScreenshotPayload {
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return mozilla::MakeStringSpan("CompositorScreenshot");
  }
  static void StreamJSONMarkerData(
      mozilla::JSONWriter& aWriter,
      const mozilla::ProfilerString8View& aScreenshotDataURL,
      const mozilla::gfx::IntSize& aWindowSize, uintptr_t aWindowIdentifier) {
    // TODO: Use UniqueStacks&Strings
    // aUniqueStacks.mUniqueStrings->WriteProperty(aWriter, "url",
    //                                             mScreenshotDataURL.get());
    aWriter.StringProperty("url", aScreenshotDataURL);

    char hexWindowID[32];
    SprintfLiteral(hexWindowID, "0x%" PRIXPTR, aWindowIdentifier);
    aWriter.StringProperty("windowID", hexWindowID);
    aWriter.DoubleProperty("windowWidth", aWindowSize.width);
    aWriter.DoubleProperty("windowHeight", aWindowSize.height);
  }
};

struct GCSlice {
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return mozilla::MakeStringSpan("GCSlice");
  }
  static void StreamJSONMarkerData(
      mozilla::JSONWriter& aWriter,
      const mozilla::ProfilerString8View& aTimingJSON) {
    if (aTimingJSON.Length() != 0) {
      // TODO: Is SplicedJSONProperty necessary here? (Guessing yes!)
      // aWriter.SplicedJSONProperty("timings", aTimingJSON);
      aWriter.StringProperty("timings", aTimingJSON);
    } else {
      aWriter.NullProperty("timings");
    }
  }
};

struct GCMajor {
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return mozilla::MakeStringSpan("GCMajor");
  }
  static void StreamJSONMarkerData(
      mozilla::JSONWriter& aWriter,
      const mozilla::ProfilerString8View& aTimingJSON) {
    if (aTimingJSON.Length() != 0) {
      // TODO: Is SplicedJSONProperty necessary here? (Guessing yes!)
      // aWriter.SplicedJSONProperty("timings", aTimingJSON);
      aWriter.StringProperty("timings", aTimingJSON);
    } else {
      aWriter.NullProperty("timings");
    }
  }
};

struct GCMinor {
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return mozilla::MakeStringSpan("GCMinor");
  }
  static void StreamJSONMarkerData(
      mozilla::JSONWriter& aWriter,
      const mozilla::ProfilerString8View& aTimingJSON) {
    if (aTimingJSON.Length() != 0) {
      // TODO: Is SplicedJSONProperty necessary here? (Guessing yes!)
      // aWriter.SplicedJSONProperty("nursery", aTimingJSON);
      aWriter.StringProperty("nursery", aTimingJSON);
    } else {
      aWriter.NullProperty("nursery");
    }
  }
};

struct StyleMarkerPayload {
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return mozilla::MakeStringSpan("Styles");
  }
  static void StreamJSONMarkerData(
      mozilla::JSONWriter& aWriter,
      const mozilla::ServoTraversalStatistics& aStats) {
    aWriter.StringProperty("category", "Paint");
    aWriter.IntProperty("elementsTraversed", aStats.mElementsTraversed);
    aWriter.IntProperty("elementsStyled", aStats.mElementsStyled);
    aWriter.IntProperty("elementsMatched", aStats.mElementsMatched);
    aWriter.IntProperty("stylesShared", aStats.mStylesShared);
    aWriter.IntProperty("stylesReused", aStats.mStylesReused);
  }
};

class JsAllocationMarkerPayload {
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return mozilla::MakeStringSpan("JS allocation");
  }
  static void StreamJSONMarkerData(
      mozilla::JSONWriter& aWriter,
      const mozilla::ProfilerString16View& aTypeName,
      const mozilla::ProfilerString8View& aClassName,
      const mozilla::ProfilerString16View& aDescriptiveTypeName,
      const mozilla::ProfilerString8View& aCoarseType, uint64_t aSize,
      bool aInNursery) {
    if (aClassName.Length() != 0) {
      aWriter.StringProperty("className", aClassName);
    }
    if (aTypeName.Length() != 0) {
      aWriter.StringProperty(
          "typeName",
          NS_ConvertUTF16toUTF8(aTypeName.Data(), aTypeName.Length()));
    }
    if (aDescriptiveTypeName.Length() != 0) {
      aWriter.StringProperty(
          "descriptiveTypeName",
          NS_ConvertUTF16toUTF8(aDescriptiveTypeName.Data(),
                                aDescriptiveTypeName.Length()));
    }
    aWriter.StringProperty("coarseType", aCoarseType);
    aWriter.IntProperty("size", aSize);
    aWriter.BoolProperty("inNursery", aInNursery);
  }
};

// This payload is for collecting information about native allocations. There is
// a memory hook into malloc and other memory functions that can sample a subset
// of the allocations. This information is then stored in this payload.
struct NativeAllocationMarkerPayload {
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return mozilla::MakeStringSpan("Native allocation");
  }
  static void StreamJSONMarkerData(mozilla::JSONWriter& aWriter, int64_t aSize,
                                   uintptr_t aMemoryAddress, int aThreadId) {
    aWriter.IntProperty("size", aSize);
    aWriter.IntProperty("memoryAddress", static_cast<int64_t>(aMemoryAddress));
    aWriter.IntProperty("threadId", aThreadId);
  }
};

struct IPCMarkerPayload {
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return mozilla::MakeStringSpan("IPC");
  }
  static void StreamJSONMarkerData(mozilla::JSONWriter& aWriter,
                                   int32_t aOtherPid, int32_t aMessageSeqno,
                                   IPC::Message::msgid_t aMessageType,
                                   mozilla::ipc::Side aSide,
                                   mozilla::ipc::MessageDirection aDirection,
                                   mozilla::ipc::MessagePhase aPhase,
                                   bool aSync,
                                   const mozilla::TimeStamp& aTime) {
    // TODO: Remove these Legacy times when frontend is updated.
    mozilla::baseprofiler::WritePropertyTime(aWriter, "startTime", aTime);
    mozilla::baseprofiler::WritePropertyTime(aWriter, "endTime", aTime);

    using namespace mozilla::ipc;
    aWriter.IntProperty("otherPid", aOtherPid);
    aWriter.IntProperty("messageSeqno", aMessageSeqno);
    aWriter.StringProperty(
        "messageType",
        mozilla::MakeStringSpan(IPC::StringFromIPCMessageType(aMessageType)));
    aWriter.StringProperty("side", IPCSideToString(aSide));
    aWriter.StringProperty("direction",
                           aDirection == MessageDirection::eSending
                               ? mozilla::MakeStringSpan("sending")
                               : mozilla::MakeStringSpan("receiving"));
    aWriter.StringProperty("phase", IPCPhaseToString(aPhase));
    aWriter.BoolProperty("sync", aSync);
  }

 private:
  static mozilla::Span<const char> IPCSideToString(mozilla::ipc::Side aSide) {
    switch (aSide) {
      case mozilla::ipc::ParentSide:
        return mozilla::MakeStringSpan("parent");
      case mozilla::ipc::ChildSide:
        return mozilla::MakeStringSpan("child");
      case mozilla::ipc::UnknownSide:
        return mozilla::MakeStringSpan("unknown");
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid IPC side");
        return mozilla::MakeStringSpan("<invalid IPC side>");
    }
  }

  static mozilla::Span<const char> IPCPhaseToString(
      mozilla::ipc::MessagePhase aPhase) {
    switch (aPhase) {
      case mozilla::ipc::MessagePhase::Endpoint:
        return mozilla::MakeStringSpan("endpoint");
      case mozilla::ipc::MessagePhase::TransferStart:
        return mozilla::MakeStringSpan("transferStart");
      case mozilla::ipc::MessagePhase::TransferEnd:
        return mozilla::MakeStringSpan("transferEnd");
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid IPC phase");
        return mozilla::MakeStringSpan("<invalid IPC phase>");
    }
  }
};

}  // namespace geckoprofiler::markers

#endif  // MOZ_GECKO_PROFILER

#endif  // ProfilerMarkerTypes_h
