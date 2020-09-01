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

// /!\ WORK IN PROGRESS /!\
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
  static constexpr const char* MarkerTypeName() { return "Budget"; }
  static void StreamJSONMarkerData(mozilla::JSONWriter& aWriter) {}
};

struct DOMEvent {
  static constexpr const char* MarkerTypeName() {
    // Note: DOMEventMarkerPayload wase originally a sub-class of
    // TracingMarkerPayload, so it uses the same payload type.
    // TODO: Change to its own distinct type, but this will require front-end
    // changes.
    return "tracing";
  }
  static void StreamJSONMarkerData(
      mozilla::JSONWriter& aWriter,
      const mozilla::ProfilerString16View& aEventType,
      const mozilla::TimeStamp& aEventTimeStamp,
      const mozilla::ProfilerString8View& aTracingCategory) {
    aWriter.StringProperty(
        "eventType",
        NS_ConvertUTF16toUTF8(aEventType.Data(), aEventType.Length()).get());
    // Note: This is the event *creation* timestamp, which should be before the
    // marker's own timestamp. It is used to compute the event processing
    // latency.
    mozilla::baseprofiler::WritePropertyTime(aWriter, "timeStamp",
                                             aEventTimeStamp);
    // TODO: This is from the old TracingMarkerPayload legacy, is it still
    // needed, once the one location where DOMEvent is used has transitioned?
    if (aTracingCategory.Length() != 0) {
      // Note: This is *not* the MarkerCategory, it's a identifier used to
      // combine pairs of markers. This should disappear after "set index" is
      // implemented in bug 1661114.
      aWriter.StringProperty("category", aTracingCategory.String().c_str());
    }
  }
};

struct Pref {
  static constexpr const char* MarkerTypeName() { return "PreferenceRead"; }
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
    aWriter.StringProperty("prefName", aPrefName.String().c_str());
    aWriter.StringProperty("prefKind", PrefValueKindToString(aPrefKind));
    aWriter.StringProperty("prefType", PrefTypeToString(aPrefType));
    aWriter.StringProperty("prefValue", aPrefValue.String().c_str());
  }

 private:
  static const char* PrefValueKindToString(
      const mozilla::Maybe<mozilla::PrefValueKind>& aKind) {
    if (aKind) {
      return *aKind == mozilla::PrefValueKind::Default ? "Default" : "User";
    }
    return "Shared";
  }

  static const char* PrefTypeToString(
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
  static constexpr const char* MarkerTypeName() { return "LayerTranslation"; }
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
  static constexpr const char* MarkerTypeName() { return "VsyncTimestamp"; }
  static void StreamJSONMarkerData(mozilla::JSONWriter& aWriter) {}
};

struct Network {
  static constexpr const char* MarkerTypeName() { return "Network"; }
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
    const char* typeString = GetNetworkState(aType);
    const char* cacheString = GetCacheState(aCacheDisposition);
    // want to use aUniqueStacks.mUniqueStrings->WriteElement(aWriter,
    // typeString);
    aWriter.StringProperty("status", typeString);
    if (cacheString) {
      aWriter.StringProperty("cache", cacheString);
    }
    aWriter.IntProperty("pri", aPri);
    if (aCount > 0) {
      aWriter.IntProperty("count", aCount);
    }
    if (aURI.Length() != 0) {
      aWriter.StringProperty("URI", aURI.String().c_str());
    }
    if (aRedirectURI.Length() != 0) {
      aWriter.StringProperty("RedirectURI", aRedirectURI.String().c_str());
    }

