/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfiledThreadData.h"

#include "platform.h"
#include "ProfileBuffer.h"

#include "mozilla/OriginAttributes.h"
#include "mozilla/Span.h"
#include "nsXULAppAPI.h"

#if defined(GP_OS_darwin)
#  include <pthread.h>
#endif

using namespace mozilla::literals::ProportionValue_literals;

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

static void StreamTables(UniqueStacks&& aUniqueStacks, JSContext* aCx,
                         SpliceableJSONWriter& aWriter,
                         const mozilla::TimeStamp& aProcessStartTime,
                         mozilla::ProgressLogger aProgressLogger) {
  aWriter.StartObjectProperty("stackTable");
  {
    {
      JSONSchemaWriter schema(aWriter);
      schema.WriteField("prefix");
      schema.WriteField("frame");
    }

    aWriter.StartArrayProperty("data");
    {
      aProgressLogger.SetLocalProgress(1_pc, "Splicing stack table...");
      aUniqueStacks.SpliceStackTableElements(aWriter);
      aProgressLogger.SetLocalProgress(30_pc, "Spliced stack table");
    }
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
    {
      aProgressLogger.SetLocalProgress(30_pc, "Splicing frame table...");
      aUniqueStacks.SpliceFrameTableElements(aWriter);
      aProgressLogger.SetLocalProgress(60_pc, "Spliced frame table");
    }
    aWriter.EndArray();
  }
  aWriter.EndObject();

  aWriter.StartArrayProperty("stringTable");
  {
    aProgressLogger.SetLocalProgress(60_pc, "Splicing string table...");
    std::move(aUniqueStacks.UniqueStrings()).SpliceStringTableElements(aWriter);
    aProgressLogger.SetLocalProgress(90_pc, "Spliced string table");
  }
  aWriter.EndArray();
}

mozilla::NotNull<mozilla::UniquePtr<UniqueStacks>>
ProfiledThreadData::PrepareUniqueStacks(
    const ProfileBuffer& aBuffer, JSContext* aCx,
    mozilla::FailureLatch& aFailureLatch, ProfilerCodeAddressService* aService,
    mozilla::ProgressLogger aProgressLogger) {
  if (mJITFrameInfoForPreviousJSContexts &&
      mJITFrameInfoForPreviousJSContexts->HasExpired(
          aBuffer.BufferRangeStart())) {
    mJITFrameInfoForPreviousJSContexts = nullptr;
  }
  aProgressLogger.SetLocalProgress(1_pc, "Checked JIT frame info presence");

  // If we have an existing JITFrameInfo in mJITFrameInfoForPreviousJSContexts,
  // copy the data from it.
  JITFrameInfo jitFrameInfo =
      mJITFrameInfoForPreviousJSContexts
          ? JITFrameInfo(*mJITFrameInfoForPreviousJSContexts,
                         aProgressLogger.CreateSubLoggerTo(
                             "Retrieving JIT frame info...", 10_pc,
                             "Retrieved JIT frame info"))
          : JITFrameInfo();

  if (aCx && mBufferPositionWhenReceivedJSContext) {
    aBuffer.AddJITInfoForRange(
        *mBufferPositionWhenReceivedJSContext, mThreadInfo.ThreadId(), aCx,
        jitFrameInfo,
        aProgressLogger.CreateSubLoggerTo("Adding JIT info...", 90_pc,
                                          "Added JIT info"));
  } else {
    aProgressLogger.SetLocalProgress(90_pc, "No JIT info");
  }

  return mozilla::MakeNotNull<mozilla::UniquePtr<UniqueStacks>>(
      aFailureLatch, std::move(jitFrameInfo), aService);
}

void ProfiledThreadData::StreamJSON(
    const ProfileBuffer& aBuffer, JSContext* aCx, SpliceableJSONWriter& aWriter,
    const nsACString& aProcessName, const nsACString& aETLDplus1,
    const mozilla::TimeStamp& aProcessStartTime, double aSinceTime,
    ProfilerCodeAddressService* aService,
    mozilla::ProgressLogger aProgressLogger) {
  mozilla::NotNull<mozilla::UniquePtr<UniqueStacks>> uniqueStacks =
      PrepareUniqueStacks(aBuffer, aCx, aWriter.SourceFailureLatch(), aService,
                          aProgressLogger.CreateSubLoggerFromTo(
                              0_pc, "Preparing unique stacks...", 10_pc,
                              "Prepared Unique stacks"));

  aWriter.SetUniqueStrings(uniqueStacks->UniqueStrings());

  aWriter.Start();
  {
    StreamSamplesAndMarkers(
        mThreadInfo.Name(), mThreadInfo.ThreadId(), aBuffer, aWriter,
        aProcessName, aETLDplus1, aProcessStartTime, mThreadInfo.RegisterTime(),
        mUnregisterTime, aSinceTime, *uniqueStacks,
        aProgressLogger.CreateSubLoggerTo(
            90_pc,
            "ProfiledThreadData::StreamJSON: Streamed samples and markers"));

    StreamTables(std::move(*uniqueStacks), aCx, aWriter, aProcessStartTime,
                 aProgressLogger.CreateSubLoggerTo(
                     99_pc, "Streamed tables and trace logger"));
  }
  aWriter.End();

  aWriter.ResetUniqueStrings();
}

