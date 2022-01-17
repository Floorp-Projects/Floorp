/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfiledThreadData.h"

#include "platform.h"
#include "ProfileBuffer.h"

#include "js/TraceLoggerAPI.h"
#include "mozilla/Span.h"
#include "nsXULAppAPI.h"

#if defined(GP_OS_darwin)
#  include <pthread.h>
#endif

ProfiledThreadData::ProfiledThreadData(
    const mozilla::profiler::ThreadRegistrationInfo& aThreadInfo)
    : mThreadInfo(aThreadInfo.Name(), aThreadInfo.ThreadId(),
                  aThreadInfo.IsMainThread(), aThreadInfo.RegisterTime()) {
  MOZ_COUNT_CTOR(ProfiledThreadData);
}

ProfiledThreadData::ProfiledThreadData(
    mozilla::profiler::ThreadRegistrationInfo&& aThreadInfo)
    : mThreadInfo(std::move(aThreadInfo)) {
  MOZ_COUNT_CTOR(ProfiledThreadData);
}

ProfiledThreadData::~ProfiledThreadData() {
  MOZ_COUNT_DTOR(ProfiledThreadData);
}

static void StreamTraceLoggerJSON(JSContext* aCx, SpliceableJSONWriter& aWriter,
                                  const mozilla::TimeStamp& aProcessStartTime) {
  aWriter.StartObjectProperty("jsTracerEvents");
  {
    JS::AutoTraceLoggerLockGuard lockGuard;
    JS::SpewTraceLoggerThread(aCx);

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

static void StreamTablesAndTraceLogger(
    UniqueStacks&& aUniqueStacks, JSContext* aCx, SpliceableJSONWriter& aWriter,
    const mozilla::TimeStamp& aProcessStartTime, bool JSTracerEnabled) {
  aWriter.StartObjectProperty("stackTable");
  {
    {
      JSONSchemaWriter schema(aWriter);
      schema.WriteField("prefix");
      schema.WriteField("frame");
    }

    aWriter.StartArrayProperty("data");
    { aUniqueStacks.SpliceStackTableElements(aWriter); }
    aWriter.EndArray();
  }
  aWriter.EndObject();

  aWriter.StartObjectProperty("frameTable");
  {
    {
      JSONSchemaWriter schema(aWriter);
      schema.WriteField("location");
      schema.WriteField("relevantForJS");
      schema.WriteField("innerWindowID");
      schema.WriteField("implementation");
      schema.WriteField("optimizations");
      schema.WriteField("line");
      schema.WriteField("column");
      schema.WriteField("category");
      schema.WriteField("subcategory");
    }

    aWriter.StartArrayProperty("data");
    { aUniqueStacks.SpliceFrameTableElements(aWriter); }
    aWriter.EndArray();
  }
  aWriter.EndObject();

  aWriter.StartArrayProperty("stringTable");
  {
    std::move(*aUniqueStacks.mUniqueStrings).SpliceStringTableElements(aWriter);
  }
  aWriter.EndArray();

  if (aCx && JSTracerEnabled) {
    StreamTraceLoggerJSON(aCx, aWriter, aProcessStartTime);
  }
}

mozilla::NotNull<mozilla::UniquePtr<UniqueStacks>>
ProfiledThreadData::PrepareUniqueStacks(const ProfileBuffer& aBuffer,
                                        JSContext* aCx,
                                        ProfilerCodeAddressService* aService) {
  if (mJITFrameInfoForPreviousJSContexts &&
      mJITFrameInfoForPreviousJSContexts->HasExpired(
          aBuffer.BufferRangeStart())) {
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
                               mThreadInfo.ThreadId(), aCx, jitFrameInfo);
  }

  return mozilla::MakeNotNull<mozilla::UniquePtr<UniqueStacks>>(
      std::move(jitFrameInfo), aService);
}

void ProfiledThreadData::StreamJSON(
    const ProfileBuffer& aBuffer, JSContext* aCx, SpliceableJSONWriter& aWriter,
    const nsACString& aProcessName, const nsACString& aETLDplus1,
    const mozilla::TimeStamp& aProcessStartTime, double aSinceTime,
    bool JSTracerEnabled, ProfilerCodeAddressService* aService) {
  mozilla::NotNull<mozilla::UniquePtr<UniqueStacks>> uniqueStacks =
      PrepareUniqueStacks(aBuffer, aCx, aService);

  MOZ_ASSERT(uniqueStacks->mUniqueStrings);
  aWriter.SetUniqueStrings(*uniqueStacks->mUniqueStrings);

  aWriter.Start();
  {
    StreamSamplesAndMarkers(mThreadInfo.Name(), mThreadInfo.ThreadId(), aBuffer,
                            aWriter, aProcessName, aETLDplus1,
                            aProcessStartTime, mThreadInfo.RegisterTime(),
                            mUnregisterTime, aSinceTime, *uniqueStacks);

    StreamTablesAndTraceLogger(std::move(*uniqueStacks), aCx, aWriter,
                               aProcessStartTime, JSTracerEnabled);
  }
  aWriter.End();

  aWriter.ResetUniqueStrings();
}

void ProfiledThreadData::StreamJSON(
    ThreadStreamingContext&& aThreadStreamingContext,
    SpliceableJSONWriter& aWriter, const nsACString& aProcessName,
    const nsACString& aETLDplus1, const mozilla::TimeStamp& aProcessStartTime,
    bool JSTracerEnabled, ProfilerCodeAddressService* aService) {
  aWriter.Start();
  {
    StreamSamplesAndMarkers(mThreadInfo.Name(), aThreadStreamingContext,
                            aWriter, aProcessName, aETLDplus1,
                            aProcessStartTime, mThreadInfo.RegisterTime(),
                            mUnregisterTime);

    StreamTablesAndTraceLogger(
        std::move(*aThreadStreamingContext.mUniqueStacks),
        aThreadStreamingContext.mJSContext, aWriter, aProcessStartTime,
        JSTracerEnabled);
  }
  aWriter.End();
}

// StreamSamplesDataCallback: () -> ProfilerThreadId
// StreamMarkersDataCallback: () -> void
// Returns the ProfilerThreadId returned by StreamSamplesDataCallback, which
// should be the thread id of the last sample that was processed (if any;
// otherwise it is left unspecified). This is mostly useful when the caller
// doesn't know where the sample comes from, e.g., when it's a backtrace in a
// marker.
template <typename StreamSamplesDataCallback,
          typename StreamMarkersDataCallback>
ProfilerThreadId DoStreamSamplesAndMarkers(
    const char* aName, SpliceableJSONWriter& aWriter,
    const nsACString& aProcessName, const nsACString& aETLDplus1,
    const mozilla::TimeStamp& aProcessStartTime,
    const mozilla::TimeStamp& aRegisterTime,
    const mozilla::TimeStamp& aUnregisterTime,
    StreamSamplesDataCallback&& aStreamSamplesDataCallback,
    StreamMarkersDataCallback&& aStreamMarkersDataCallback) {
  ProfilerThreadId processedThreadId;

  aWriter.StringProperty("processType",
                         mozilla::MakeStringSpan(XRE_GetProcessTypeString()));

  aWriter.StringProperty("name", mozilla::MakeStringSpan(aName));

  // Use given process name (if any), unless we're the parent process.
  if (XRE_IsParentProcess()) {
    aWriter.StringProperty("processName", "Parent Process");
  } else if (!aProcessName.IsEmpty()) {
    aWriter.StringProperty("processName", aProcessName);
  }
  if (!aETLDplus1.IsEmpty()) {
    aWriter.StringProperty("eTLD+1", aETLDplus1);
  }

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
      schema.WriteField("eventDelay");
#define RUNNING_TIME_FIELD(index, name, unit, jsonProperty) \
  schema.WriteField(#jsonProperty);
      PROFILER_FOR_EACH_RUNNING_TIME(RUNNING_TIME_FIELD)
#undef RUNNING_TIME_FIELD
    }

    aWriter.StartArrayProperty("data");
    {
      processedThreadId =
          std::forward<StreamSamplesDataCallback>(aStreamSamplesDataCallback)();
    }
    aWriter.EndArray();
  }
  aWriter.EndObject();

  aWriter.StartObjectProperty("markers");
  {
    {
      JSONSchemaWriter schema(aWriter);
      schema.WriteField("name");
      schema.WriteField("startTime");
      schema.WriteField("endTime");
      schema.WriteField("phase");
      schema.WriteField("category");
      schema.WriteField("data");
    }

    aWriter.StartArrayProperty("data");
    { std::forward<StreamMarkersDataCallback>(aStreamMarkersDataCallback)(); }
    aWriter.EndArray();
  }
  aWriter.EndObject();

  // Tech note: If `ToNumber()` returns a uint64_t, the conversion to int64_t is
  // "implementation-defined" before C++20. This is acceptable here, because
  // this is a one-way conversion to a unique identifier that's used to visually
  // separate data by thread on the front-end.
  aWriter.IntProperty(
      "pid", static_cast<int64_t>(profiler_current_process_id().ToNumber()));
  aWriter.IntProperty("tid",
                      static_cast<int64_t>(processedThreadId.ToNumber()));

  return processedThreadId;
}

