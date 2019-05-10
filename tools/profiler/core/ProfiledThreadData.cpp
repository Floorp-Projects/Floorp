/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfiledThreadData.h"

#include "ProfileBuffer.h"
#include "ProfileJSONWriter.h"

#include "js/TraceLoggerAPI.h"
#include "mozilla/dom/ContentChild.h"

#if defined(GP_OS_darwin)
#  include <pthread.h>
#endif

ProfiledThreadData::ProfiledThreadData(ThreadInfo* aThreadInfo,
                                       nsIEventTarget* aEventTarget,
                                       bool aIncludeResponsiveness)
    : mThreadInfo(aThreadInfo) {
  MOZ_COUNT_CTOR(ProfiledThreadData);
  if (aIncludeResponsiveness) {
    mResponsiveness.emplace(aEventTarget, aThreadInfo->IsMainThread());
  }
}

ProfiledThreadData::~ProfiledThreadData() {
  MOZ_COUNT_DTOR(ProfiledThreadData);
}

void ProfiledThreadData::StreamJSON(const ProfileBuffer& aBuffer,
                                    JSContext* aCx,
                                    SpliceableJSONWriter& aWriter,
                                    const nsACString& aProcessName,
                                    const mozilla::TimeStamp& aProcessStartTime,
                                    double aSinceTime, bool JSTracerEnabled) {
  if (mJITFrameInfoForPreviousJSContexts &&
      mJITFrameInfoForPreviousJSContexts->HasExpired(aBuffer.mRangeStart)) {
    mJITFrameInfoForPreviousJSContexts = nullptr;
  }

  // If we have an existing JITFrameInfo in mJITFrameInfoForPreviousJSContexts,
  // copy the data from it.
  JITFrameInfo jitFrameInfo =
      mJITFrameInfoForPreviousJSContexts
          ? JITFrameInfo(*mJITFrameInfoForPreviousJSContexts)
          : JITFrameInfo();

  if (aCx && mBufferPositionWhenReceivedJSContext) {
    aBuffer.AddJITInfoForRange(*mBufferPositionWhenReceivedJSContext,
                               mThreadInfo->ThreadId(), aCx, jitFrameInfo);
  }

  UniqueStacks uniqueStacks(std::move(jitFrameInfo));

  aWriter.Start();
  {
    StreamSamplesAndMarkers(mThreadInfo->Name(), mThreadInfo->ThreadId(),
                            aBuffer, aWriter, aProcessName, aProcessStartTime,
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
      { uniqueStacks.SpliceStackTableElements(aWriter); }
      aWriter.EndArray();
    }
    aWriter.EndObject();

    aWriter.StartObjectProperty("frameTable");
    {
      {
        JSONSchemaWriter schema(aWriter);
        schema.WriteField("location");
        schema.WriteField("relevantForJS");
        schema.WriteField("implementation");
        schema.WriteField("optimizations");
        schema.WriteField("line");
        schema.WriteField("column");
        schema.WriteField("category");
      }

      aWriter.StartArrayProperty("data");
      { uniqueStacks.SpliceFrameTableElements(aWriter); }
      aWriter.EndArray();
    }
    aWriter.EndObject();

    aWriter.StartArrayProperty("stringTable");
    { uniqueStacks.mUniqueStrings->SpliceStringTableElements(aWriter); }
    aWriter.EndArray();
  }

  if (aCx && JSTracerEnabled) {
    StreamTraceLoggerJSON(aCx, aWriter, aProcessStartTime);
  }

  aWriter.End();
}

