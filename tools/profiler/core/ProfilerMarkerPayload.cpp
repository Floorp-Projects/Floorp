/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoProfiler.h"
#include "ProfilerBacktrace.h"
#include "ProfilerMarkerPayload.h"
#include "gfxASurface.h"
#include "Layers.h"
#include "mozilla/Sprintf.h"

using namespace mozilla;

ProfilerMarkerPayload::ProfilerMarkerPayload(UniqueProfilerBacktrace aStack)
  : mStack(Move(aStack))
{}

ProfilerMarkerPayload::ProfilerMarkerPayload(const TimeStamp& aStartTime,
                                             const TimeStamp& aEndTime,
                                             UniqueProfilerBacktrace aStack)
  : mStartTime(aStartTime)
  , mEndTime(aEndTime)
  , mStack(Move(aStack))
{}

ProfilerMarkerPayload::~ProfilerMarkerPayload()
{
}

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

TracingMarkerPayload::TracingMarkerPayload(const char* aCategory,
                                           TracingKind aKind)
  : mCategory(aCategory)
  , mKind(aKind)
{
}

TracingMarkerPayload::TracingMarkerPayload(const char* aCategory,
                                           TracingKind aKind,
                                           UniqueProfilerBacktrace aCause)
  : mCategory(aCategory)
  , mKind(aKind)
{
  if (aCause) {
    SetStack(Move(aCause));
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

GPUMarkerPayload::GPUMarkerPayload(
  const TimeStamp& aCpuTimeStart,
  const TimeStamp& aCpuTimeEnd,
  uint64_t aGpuTimeStart,
  uint64_t aGpuTimeEnd)

  : ProfilerMarkerPayload(aCpuTimeStart, aCpuTimeEnd)
  , mCpuTimeStart(aCpuTimeStart)
  , mCpuTimeEnd(aCpuTimeEnd)
  , mGpuTimeStart(aGpuTimeStart)
  , mGpuTimeEnd(aGpuTimeEnd)
{ }

void
GPUMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                const TimeStamp& aProcessStartTime,
                                UniqueStacks& aUniqueStacks)
{
  StreamCommonProps("gpu_timer_query", aWriter, aProcessStartTime,
                    aUniqueStacks);

  aWriter.DoubleProperty("cpustart",
                         (mCpuTimeStart - aProcessStartTime).ToMilliseconds());
  aWriter.DoubleProperty("cpuend",
                         (mCpuTimeEnd - aProcessStartTime).ToMilliseconds());
  aWriter.IntProperty("gpustart", (int)mGpuTimeStart);
  aWriter.IntProperty("gpuend", (int)mGpuTimeEnd);
}

IOMarkerPayload::IOMarkerPayload(const char* aSource,
                                 const char* aFilename,
                                 const TimeStamp& aStartTime,
                                 const TimeStamp& aEndTime,
                                 UniqueProfilerBacktrace aStack)
  : ProfilerMarkerPayload(aStartTime, aEndTime, Move(aStack)),
    mSource(aSource)
{
  mFilename = aFilename ? strdup(aFilename) : nullptr;
  MOZ_ASSERT(aSource);
}

IOMarkerPayload::~IOMarkerPayload(){
  free(mFilename);
}

void
IOMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                               const TimeStamp& aProcessStartTime,
                               UniqueStacks& aUniqueStacks)
{
  StreamCommonProps("io", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("source", mSource);
  if (mFilename != nullptr) {
    aWriter.StringProperty("filename", mFilename);
  }
}

UserTimingMarkerPayload::UserTimingMarkerPayload(const nsAString& aName,
                                                 const TimeStamp& aStartTime)
  : ProfilerMarkerPayload(aStartTime, aStartTime, nullptr)
  , mEntryType("mark")
  , mName(aName)
{
}

UserTimingMarkerPayload::UserTimingMarkerPayload(const nsAString& aName,
                                                 const TimeStamp& aStartTime,
                                                 const TimeStamp& aEndTime)
  : ProfilerMarkerPayload(aStartTime, aEndTime, nullptr)
  , mEntryType("measure")
  , mName(aName)
{
}

UserTimingMarkerPayload::~UserTimingMarkerPayload()
{
}

void
UserTimingMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                       const TimeStamp& aProcessStartTime,
                                       UniqueStacks& aUniqueStacks)
{
  StreamCommonProps("UserTiming", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("name", NS_ConvertUTF16toUTF8(mName).get());
  aWriter.StringProperty("entryType", mEntryType);
}

DOMEventMarkerPayload::DOMEventMarkerPayload(const nsAString& aType, uint16_t aPhase,
                                             const TimeStamp& aStartTime,
                                             const TimeStamp& aEndTime)
  : ProfilerMarkerPayload(aStartTime, aEndTime, nullptr)
  , mType(aType)
  , mPhase(aPhase)
{
}

DOMEventMarkerPayload::~DOMEventMarkerPayload()
{
}

void
DOMEventMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                     const TimeStamp& aProcessStartTime,
                                     UniqueStacks& aUniqueStacks)
{
  StreamCommonProps("DOMEvent", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("type", NS_ConvertUTF16toUTF8(mType).get());
  aWriter.IntProperty("phase", mPhase);
}

LayerTranslationMarkerPayload::LayerTranslationMarkerPayload(layers::Layer* aLayer,
                                                             gfx::Point aPoint)
  : ProfilerMarkerPayload(TimeStamp::Now(), TimeStamp::Now(), nullptr)
  , mLayer(aLayer)
  , mPoint(aPoint)
{
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

VsyncMarkerPayload::VsyncMarkerPayload(TimeStamp aVsyncTimestamp)
  : ProfilerMarkerPayload(aVsyncTimestamp, aVsyncTimestamp, nullptr)
  , mVsyncTimestamp(aVsyncTimestamp)
{
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
    aWriter.SplicedJSONProperty("nurseryTimings", mTimingData.get());
  } else {
    aWriter.NullProperty("nurseryTimings");
  }
}
