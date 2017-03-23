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

class ThreadInfo {
public:
  ThreadInfo(const char* aName, int aThreadId, bool aIsMainThread,
             mozilla::NotNull<PseudoStack*> aPseudoStack, void* aStackTop);

  virtual ~ThreadInfo();

  const char* Name() const { return mName.get(); }
  int ThreadId() const { return mThreadId; }

  bool IsMainThread() const { return mIsMainThread; }
  mozilla::NotNull<PseudoStack*> Stack() const { return mPseudoStack; }

  void SetHasProfile() { mHasProfile = true; }

  PlatformData* GetPlatformData() const { return mPlatformData.get(); }
  void* StackTop() const { return mStackTop; }

  virtual void SetPendingDelete();
  bool IsPendingDelete() const { return mPendingDelete; }

  bool CanInvokeJS() const;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  ProfileBuffer::LastSample& LastSample() { return mLastSample; }

 private:
  mozilla::UniqueFreePtr<char> mName;
  int mThreadId;
  const bool mIsMainThread;

  // The thread's PseudoStack. This is an owning pointer iff mIsPendingDelete
  // is set.
  mozilla::NotNull<PseudoStack*> mPseudoStack;

  UniquePlatformData mPlatformData;
  void* mStackTop;

  // May be null for the main thread if the profiler was started during startup.
  nsCOMPtr<nsIThread> mThread;

  // When a thread dies while the profiler is active we keep its ThreadInfo
  // (and its PseudoStack) around for a while, and put it in a "pending delete"
  // state. In this state, mPseudoStack is an owning pointer.
  bool mPendingDelete;

  //
  // The following code is only used for threads that are being profiled, i.e.
  // for which SetHasProfile() has been called.
  //

public:
  bool HasProfile() { return mHasProfile; }

  void StreamJSON(ProfileBuffer* aBuffer, SpliceableJSONWriter& aWriter,
                  const mozilla::TimeStamp& aStartTime, double aSinceTime);

  // Call this method when the JS entries inside the buffer are about to
  // become invalid, i.e., just before JS shutdown.
  void FlushSamplesAndMarkers(ProfileBuffer* aBuffer,
                              const mozilla::TimeStamp& aStartTime);

  ThreadResponsiveness* GetThreadResponsiveness() { return &mRespInfo; }

  void UpdateThreadResponsiveness() {
    mRespInfo.Update(mIsMainThread, mThread);
  }

  void StreamSamplesAndMarkers(ProfileBuffer* aBuffer,
                               SpliceableJSONWriter& aWriter,
                               const mozilla::TimeStamp& aStartTime,
                               double aSinceTime,
                               UniqueStacks& aUniqueStacks);

private:
  FRIEND_TEST(ThreadProfile, InsertOneTag);
  FRIEND_TEST(ThreadProfile, InsertOneTagWithTinyBuffer);
  FRIEND_TEST(ThreadProfile, InsertTagsNoWrap);
  FRIEND_TEST(ThreadProfile, InsertTagsWrap);
  FRIEND_TEST(ThreadProfile, MemoryMeasure);

  bool mHasProfile;

  // JS frames in the buffer may require a live JSRuntime to stream (e.g.,
  // stringifying JIT frames). In the case of JSRuntime destruction,
  // FlushSamplesAndMarkers should be called to save them. These are spliced
  // into the final stream.
  mozilla::UniquePtr<char[]> mSavedStreamedSamples;
  mozilla::UniquePtr<char[]> mSavedStreamedMarkers;
  mozilla::Maybe<UniqueStacks> mUniqueStacks;

  ThreadResponsiveness mRespInfo;

  // When sampling, this holds the generation number and offset in the
  // ProfileBuffer of the most recent sample for this thread.
  // mLastSample.mThreadId duplicates mThreadId in this structure, which
  // simplifies some uses of mLastSample.
  ProfileBuffer::LastSample mLastSample;
};

#endif
