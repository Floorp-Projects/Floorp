/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoProfiler.h"
#include "ProfilerBacktrace.h"
#include "ProfilerMarkers.h"
#include "gfxASurface.h"
#include "SyncProfile.h"
#include "Layers.h"
#include "prprf.h"

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
                                          JSStreamWriter& b)
{
  MOZ_ASSERT(aMarkerType);
  b.NameValue("type", aMarkerType);
  if (!mStartTime.IsNull()) {
    b.NameValue("startTime", profiler_time(mStartTime));
  }
  if (!mEndTime.IsNull()) {
    b.NameValue("endTime", profiler_time(mEndTime));
  }
  if (mStack) {
    b.Name("stack");
    mStack->StreamJSObject(b);
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
ProfilerMarkerTracing::streamPayloadImp(JSStreamWriter& b)
{
  b.BeginObject();
    streamCommonProps("tracing", b);

    if (GetCategory()) {
      b.NameValue("category", GetCategory());
    }
    if (GetMetaData() != TRACING_DEFAULT) {
      if (GetMetaData() == TRACING_INTERVAL_START) {
        b.NameValue("interval", "start");
      } else if (GetMetaData() == TRACING_INTERVAL_END) {
        b.NameValue("interval", "end");
      }
    }
  b.EndObject();
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
{

}

void
GPUMarkerPayload::streamPayloadImp(JSStreamWriter& b)
{
  b.BeginObject();
    streamCommonProps("gpu_timer_query", b);

    b.NameValue("cpustart", profiler_time(mCpuTimeStart));
    b.NameValue("cpuend", profiler_time(mCpuTimeEnd));
    b.NameValue("gpustart", (int)mGpuTimeStart);
    b.NameValue("gpuend", (int)mGpuTimeEnd);
  b.EndObject();
}

ProfilerMarkerImagePayload::ProfilerMarkerImagePayload(gfxASurface *aImg)
  : mImg(aImg)
{}

void
ProfilerMarkerImagePayload::streamPayloadImp(JSStreamWriter& b)
{
  b.BeginObject();
    streamCommonProps("innerHTML", b);
    // TODO: Finish me
    //b.NameValue("innerHTML", "<img src=''/>");
  b.EndObject();
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
IOMarkerPayload::streamPayloadImp(JSStreamWriter& b)
{
  b.BeginObject();
    streamCommonProps("io", b);
    b.NameValue("source", mSource);
    if (mFilename != nullptr) {
      b.NameValue("filename", mFilename);
    }
  b.EndObject();
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
LayerTranslationPayload::streamPayloadImpl(JSStreamWriter& b)
{
  const size_t bufferSize = 32;
  char buffer[bufferSize];
  PR_snprintf(buffer, bufferSize, "%p", mLayer);

  b.BeginObject();
  b.NameValue("layer", buffer);
  b.NameValue("x", mPoint.x);
  b.NameValue("y", mPoint.y);
  b.NameValue("category", "LayerTranslation");
  b.EndObject();
}

TouchDataPayload::TouchDataPayload(const mozilla::ScreenIntPoint& aPoint)
  : ProfilerMarkerPayload(mozilla::TimeStamp::Now(), mozilla::TimeStamp::Now(), nullptr)
{
  mPoint = aPoint;
}

void
TouchDataPayload::streamPayloadImpl(JSStreamWriter& b)
{
  b.BeginObject();
  b.NameValue("x", mPoint.x);
  b.NameValue("y", mPoint.y);
  b.EndObject();
}

VsyncPayload::VsyncPayload(mozilla::TimeStamp aVsyncTimestamp)
  : ProfilerMarkerPayload(aVsyncTimestamp, aVsyncTimestamp, nullptr)
  , mVsyncTimestamp(aVsyncTimestamp)
{
}

void
VsyncPayload::streamPayloadImpl(JSStreamWriter& b)
{
  b.BeginObject();
  b.NameValue("vsync", profiler_time(mVsyncTimestamp));
  b.NameValue("category", "VsyncTimestamp");
  b.EndObject();
}