void ProfiledThreadData::StreamJSON(
    ThreadStreamingContext&& aThreadStreamingContext,
    SpliceableJSONWriter& aWriter, const nsACString& aProcessName,
    const nsACString& aETLDplus1, const mozilla::TimeStamp& aProcessStartTime,
    ProfilerCodeAddressService* aService,
    mozilla::ProgressLogger aProgressLogger) {
  aWriter.Start();
  {
    StreamSamplesAndMarkers(
        mThreadInfo.Name(), aThreadStreamingContext, aWriter, aProcessName,
        aETLDplus1, aProcessStartTime, mThreadInfo.RegisterTime(),
        mUnregisterTime,
        aProgressLogger.CreateSubLoggerFromTo(
            1_pc, "ProfiledThreadData::StreamJSON(context): Streaming...",
            90_pc,
            "ProfiledThreadData::StreamJSON(context): Streamed samples and "
            "markers"));

    StreamTables(
        std::move(*aThreadStreamingContext.mUniqueStacks),
        aThreadStreamingContext.mJSContext, aWriter, aProcessStartTime,
        aProgressLogger.CreateSubLoggerTo(
            "ProfiledThreadData::StreamJSON(context): Streaming tables...",
            99_pc, "ProfiledThreadData::StreamJSON(context): Streamed tables"));
  }
  aWriter.End();
}

// StreamSamplesDataCallback: (ProgressLogger) -> ProfilerThreadId
// StreamMarkersDataCallback: (ProgressLogger) -> void
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
    mozilla::ProgressLogger aProgressLogger,
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
    nsAutoCString originNoSuffix;
    mozilla::OriginAttributes attrs;
    if (!attrs.PopulateFromOrigin(aETLDplus1, originNoSuffix)) {
      aWriter.StringProperty("eTLD+1", aETLDplus1);
    } else {
      aWriter.StringProperty("eTLD+1", originNoSuffix);
      aWriter.BoolProperty("isPrivateBrowsing", attrs.mPrivateBrowsingId > 0);
      aWriter.IntProperty("userContextId", attrs.mUserContextId);
    }
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
      processedThreadId = std::forward<StreamSamplesDataCallback>(
          aStreamSamplesDataCallback)(aProgressLogger.CreateSubLoggerFromTo(
          1_pc, "Streaming samples...", 49_pc, "Streamed samples"));
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
    {
      std::forward<StreamMarkersDataCallback>(aStreamMarkersDataCallback)(
          aProgressLogger.CreateSubLoggerFromTo(50_pc, "Streaming markers...",
                                                99_pc, "Streamed markers"));
    }
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
    UniqueStacks& aUniqueStacks, mozilla::ProgressLogger aProgressLogger) {
  return DoStreamSamplesAndMarkers(
      aName, aWriter, aProcessName, aETLDplus1, aProcessStartTime,
      aRegisterTime, aUnregisterTime, std::move(aProgressLogger),
      [&](mozilla::ProgressLogger aSubProgressLogger) {
        ProfilerThreadId processedThreadId = aBuffer.StreamSamplesToJSON(
            aWriter, aThreadId, aSinceTime, aUniqueStacks,
            std::move(aSubProgressLogger));
        return aThreadId.IsSpecified() ? aThreadId : processedThreadId;
      },
      [&](mozilla::ProgressLogger aSubProgressLogger) {
        aBuffer.StreamMarkersToJSON(aWriter, aThreadId, aProcessStartTime,
                                    aSinceTime, aUniqueStacks,
                                    std::move(aSubProgressLogger));
      });
}

