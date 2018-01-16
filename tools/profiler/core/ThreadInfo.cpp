/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadInfo.h"

#include "mozilla/DebugOnly.h"

#if defined(GP_OS_darwin)
#include <pthread.h>
#endif

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h> // for getpid()
#endif

ThreadInfo::ThreadInfo(const char* aName,
                       int aThreadId,
                       bool aIsMainThread,
                       void* aStackTop)
  : mName(strdup(aName))
  , mRegisterTime(TimeStamp::Now())
  , mIsMainThread(aIsMainThread)
  , mRacyInfo(mozilla::MakeNotNull<RacyThreadInfo*>(aThreadId))
  , mPlatformData(AllocPlatformData(aThreadId))
  , mStackTop(aStackTop)
  , mIsBeingProfiled(false)
  , mFirstSavedStreamedSampleTime{0.0}
  , mContext(nullptr)
  , mJSSampling(INACTIVE)
  , mLastSample()
{
  MOZ_COUNT_CTOR(ThreadInfo);

  // We don't have to guess on mac
#if defined(GP_OS_darwin)
  pthread_t self = pthread_self();
  mStackTop = pthread_get_stackaddr_np(self);
#endif

  // I don't know if we can assert this. But we should warn.
  MOZ_ASSERT(aThreadId >= 0, "native thread ID is < 0");
  MOZ_ASSERT(aThreadId <= INT32_MAX, "native thread ID is > INT32_MAX");
}

ThreadInfo::~ThreadInfo()
{
  MOZ_COUNT_DTOR(ThreadInfo);

  delete mRacyInfo;
}

void
ThreadInfo::StartProfiling()
{
  mIsBeingProfiled = true;
  mRacyInfo->ReinitializeOnResume();
  if (mIsMainThread) {
    mResponsiveness.emplace();
  }
}

void
ThreadInfo::StopProfiling()
{
  mResponsiveness.reset();
  mIsBeingProfiled = false;
}

double
ThreadInfo::StreamJSON(const ProfileBuffer& aBuffer,
                       SpliceableJSONWriter& aWriter,
                       const TimeStamp& aProcessStartTime, double aSinceTime)
{
  // mUniqueStacks may already be emplaced from FlushSamplesAndMarkers.
  if (!mUniqueStacks.isSome()) {
    mUniqueStacks.emplace(mContext);
  }

  double firstSampleTime = 0.0;

  aWriter.Start();
  {
    StreamSamplesAndMarkers(Name(), ThreadId(), aBuffer, aWriter,
                            aProcessStartTime,
                            mRegisterTime, mUnregisterTime,
                            aSinceTime, &firstSampleTime,
                            mContext,
                            mSavedStreamedSamples.get(),
                            mFirstSavedStreamedSampleTime,
                            mSavedStreamedMarkers.get(),
                            *mUniqueStacks);
    mSavedStreamedSamples.reset();
    mFirstSavedStreamedSampleTime = 0.0;
    mSavedStreamedMarkers.reset();

    aWriter.StartObjectProperty("stackTable");
    {
      {
        JSONSchemaWriter schema(aWriter);
        schema.WriteField("prefix");
        schema.WriteField("frame");
      }

      aWriter.StartArrayProperty("data");
      {
        mUniqueStacks->SpliceStackTableElements(aWriter);
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
        mUniqueStacks->SpliceFrameTableElements(aWriter);
      }
      aWriter.EndArray();
    }
    aWriter.EndObject();

    aWriter.StartArrayProperty("stringTable");
    {
      mUniqueStacks->mUniqueStrings.SpliceStringTableElements(aWriter);
    }
    aWriter.EndArray();
  }
  aWriter.End();

  mUniqueStacks.reset();

  return firstSampleTime;
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
                        double* aOutFirstSampleTime,
                        JSContext* aContext,
                        char* aSavedStreamedSamples,
                        double aFirstSavedStreamedSampleTime,
                        char* aSavedStreamedMarkers,
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
      if (aSavedStreamedSamples) {
        // We would only have saved streamed samples during shutdown
        // streaming, which cares about dumping the entire buffer, and thus
        // should have passed in 0 for aSinceTime.
        MOZ_ASSERT(aSinceTime == 0);
        aWriter.Splice(aSavedStreamedSamples);
      }
      aBuffer.StreamSamplesToJSON(aWriter, aThreadId, aSinceTime,
                                  aOutFirstSampleTime, aContext,
                                  aUniqueStacks);
      if (aSavedStreamedSamples) {
        *aOutFirstSampleTime = aFirstSavedStreamedSampleTime;
      }
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
      if (aSavedStreamedMarkers) {
        MOZ_ASSERT(aSinceTime == 0);
        aWriter.Splice(aSavedStreamedMarkers);
      }
      aBuffer.StreamMarkersToJSON(aWriter, aThreadId, aProcessStartTime,
                                  aSinceTime, aUniqueStacks);
    }
    aWriter.EndArray();
  }
  aWriter.EndObject();
}

void
ThreadInfo::FlushSamplesAndMarkers(const TimeStamp& aProcessStartTime,
                                   ProfileBuffer& aBuffer)
{
  // This function is used to serialize the current buffer just before
  // JSContext destruction.
  MOZ_ASSERT(mContext);

  // Unlike StreamJSObject, do not surround the samples in brackets by calling
  // aWriter.{Start,End}BareList. The result string will be a comma-separated
  // list of JSON object literals that will prepended by StreamJSObject into
  // an existing array.
  //
  // Note that the UniqueStacks instance is persisted so that the frame-index
  // mapping is stable across JS shutdown.
  mUniqueStacks.emplace(mContext);

  {
    SpliceableChunkedJSONWriter b;
    b.StartBareList();
    {
      aBuffer.StreamSamplesToJSON(b, ThreadId(), /* aSinceTime = */ 0,
                                  &mFirstSavedStreamedSampleTime,
                                  mContext, *mUniqueStacks);
    }
    b.EndBareList();
    mSavedStreamedSamples = b.WriteFunc()->CopyData();
  }

  {
    SpliceableChunkedJSONWriter b;
    b.StartBareList();
    {
      aBuffer.StreamMarkersToJSON(b, ThreadId(), aProcessStartTime,
                                  /* aSinceTime = */ 0, *mUniqueStacks);
    }
    b.EndBareList();
    mSavedStreamedMarkers = b.WriteFunc()->CopyData();
  }

  // Reset the buffer. Attempting to symbolicate JS samples after mContext has
  // gone away will crash.
  aBuffer.Reset();
}

size_t
ThreadInfo::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += aMallocSizeOf(mName.get());
  n += mRacyInfo->SizeOfIncludingThis(aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - mPlatformData
  // - mSavedStreamedSamples
  // - mSavedStreamedMarkers
  // - mUniqueStacks
  //
  // The following members are not measured:
  // - mThread: because it is non-owning

  return n;
}
