/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseProfiler.h"

#ifdef MOZ_BASE_PROFILER

#  include "BaseProfilerMarkerPayload.h"

#  include "ProfileBufferEntry.h"
#  include "BaseProfileJSONWriter.h"
#  include "ProfilerBacktrace.h"

#  include "mozilla/Maybe.h"
#  include "mozilla/Sprintf.h"

#  include <inttypes.h>

namespace mozilla {
namespace baseprofiler {

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
    aWriter.StringProperty("docShellId", mDocShellId->c_str());
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
  aWriter.StringProperty("name", mName.c_str());
  aWriter.StringProperty("entryType", mEntryType);

  if (mStartMark.isSome()) {
    aWriter.StringProperty("startMark", mStartMark.value().c_str());
  } else {
    aWriter.NullProperty("startMark");
  }
  if (mEndMark.isSome()) {
    aWriter.StringProperty("endMark", mEndMark.value().c_str());
  } else {
    aWriter.NullProperty("endMark");
  }
}

void TextMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                      const TimeStamp& aProcessStartTime,
                                      UniqueStacks& aUniqueStacks) {
  StreamCommonProps("Text", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("name", mText.c_str());
}

void LogMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                     const TimeStamp& aProcessStartTime,
                                     UniqueStacks& aUniqueStacks) {
  StreamCommonProps("Log", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("name", mText.c_str());
  aWriter.StringProperty("module", mModule.c_str());
}

void HangMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                      const TimeStamp& aProcessStartTime,
                                      UniqueStacks& aUniqueStacks) {
  StreamCommonProps("BHR-detected hang", aWriter, aProcessStartTime,
                    aUniqueStacks);
}

void LongTaskMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                          const TimeStamp& aProcessStartTime,
                                          UniqueStacks& aUniqueStacks) {
  StreamCommonProps("MainThreadLongTask", aWriter, aProcessStartTime,
                    aUniqueStacks);
  aWriter.StringProperty("category", "LongTask");
}

}  // namespace baseprofiler
}  // namespace mozilla

#endif  // MOZ_BASE_PROFILER