ProfilerThreadId StreamSamplesAndMarkers(
    const char* aName, ProfilerThreadId aThreadId, const ProfileBuffer& aBuffer,
    SpliceableJSONWriter& aWriter, const nsACString& aProcessName,
    const nsACString& aETLDplus1, const mozilla::TimeStamp& aProcessStartTime,
    const mozilla::TimeStamp& aRegisterTime,
    const mozilla::TimeStamp& aUnregisterTime, double aSinceTime,
    UniqueStacks& aUniqueStacks) {
  return DoStreamSamplesAndMarkers(
      aName, aWriter, aProcessName, aETLDplus1, aProcessStartTime,
      aRegisterTime, aUnregisterTime,
      [&]() {
        ProfilerThreadId processedThreadId = aBuffer.StreamSamplesToJSON(
            aWriter, aThreadId, aSinceTime, aUniqueStacks);
        return aThreadId.IsSpecified() ? aThreadId : processedThreadId;
      },
      [&]() {
        aBuffer.StreamMarkersToJSON(aWriter, aThreadId, aProcessStartTime,
                                    aSinceTime, aUniqueStacks);
      });
}

void StreamSamplesAndMarkers(const char* aName,
                             ThreadStreamingContext& aThreadData,
                             SpliceableJSONWriter& aWriter,
                             const nsACString& aProcessName,
                             const nsACString& aETLDplus1,
                             const mozilla::TimeStamp& aProcessStartTime,
                             const mozilla::TimeStamp& aRegisterTime,
                             const mozilla::TimeStamp& aUnregisterTime) {
  (void)DoStreamSamplesAndMarkers(
      aName, aWriter, aProcessName, aETLDplus1, aProcessStartTime,
      aRegisterTime, aUnregisterTime,
      [&]() {
        aWriter.TakeAndSplice(
            aThreadData.mSamplesDataWriter.TakeChunkedWriteFunc());
        return aThreadData.mProfiledThreadData.Info().ThreadId();
      },
      [&]() {
        aWriter.TakeAndSplice(
            aThreadData.mMarkersDataWriter.TakeChunkedWriteFunc());
      });
}

