/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <inttypes.h>

#include "GeckoProfiler.h"
#include "ProfilerBacktrace.h"
#include "ProfilerMarkerPayload.h"
#include "gfxASurface.h"
#include "Layers.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Maybe.h"

using namespace mozilla;

void
ProfilerMarkerPayload::StreamCommonProps(const char* aMarkerType,
                                         SpliceableJSONWriter& aWriter,
                                         const TimeStamp& aProcessStartTime,
                                         UniqueStacks& aUniqueStacks)
{
  MOZ_ASSERT(aMarkerType);
  aWriter.StringProperty("type", aMarkerType);
  if (!mStartTime.IsNull()) {
    aWriter.DoubleProperty("startTime",
                           (mStartTime - aProcessStartTime).ToMilliseconds());
  }
  if (!mEndTime.IsNull()) {
    aWriter.DoubleProperty("endTime",
                           (mEndTime - aProcessStartTime).ToMilliseconds());
  }
  if (mStack) {
    aWriter.StartObjectProperty("stack");
    {
      mStack->StreamJSON(aWriter, aProcessStartTime, aUniqueStacks);
    }
    aWriter.EndObject();
  }
}

void
TracingMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                    const TimeStamp& aProcessStartTime,
                                    UniqueStacks& aUniqueStacks)
{
  StreamCommonProps("tracing", aWriter, aProcessStartTime, aUniqueStacks);

  if (mCategory) {
    aWriter.StringProperty("category", mCategory);
  }

  if (mKind == TRACING_INTERVAL_START) {
    aWriter.StringProperty("interval", "start");
  } else if (mKind == TRACING_INTERVAL_END) {
    aWriter.StringProperty("interval", "end");
  }
}

void
IOMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                               const TimeStamp& aProcessStartTime,
                               UniqueStacks& aUniqueStacks)
{
  StreamCommonProps("io", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("source", mSource);
  if (mFilename) {
    aWriter.StringProperty("filename", mFilename.get());
  }
}

void
UserTimingMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                       const TimeStamp& aProcessStartTime,
                                       UniqueStacks& aUniqueStacks)
{
  StreamCommonProps("UserTiming", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("name", NS_ConvertUTF16toUTF8(mName).get());
  aWriter.StringProperty("entryType", mEntryType);

  if (mStartMark.isSome()) {
    aWriter.StringProperty("startMark", NS_ConvertUTF16toUTF8(mStartMark.value()).get());
  } else {
    aWriter.NullProperty("startMark");
  }
  if (mEndMark.isSome()) {
    aWriter.StringProperty("endMark", NS_ConvertUTF16toUTF8(mEndMark.value()).get());
  } else {
    aWriter.NullProperty("endMark");
  }
}

void
DOMEventMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                     const TimeStamp& aProcessStartTime,
                                     UniqueStacks& aUniqueStacks)
{
  StreamCommonProps("DOMEvent", aWriter, aProcessStartTime, aUniqueStacks);
  if (!mTimeStamp.IsNull()) {
    aWriter.DoubleProperty("timeStamp",
                           (mTimeStamp - aProcessStartTime).ToMilliseconds());
  }
  aWriter.StringProperty("eventType", NS_ConvertUTF16toUTF8(mEventType).get());
  aWriter.IntProperty("phase", mPhase);
}

void
LayerTranslationMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                             const TimeStamp& aProcessStartTime,
                                             UniqueStacks& aUniqueStacks)
{
  const size_t bufferSize = 32;
  char buffer[bufferSize];
  SprintfLiteral(buffer, "%p", mLayer);

  aWriter.StringProperty("layer", buffer);
  aWriter.IntProperty("x", mPoint.x);
  aWriter.IntProperty("y", mPoint.y);
  aWriter.StringProperty("category", "LayerTranslation");
}

void
VsyncMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                  const TimeStamp& aProcessStartTime,
                                  UniqueStacks& aUniqueStacks)
{
  aWriter.DoubleProperty("vsync",
                         (mVsyncTimestamp - aProcessStartTime).ToMilliseconds());
  aWriter.StringProperty("category", "VsyncTimestamp");
}

