/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_THREAD_INFO_H
#define MOZ_THREAD_INFO_H

#include "mozilla/NotNull.h"
#include "mozilla/UniquePtrExtensions.h"

#include "ProfileBuffer.h"
#include "platform.h"

class ThreadInfo final
{
public:
  ThreadInfo(const char* aName, int aThreadId, bool aIsMainThread,
             void* aStackTop);

  ~ThreadInfo();

  const char* Name() const { return mName.get(); }
  int ThreadId() const { return mThreadId; }

  bool IsMainThread() const { return mIsMainThread; }

  mozilla::NotNull<PseudoStack*> Stack() const { return mPseudoStack; }

  void StartProfiling();
  void StopProfiling();
  bool IsBeingProfiled() { return mIsBeingProfiled; }

  PlatformData* GetPlatformData() const { return mPlatformData.get(); }
  void* StackTop() const { return mStackTop; }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  ProfileBuffer::LastSample& LastSample() { return mLastSample; }

private:
  mozilla::UniqueFreePtr<char> mName;
  int mThreadId;
  const bool mIsMainThread;

  // The thread's PseudoStack. This is an owning pointer. It could be an inline
  // member, but we don't do that because PseudoStack is quite large, and we
  // have ThreadInfo vectors and so we'd end up wasting a lot of space in those
  // vectors for excess elements.
  mozilla::NotNull<PseudoStack*> mPseudoStack;

  UniquePlatformData mPlatformData;
  void* mStackTop;

  //
  // The following code is only used for threads that are being profiled, i.e.
  // for which IsBeingProfiled() returns true.
  //

public:
  void StreamJSON(ProfileBuffer* aBuffer, SpliceableJSONWriter& aWriter,
                  const mozilla::TimeStamp& aStartTime, double aSinceTime);

  // Call this method when the JS entries inside the buffer are about to
  // become invalid, i.e., just before JS shutdown.
  void FlushSamplesAndMarkers(ProfileBuffer* aBuffer,
                              const mozilla::TimeStamp& aStartTime);

  // Returns nullptr if this is not the main thread or if this thread is not
  // being profiled.
  ThreadResponsiveness* GetThreadResponsiveness()
  {
    ThreadResponsiveness* responsiveness = mResponsiveness.ptrOr(nullptr);
    MOZ_ASSERT(!!responsiveness == (mIsMainThread && mIsBeingProfiled));
    return responsiveness;
  }

private:
  bool mIsBeingProfiled;

  // JS frames in the buffer may require a live JSRuntime to stream (e.g.,
  // stringifying JIT frames). In the case of JSRuntime destruction,
  // FlushSamplesAndMarkers should be called to save them. These are spliced
  // into the final stream.
  mozilla::UniquePtr<char[]> mSavedStreamedSamples;
  mozilla::UniquePtr<char[]> mSavedStreamedMarkers;
  mozilla::Maybe<UniqueStacks> mUniqueStacks;

  // This is only used for the main thread.
  mozilla::Maybe<ThreadResponsiveness> mResponsiveness;

  // When sampling, this holds the generation number and offset in PS::mBuffer
  // of the most recent sample for this thread.
  ProfileBuffer::LastSample mLastSample;
};

void
StreamSamplesAndMarkers(const char* aName, int aThreadId,
                        ProfileBuffer* aBuffer,
                        SpliceableJSONWriter& aWriter,
                        const mozilla::TimeStamp& aStartTime,
                        double aSinceTime,
                        JSContext* aContext,
                        char* aSavedStreamedSamples,
                        char* aSavedStreamedMarkers,
                        UniqueStacks& aUniqueStacks);

#endif