void ProfiledThreadData::NotifyAboutToLoseJSContext(
    JSContext* aContext, const mozilla::TimeStamp& aProcessStartTime,
    ProfileBuffer& aBuffer) {
  if (!mBufferPositionWhenReceivedJSContext) {
    return;
  }

  MOZ_RELEASE_ASSERT(aContext);

  if (mJITFrameInfoForPreviousJSContexts &&
      mJITFrameInfoForPreviousJSContexts->HasExpired(
          aBuffer.BufferRangeStart())) {
    mJITFrameInfoForPreviousJSContexts = nullptr;
  }

  mozilla::UniquePtr<JITFrameInfo> jitFrameInfo =
      mJITFrameInfoForPreviousJSContexts
          ? std::move(mJITFrameInfoForPreviousJSContexts)
          : mozilla::MakeUnique<JITFrameInfo>();

  aBuffer.AddJITInfoForRange(*mBufferPositionWhenReceivedJSContext,
                             mThreadInfo.ThreadId(), aContext, *jitFrameInfo);

  mJITFrameInfoForPreviousJSContexts = std::move(jitFrameInfo);
  mBufferPositionWhenReceivedJSContext = mozilla::Nothing();
}

ThreadStreamingContext::ThreadStreamingContext(
    ProfiledThreadData& aProfiledThreadData, const ProfileBuffer& aBuffer,
    JSContext* aCx, ProfilerCodeAddressService* aService)
    : mProfiledThreadData(aProfiledThreadData),
      mJSContext(aCx),
      mUniqueStacks(
          mProfiledThreadData.PrepareUniqueStacks(aBuffer, aCx, aService)) {
  mSamplesDataWriter.SetUniqueStrings(*mUniqueStacks->mUniqueStrings);
  mSamplesDataWriter.StartBareList();
  mMarkersDataWriter.SetUniqueStrings(*mUniqueStacks->mUniqueStrings);
  mMarkersDataWriter.StartBareList();
}

void ThreadStreamingContext::FinalizeWriter() {
  mSamplesDataWriter.EndBareList();
  mMarkersDataWriter.EndBareList();
}

ProcessStreamingContext::ProcessStreamingContext(
    size_t aThreadCount, const mozilla::TimeStamp& aProcessStartTime,
    double aSinceTime)
    : mProcessStartTime(aProcessStartTime), mSinceTime(aSinceTime) {
  MOZ_RELEASE_ASSERT(mTIDList.initCapacity(aThreadCount));
  MOZ_RELEASE_ASSERT(mThreadStreamingContextList.initCapacity(aThreadCount));
}

ProcessStreamingContext::~ProcessStreamingContext() {
  MOZ_ASSERT(mTIDList.length() == mThreadStreamingContextList.length());
  MOZ_ASSERT(mTIDList.length() == mTIDList.capacity(),
             "Didn't pre-allocate exactly right");
}

void ProcessStreamingContext::AddThreadStreamingContext(
    ProfiledThreadData& aProfiledThreadData, const ProfileBuffer& aBuffer,
    JSContext* aCx, ProfilerCodeAddressService* aService) {
  MOZ_ASSERT(mTIDList.length() == mThreadStreamingContextList.length());
  MOZ_ASSERT(mTIDList.length() < mTIDList.capacity(),
             "Didn't pre-allocate enough");
  mTIDList.infallibleAppend(aProfiledThreadData.Info().ThreadId());
  mThreadStreamingContextList.infallibleEmplaceBack(aProfiledThreadData,
                                                    aBuffer, aCx, aService);
}
