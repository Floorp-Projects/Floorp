/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_PROFILE_ENTRY_H
#define MOZ_PROFILE_ENTRY_H

#include <ostream>
#include "GeckoProfiler.h"
#include "platform.h"
#include "ProfileJSONWriter.h"
#include "ProfilerBacktrace.h"
#include "nsRefPtr.h"
#include "nsHashKeys.h"
#include "nsDataHashtable.h"
#include "js/ProfilingFrameIterator.h"
#include "js/TrackedOptimizationInfo.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/Vector.h"
#include "gtest/MozGtestFriend.h"
#include "mozilla/UniquePtr.h"

class ThreadProfile;

#pragma pack(push, 1)

class ProfileEntry
{
public:
  ProfileEntry();

  // aTagData must not need release (i.e. be a string from the text segment)
  ProfileEntry(char aTagName, const char *aTagData);
  ProfileEntry(char aTagName, void *aTagPtr);
  ProfileEntry(char aTagName, ProfilerMarker *aTagMarker);
  ProfileEntry(char aTagName, float aTagFloat);
  ProfileEntry(char aTagName, uintptr_t aTagOffset);
  ProfileEntry(char aTagName, Address aTagAddress);
  ProfileEntry(char aTagName, int aTagLine);
  ProfileEntry(char aTagName, char aTagChar);
  bool is_ent_hint(char hintChar);
  bool is_ent_hint();
  bool is_ent(char tagName);
  void* get_tagPtr();
  const ProfilerMarker* getMarker() {
    MOZ_ASSERT(mTagName == 'm');
    return mTagMarker;
  }

  char getTagName() const { return mTagName; }

private:
  FRIEND_TEST(ThreadProfile, InsertOneTag);
  FRIEND_TEST(ThreadProfile, InsertOneTagWithTinyBuffer);
  FRIEND_TEST(ThreadProfile, InsertTagsNoWrap);
  FRIEND_TEST(ThreadProfile, InsertTagsWrap);
  FRIEND_TEST(ThreadProfile, MemoryMeasure);
  friend class ProfileBuffer;
  union {
    const char* mTagData;
    char        mTagChars[sizeof(void*)];
    void*       mTagPtr;
    ProfilerMarker* mTagMarker;
    float       mTagFloat;
    Address     mTagAddress;
    uintptr_t   mTagOffset;
    int         mTagInt;
    char        mTagChar;
  };
  char mTagName;
};

#pragma pack(pop)

typedef void (*IterateTagsCallback)(const ProfileEntry& entry, const char* tagStringData);

class UniqueJSONStrings
{
public:
  UniqueJSONStrings() {
    mStringTableWriter.StartBareList();
  }

  ~UniqueJSONStrings() {
    mStringTableWriter.EndBareList();
  }

  void SpliceStringTableElements(SpliceableJSONWriter& aWriter) const {
    aWriter.Splice(mStringTableWriter.WriteFunc());
  }

  void WriteProperty(mozilla::JSONWriter& aWriter, const char* aName, const char* aStr) {
    aWriter.IntProperty(aName, GetOrAddIndex(aStr));
  }

  void WriteElement(mozilla::JSONWriter& aWriter, const char* aStr) {
    aWriter.IntElement(GetOrAddIndex(aStr));
  }

  uint32_t GetOrAddIndex(const char* aStr);

private:
  SpliceableChunkedJSONWriter mStringTableWriter;
  nsDataHashtable<nsCharPtrHashKey, uint32_t> mStringToIndexMap;
};

class UniqueStacks
{
public:
  struct FrameKey {
    std::string mLocation;
    mozilla::Maybe<unsigned> mLine;
    mozilla::Maybe<unsigned> mCategory;
    mozilla::Maybe<void*> mJITAddress;
    mozilla::Maybe<uint32_t> mJITDepth;

    explicit FrameKey(const char* aLocation)
     : mLocation(aLocation)
    { }

    FrameKey(void* aJITAddress, uint32_t aJITDepth)
     : mJITAddress(mozilla::Some(aJITAddress))
     , mJITDepth(mozilla::Some(aJITDepth))
    { }

    uint32_t Hash() const;
    bool operator==(const FrameKey& aOther) const;
  };

  // A FrameKey that holds a scoped reference to a JIT FrameHandle.
  struct MOZ_STACK_CLASS OnStackFrameKey : public FrameKey {
    const JS::ForEachProfiledFrameOp::FrameHandle* mJITFrameHandle;

    explicit OnStackFrameKey(const char* aLocation)
      : FrameKey(aLocation)
      , mJITFrameHandle(nullptr)
    { }