void StreamSamplesAndMarkers(const char* aName,
                             ThreadStreamingContext& aThreadData,
                             SpliceableJSONWriter& aWriter,
                             const nsACString& aProcessName,
                             const nsACString& aETLDplus1,
                             const mozilla::TimeStamp& aProcessStartTime,
                             const mozilla::TimeStamp& aRegisterTime,
                             const mozilla::TimeStamp& aUnregisterTime,
                             mozilla::ProgressLogger aProgressLogger) {
  (void)DoStreamSamplesAndMarkers(
      aName, aWriter, aProcessName, aETLDplus1, aProcessStartTime,
      aRegisterTime, aUnregisterTime, std::move(aProgressLogger),
      [&](mozilla::ProgressLogger aSubProgressLogger) {
        aWriter.TakeAndSplice(
            aThreadData.mSamplesDataWriter.TakeChunkedWriteFunc());
        return aThreadData.mProfiledThreadData.Info().ThreadId();
      },
      [&](mozilla::ProgressLogger aSubProgressLogger) {
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
                             mThreadInfo.ThreadId(), aContext, *jitFrameInfo,
                             mozilla::ProgressLogger{});

  mJITFrameInfoForPreviousJSContexts = std::move(jitFrameInfo);
  mBufferPositionWhenReceivedJSContext = mozilla::Nothing();
}

ThreadStreamingContext::ThreadStreamingContext(
    ProfiledThreadData& aProfiledThreadData, const ProfileBuffer& aBuffer,
    JSContext* aCx, mozilla::FailureLatch& aFailureLatch,
    ProfilerCodeAddressService* aService,
    mozilla::ProgressLogger aProgressLogger)
    : mProfiledThreadData(aProfiledThreadData),
      mJSContext(aCx),
      mSamplesDataWriter(aFailureLatch),
      mMarkersDataWriter(aFailureLatch),
      mUniqueStacks(mProfiledThreadData.PrepareUniqueStacks(
          aBuffer, aCx, aFailureLatch, aService,
          aProgressLogger.CreateSubLoggerFromTo(
              0_pc, "Preparing thread streaming context unique stacks...",
              99_pc, "Prepared thread streaming context Unique stacks"))) {
  if (aFailureLatch.Failed()) {
    return;
  }
  mSamplesDataWriter.SetUniqueStrings(mUniqueStacks->UniqueStrings());
  mSamplesDataWriter.StartBareList();
  mMarkersDataWriter.SetUniqueStrings(mUniqueStacks->UniqueStrings());
  mMarkersDataWriter.StartBareList();
}

void ThreadStreamingContext::FinalizeWriter() {
  mSamplesDataWriter.EndBareList();
  mMarkersDataWriter.EndBareList();
}

ProcessStreamingContext::ProcessStreamingContext(
    size_t aThreadCount, mozilla::FailureLatch& aFailureLatch,
    const mozilla::TimeStamp& aProcessStartTime, double aSinceTime)
    : mFailureLatch(aFailureLatch),
      mProcessStartTime(aProcessStartTime),
      mSinceTime(aSinceTime) {
  if (mFailureLatch.Failed()) {
    return;
  }
  if (!mTIDList.initCapacity(aThreadCount)) {
    mFailureLatch.SetFailure(
        "OOM in ProcessStreamingContext allocating TID list");
    return;
  }
  if (!mThreadStreamingContextList.initCapacity(aThreadCount)) {
    mFailureLatch.SetFailure(
        "OOM in ProcessStreamingContext allocating context list");
    mTIDList.clear();
    return;
  }
}

ProcessStreamingContext::~ProcessStreamingContext() {
  if (mFailureLatch.Failed()) {
    return;
  }
  MOZ_ASSERT(mTIDList.length() == mThreadStreamingContextList.length());
  MOZ_ASSERT(mTIDList.length() == mTIDList.capacity(),
             "Didn't pre-allocate exactly right");
}

void ProcessStreamingContext::AddThreadStreamingContext(
    ProfiledThreadData& aProfiledThreadData, const ProfileBuffer& aBuffer,
    JSContext* aCx, ProfilerCodeAddressService* aService,
    mozilla::ProgressLogger aProgressLogger) {
  if (mFailureLatch.Failed()) {
    return;
  }
  MOZ_ASSERT(mTIDList.length() == mThreadStreamingContextList.length());
  MOZ_ASSERT(mTIDList.length() < mTIDList.capacity(),
             "Didn't pre-allocate enough");
  mTIDList.infallibleAppend(aProfiledThreadData.Info().ThreadId());
  mThreadStreamingContextList.infallibleEmplaceBack(
      aProfiledThreadData, aBuffer, aCx, mFailureLatch, aService,
      aProgressLogger.CreateSubLoggerFromTo(
          1_pc, "Prepared streaming thread id", 100_pc,
          "Added thread streaming context"));
}
