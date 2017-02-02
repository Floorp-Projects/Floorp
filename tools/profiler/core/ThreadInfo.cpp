/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadInfo.h"

#include "mozilla/DebugOnly.h"

ThreadInfo::ThreadInfo(const char* aName, int aThreadId,
                       bool aIsMainThread, PseudoStack* aPseudoStack,
                       void* aStackTop)
  : mName(strdup(aName))
  , mThreadId(aThreadId)
  , mIsMainThread(aIsMainThread)
  , mPseudoStack(aPseudoStack)
  , mPlatformData(Sampler::AllocPlatformData(aThreadId))
  , mStackTop(aStackTop)
  , mPendingDelete(false)
  , mMutex(MakeUnique<mozilla::Mutex>("ThreadInfo::mMutex"))
#ifdef XP_LINUX
  , mRssMemory(0)
  , mUssMemory(0)
#endif
{
  MOZ_COUNT_CTOR(ThreadInfo);
  mThread = NS_GetCurrentThread();

  // We don't have to guess on mac
#ifdef XP_MACOSX
  pthread_t self = pthread_self();
  mStackTop = pthread_get_stackaddr_np(self);
#endif

  // I don't know if we can assert this. But we should warn.
  MOZ_ASSERT(aThreadId >= 0, "native thread ID is < 0");
  MOZ_ASSERT(aThreadId <= INT32_MAX, "native thread ID is > INT32_MAX");
}

ThreadInfo::~ThreadInfo() {
  MOZ_COUNT_DTOR(ThreadInfo);
}

void
ThreadInfo::SetPendingDelete()
{
  mPendingDelete = true;
  // We don't own the pseudostack so disconnect it.
  mPseudoStack = nullptr;
}

bool
ThreadInfo::CanInvokeJS() const
{
  if (!mThread) {
    MOZ_ASSERT(IsMainThread());
    return true;
  }
  bool result;
  mozilla::DebugOnly<nsresult> rv = mThread->GetCanInvokeJS(&result);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return result;
}

void
ThreadInfo::addTag(const ProfileEntry& aTag)
{
  mBuffer->addTag(aTag);
}

void
ThreadInfo::addStoredMarker(ProfilerMarker* aStoredMarker) {
  mBuffer->addStoredMarker(aStoredMarker);
}

void
ThreadInfo::StreamJSON(SpliceableJSONWriter& aWriter, double aSinceTime)
{
  // mUniqueStacks may already be emplaced from FlushSamplesAndMarkers.
  if (!mUniqueStacks.isSome()) {
    mUniqueStacks.emplace(mPseudoStack->mContext);
  }

  aWriter.Start(SpliceableJSONWriter::SingleLineStyle);
  {
    StreamSamplesAndMarkers(aWriter, aSinceTime, *mUniqueStacks);

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
ThreadInfo::StreamSamplesAndMarkers(SpliceableJSONWriter& aWriter,
                                    double aSinceTime,
                                    UniqueStacks& aUniqueStacks)
{
  aWriter.StringProperty("processType",
                         XRE_ChildProcessTypeToString(XRE_GetProcessType()));

  aWriter.StringProperty("name", Name());
  aWriter.IntProperty("tid", static_cast<int>(mThreadId));

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
      if (mSavedStreamedSamples) {
        // We would only have saved streamed samples during shutdown
        // streaming, which cares about dumping the entire buffer, and thus
        // should have passed in 0 for aSinceTime.
        MOZ_ASSERT(aSinceTime == 0);
        aWriter.Splice(mSavedStreamedSamples.get());
        mSavedStreamedSamples.reset();
      }
      mBuffer->StreamSamplesToJSON(aWriter, mThreadId, aSinceTime,
                                   mPseudoStack->mContext, aUniqueStacks);
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
      if (mSavedStreamedMarkers) {
        MOZ_ASSERT(aSinceTime == 0);
        aWriter.Splice(mSavedStreamedMarkers.get());
        mSavedStreamedMarkers.reset();
      }
      mBuffer->StreamMarkersToJSON(aWriter, mThreadId, aSinceTime, aUniqueStacks);
    }
    aWriter.EndArray();
  }
  aWriter.EndObject();
}

void
ThreadInfo::FlushSamplesAndMarkers()
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
      mBuffer->StreamSamplesToJSON(b, mThreadId, /* aSinceTime = */ 0,
                                   mPseudoStack->mContext, *mUniqueStacks);
    }
    b.EndBareList();
    mSavedStreamedSamples = b.WriteFunc()->CopyData();
  }

  {
    SpliceableChunkedJSONWriter b;
    b.StartBareList();
    {
      mBuffer->StreamMarkersToJSON(b, mThreadId, /* aSinceTime = */ 0, *mUniqueStacks);
    }
    b.EndBareList();
    mSavedStreamedMarkers = b.WriteFunc()->CopyData();
  }

  // Reset the buffer. Attempting to symbolicate JS samples after mContext has
  // gone away will crash.
  mBuffer->reset();
}

void
ThreadInfo::BeginUnwind()
{
  mMutex->Lock();
}

void
ThreadInfo::EndUnwind()
{
  mMutex->Unlock();
}

mozilla::Mutex&
ThreadInfo::GetMutex()
{
  return *mMutex.get();
}

void
ThreadInfo::DuplicateLastSample()
{
  mBuffer->DuplicateLastSample(mThreadId);
}

