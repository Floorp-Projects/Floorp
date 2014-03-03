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

template<typename Builder> void
ProfilerMarkerPayload::prepareCommonProps(const char* aMarkerType,
                                          Builder& aBuilder,
                                          typename Builder::ObjectHandle aObject)
{
  MOZ_ASSERT(aMarkerType);
  aBuilder.DefineProperty(aObject, "type", aMarkerType);
  if (!mStartTime.IsNull()) {
    aBuilder.DefineProperty(aObject, "startTime", profiler_time(mStartTime));
  }
  if (!mEndTime.IsNull()) {
    aBuilder.DefineProperty(aObject, "endTime", profiler_time(mEndTime));
  }
  if (mStack) {
    typename Builder::RootedObject stack(aBuilder.context(),
                                         aBuilder.CreateObject());
    aBuilder.DefineProperty(aObject, "stack", stack);
    mStack->BuildJSObject(aBuilder, stack);
  }
}

template void
ProfilerMarkerPayload::prepareCommonProps<JSCustomObjectBuilder>(
                                   const char* aMarkerType,
                                   JSCustomObjectBuilder& b,
                                   JSCustomObjectBuilder::ObjectHandle aObject);
template void
ProfilerMarkerPayload::prepareCommonProps<JSObjectBuilder>(
                                         const char* aMarkerType,
                                         JSObjectBuilder& b,
                                         JSObjectBuilder::ObjectHandle aObject);

ProfilerMarkerTracing::ProfilerMarkerTracing(const char* aCategory, TracingMetadata aMetaData)
  : mCategory(aCategory)
  , mMetaData(aMetaData)
{}

template<typename Builder>
typename Builder::Object
ProfilerMarkerTracing::preparePayloadImp(Builder& b)
{
  typename Builder::RootedObject data(b.context(), b.CreateObject());
  prepareCommonProps("tracing", b, data);

  if (GetCategory()) {
    b.DefineProperty(data, "category", GetCategory());
  }
  if (GetMetaData() != TRACING_DEFAULT) {
    if (GetMetaData() == TRACING_INTERVAL_START) {
      b.DefineProperty(data, "interval", "start");
    } else if (GetMetaData() == TRACING_INTERVAL_END) {
      b.DefineProperty(data, "interval", "end");
    }
  }

  return data;
}

template JSCustomObjectBuilder::Object
ProfilerMarkerTracing::preparePayloadImp<JSCustomObjectBuilder>(JSCustomObjectBuilder& b);
template JSObjectBuilder::Object
ProfilerMarkerTracing::preparePayloadImp<JSObjectBuilder>(JSObjectBuilder& b);

ProfilerMarkerImagePayload::ProfilerMarkerImagePayload(gfxASurface *aImg)
  : mImg(aImg)
{}

template<typename Builder>
typename Builder::Object
ProfilerMarkerImagePayload::preparePayloadImp(Builder& b)
{
  typename Builder::RootedObject data(b.context(), b.CreateObject());
  prepareCommonProps("innerHTML", b, data);
  // TODO: Finish me
  //b.DefineProperty(data, "innerHTML", "<img src=''/>");
  return data;
}

template JSCustomObjectBuilder::Object
ProfilerMarkerImagePayload::preparePayloadImp<JSCustomObjectBuilder>(JSCustomObjectBuilder& b);
template JSObjectBuilder::Object
ProfilerMarkerImagePayload::preparePayloadImp<JSObjectBuilder>(JSObjectBuilder& b);

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

template<typename Builder> typename Builder::Object
IOMarkerPayload::preparePayloadImp(Builder& b)
{
  typename Builder::RootedObject data(b.context(), b.CreateObject());
  prepareCommonProps("io", b, data);
  b.DefineProperty(data, "source", mSource);
  if (mFilename != nullptr) {
    b.DefineProperty(data, "filename", mFilename);
  }

  return data;
}

template JSCustomObjectBuilder::Object
IOMarkerPayload::preparePayloadImp<JSCustomObjectBuilder>(JSCustomObjectBuilder& b);
template JSObjectBuilder::Object
IOMarkerPayload::preparePayloadImp<JSObjectBuilder>(JSObjectBuilder& b);

void
ProfilerJSEventMarker(const char *event)
{
    PROFILER_MARKER(event);
}
