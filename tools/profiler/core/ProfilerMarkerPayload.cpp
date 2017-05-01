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

ProfilerMarkerPayload::ProfilerMarkerPayload(UniqueProfilerBacktrace aStack)
  : mStack(mozilla::Move(aStack))
{}

ProfilerMarkerPayload::ProfilerMarkerPayload(const mozilla::TimeStamp& aStartTime,
                                             const mozilla::TimeStamp& aEndTime,
                                             UniqueProfilerBacktrace aStack)
  : mStartTime(aStartTime)
  , mEndTime(aEndTime)
  , mStack(mozilla::Move(aStack))
{}

ProfilerMarkerPayload::~ProfilerMarkerPayload()
{
}

void
ProfilerMarkerPayload::streamCommonProps(const char* aMarkerType,
                                         SpliceableJSONWriter& aWriter,
                                         const TimeStamp& aStartTime,
                                         UniqueStacks& aUniqueStacks)
{
  MOZ_ASSERT(aMarkerType);
  aWriter.StringProperty("type", aMarkerType);
  if (!mStartTime.IsNull()) {
    aWriter.DoubleProperty("startTime",
                           (mStartTime - aStartTime).ToMilliseconds());
  }
  if (!mEndTime.IsNull()) {
    aWriter.DoubleProperty("endTime", (mEndTime - aStartTime).ToMilliseconds());
  }
  if (mStack) {
    aWriter.StartObjectProperty("stack");
    {
      mStack->StreamJSON(aWriter, aStartTime, aUniqueStacks);
    }
    aWriter.EndObject();
  }
}

ProfilerMarkerTracing::ProfilerMarkerTracing(const char* aCategory,
                                             TracingKind aKind)
  : mCategory(aCategory)
  , mKind(aKind)
{
}

ProfilerMarkerTracing::ProfilerMarkerTracing(const char* aCategory,
                                             TracingKind aKind,
                                             UniqueProfilerBacktrace aCause)
  : mCategory(aCategory)
  , mKind(aKind)
{
  if (aCause) {
    SetStack(mozilla::Move(aCause));
  }
}

void
ProfilerMarkerTracing::StreamPayload(SpliceableJSONWriter& aWriter,
                                     const TimeStamp& aStartTime,
                                     UniqueStacks& aUniqueStacks)
{
  streamCommonProps("tracing", aWriter, aStartTime, aUniqueStacks);

  if (GetCategory()) {
    aWriter.StringProperty("category", GetCategory());
  }

  if (GetKind() == TRACING_INTERVAL_START) {
    aWriter.StringProperty("interval", "start");
  } else if (GetKind() == TRACING_INTERVAL_END) {
    aWriter.StringProperty("interval", "end");
  }
}

GPUMarkerPayload::GPUMarkerPayload(
  const mozilla::TimeStamp& aCpuTimeStart,
  const mozilla::TimeStamp& aCpuTimeEnd,
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
                                const TimeStamp& aStartTime,
                                UniqueStacks& aUniqueStacks)
{
  streamCommonProps("gpu_timer_query", aWriter, aStartTime, aUniqueStacks);

  aWriter.DoubleProperty("cpustart",
                         (mCpuTimeStart - aStartTime).ToMilliseconds());
  aWriter.DoubleProperty("cpuend",
                         (mCpuTimeEnd - aStartTime).ToMilliseconds());
  aWriter.IntProperty("gpustart", (int)mGpuTimeStart);
  aWriter.IntProperty("gpuend", (int)mGpuTimeEnd);
}

ProfilerMarkerImagePayload::ProfilerMarkerImagePayload(gfxASurface *aImg)
  : mImg(aImg)
{ }

void
ProfilerMarkerImagePayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                          const TimeStamp& aStartTime,
                                          UniqueStacks& aUniqueStacks)
{
  streamCommonProps("innerHTML", aWriter, aStartTime, aUniqueStacks);
  // TODO: Finish me
  //aWriter.NameValue("innerHTML", "<img src=''/>");
}

