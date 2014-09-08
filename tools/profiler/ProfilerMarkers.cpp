/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoProfiler.h"
#include "ProfilerBacktrace.h"
#include "ProfilerMarkers.h"
#include "gfxASurface.h"
#include "SyncProfile.h"

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
