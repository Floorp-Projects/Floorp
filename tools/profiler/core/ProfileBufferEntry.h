/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfileBufferEntry_h
#define ProfileBufferEntry_h

#include <ostream>
#include "GeckoProfiler.h"
#include "platform.h"
#include "ProfileJSONWriter.h"
#include "ProfilerBacktrace.h"
#include "mozilla/RefPtr.h"
#include <string>
#include <map>
#include "js/ProfilingFrameIterator.h"
#include "js/TrackedOptimizationInfo.h"
#include "nsHashKeys.h"
#include "nsDataHashtable.h"
#include "mozilla/Maybe.h"
#include "mozilla/Vector.h"
#include "gtest/MozGtestFriend.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/UniquePtr.h"
#include "nsClassHashtable.h"

class ProfilerMarker;

#define FOR_EACH_PROFILE_BUFFER_ENTRY_KIND(macro) \
  macro(Category,              int) \
  macro(CollectionStart,       double) \
  macro(CollectionEnd,         double) \
  macro(Label,                 const char*) \
  macro(DynamicStringFragment, char*) /* char[kNumChars], really */ \
  macro(JitReturnAddr,         void*) \
  macro(LineNumber,            int) \
  macro(NativeLeafAddr,        void*) \
  macro(Marker,                ProfilerMarker*) \
  macro(Pause,                 double) \
  macro(ResidentMemory,        double) \
  macro(Responsiveness,        double) \
  macro(Resume,                double) \
  macro(ThreadId,              int) \
  macro(Time,                  double) \
  macro(UnsharedMemory,        double)

// NB: Packing this structure has been shown to cause SIGBUS issues on ARM.
#if !defined(GP_ARCH_arm)
#pragma pack(push, 1)
#endif

class ProfileBufferEntry
{
public:
  enum class Kind : uint8_t {
    INVALID = 0,
    #define KIND(k, t) k,
    FOR_EACH_PROFILE_BUFFER_ENTRY_KIND(KIND)
    #undef KIND
    LIMIT
  };

  ProfileBufferEntry();

  // This is equal to sizeof(double), which is the largest non-char variant in
  // |u|.
  static const size_t kNumChars = 8;

private:
  // aString must be a static string.
  ProfileBufferEntry(Kind aKind, const char *aString);
  ProfileBufferEntry(Kind aKind, char aChars[kNumChars]);
  ProfileBufferEntry(Kind aKind, void *aPtr);
  ProfileBufferEntry(Kind aKind, ProfilerMarker *aMarker);
  ProfileBufferEntry(Kind aKind, double aDouble);
  ProfileBufferEntry(Kind aKind, int aInt);

public:
  #define CTOR(k, t) \
    static ProfileBufferEntry k(t aVal) { \
      return ProfileBufferEntry(Kind::k, aVal); \
    }
  FOR_EACH_PROFILE_BUFFER_ENTRY_KIND(CTOR)
  #undef CTOR

  Kind GetKind() const { return mKind; }

  #define IS_KIND(k, t) bool Is##k() const { return mKind == Kind::k; }
  FOR_EACH_PROFILE_BUFFER_ENTRY_KIND(IS_KIND)
  #undef IS_KIND

private:
  FRIEND_TEST(ThreadProfile, InsertOneEntry);
  FRIEND_TEST(ThreadProfile, InsertOneEntryWithTinyBuffer);
  FRIEND_TEST(ThreadProfile, InsertEntriesNoWrap);
  FRIEND_TEST(ThreadProfile, InsertEntriesWrap);
  FRIEND_TEST(ThreadProfile, MemoryMeasure);
  friend class ProfileBuffer;

  Kind mKind;
  union {
    const char*     mString;
    char            mChars[kNumChars];
    void*           mPtr;
    ProfilerMarker* mMarker;
    double          mDouble;
    int             mInt;
  } u;
};

#if !defined(GP_ARCH_arm)
// Packed layout: 1 byte for the tag + 8 bytes for the value.
static_assert(sizeof(ProfileBufferEntry) == 9, "bad ProfileBufferEntry size");
#pragma pack(pop)
#endif

class UniqueJSONStrings
{
public:
  UniqueJSONStrings() {
    mStringTableWriter.StartBareList();
  }

  void SpliceStringTableElements(SpliceableJSONWriter& aWriter) {
    aWriter.TakeAndSplice(mStringTableWriter.WriteFunc());
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
  nsDataHashtable<nsCStringHashKey, uint32_t> mStringToIndexMap;
};

class UniqueStacks
{
public:
  // We de-duplicate information about JIT frames based on the return address
  // of the frame. However, if the same UniqueStacks object is used to stream
  // profiler buffer contents more than once, then in the time between the two
  // stream attempts ("streaming generations"), JIT code can have been freed
  // and reallocated in the same areas of memory. Consequently, during the next
  // streaming generation, we may see JIT return addresses that we've already
  // seen before, but which now represent completely different JS functions.
  // So we need to make sure that any de-duplication takes the streaming
  // generation into account and does not only compare the address itself.
  // The JITAddress struct packages the two together and can be used as a hash
  // key.
  struct JITAddress
  {
    void* mAddress;
    uint32_t mStreamingGen;

    uint32_t Hash() const;
    bool operator==(const JITAddress& aRhs) const
    {
      return mAddress == aRhs.mAddress && mStreamingGen == aRhs.mStreamingGen;
    }
    bool operator!=(const JITAddress& aRhs) const { return !(*this == aRhs); }
  };