IOMarkerPayload::IOMarkerPayload(const char* aSource,
                                 const char* aFilename,
                                 const mozilla::TimeStamp& aStartTime,
                                 const mozilla::TimeStamp& aEndTime,
                                 UniqueProfilerBacktrace aStack)
  : ProfilerMarkerPayload(aStartTime, aEndTime,
                          mozilla::Move(aStack)),
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
                               const TimeStamp& aStartTime,
                               UniqueStacks& aUniqueStacks)
{
  streamCommonProps("io", aWriter, aStartTime, aUniqueStacks);
  aWriter.StringProperty("source", mSource);
  if (mFilename != nullptr) {
    aWriter.StringProperty("filename", mFilename);
  }
}

UserTimingMarkerPayload::UserTimingMarkerPayload(const nsAString& aName,
                                                 const mozilla::TimeStamp& aStartTime)
  : ProfilerMarkerPayload(aStartTime, aStartTime, nullptr)
  , mEntryType("mark")
  , mName(aName)
{
}

UserTimingMarkerPayload::UserTimingMarkerPayload(const nsAString& aName,
                                                 const mozilla::TimeStamp& aStartTime,
                                                 const mozilla::TimeStamp& aEndTime)
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
                                       const TimeStamp& aStartTime,
                                       UniqueStacks& aUniqueStacks)
{
  streamCommonProps("UserTiming", aWriter, aStartTime, aUniqueStacks);
  aWriter.StringProperty("name", NS_ConvertUTF16toUTF8(mName).get());
  aWriter.StringProperty("entryType", mEntryType);
}

DOMEventMarkerPayload::DOMEventMarkerPayload(const nsAString& aType, uint16_t aPhase,
                                             const mozilla::TimeStamp& aStartTime,
                                             const mozilla::TimeStamp& aEndTime)
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
                                     const TimeStamp& aStartTime,
                                     UniqueStacks& aUniqueStacks)
{
  streamCommonProps("DOMEvent", aWriter, aStartTime, aUniqueStacks);
  aWriter.StringProperty("type", NS_ConvertUTF16toUTF8(mType).get());
  aWriter.IntProperty("phase", mPhase);
}

void
ProfilerJSEventMarker(const char *event)
{
    PROFILER_MARKER(event);
}

LayerTranslationPayload::LayerTranslationPayload(mozilla::layers::Layer* aLayer,
                                                 mozilla::gfx::Point aPoint)
  : ProfilerMarkerPayload(mozilla::TimeStamp::Now(), mozilla::TimeStamp::Now(), nullptr)
  , mLayer(aLayer)
  , mPoint(aPoint)
{
}

void
LayerTranslationPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                       const TimeStamp& aStartTime,
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

TouchDataPayload::TouchDataPayload(const mozilla::ScreenIntPoint& aPoint)
  : ProfilerMarkerPayload(mozilla::TimeStamp::Now(), mozilla::TimeStamp::Now(), nullptr)
{
  mPoint = aPoint;
}

void
TouchDataPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                const TimeStamp& aStartTime,
                                UniqueStacks& aUniqueStacks)
{
  aWriter.IntProperty("x", mPoint.x);
  aWriter.IntProperty("y", mPoint.y);
}

VsyncPayload::VsyncPayload(mozilla::TimeStamp aVsyncTimestamp)
  : ProfilerMarkerPayload(aVsyncTimestamp, aVsyncTimestamp, nullptr)
  , mVsyncTimestamp(aVsyncTimestamp)
{
}

void
VsyncPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                            const TimeStamp& aStartTime,
                            UniqueStacks& aUniqueStacks)
{
  aWriter.DoubleProperty("vsync",
                         (mVsyncTimestamp - aStartTime).ToMilliseconds());
  aWriter.StringProperty("category", "VsyncTimestamp");
}