    OnStackFrameKey(void* aJITAddress, unsigned aJITDepth)
      : FrameKey(aJITAddress, aJITDepth)
      , mJITFrameHandle(nullptr)
    { }

    OnStackFrameKey(void* aJITAddress, unsigned aJITDepth,
                    const JS::ForEachProfiledFrameOp::FrameHandle& aJITFrameHandle)
      : FrameKey(aJITAddress, aJITDepth)
      , mJITFrameHandle(&aJITFrameHandle)
    { }
  };

  struct StackKey {
    mozilla::Maybe<uint32_t> mPrefixHash;
    mozilla::Maybe<uint32_t> mPrefix;
    uint32_t mFrame;

    explicit StackKey(uint32_t aFrame)
     : mFrame(aFrame)
    { }

    uint32_t Hash() const;
    bool operator==(const StackKey& aOther) const;
  };

  class Stack {
  public:
    Stack(UniqueStacks& aUniqueStacks, const OnStackFrameKey& aRoot);

    void AppendFrame(const OnStackFrameKey& aFrame);
    uint32_t GetOrAddIndex() const;

  private:
    UniqueStacks& mUniqueStacks;
    StackKey mStack;
  };

  explicit UniqueStacks(JSRuntime* aRuntime);
  ~UniqueStacks();

  Stack BeginStack(const OnStackFrameKey& aRoot);
  uint32_t LookupJITFrameDepth(void* aAddr);
  void AddJITFrameDepth(void* aAddr, unsigned depth);
  void SpliceFrameTableElements(SpliceableJSONWriter& aWriter) const;
  void SpliceStackTableElements(SpliceableJSONWriter& aWriter) const;

private:
  uint32_t GetOrAddFrameIndex(const OnStackFrameKey& aFrame);
  uint32_t GetOrAddStackIndex(const StackKey& aStack);
  void StreamFrame(const OnStackFrameKey& aFrame);
  void StreamStack(const StackKey& aStack);

public:
  UniqueJSONStrings mUniqueStrings;

private:
  JSRuntime* mRuntime;

  // To avoid incurring JitcodeGlobalTable lookup costs for every JIT frame,
  // we cache the depth of frames keyed by JIT code address. If an address a
  // maps to a depth d, then frames keyed by a for depths 0 to d are
  // guaranteed to be in mFrameToIndexMap.
  nsDataHashtable<nsVoidPtrHashKey, uint32_t> mJITFrameDepthMap;

  uint32_t mFrameCount;
  SpliceableChunkedJSONWriter mFrameTableWriter;
  nsDataHashtable<nsGenericHashKey<FrameKey>, uint32_t> mFrameToIndexMap;

  SpliceableChunkedJSONWriter mStackTableWriter;
  nsDataHashtable<nsGenericHashKey<StackKey>, uint32_t> mStackToIndexMap;
};

class ProfileBuffer {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ProfileBuffer)

  explicit ProfileBuffer(int aEntrySize);

  void addTag(const ProfileEntry& aTag);
  void IterateTagsForThread(IterateTagsCallback aCallback, int aThreadId);
  void StreamSamplesToJSON(SpliceableJSONWriter& aWriter, int aThreadId, float aSinceTime,
                           JSRuntime* rt, UniqueStacks& aUniqueStacks);
  void StreamMarkersToJSON(SpliceableJSONWriter& aWriter, int aThreadId, float aSinceTime,
                           UniqueStacks& aUniqueStacks);
  void DuplicateLastSample(int aThreadId);

  void addStoredMarker(ProfilerMarker* aStoredMarker);

  // The following two methods are not signal safe! They delete markers.
  void deleteExpiredStoredMarkers();
  void reset();

protected:
  char* processDynamicTag(int readPos, int* tagsConsumed, char* tagBuff);
  int FindLastSampleOfThread(int aThreadId);

  ~ProfileBuffer();

public:
  // Circular buffer 'Keep One Slot Open' implementation for simplicity
  mozilla::UniquePtr<ProfileEntry[]> mEntries;

  // Points to the next entry we will write to, which is also the one at which
  // we need to stop reading.
  int mWritePos;

  // Points to the entry at which we can start reading.
  int mReadPos;

  // The number of entries in our buffer.
  int mEntrySize;

  // How many times mWritePos has wrapped around.
  uint32_t mGeneration;

  // Markers that marker entries in the buffer might refer to.
  ProfilerMarkerLinkedList mStoredMarkers;
};

