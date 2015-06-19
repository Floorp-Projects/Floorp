/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoProfiler.h"
#include "ProfilerBacktrace.h"
#include "ProfilerMarkers.h"
#include "SyncProfile.h"
#ifndef SPS_STANDALONE
#include "gfxASurface.h"
#include "Layers.h"
#include "prprf.h"
#endif

ProfilerMarkerPayload::ProfilerMarkerPayload(ProfilerBacktrace* aStack)
  : mStack(aStack)
{}

ProfilerMarkerPayload::ProfilerMarkerPayload(const mozilla::TimeStamp& aStartTime,
                                             const mozilla::TimeStamp& aEndTime,
                                             ProfilerBacktrace* aStack)
  : mStartTime(aStartTime)
  , mEndTime(aEndTime)
  , mStack(aStack)
{}

ProfilerMarkerPayload::~ProfilerMarkerPayload()
{
  profiler_free_backtrace(mStack);
}

void
ProfilerMarkerPayload::streamCommonProps(const char* aMarkerType,
                                         SpliceableJSONWriter& aWriter,
                                         UniqueStacks& aUniqueStacks)
{
  MOZ_ASSERT(aMarkerType);
  aWriter.StringProperty("type", aMarkerType);
  if (!mStartTime.IsNull()) {
    aWriter.DoubleProperty("startTime", profiler_time(mStartTime));
  }
  if (!mEndTime.IsNull()) {
    aWriter.DoubleProperty("endTime", profiler_time(mEndTime));
  }
  if (mStack) {
    aWriter.StartObjectProperty("stack");
    {
      mStack->StreamJSON(aWriter, aUniqueStacks);
    }
    aWriter.EndObject();
  }
}

ProfilerMarkerTracing::ProfilerMarkerTracing(const char* aCategory, TracingMetadata aMetaData)
  : mCategory(aCategory)
  , mMetaData(aMetaData)
{
  if (aMetaData == TRACING_EVENT_BACKTRACE) {
    SetStack(profiler_get_backtrace());
  }
}

ProfilerMarkerTracing::ProfilerMarkerTracing(const char* aCategory, TracingMetadata aMetaData,
                                             ProfilerBacktrace* aCause)
  : mCategory(aCategory)
  , mMetaData(aMetaData)
{
  if (aCause) {
    SetStack(aCause);
  }
}

void
ProfilerMarkerTracing::StreamPayload(SpliceableJSONWriter& aWriter,
                                     UniqueStacks& aUniqueStacks)
{
  streamCommonProps("tracing", aWriter, aUniqueStacks);

  if (GetCategory()) {
    aWriter.StringProperty("category", GetCategory());
  }
  if (GetMetaData() != TRACING_DEFAULT) {
    if (GetMetaData() == TRACING_INTERVAL_START) {
      aWriter.StringProperty("interval", "start");
    } else if (GetMetaData() == TRACING_INTERVAL_END) {
      aWriter.StringProperty("interval", "end");
    }
  }
}

#ifndef SPS_STANDALONE
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
                                UniqueStacks& aUniqueStacks)
{
  streamCommonProps("gpu_timer_query", aWriter, aUniqueStacks);

  aWriter.DoubleProperty("cpustart", profiler_time(mCpuTimeStart));
  aWriter.DoubleProperty("cpuend", profiler_time(mCpuTimeEnd));
  aWriter.IntProperty("gpustart", (int)mGpuTimeStart);
  aWriter.IntProperty("gpuend", (int)mGpuTimeEnd);
}

ProfilerMarkerImagePayload::ProfilerMarkerImagePayload(gfxASurface *aImg)
  : mImg(aImg)
{ }

void
ProfilerMarkerImagePayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                          UniqueStacks& aUniqueStacks)
{
  streamCommonProps("innerHTML", aWriter, aUniqueStacks);
  // TODO: Finish me
  //aWriter.NameValue("innerHTML", "<img src=''/>");
}

IOMarkerPayload::IOMarkerPayload(const char* aSource,
                                 const char* aFilename,
                                 const mozilla::TimeStamp& aStartTime,
                                 const mozilla::TimeStamp& aEndTime,
                                 ProfilerBacktrace* aStack)
  : ProfilerMarkerPayload(aStartTime, aEndTime, aStack),
    mSource(aSource)
{
  mFilename = aFilename ? strdup(aFilename) : nullptr;
  MOZ_ASSERT(aSource);
}

IOMarkerPayload::~IOMarkerPayload(){
  free(mFilename);
}

void
IOMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter, UniqueStacks& aUniqueStacks)
{
  streamCommonProps("io", aWriter, aUniqueStacks);
  aWriter.StringProperty("source", mSource);
  if (mFilename != nullptr) {
    aWriter.StringProperty("filename", mFilename);
  }
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
                                       UniqueStacks& aUniqueStacks)
{
  const size_t bufferSize = 32;
  char buffer[bufferSize];
  PR_snprintf(buffer, bufferSize, "%p", mLayer);

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
TouchDataPayload::StreamPayload(SpliceableJSONWriter& aWriter, UniqueStacks& aUniqueStacks)
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
VsyncPayload::StreamPayload(SpliceableJSONWriter& aWriter, UniqueStacks& aUniqueStacks)
{
  aWriter.DoubleProperty("vsync", profiler_time(mVsyncTimestamp));
  aWriter.StringProperty("category", "VsyncTimestamp");
}
#endif
