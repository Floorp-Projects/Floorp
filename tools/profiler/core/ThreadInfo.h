/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_THREAD_INFO_H
#define MOZ_THREAD_INFO_H

#include "mozilla/UniquePtrExtensions.h"

#include "ProfileBuffer.h"
#include "platform.h"

class ThreadInfo {
 public:
  ThreadInfo(const char* aName, int aThreadId, bool aIsMainThread, PseudoStack* aPseudoStack, void* aStackTop);

  virtual ~ThreadInfo();

  const char* Name() const { return mName.get(); }
  int ThreadId() const { return mThreadId; }

  bool IsMainThread() const { return mIsMainThread; }
  PseudoStack* Stack() const { return mPseudoStack; }

  void SetProfile(ProfileBuffer* aBuffer) { mBuffer = aBuffer; }

  PlatformData* GetPlatformData() const { return mPlatformData.get(); }
  void* StackTop() const { return mStackTop; }

  virtual void SetPendingDelete();
  bool IsPendingDelete() const { return mPendingDelete; }

  bool CanInvokeJS() const;

 private:
  mozilla::UniqueFreePtr<char> mName;
  int mThreadId;
  const bool mIsMainThread;
  PseudoStack* mPseudoStack;
  Sampler::UniquePlatformData mPlatformData;
  void* mStackTop;

  // May be null for the main thread if the profiler was started during startup.
  nsCOMPtr<nsIThread> mThread;

  bool mPendingDelete;

  //
  // The following code is only used for threads that are being profiled, i.e.
  // for which SetProfile() has been called.
  //

public:
  bool hasProfile() { return !!mBuffer; }

  void addTag(const ProfileEntry& aTag);

  // Track a marker which has been inserted into the thread profile.
  // This marker can safely be deleted once the generation has
  // expired.
  void addStoredMarker(ProfilerMarker* aStoredMarker);
  mozilla::Mutex& GetMutex();
  void StreamJSON(SpliceableJSONWriter& aWriter, double aSinceTime = 0);

  // Call this method when the JS entries inside the buffer are about to
  // become invalid, i.e., just before JS shutdown.
  void FlushSamplesAndMarkers();

  void BeginUnwind();
  virtual void EndUnwind();

  void DuplicateLastSample();

  ThreadResponsiveness* GetThreadResponsiveness() { return &mRespInfo; }

  void UpdateThreadResponsiveness() {
    mRespInfo.Update(mIsMainThread, mThread);
  }

  uint32_t bufferGeneration() const { return mBuffer->mGeneration; }

protected:
  void StreamSamplesAndMarkers(SpliceableJSONWriter& aWriter, double aSinceTime,
                               UniqueStacks& aUniqueStacks);

private:
  FRIEND_TEST(ThreadProfile, InsertOneTag);
  FRIEND_TEST(ThreadProfile, InsertOneTagWithTinyBuffer);
  FRIEND_TEST(ThreadProfile, InsertTagsNoWrap);
  FRIEND_TEST(ThreadProfile, InsertTagsWrap);
  FRIEND_TEST(ThreadProfile, MemoryMeasure);

  RefPtr<ProfileBuffer> mBuffer;

  // JS frames in the buffer may require a live JSRuntime to stream (e.g.,
  // stringifying JIT frames). In the case of JSRuntime destruction,
  // FlushSamplesAndMarkers should be called to save them. These are spliced
  // into the final stream.
  mozilla::UniquePtr<char[]> mSavedStreamedSamples;
  mozilla::UniquePtr<char[]> mSavedStreamedMarkers;
  mozilla::Maybe<UniqueStacks> mUniqueStacks;

  mozilla::UniquePtr<mozilla::Mutex> mMutex;
  ThreadResponsiveness mRespInfo;

#ifdef XP_LINUX
  // Only Linux is using a signal sender, instead of stopping the thread, so we
  // need some space to store the data which cannot be collected in the signal
  // handler code.
public:
  int64_t mRssMemory;
  int64_t mUssMemory;
#endif
};

// Just like ThreadInfo, but owns a reference to the PseudoStack.
class StackOwningThreadInfo : public ThreadInfo {
 public:
  StackOwningThreadInfo(const char* aName, int aThreadId, bool aIsMainThread, PseudoStack* aPseudoStack, void* aStackTop);
  virtual ~StackOwningThreadInfo();

  virtual void SetPendingDelete();
};

#endif