//
// ThreadProfile JSON Format
// -------------------------
//
// The profile contains much duplicate information. The output JSON of the
// profile attempts to deduplicate strings, frames, and stack prefixes, to cut
// down on size and to increase JSON streaming speed. Deduplicated values are
// streamed as indices into their respective tables.
//
// Further, arrays of objects with the same set of properties (e.g., samples,
// frames) are output as arrays according to a schema instead of an object
// with property names. A property that is not present is represented in the
// array as null or undefined.
//
// The format of the thread profile JSON is shown by the following example
// with 1 sample and 1 marker:
//
// {
//   "name": "Foo",
//   "tid": 42,
//   "samples":
//   {
//     "schema":
//     {
//       "stack": 0,           /* index into stackTable */
//       "time": 1,            /* number */
//       "responsiveness": 2,  /* number */
//       "rss": 3,             /* number */
//       "uss": 4,             /* number */
//       "frameNumber": 5,     /* number */
//       "power": 6            /* number */
//     },
//     "data":
//     [
//       [ 1, 0.0, 0.0 ]       /* { stack: 1, time: 0.0, responsiveness: 0.0 } */
//     ]
//   },
//
//   "markers":
//   {
//     "schema":
//     {
//       "name": 0,            /* index into stringTable */
//       "time": 1,            /* number */
//       "data": 2             /* arbitrary JSON */
//     },
//     "data":
//     [
//       [ 3, 0.1 ]            /* { name: 'example marker', time: 0.1 } */
//     ]
//   },
//
//   "stackTable":
//   {
//     "schema":
//     {
//       "prefix": 0,          /* index into stackTable */
//       "frame": 1            /* index into frameTable */
//     },
//     "data":
//     [
//       [ null, 0 ],          /* (root) */
//       [ 0,    1 ]           /* (root) > foo.js */
//     ]
//   },
//
//   "frameTable":
//   {
//     "schema":
//     {
//       "location": 0,        /* index into stringTable */
//       "implementation": 1,  /* index into stringTable */
//       "optimizations": 2,   /* arbitrary JSON */
//       "line": 3,            /* number */
//       "category": 4         /* number */
//     },
//     "data":
//     [
//       [ 0 ],                /* { location: '(root)' } */
//       [ 1, 2 ]              /* { location: 'foo.js', implementation: 'baseline' } */
//     ]
//   },
//
//   "stringTable":
//   [
//     "(root)",
//     "foo.js",
//     "baseline",
//     "example marker"
//   ]
// }
//

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
  void IterateTags(IterateTagsCallback aCallback);
  void ToStreamAsJSON(std::ostream& stream, float aSinceTime = 0);
  JSObject* ToJSObject(JSContext *aCx, float aSinceTime = 0);
  PseudoStack* GetPseudoStack();
  mozilla::Mutex* GetMutex();
  void StreamJSON(SpliceableJSONWriter& aWriter, float aSinceTime = 0);

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
  ThreadResponsiveness* GetThreadResponsiveness() { return &mRespInfo; }
  void SetPendingDelete()
  {
    mPseudoStack = nullptr;
    mPlatformData = nullptr;
  }

  uint32_t bufferGeneration() const {
    return mBuffer->mGeneration;
  }

protected:
  void StreamSamplesAndMarkers(SpliceableJSONWriter& aWriter, float aSinceTime,
                               UniqueStacks& aUniqueStacks);

private:
  FRIEND_TEST(ThreadProfile, InsertOneTag);
  FRIEND_TEST(ThreadProfile, InsertOneTagWithTinyBuffer);
  FRIEND_TEST(ThreadProfile, InsertTagsNoWrap);
  FRIEND_TEST(ThreadProfile, InsertTagsWrap);
  FRIEND_TEST(ThreadProfile, MemoryMeasure);
  ThreadInfo* mThreadInfo;

  const nsRefPtr<ProfileBuffer> mBuffer;

  // JS frames in the buffer may require a live JSRuntime to stream (e.g.,
  // stringifying JIT frames). In the case of JSRuntime destruction,
  // FlushSamplesAndMarkers should be called to save them. These are spliced
  // into the final stream.
  mozilla::UniquePtr<char[]> mSavedStreamedSamples;
  mozilla::UniquePtr<char[]> mSavedStreamedMarkers;
  mozilla::Maybe<UniqueStacks> mUniqueStacks;

  PseudoStack*   mPseudoStack;
  mozilla::Mutex mMutex;
  int            mThreadId;
  bool           mIsMainThread;
  PlatformData*  mPlatformData;  // Platform specific data.
  void* const    mStackTop;
  ThreadResponsiveness mRespInfo;

  // Linux and OSX use a signal sender, instead of stopping the thread, so we
  // need some space to store the data which cannot be collected in the signal
  // handler code.
#if defined(XP_LINUX) || defined(XP_MACOSX)
public:
  int64_t        mRssMemory;
  int64_t        mUssMemory;
#endif
};

#endif /* ndef MOZ_PROFILE_ENTRY_H */
