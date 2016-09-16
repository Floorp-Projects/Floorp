/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_THREAD_PROFILE_H
#define MOZ_THREAD_PROFILE_H

#include "ProfileBuffer.h"
#include "ThreadInfo.h"

class ThreadProfile
{
public:
  ThreadProfile(ThreadInfo* aThreadInfo, ProfileBuffer* aBuffer);
  virtual ~ThreadProfile();
  void addTag(const ProfileEntry& aTag);

  /**
   * Track a marker which has been inserted into the ThreadProfile.
   * This marker can safely be deleted once the generation has
   * expired.
   */
  void addStoredMarker(ProfilerMarker *aStoredMarker);
  PseudoStack* GetPseudoStack();
  ::Mutex& GetMutex();
  void StreamJSON(SpliceableJSONWriter& aWriter, double aSinceTime = 0);

  /**
   * Call this method when the JS entries inside the buffer are about to
   * become invalid, i.e., just before JS shutdown.
   */
  void FlushSamplesAndMarkers();

  void BeginUnwind();
  virtual void EndUnwind();
  virtual SyncProfile* AsSyncProfile() { return nullptr; }

  bool IsMainThread() const { return mIsMainThread; }
  const char* Name() const { return mThreadInfo->Name(); }
  int ThreadId() const { return mThreadId; }

  PlatformData* GetPlatformData() const { return mPlatformData; }
  void* GetStackTop() const { return mStackTop; }
  void DuplicateLastSample();

  ThreadInfo* GetThreadInfo() const { return mThreadInfo; }
#ifndef SPS_STANDALONE
  ThreadResponsiveness* GetThreadResponsiveness() { return &mRespInfo; }
#endif

  bool CanInvokeJS() const { return mThreadInfo->CanInvokeJS(); }

  void SetPendingDelete()
  {
    mPseudoStack = nullptr;
    mPlatformData = nullptr;
  }

  uint32_t bufferGeneration() const {
    return mBuffer->mGeneration;
  }

protected:
  void StreamSamplesAndMarkers(SpliceableJSONWriter& aWriter, double aSinceTime,
                               UniqueStacks& aUniqueStacks);

private:
  FRIEND_TEST(ThreadProfile, InsertOneTag);
  FRIEND_TEST(ThreadProfile, InsertOneTagWithTinyBuffer);
  FRIEND_TEST(ThreadProfile, InsertTagsNoWrap);
  FRIEND_TEST(ThreadProfile, InsertTagsWrap);
  FRIEND_TEST(ThreadProfile, MemoryMeasure);
  ThreadInfo* mThreadInfo;

  const RefPtr<ProfileBuffer> mBuffer;

  // JS frames in the buffer may require a live JSRuntime to stream (e.g.,
  // stringifying JIT frames). In the case of JSRuntime destruction,
  // FlushSamplesAndMarkers should be called to save them. These are spliced
  // into the final stream.
  mozilla::UniquePtr<char[]> mSavedStreamedSamples;
  mozilla::UniquePtr<char[]> mSavedStreamedMarkers;
  mozilla::Maybe<UniqueStacks> mUniqueStacks;

  PseudoStack*   mPseudoStack;
  mozilla::UniquePtr<Mutex>  mMutex;
  int            mThreadId;
  bool           mIsMainThread;
  PlatformData*  mPlatformData;  // Platform specific data.
  void* const    mStackTop;
#ifndef SPS_STANDALONE
  ThreadResponsiveness mRespInfo;
#endif

  // Only Linux is using a signal sender, instead of stopping the thread, so we
  // need some space to store the data which cannot be collected in the signal
  // handler code.
#ifdef XP_LINUX
public:
  int64_t        mRssMemory;
  int64_t        mUssMemory;
#endif
};

#endif
