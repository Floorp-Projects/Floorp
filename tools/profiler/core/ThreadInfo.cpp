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

ThreadInfo::ThreadInfo(const char* aName, int aThreadId, bool aIsMainThread,
                       void* aStackTop)
  : mName(strdup(aName))
  , mThreadId(aThreadId)
  , mIsMainThread(aIsMainThread)
  , mPseudoStack(mozilla::WrapNotNull(new PseudoStack()))
  , mPlatformData(AllocPlatformData(aThreadId))
  , mStackTop(aStackTop)
  , mIsBeingProfiled(false)
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

  delete mPseudoStack;
}

void
ThreadInfo::StartProfiling()
{
  mIsBeingProfiled = true;
  mPseudoStack->reinitializeOnResume();
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

void
ThreadInfo::StreamJSON(ProfileBuffer* aBuffer, SpliceableJSONWriter& aWriter,
                       const TimeStamp& aStartTime, double aSinceTime)
{
  // mUniqueStacks may already be emplaced from FlushSamplesAndMarkers.
  if (!mUniqueStacks.isSome()) {
    mUniqueStacks.emplace(mPseudoStack->mContext);
  }

  aWriter.Start(SpliceableJSONWriter::SingleLineStyle);
  {
    StreamSamplesAndMarkers(Name(), ThreadId(), aBuffer, aWriter, aStartTime,
                            aSinceTime, mPseudoStack->mContext,
                            mSavedStreamedSamples.get(),
                            mSavedStreamedMarkers.get(), *mUniqueStacks);
    mSavedStreamedSamples.reset();
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
}

void
StreamSamplesAndMarkers(const char* aName,
                        int aThreadId,
                        ProfileBuffer* aBuffer,
                        SpliceableJSONWriter& aWriter,
                        const TimeStamp& aStartTime,
                        double aSinceTime,
                        JSContext* aContext,
                        char* aSavedStreamedSamples,
                        char* aSavedStreamedMarkers,
                        UniqueStacks& aUniqueStacks)
{
  aWriter.StringProperty("processType",
                         XRE_ChildProcessTypeToString(XRE_GetProcessType()));

  aWriter.StringProperty("name", aName);
  aWriter.IntProperty("tid", static_cast<int64_t>(aThreadId));
  aWriter.IntProperty("pid", static_cast<int64_t>(getpid()));

  aWriter.StartObjectProperty("samples");
  {
    {
      JSONSchemaWriter schema(aWriter);
      schema.WriteField("stack");
      schema.WriteField("time");
      schema.WriteField("responsiveness");
      schema.WriteField("rss");
      schema.WriteField("uss");
      schema.WriteField("frameNumber");
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
      aBuffer->StreamSamplesToJSON(aWriter, aThreadId, aSinceTime, aContext,
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
      if (aSavedStreamedMarkers) {
        MOZ_ASSERT(aSinceTime == 0);
        aWriter.Splice(aSavedStreamedMarkers);
      }
      aBuffer->StreamMarkersToJSON(aWriter, aThreadId, aStartTime, aSinceTime,
                                   aUniqueStacks);
    }
    aWriter.EndArray();
  }
  aWriter.EndObject();
}

void
ThreadInfo::FlushSamplesAndMarkers(ProfileBuffer* aBuffer,
                                   const TimeStamp& aStartTime)
{
  // This function is used to serialize the current buffer just before
  // JSContext destruction.
  MOZ_ASSERT(mPseudoStack->mContext);

  // Unlike StreamJSObject, do not surround the samples in brackets by calling
  // aWriter.{Start,End}BareList. The result string will be a comma-separated
  // list of JSON object literals that will prepended by StreamJSObject into
  // an existing array.
  //
  // Note that the UniqueStacks instance is persisted so that the frame-index
  // mapping is stable across JS shutdown.
  mUniqueStacks.emplace(mPseudoStack->mContext);

  {
    SpliceableChunkedJSONWriter b;
    b.StartBareList();
    {
      aBuffer->StreamSamplesToJSON(b, mThreadId, /* aSinceTime = */ 0,
                                   mPseudoStack->mContext, *mUniqueStacks);
    }
    b.EndBareList();
    mSavedStreamedSamples = b.WriteFunc()->CopyData();
  }

  {
    SpliceableChunkedJSONWriter b;
    b.StartBareList();
    {
      aBuffer->StreamMarkersToJSON(b, mThreadId, aStartTime,
                                   /* aSinceTime = */ 0, *mUniqueStacks);
    }
    b.EndBareList();
    mSavedStreamedMarkers = b.WriteFunc()->CopyData();
  }

  // Reset the buffer. Attempting to symbolicate JS samples after mContext has
  // gone away will crash.
  aBuffer->reset();
}

size_t
ThreadInfo::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += aMallocSizeOf(mName.get());
  n += mPseudoStack->SizeOfIncludingThis(aMallocSizeOf);

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
