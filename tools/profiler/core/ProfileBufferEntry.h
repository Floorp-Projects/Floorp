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

#define PROFILE_BUFFER_ENTRY_KIND_LIST(_) \
    _(Category,        int)               \
    _(CodeLocation,    const char *)      \
    _(EmbeddedString,  void *)            \
    _(JitReturnAddr,   void *)            \
    _(LineNumber,      int)               \
    _(NativeLeafAddr,  void *)            \
    _(Marker,          ProfilerMarker *)  \
    _(ResidentMemory,  double)            \
    _(Responsiveness,  double)            \
    _(Sample,          const char *)      \
    _(ThreadId,        int)               \
    _(Time,            double)            \
    _(UnsharedMemory,  double)

// NB: Packing this structure has been shown to cause SIGBUS issues on ARM.
#if !defined(GP_ARCH_arm)
#pragma pack(push, 1)
#endif

class ProfileBufferEntry
{
public:
  enum class Kind : uint8_t {
    INVALID = 0,
#   define DEF_ENUM_(k, t) k,
    PROFILE_BUFFER_ENTRY_KIND_LIST(DEF_ENUM_)
#   undef DEF_ENUM_
    LIMIT
  };

  ProfileBufferEntry();

private:
  // aTagData must not need release (i.e. be a string from the text segment)
  ProfileBufferEntry(Kind aKind, const char *aTagData);
  ProfileBufferEntry(Kind aKind, void *aTagPtr);
  ProfileBufferEntry(Kind aKind, ProfilerMarker *aTagMarker);
  ProfileBufferEntry(Kind aKind, double aTagDouble);
  ProfileBufferEntry(Kind aKind, uintptr_t aTagOffset);
  ProfileBufferEntry(Kind aKind, Address aTagAddress);
  ProfileBufferEntry(Kind aKind, int aTagLine);
  ProfileBufferEntry(Kind aKind, char aTagChar);

public:
# define DEF_MAKE_(k, t) \
    static ProfileBufferEntry k(t val) { \
      return ProfileBufferEntry(Kind::k, val); \
    }
  PROFILE_BUFFER_ENTRY_KIND_LIST(DEF_MAKE_)
# undef DEF_MAKE_

  Kind kind() const { return mKind; }
  bool hasKind(Kind k) const { return kind() == k; }

# define DEF_METHODS_(k, t) \
    bool is##k() const { return hasKind(Kind::k); }
  PROFILE_BUFFER_ENTRY_KIND_LIST(DEF_METHODS_)
# undef DEF_METHODS_

  const ProfilerMarker* getMarker() {
    MOZ_ASSERT(isMarker());
    return mTagMarker;
  }

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
    double      mTagDouble;
    Address     mTagAddress;
    uintptr_t   mTagOffset;
    int         mTagInt;
    char        mTagChar;
  };
  Kind mKind;
};

#if !defined(GP_ARCH_arm)
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

  struct StringKey {

    explicit StringKey(const char* aStr)
     : mStr(strdup(aStr))
    {
      mHash = mozilla::HashString(mStr);
    }

    StringKey(const StringKey& aOther)
      : mStr(strdup(aOther.mStr))
    {
      mHash = aOther.mHash;
    }

    ~StringKey() {
      free(mStr);
    }

    uint32_t Hash() const;
    bool operator==(const StringKey& aOther) const {
      return strcmp(mStr, aOther.mStr) == 0;
    }
    bool operator<(const StringKey& aOther) const {
      return mHash < aOther.mHash;
    }

  private:
    uint32_t mHash;
    char* mStr;
  };
private:
  SpliceableChunkedJSONWriter mStringTableWriter;
  std::map<StringKey, uint32_t> mStringToIndexMap;
};

class UniqueStacks
{
public:
  struct FrameKey {
    // This cannot be a std::string, as it is not memmove compatible, which
    // is used by nsHashTable
    nsCString mLocation;
    mozilla::Maybe<unsigned> mLine;
    mozilla::Maybe<unsigned> mCategory;
    mozilla::Maybe<void*> mJITAddress;
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

    FrameKey(void* aJITAddress, uint32_t aJITDepth)
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

  // A FrameKey that holds a scoped reference to a JIT FrameHandle.
  struct MOZ_STACK_CLASS OnStackFrameKey : public FrameKey {
    explicit OnStackFrameKey(const char* aLocation)
      : FrameKey(aLocation)
      , mJITFrameHandle(nullptr)
    { }

    OnStackFrameKey(const OnStackFrameKey& aToCopy)
      : FrameKey(aToCopy)
      , mJITFrameHandle(aToCopy.mJITFrameHandle)
    { }

    const JS::ForEachProfiledFrameOp::FrameHandle* mJITFrameHandle;

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
    {
      mHash = Hash();
    }

    uint32_t Hash() const;
    bool operator==(const StackKey& aOther) const;
    bool operator<(const StackKey& aOther) const {
      return mHash < aOther.mHash;
    }

    void UpdateHash(uint32_t aPrefixHash, uint32_t aPrefix, uint32_t aFrame) {
      mPrefixHash = mozilla::Some(aPrefixHash);
      mPrefix = mozilla::Some(aPrefix);
      mFrame = aFrame;
      mHash = Hash();
    }

  private:
    uint32_t mHash;
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

  explicit UniqueStacks(JSContext* aContext);

  Stack BeginStack(const OnStackFrameKey& aRoot);
  uint32_t LookupJITFrameDepth(void* aAddr);
  void AddJITFrameDepth(void* aAddr, unsigned depth);
  void SpliceFrameTableElements(SpliceableJSONWriter& aWriter);
  void SpliceStackTableElements(SpliceableJSONWriter& aWriter);

private:
  uint32_t GetOrAddFrameIndex(const OnStackFrameKey& aFrame);
  uint32_t GetOrAddStackIndex(const StackKey& aStack);
  void StreamFrame(const OnStackFrameKey& aFrame);
  void StreamStack(const StackKey& aStack);

public:
  UniqueJSONStrings mUniqueStrings;

private:
  JSContext* mContext;

  // To avoid incurring JitcodeGlobalTable lookup costs for every JIT frame,
  // we cache the depth of frames keyed by JIT code address. If an address a
  // maps to a depth d, then frames keyed by a for depths 0 to d are
  // guaranteed to be in mFrameToIndexMap.
  std::map<void*, uint32_t> mJITFrameDepthMap;

  uint32_t mFrameCount;
  SpliceableChunkedJSONWriter mFrameTableWriter;
  nsDataHashtable<nsGenericHashKey<FrameKey>, uint32_t> mFrameToIndexMap;

  SpliceableChunkedJSONWriter mStackTableWriter;

  nsDataHashtable<nsGenericHashKey<StackKey>, uint32_t> mStackToIndexMap;
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