void ProfiledThreadData::StreamTraceLoggerJSON(
    JSContext* aCx, SpliceableJSONWriter& aWriter,
    const mozilla::TimeStamp& aProcessStartTime) {
  aWriter.StartObjectProperty("jsTracerEvents");
  {
    JS::AutoTraceLoggerLockGuard lockGuard;
    uint32_t length = 0;

    // Collect Event Ids
    aWriter.StartArrayProperty("events", mozilla::JSONWriter::SingleLineStyle);
    {
      JS::TraceLoggerIdBuffer collectionBuffer(lockGuard, aCx);
      while (collectionBuffer.NextChunk()) {
        for (uint32_t val : collectionBuffer) {
          aWriter.IntElement(val);
          length++;
        }
      }
    }
    aWriter.EndArray();

    // Collect Event Timestamps
    aWriter.StartArrayProperty("timestamps",
                               mozilla::JSONWriter::SingleLineStyle);
    {
      JS::TraceLoggerTimeStampBuffer collectionBuffer(lockGuard, aCx);
      while (collectionBuffer.NextChunk()) {
        for (mozilla::TimeStamp val : collectionBuffer) {
          aWriter.DoubleElement((val - aProcessStartTime).ToMicroseconds());
        }
      }
    }
    aWriter.EndArray();

    // Collect Event Durations
    aWriter.StartArrayProperty("durations",
                               mozilla::JSONWriter::SingleLineStyle);
    {
      JS::TraceLoggerDurationBuffer collectionBuffer(lockGuard, aCx);
      while (collectionBuffer.NextChunk()) {
        for (double val : collectionBuffer) {
          if (val == -1) {
            aWriter.NullElement();
          } else {
            aWriter.DoubleElement(val);
          }
        }
      }
    }
    aWriter.EndArray();

    // Collect Event LineNo
    aWriter.StartArrayProperty("line", mozilla::JSONWriter::SingleLineStyle);
    {
      JS::TraceLoggerLineNoBuffer collectionBuffer(lockGuard, aCx);
      while (collectionBuffer.NextChunk()) {
        for (int32_t val : collectionBuffer) {
          if (val == -1) {
            aWriter.NullElement();
          } else {
            aWriter.IntElement(val);
          }
        }
      }
    }
    aWriter.EndArray();

    // Collect Event ColNo
    aWriter.StartArrayProperty("column", mozilla::JSONWriter::SingleLineStyle);
    {
      JS::TraceLoggerColNoBuffer collectionBuffer(lockGuard, aCx);
      while (collectionBuffer.NextChunk()) {
        for (int32_t val : collectionBuffer) {
          if (val == -1) {
            aWriter.NullElement();
          } else {
            aWriter.IntElement(val);
          }
        }
      }
    }
    aWriter.EndArray();

    aWriter.IntProperty("length", length);
  }
  aWriter.EndObject();
}

void StreamSamplesAndMarkers(const char* aName, int aThreadId,
                             const ProfileBuffer& aBuffer,
                             SpliceableJSONWriter& aWriter,
                             const nsACString& aProcessName,
                             const mozilla::TimeStamp& aProcessStartTime,
                             const mozilla::TimeStamp& aRegisterTime,
                             const mozilla::TimeStamp& aUnregisterTime,
                             double aSinceTime, UniqueStacks& aUniqueStacks) {
  aWriter.StringProperty("processType",
                         XRE_ChildProcessTypeToString(XRE_GetProcessType()));

  aWriter.StringProperty("name", aName);

  // Use given process name (if any), unless we're the parent process.
  if (XRE_IsParentProcess()) {
    aWriter.StringProperty("processName", "Parent Process");
  } else if (!aProcessName.IsEmpty()) {
    aWriter.StringProperty("processName", aProcessName.Data());
  }

  aWriter.IntProperty("tid", static_cast<int64_t>(aThreadId));
  aWriter.IntProperty("pid",
                      static_cast<int64_t>(profiler_current_process_id()));

  if (aRegisterTime) {
    aWriter.DoubleProperty(
        "registerTime", (aRegisterTime - aProcessStartTime).ToMilliseconds());
  } else {
    aWriter.NullProperty("registerTime");
  }

  if (aUnregisterTime) {
    aWriter.DoubleProperty(
        "unregisterTime",
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
      schema.WriteField("category");
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

void ProfiledThreadData::NotifyAboutToLoseJSContext(
    JSContext* aContext, const mozilla::TimeStamp& aProcessStartTime,
    ProfileBuffer& aBuffer) {
  if (!mBufferPositionWhenReceivedJSContext) {
    return;
  }

  MOZ_RELEASE_ASSERT(aContext);

  if (mJITFrameInfoForPreviousJSContexts &&
      mJITFrameInfoForPreviousJSContexts->HasExpired(aBuffer.mRangeStart)) {
    mJITFrameInfoForPreviousJSContexts = nullptr;
  }

  mozilla::UniquePtr<JITFrameInfo> jitFrameInfo =
      mJITFrameInfoForPreviousJSContexts
          ? std::move(mJITFrameInfoForPreviousJSContexts)
          : mozilla::MakeUnique<JITFrameInfo>();

  aBuffer.AddJITInfoForRange(*mBufferPositionWhenReceivedJSContext,
                             mThreadInfo->ThreadId(), aContext, *jitFrameInfo);

  mJITFrameInfoForPreviousJSContexts = std::move(jitFrameInfo);
  mBufferPositionWhenReceivedJSContext = mozilla::Nothing();
}
