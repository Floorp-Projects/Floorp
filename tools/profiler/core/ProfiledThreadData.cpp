/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfiledThreadData.h"

#if defined(GP_OS_darwin)
#include <pthread.h>
#endif

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h> // for getpid()
#endif

ProfiledThreadData::ProfiledThreadData(ThreadInfo* aThreadInfo,
                                       nsIEventTarget* aEventTarget,
                                       bool aIncludeResponsiveness)
  : mThreadInfo(aThreadInfo)
{
  MOZ_COUNT_CTOR(ProfiledThreadData);
  if (aIncludeResponsiveness) {
    mResponsiveness.emplace(aEventTarget, aThreadInfo->IsMainThread());
  }
}

ProfiledThreadData::~ProfiledThreadData()
{
  MOZ_COUNT_DTOR(ProfiledThreadData);
}

void
ProfiledThreadData::StreamJSON(const ProfileBuffer& aBuffer, JSContext* aCx,
                               SpliceableJSONWriter& aWriter,
                               const TimeStamp& aProcessStartTime, double aSinceTime)
{
  if (mJITFrameInfoForPreviousJSContexts &&
      mJITFrameInfoForPreviousJSContexts->HasExpired(aBuffer.mRangeStart)) {
    mJITFrameInfoForPreviousJSContexts = nullptr;
  }

  // If we have an existing JITFrameInfo in mJITFrameInfoForPreviousJSContexts,
  // copy the data from it.
  JITFrameInfo jitFrameInfo = mJITFrameInfoForPreviousJSContexts
    ? JITFrameInfo(*mJITFrameInfoForPreviousJSContexts) : JITFrameInfo();

  if (aCx && mBufferPositionWhenReceivedJSContext) {
    aBuffer.AddJITInfoForRange(*mBufferPositionWhenReceivedJSContext,
      mThreadInfo->ThreadId(), aCx, jitFrameInfo);
  }

  UniqueStacks uniqueStacks(std::move(jitFrameInfo));

  aWriter.Start();
  {
    StreamSamplesAndMarkers(mThreadInfo->Name(), mThreadInfo->ThreadId(),
                            aBuffer, aWriter,
                            aProcessStartTime,
                            mThreadInfo->RegisterTime(), mUnregisterTime,
                            aSinceTime, uniqueStacks);

    aWriter.StartObjectProperty("stackTable");
    {
      {
        JSONSchemaWriter schema(aWriter);
        schema.WriteField("prefix");
        schema.WriteField("frame");
      }

      aWriter.StartArrayProperty("data");
      {
        uniqueStacks.SpliceStackTableElements(aWriter);
      }
      aWriter.EndArray();
    }
    aWriter.EndObject();

    aWriter.StartObjectProperty("frameTable");
    {
      {
        JSONSchemaWriter schema(aWriter);
        schema.WriteField("location");
        schema.WriteField("implementation");
        schema.WriteField("optimizations");
        schema.WriteField("line");
        schema.WriteField("category");
      }

      aWriter.StartArrayProperty("data");
      {
        uniqueStacks.SpliceFrameTableElements(aWriter);
      }
      aWriter.EndArray();
    }
    aWriter.EndObject();

    aWriter.StartArrayProperty("stringTable");
    {
      uniqueStacks.mUniqueStrings->SpliceStringTableElements(aWriter);
    }
    aWriter.EndArray();
  }

  aWriter.End();
}

void
StreamSamplesAndMarkers(const char* aName,
                        int aThreadId,
                        const ProfileBuffer& aBuffer,
                        SpliceableJSONWriter& aWriter,
                        const TimeStamp& aProcessStartTime,
                        const TimeStamp& aRegisterTime,
                        const TimeStamp& aUnregisterTime,
                        double aSinceTime,
                        UniqueStacks& aUniqueStacks)
{
  aWriter.StringProperty("processType",
                         XRE_ChildProcessTypeToString(XRE_GetProcessType()));

  aWriter.StringProperty("name", aName);
  aWriter.IntProperty("tid", static_cast<int64_t>(aThreadId));
  aWriter.IntProperty("pid", static_cast<int64_t>(getpid()));

  if (aRegisterTime) {
    aWriter.DoubleProperty("registerTime",
      (aRegisterTime - aProcessStartTime).ToMilliseconds());
  } else {
    aWriter.NullProperty("registerTime");
  }

  if (aUnregisterTime) {
    aWriter.DoubleProperty("unregisterTime",
      (aUnregisterTime - aProcessStartTime).ToMilliseconds());
  } else {
    aWriter.NullProperty("unregisterTime");
  }

  aWriter.StartObjectProperty("samples");
  {
    {
      JSONSchemaWriter schema(aWriter);
      schema.WriteField("stack");
      schema.WriteField("time");
      schema.WriteField("responsiveness");
      schema.WriteField("rss");
      schema.WriteField("uss");
    }

    aWriter.StartArrayProperty("data");
    {
      aBuffer.StreamSamplesToJSON(aWriter, aThreadId, aSinceTime,
                                  aUniqueStacks);
    }
    aWriter.EndArray();
  }
  aWriter.EndObject();

  aWriter.StartObjectProperty("markers");
  {
    {
      JSONSchemaWriter schema(aWriter);
      schema.WriteField("name");
      schema.WriteField("time");
      schema.WriteField("data");
    }

    aWriter.StartArrayProperty("data");
    {
      aBuffer.StreamMarkersToJSON(aWriter, aThreadId, aProcessStartTime,
                                  aSinceTime, aUniqueStacks);
    }
    aWriter.EndArray();
  }
  aWriter.EndObject();
}

void
ProfiledThreadData::NotifyAboutToLoseJSContext(JSContext* aContext,
                                               const TimeStamp& aProcessStartTime,
                                               ProfileBuffer& aBuffer)
{
  if (!mBufferPositionWhenReceivedJSContext) {
    return;
  }

  MOZ_RELEASE_ASSERT(aContext);

  if (mJITFrameInfoForPreviousJSContexts &&
      mJITFrameInfoForPreviousJSContexts->HasExpired(aBuffer.mRangeStart)) {
    mJITFrameInfoForPreviousJSContexts = nullptr;
  }

  UniquePtr<JITFrameInfo> jitFrameInfo = mJITFrameInfoForPreviousJSContexts
    ? std::move(mJITFrameInfoForPreviousJSContexts) : MakeUnique<JITFrameInfo>();

  aBuffer.AddJITInfoForRange(*mBufferPositionWhenReceivedJSContext,
     mThreadInfo->ThreadId(), aContext, *jitFrameInfo);

  mJITFrameInfoForPreviousJSContexts = std::move(jitFrameInfo);
  mBufferPositionWhenReceivedJSContext = Nothing();
}
