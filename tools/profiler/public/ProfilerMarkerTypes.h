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

#  include "js/ProfilingFrameIterator.h"
#  include "js/Utility.h"
#  include "mozilla/ipc/ProtocolUtils.h"
#  include "mozilla/Preferences.h"
#  include "mozilla/ServoTraversalStatistics.h"

namespace geckoprofiler::markers {

// Import some common markers from mozilla::baseprofiler::markers.
using Tracing = mozilla::baseprofiler::markers::Tracing;
using UserTimingMark = mozilla::baseprofiler::markers::UserTimingMark;
using UserTimingMeasure = mozilla::baseprofiler::markers::UserTimingMeasure;
using MediaSampleMarker = mozilla::baseprofiler::markers::MediaSampleMarker;
using ContentBuildMarker = mozilla::baseprofiler::markers::ContentBuildMarker;

struct IPCMarkerPayload {
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return mozilla::MakeStringSpan("IPC");
  }
  static void StreamJSONMarkerData(
      mozilla::baseprofiler::SpliceableJSONWriter& aWriter, int32_t aOtherPid,
      int32_t aMessageSeqno, IPC::Message::msgid_t aMessageType,
      mozilla::ipc::Side aSide, mozilla::ipc::MessageDirection aDirection,
      mozilla::ipc::MessagePhase aPhase, bool aSync,
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
  static mozilla::MarkerSchema MarkerTypeDisplay() {
    return mozilla::MarkerSchema::SpecialFrontendLocation{};
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