  struct FrameKey {
    // This cannot be a std::string, as it is not memmove compatible, which
    // is used by nsHashTable
    nsCString mLocation;
    mozilla::Maybe<unsigned> mLine;
    mozilla::Maybe<unsigned> mCategory;
    mozilla::Maybe<JITAddress> mJITAddress;
    mozilla::Maybe<uint32_t> mJITDepth;

    explicit FrameKey(const char* aLocation)
     : mLocation(aLocation)
    {
      mHash = Hash();
    }

    FrameKey(const FrameKey& aToCopy)
     : mLocation(aToCopy.mLocation)
     , mLine(aToCopy.mLine)
     , mCategory(aToCopy.mCategory)
     , mJITAddress(aToCopy.mJITAddress)
     , mJITDepth(aToCopy.mJITDepth)
    {
      mHash = Hash();
    }

    FrameKey(const JITAddress& aJITAddress, uint32_t aJITDepth)
     : mJITAddress(mozilla::Some(aJITAddress))
     , mJITDepth(mozilla::Some(aJITDepth))
    {
      mHash = Hash();
    }

    uint32_t Hash() const;
    bool operator==(const FrameKey& aOther) const;
    bool operator<(const FrameKey& aOther) const {
      return mHash < aOther.mHash;
    }

  private:
    uint32_t mHash;
  };

  struct StackKey {
    mozilla::Maybe<uint32_t> mPrefixStackIndex;
    uint32_t mFrameIndex;

    explicit StackKey(uint32_t aFrame)
     : mFrameIndex(aFrame)
     , mHash(mozilla::HashGeneric(aFrame))
    {}

    StackKey(const StackKey& aPrefix, uint32_t aPrefixStackIndex, uint32_t aFrame)
      : mPrefixStackIndex(mozilla::Some(aPrefixStackIndex))
      , mFrameIndex(aFrame)
      , mHash(mozilla::AddToHash(aPrefix.mHash, aFrame))
    {}

    uint32_t Hash() const { return mHash; }

    bool operator==(const StackKey& aOther) const
    {
      return mPrefixStackIndex == aOther.mPrefixStackIndex &&
             mFrameIndex == aOther.mFrameIndex;
    }
    bool operator<(const StackKey& aOther) const
    {
      return mHash < aOther.mHash;
    }

  private:
    uint32_t mHash;
  };

  explicit UniqueStacks();

  // Needs to be called when using a UniqueStacks object again after having
  // streamed entries that are no longer in the buffer (and which could have
  // been GC'ed and their memory reused).
  void AdvanceStreamingGeneration() { mStreamingGeneration++; }
  uint32_t CurrentGen() { return mStreamingGeneration; }

  // Return a StackKey for aFrame as the stack's root frame (no prefix).
  MOZ_MUST_USE StackKey BeginStack(const FrameKey& aFrame);

  // Return a new StackKey that is obtained by appending aFrame to aStack.
  MOZ_MUST_USE StackKey AppendFrame(const StackKey& aStack,
                                    const FrameKey& aFrame);

  MOZ_MUST_USE nsTArray<FrameKey>
  GetOrAddJITFrameKeysForAddress(JSContext* aContext,
                                 const JITAddress& aJITAddress);

  MOZ_MUST_USE uint32_t GetOrAddFrameIndex(const FrameKey& aFrame);
  MOZ_MUST_USE uint32_t GetOrAddStackIndex(const StackKey& aStack);

  void SpliceFrameTableElements(SpliceableJSONWriter& aWriter);
  void SpliceStackTableElements(SpliceableJSONWriter& aWriter);

private:
  // Make sure that there exists a frame index for aFrame, and if there isn't
  // one already, create one and call StreamJITFrame for the frame.
  void MaybeAddJITFrameIndex(JSContext* aContext,
                             const FrameKey& aFrame,
                             const JS::ProfiledFrameHandle& aJITFrame);

  void StreamNonJITFrame(const FrameKey& aFrame);
  void StreamJITFrame(JSContext* aContext,
                      const JS::ProfiledFrameHandle& aJITFrame);
  void StreamStack(const StackKey& aStack);

public:
  UniqueJSONStrings mUniqueStrings;

private:
  // To avoid incurring JitcodeGlobalTable lookup costs for every JIT frame,
  // we cache the frame keys of frames keyed by JIT code address. All FrameKeys
  // in mAddressToJITFrameKeysMap are guaranteed to be in mFrameToIndexMap.
  nsClassHashtable<nsGenericHashKey<JITAddress>, nsTArray<FrameKey>> mAddressToJITFrameKeysMap;

  SpliceableChunkedJSONWriter mFrameTableWriter;
  nsDataHashtable<nsGenericHashKey<FrameKey>, uint32_t> mFrameToIndexMap;

  SpliceableChunkedJSONWriter mStackTableWriter;
  nsDataHashtable<nsGenericHashKey<StackKey>, uint32_t> mStackToIndexMap;

  // Used to avoid collisions between JITAddresses that refer to different
  // frames.
  uint32_t mStreamingGeneration;
};

//
// Thread profile JSON Format
// --------------------------
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
//       "uss": 4              /* number */
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

#endif /* ndef ProfileBufferEntry_h */