    if (aContentType.Length() != 0) {
      aWriter.StringProperty("contentType", aContentType.String().c_str());
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
  static const char* GetNetworkState(NetworkLoadType aType) {
    switch (aType) {
      case NetworkLoadType::LOAD_START:
        return "STATUS_START";
      case NetworkLoadType::LOAD_STOP:
        return "STATUS_STOP";
      case NetworkLoadType::LOAD_REDIRECT:
        return "STATUS_REDIRECT";
    }
    return "";
  }

  static const char* GetCacheState(
      mozilla::net::CacheDisposition aCacheDisposition) {
    switch (aCacheDisposition) {
      case mozilla::net::kCacheUnresolved:
        return "Unresolved";
      case mozilla::net::kCacheHit:
        return "Hit";
      case mozilla::net::kCacheHitViaReval:
        return "HitViaReval";
      case mozilla::net::kCacheMissedViaReval:
        return "MissedViaReval";
      case mozilla::net::kCacheMissed:
        return "Missed";
      case mozilla::net::kCacheUnknown:
      default:
        return nullptr;
    }
  }
};

struct ScreenshotPayload {
  static constexpr const char* MarkerTypeName() {
    return "CompositorScreenshot";
  }
  static void StreamJSONMarkerData(
      mozilla::JSONWriter& aWriter,
      const mozilla::ProfilerString8View& aScreenshotDataURL,
      const mozilla::gfx::IntSize& aWindowSize, uintptr_t aWindowIdentifier) {
    // TODO: Use UniqueStacks&Strings
    // aUniqueStacks.mUniqueStrings->WriteProperty(aWriter, "url",
    //                                             mScreenshotDataURL.get());
    aWriter.StringProperty("url", aScreenshotDataURL.String().c_str());

    char hexWindowID[32];
    SprintfLiteral(hexWindowID, "0x%" PRIXPTR, aWindowIdentifier);
    aWriter.StringProperty("windowID", hexWindowID);
    aWriter.DoubleProperty("windowWidth", aWindowSize.width);
    aWriter.DoubleProperty("windowHeight", aWindowSize.height);
  }
};

struct GCSlice {
  static constexpr const char* MarkerTypeName() { return "GCSlice"; }
  static void StreamJSONMarkerData(
      mozilla::JSONWriter& aWriter,
      const mozilla::ProfilerString8View& aTimingJSON) {
    if (aTimingJSON.Length() != 0) {
      // TODO: Is SplicedJSONProperty necessary here? (Guessing yes!)
      // aWriter.SplicedJSONProperty("timings", aTimingJSON.String().c_str());
      aWriter.StringProperty("timings", aTimingJSON.String().c_str());
    } else {
      aWriter.NullProperty("timings");
    }
  }
};

struct GCMajor {
  static constexpr const char* MarkerTypeName() { return "GCMajor"; }
  static void StreamJSONMarkerData(
      mozilla::JSONWriter& aWriter,
      const mozilla::ProfilerString8View& aTimingJSON) {
    if (aTimingJSON.Length() != 0) {
      // TODO: Is SplicedJSONProperty necessary here? (Guessing yes!)
      // aWriter.SplicedJSONProperty("timings", aTimingJSON.String().c_str());
      aWriter.StringProperty("timings", aTimingJSON.String().c_str());
    } else {
      aWriter.NullProperty("timings");
    }
  }
};

struct GCMinor {
  static constexpr const char* MarkerTypeName() { return "GCMinor"; }
  static void StreamJSONMarkerData(
      mozilla::JSONWriter& aWriter,
      const mozilla::ProfilerString8View& aTimingJSON) {
    if (aTimingJSON.Length() != 0) {
      // TODO: Is SplicedJSONProperty necessary here? (Guessing yes!)
      // aWriter.SplicedJSONProperty("nursery", aTimingJSON.String().c_str());
      aWriter.StringProperty("nursery", aTimingJSON.String().c_str());
    } else {
      aWriter.NullProperty("nursery");
    }
  }
};

struct StyleMarkerPayload {
  static constexpr const char* MarkerTypeName() { return "Styles"; }
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
  static constexpr const char* MarkerTypeName() { return "JS allocation"; }
  static void StreamJSONMarkerData(
      mozilla::JSONWriter& aWriter,
      const mozilla::ProfilerString16View& aTypeName,
      const mozilla::ProfilerString8View& aClassName,
      const mozilla::ProfilerString16View& aDescriptiveTypeName,
      const mozilla::ProfilerString8View& aCoarseType, uint64_t aSize,
      bool aInNursery) {
    if (aClassName.Length() != 0) {
      aWriter.StringProperty("className", aClassName.String().c_str());
    }
    if (aTypeName.Length() != 0) {
      aWriter.StringProperty(
          "typeName",
          NS_ConvertUTF16toUTF8(aTypeName.Data(), aTypeName.Length()).get());
    }
    if (aDescriptiveTypeName.Length() != 0) {
      aWriter.StringProperty(
          "descriptiveTypeName",
          NS_ConvertUTF16toUTF8(aDescriptiveTypeName.Data(),
                                aDescriptiveTypeName.Length())
              .get());
    }
    aWriter.StringProperty("coarseType", aCoarseType.String().c_str());
    aWriter.IntProperty("size", aSize);
    aWriter.BoolProperty("inNursery", aInNursery);
  }
};

// This payload is for collecting information about native allocations. There is
// a memory hook into malloc and other memory functions that can sample a subset
// of the allocations. This information is then stored in this payload.
struct NativeAllocationMarkerPayload {
  static constexpr const char* MarkerTypeName() { return "Native allocation"; }
  static void StreamJSONMarkerData(mozilla::JSONWriter& aWriter, int64_t aSize,
                                   uintptr_t aMemoryAddress, int aThreadId) {
    aWriter.IntProperty("size", aSize);
    aWriter.IntProperty("memoryAddress", static_cast<int64_t>(aMemoryAddress));
    aWriter.IntProperty("threadId", aThreadId);
  }
};

struct IPCMarkerPayload {
  static constexpr const char* MarkerTypeName() { return "IPC"; }
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
    aWriter.StringProperty("messageType",
                           IPC::StringFromIPCMessageType(aMessageType));
    aWriter.StringProperty("side", IPCSideToString(aSide));
    aWriter.StringProperty("direction", aDirection == MessageDirection::eSending
                                            ? "sending"
                                            : "receiving");
    aWriter.StringProperty("phase", IPCPhaseToString(aPhase));
    aWriter.BoolProperty("sync", aSync);
  }

 private:
  static const char* IPCSideToString(mozilla::ipc::Side aSide) {
    switch (aSide) {
      case mozilla::ipc::ParentSide:
        return "parent";
      case mozilla::ipc::ChildSide:
        return "child";
      case mozilla::ipc::UnknownSide:
        return "unknown";
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid IPC side");
        return "<invalid IPC side>";
    }
  }

  static const char* IPCPhaseToString(mozilla::ipc::MessagePhase aPhase) {
    switch (aPhase) {
      case mozilla::ipc::MessagePhase::Endpoint:
        return "endpoint";
      case mozilla::ipc::MessagePhase::TransferStart:
        return "transferStart";
      case mozilla::ipc::MessagePhase::TransferEnd:
        return "transferEnd";
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid IPC phase");
        return "<invalid IPC phase>";
    }
  }
};

}  // namespace geckoprofiler::markers

#endif  // MOZ_GECKO_PROFILER

#endif  // ProfilerMarkerTypes_h