static const char *GetNetworkState(uint32_t aState)
{
  const struct NetworkState {
    uint32_t mStatus;
    const char *mName;
  } NetworkStates[] = {
    { 0, "STATUS_START" },
    // it's unclear if these will occur; they're in network-monitor.js
    { 0x5001, "REQUEST_HEADER" },
    { 0x5002, "REQUEST_BODY_SENT" },
    { 0x5003, "RESPONSE_START" },
    { 0x5004, "RESPONSE_HEADER" },
    { 0x5005, "RESPONSE_COMPLETE" },
    { 0x5006, "TRANSACTION_CLOSE" },

    { 0x804b0003, "STATUS_RESOLVING" },
    { 0x804b000b, "STATUS_RESOLVED" },
    { 0x804b0007, "STATUS_CONNECTING_TO" },
    { 0x804b0004, "STATUS_CONNECTED_TO" },
    { 0x804b0005, "STATUS_SENDING_TO" },
    { 0x804b000a, "STATUS_WAITING_FOR" },
    { 0x804b0006, "STATUS_RECEIVING_FROM" },
    { 0x804b0008, "STATUS_READING" },
    { 0x804b000c, "STATUS_TLS_STARTING" },
    { 0x804b000d, "STATUS_TLS_ENDING" },
  };

  for (const NetworkState& entry : NetworkStates) {
    if (aState == entry.mStatus) {
      return entry.mName;
    }
  }
  // XXX perhaps sprintf 0x* into a static buffer and return that?
  return ""; // Shouldn't happen...
}

void
NetworkMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                    const TimeStamp& aProcessStartTime,
                                    UniqueStacks& aUniqueStacks)
{
  StreamCommonProps("Network", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.IntProperty("id", mID);
  const char *statusString = GetNetworkState(static_cast<uint32_t>(mStatus));
  // want to use aUniqueStacks.mUniqueStrings->WriteElement(aWriter, statusString);
  aWriter.StringProperty("status", statusString);
  if (mURI) {
    aWriter.StringProperty("URI", mURI.get());
  }
}

void
ScreenshotPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                  const TimeStamp& aProcessStartTime,
                                  UniqueStacks& aUniqueStacks)
{
  aUniqueStacks.mUniqueStrings->WriteProperty(aWriter, "url", mScreenshotDataURL.get());

  char hexWindowID[32];
  SprintfLiteral(hexWindowID, "0x%" PRIXPTR, mWindowIdentifier);
  aWriter.StringProperty("windowID", hexWindowID);
  aWriter.DoubleProperty("windowWidth", mWindowSize.width);
  aWriter.DoubleProperty("windowHeight", mWindowSize.height);
}

void
GCSliceMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                    const TimeStamp& aProcessStartTime,
                                    UniqueStacks& aUniqueStacks)
{
  MOZ_ASSERT(mTimingJSON);
  StreamCommonProps("GCSlice", aWriter, aProcessStartTime, aUniqueStacks);
  if (mTimingJSON) {
    aWriter.SplicedJSONProperty("timings", mTimingJSON.get());
  } else {
    aWriter.NullProperty("timings");
  }
}

void
GCMajorMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                    const TimeStamp& aProcessStartTime,
                                    UniqueStacks& aUniqueStacks)
{
  MOZ_ASSERT(mTimingJSON);
  StreamCommonProps("GCMajor", aWriter, aProcessStartTime, aUniqueStacks);
  if (mTimingJSON) {
    aWriter.SplicedJSONProperty("timings", mTimingJSON.get());
  } else {
    aWriter.NullProperty("timings");
  }
}

void
GCMinorMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                    const TimeStamp& aProcessStartTime,
                                    UniqueStacks& aUniqueStacks)
{
  MOZ_ASSERT(mTimingData);
  StreamCommonProps("GCMinor", aWriter, aProcessStartTime, aUniqueStacks);
  if (mTimingData) {
    aWriter.SplicedJSONProperty("nursery", mTimingData.get());
  } else {
    aWriter.NullProperty("nursery");
  }
}

void
HangMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                 const TimeStamp& aProcessStartTime,
                                 UniqueStacks& aUniqueStacks)
{
  StreamCommonProps("BHR-detected hang", aWriter, aProcessStartTime, aUniqueStacks);
}

void
StyleMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                  const TimeStamp& aProcessStartTime,
                                  UniqueStacks& aUniqueStacks)
{
  StreamCommonProps("Styles", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("category", "Paint");
  aWriter.IntProperty("elementsTraversed", mStats.mElementsTraversed);
  aWriter.IntProperty("elementsStyled", mStats.mElementsStyled);
  aWriter.IntProperty("elementsMatched", mStats.mElementsMatched);
  aWriter.IntProperty("stylesShared", mStats.mStylesShared);
  aWriter.IntProperty("stylesReused", mStats.mStylesReused);
}
