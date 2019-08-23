/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerMarkerPayload.h"

#include "GeckoProfiler.h"
#include "ProfileBufferEntry.h"
#include "ProfileJSONWriter.h"
#include "ProfilerBacktrace.h"

#include "gfxASurface.h"
#include "Layers.h"
#include "mozilla/Maybe.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/Sprintf.h"

#include <inttypes.h>

using namespace mozilla;

static void MOZ_ALWAYS_INLINE WriteTime(SpliceableJSONWriter& aWriter,
                                        const TimeStamp& aProcessStartTime,
                                        const TimeStamp& aTime,
                                        const char* aName) {
  if (!aTime.IsNull()) {
    aWriter.DoubleProperty(aName, (aTime - aProcessStartTime).ToMilliseconds());
  }
}

void ProfilerMarkerPayload::StreamType(const char* aMarkerType,
                                       SpliceableJSONWriter& aWriter) {
  MOZ_ASSERT(aMarkerType);
  aWriter.StringProperty("type", aMarkerType);
}

void ProfilerMarkerPayload::StreamCommonProps(
    const char* aMarkerType, SpliceableJSONWriter& aWriter,
    const TimeStamp& aProcessStartTime, UniqueStacks& aUniqueStacks) {
  StreamType(aMarkerType, aWriter);
  WriteTime(aWriter, aProcessStartTime, mStartTime, "startTime");
  WriteTime(aWriter, aProcessStartTime, mEndTime, "endTime");
  if (mDocShellId) {
    aWriter.StringProperty("docShellId", nsIDToCString(*mDocShellId).get());
  }
  if (mDocShellHistoryId) {
    aWriter.DoubleProperty("docshellHistoryId", mDocShellHistoryId.ref());
  }
  if (mStack) {
    aWriter.StartObjectProperty("stack");
    { mStack->StreamJSON(aWriter, aProcessStartTime, aUniqueStacks); }
    aWriter.EndObject();
  }
}

void TracingMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                         const TimeStamp& aProcessStartTime,
                                         UniqueStacks& aUniqueStacks) {
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

void FileIOMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                        const TimeStamp& aProcessStartTime,
                                        UniqueStacks& aUniqueStacks) {
  StreamCommonProps("FileIO", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("operation", mOperation.get());
  aWriter.StringProperty("source", mSource);
  if (mFilename) {
    aWriter.StringProperty("filename", mFilename.get());
  }
}

void UserTimingMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                            const TimeStamp& aProcessStartTime,
                                            UniqueStacks& aUniqueStacks) {
  StreamCommonProps("UserTiming", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("name", NS_ConvertUTF16toUTF8(mName).get());
  aWriter.StringProperty("entryType", mEntryType);

  if (mStartMark.isSome()) {
    aWriter.StringProperty("startMark",
                           NS_ConvertUTF16toUTF8(mStartMark.value()).get());
  } else {
    aWriter.NullProperty("startMark");
  }
  if (mEndMark.isSome()) {
    aWriter.StringProperty("endMark",
                           NS_ConvertUTF16toUTF8(mEndMark.value()).get());
  } else {
    aWriter.NullProperty("endMark");
  }
}

void TextMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                      const TimeStamp& aProcessStartTime,
                                      UniqueStacks& aUniqueStacks) {
  StreamCommonProps("Text", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("name", mText.get());
}

void LogMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                     const TimeStamp& aProcessStartTime,
                                     UniqueStacks& aUniqueStacks) {
  StreamCommonProps("Log", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("name", mText.get());
  aWriter.StringProperty("module", mModule.get());
}

void DOMEventMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                          const TimeStamp& aProcessStartTime,
                                          UniqueStacks& aUniqueStacks) {
  TracingMarkerPayload::StreamPayload(aWriter, aProcessStartTime,
                                      aUniqueStacks);

  WriteTime(aWriter, aProcessStartTime, mTimeStamp, "timeStamp");
  aWriter.StringProperty("eventType", NS_ConvertUTF16toUTF8(mEventType).get());
}

void LayerTranslationMarkerPayload::StreamPayload(
    SpliceableJSONWriter& aWriter, const TimeStamp& aProcessStartTime,
    UniqueStacks& aUniqueStacks) {
  StreamType("LayerTranslation", aWriter);
  const size_t bufferSize = 32;
  char buffer[bufferSize];
  SprintfLiteral(buffer, "%p", mLayer);

  aWriter.StringProperty("layer", buffer);
  aWriter.IntProperty("x", mPoint.x);
  aWriter.IntProperty("y", mPoint.y);
}

void VsyncMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                       const TimeStamp& aProcessStartTime,
                                       UniqueStacks& aUniqueStacks) {
  StreamType("VsyncTimestamp", aWriter);
}

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

void NetworkMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                         const TimeStamp& aProcessStartTime,
                                         UniqueStacks& aUniqueStacks) {
  StreamCommonProps("Network", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.IntProperty("id", mID);
  const char* typeString = GetNetworkState(mType);
  const char* cacheString = GetCacheState(mCacheDisposition);
  // want to use aUniqueStacks.mUniqueStrings->WriteElement(aWriter,
  // typeString);
  aWriter.StringProperty("status", typeString);
  if (cacheString) {
    aWriter.StringProperty("cache", cacheString);
  }
  aWriter.IntProperty("pri", mPri);
  if (mCount > 0) {
    aWriter.IntProperty("count", mCount);
  }
  if (mURI) {
    aWriter.StringProperty("URI", mURI.get());
  }
  if (mRedirectURI) {
    aWriter.StringProperty("RedirectURI", mRedirectURI.get());
  }
  if (mType != NetworkLoadType::LOAD_START) {
    WriteTime(aWriter, aProcessStartTime, mTimings.domainLookupStart,
              "domainLookupStart");
    WriteTime(aWriter, aProcessStartTime, mTimings.domainLookupEnd,
              "domainLookupEnd");
    WriteTime(aWriter, aProcessStartTime, mTimings.connectStart,
              "connectStart");
    WriteTime(aWriter, aProcessStartTime, mTimings.tcpConnectEnd,
              "tcpConnectEnd");
    WriteTime(aWriter, aProcessStartTime, mTimings.secureConnectionStart,
              "secureConnectionStart");
    WriteTime(aWriter, aProcessStartTime, mTimings.connectEnd, "connectEnd");
    WriteTime(aWriter, aProcessStartTime, mTimings.requestStart,
              "requestStart");
    WriteTime(aWriter, aProcessStartTime, mTimings.responseStart,
              "responseStart");
    WriteTime(aWriter, aProcessStartTime, mTimings.responseEnd, "responseEnd");
  }
}

void ScreenshotPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                      const TimeStamp& aProcessStartTime,
                                      UniqueStacks& aUniqueStacks) {
  StreamType("CompositorScreenshot", aWriter);
  aUniqueStacks.mUniqueStrings->WriteProperty(aWriter, "url",
                                              mScreenshotDataURL.get());

  char hexWindowID[32];
  SprintfLiteral(hexWindowID, "0x%" PRIXPTR, mWindowIdentifier);
  aWriter.StringProperty("windowID", hexWindowID);
  aWriter.DoubleProperty("windowWidth", mWindowSize.width);
  aWriter.DoubleProperty("windowHeight", mWindowSize.height);
}

void GCSliceMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                         const TimeStamp& aProcessStartTime,
                                         UniqueStacks& aUniqueStacks) {
  MOZ_ASSERT(mTimingJSON);
  StreamCommonProps("GCSlice", aWriter, aProcessStartTime, aUniqueStacks);
  if (mTimingJSON) {
    aWriter.SplicedJSONProperty("timings", mTimingJSON.get());
  } else {
    aWriter.NullProperty("timings");
  }
}

void GCMajorMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                         const TimeStamp& aProcessStartTime,
                                         UniqueStacks& aUniqueStacks) {
  MOZ_ASSERT(mTimingJSON);
  StreamCommonProps("GCMajor", aWriter, aProcessStartTime, aUniqueStacks);
  if (mTimingJSON) {
    aWriter.SplicedJSONProperty("timings", mTimingJSON.get());
  } else {
    aWriter.NullProperty("timings");
  }
}

void GCMinorMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                         const TimeStamp& aProcessStartTime,
                                         UniqueStacks& aUniqueStacks) {
  MOZ_ASSERT(mTimingData);
  StreamCommonProps("GCMinor", aWriter, aProcessStartTime, aUniqueStacks);
  if (mTimingData) {
    aWriter.SplicedJSONProperty("nursery", mTimingData.get());
  } else {
    aWriter.NullProperty("nursery");
  }
}

void HangMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                      const TimeStamp& aProcessStartTime,
                                      UniqueStacks& aUniqueStacks) {
  StreamCommonProps("BHR-detected hang", aWriter, aProcessStartTime,
                    aUniqueStacks);
}

void StyleMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                       const TimeStamp& aProcessStartTime,
                                       UniqueStacks& aUniqueStacks) {
  StreamCommonProps("Styles", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("category", "Paint");
  aWriter.IntProperty("elementsTraversed", mStats.mElementsTraversed);
  aWriter.IntProperty("elementsStyled", mStats.mElementsStyled);
  aWriter.IntProperty("elementsMatched", mStats.mElementsMatched);
  aWriter.IntProperty("stylesShared", mStats.mStylesShared);
  aWriter.IntProperty("stylesReused", mStats.mStylesReused);
}

void LongTaskMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                          const TimeStamp& aProcessStartTime,
                                          UniqueStacks& aUniqueStacks) {
  StreamCommonProps("MainThreadLongTask", aWriter, aProcessStartTime,
                    aUniqueStacks);
  aWriter.StringProperty("category", "LongTask");
}

void JsAllocationMarkerPayload::StreamPayload(
    SpliceableJSONWriter& aWriter, const TimeStamp& aProcessStartTime,
    UniqueStacks& aUniqueStacks) {
  StreamCommonProps("JS allocation", aWriter, aProcessStartTime, aUniqueStacks);

  if (mClassName) {
    aWriter.StringProperty("className", mClassName.get());
  }
  if (mTypeName) {
    aWriter.StringProperty("typeName",
                           NS_ConvertUTF16toUTF8(mTypeName.get()).get());
  }
  if (mDescriptiveTypeName) {
    aWriter.StringProperty(
        "descriptiveTypeName",
        NS_ConvertUTF16toUTF8(mDescriptiveTypeName.get()).get());
  }
  aWriter.StringProperty("coarseType", mCoarseType);
  aWriter.IntProperty("size", mSize);
  aWriter.BoolProperty("inNursery", mInNursery);
}
