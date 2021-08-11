/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfileBufferEntry_h
#define ProfileBufferEntry_h

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <utility>
#include "gtest/MozGtestFriend.h"
#include "js/ProfilingCategory.h"
#include "mozilla/Attributes.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/HashTable.h"
#include "mozilla/Maybe.h"
#include "mozilla/ProfileBufferEntryKinds.h"
#include "mozilla/ProfileJSONWriter.h"
#include "mozilla/ProfilerUtils.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Variant.h"
#include "mozilla/Vector.h"
#include "nsString.h"

class ProfilerCodeAddressService;
struct JSContext;

class ProfileBufferEntry {
 public:
  using KindUnderlyingType = mozilla::ProfileBufferEntryKindUnderlyingType;
  using Kind = mozilla::ProfileBufferEntryKind;

  ProfileBufferEntry();

  static constexpr size_t kNumChars = mozilla::ProfileBufferEntryNumChars;

 private:
  // aString must be a static string.
  ProfileBufferEntry(Kind aKind, const char* aString);
  ProfileBufferEntry(Kind aKind, char aChars[kNumChars]);
  ProfileBufferEntry(Kind aKind, void* aPtr);
  ProfileBufferEntry(Kind aKind, double aDouble);
  ProfileBufferEntry(Kind aKind, int64_t aInt64);
  ProfileBufferEntry(Kind aKind, uint64_t aUint64);
  ProfileBufferEntry(Kind aKind, int aInt);
  ProfileBufferEntry(Kind aKind, ProfilerThreadId aThreadId);

 public:
#define CTOR(KIND, TYPE, SIZE)                   \
  static ProfileBufferEntry KIND(TYPE aVal) {    \
    return ProfileBufferEntry(Kind::KIND, aVal); \
  }
  FOR_EACH_PROFILE_BUFFER_ENTRY_KIND(CTOR)
#undef CTOR

  Kind GetKind() const { return mKind; }

#define IS_KIND(KIND, TYPE, SIZE) \
  bool Is##KIND() const { return mKind == Kind::KIND; }
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
  uint8_t mStorage[kNumChars];

  const char* GetString() const;
  void* GetPtr() const;
  double GetDouble() const;
  int GetInt() const;
  int64_t GetInt64() const;
  uint64_t GetUint64() const;
  ProfilerThreadId GetThreadId() const;
  void CopyCharsInto(char (&aOutArray)[kNumChars]) const;
};

// Packed layout: 1 byte for the tag + 8 bytes for the value.
static_assert(sizeof(ProfileBufferEntry) == 9, "bad ProfileBufferEntry size");

// Contains all the information about JIT frames that is needed to stream stack
// frames for JitReturnAddr entries in the profiler buffer.
// Every return address (void*) is mapped to one or more JITFrameKeys, and
// every JITFrameKey is mapped to a JSON string for that frame.
// mRangeStart and mRangeEnd describe the range in the buffer for which this
// mapping is valid. Only JitReturnAddr entries within that buffer range can be
// processed using this JITFrameInfoForBufferRange object.
struct JITFrameInfoForBufferRange final {
  JITFrameInfoForBufferRange Clone() const;

  uint64_t mRangeStart;
  uint64_t mRangeEnd;  // mRangeEnd marks the first invalid index.

  struct JITFrameKey {
    bool operator==(const JITFrameKey& aOther) const {
      return mCanonicalAddress == aOther.mCanonicalAddress &&
             mDepth == aOther.mDepth;
    }
    bool operator!=(const JITFrameKey& aOther) const {
      return !(*this == aOther);
    }

    void* mCanonicalAddress;
    uint32_t mDepth;
  };
  struct JITFrameKeyHasher {
    using Lookup = JITFrameKey;

    static mozilla::HashNumber hash(const JITFrameKey& aLookup) {
      mozilla::HashNumber hash = 0;
      hash = mozilla::AddToHash(hash, aLookup.mCanonicalAddress);
      hash = mozilla::AddToHash(hash, aLookup.mDepth);
      return hash;
    }

    static bool match(const JITFrameKey& aKey, const JITFrameKey& aLookup) {
      return aKey == aLookup;
    }

    static void rekey(JITFrameKey& aKey, const JITFrameKey& aNewKey) {
      aKey = aNewKey;
    }
  };

  using JITAddressToJITFramesMap =
      mozilla::HashMap<void*, mozilla::Vector<JITFrameKey>>;
  JITAddressToJITFramesMap mJITAddressToJITFramesMap;
  using JITFrameToFrameJSONMap =
      mozilla::HashMap<JITFrameKey, nsCString, JITFrameKeyHasher>;
  JITFrameToFrameJSONMap mJITFrameToFrameJSONMap;
};

// Contains JITFrameInfoForBufferRange objects for multiple profiler buffer
// ranges.
struct JITFrameInfo final {
  JITFrameInfo() : mUniqueStrings(mozilla::MakeUnique<UniqueJSONStrings>()) {}

  MOZ_IMPLICIT JITFrameInfo(const JITFrameInfo& aOther);

  // Creates a new JITFrameInfoForBufferRange object in mRanges by looking up
  // information about the provided JIT return addresses using aCx.
  // Addresses are provided like this:
  // The caller of AddInfoForRange supplies a function in aJITAddressProvider.
  // This function will be called once, synchronously, with an
  // aJITAddressConsumer argument, which is a function that needs to be called
  // for every address. That function can be called multiple times for the same
  // address.
  void AddInfoForRange(
      uint64_t aRangeStart, uint64_t aRangeEnd, JSContext* aCx,
      const std::function<void(const std::function<void(void*)>&)>&
          aJITAddressProvider);

  // Returns whether the information stored in this object is still relevant
  // for any entries in the buffer.
  bool HasExpired(uint64_t aCurrentBufferRangeStart) const {
    if (mRanges.empty()) {
      // No information means no relevant information. Allow this object to be
      // discarded.
      return true;
    }
    return mRanges.back().mRangeEnd <= aCurrentBufferRangeStart;
  }

  // The array of ranges of JIT frame information, sorted by buffer position.
  // Ranges are non-overlapping.
  // The JSON of the cached frames can contain string indexes, which refer
  // to strings in mUniqueStrings.
  mozilla::Vector<JITFrameInfoForBufferRange> mRanges;

  // The string table which contains strings used in the frame JSON that's
  // cached in mRanges.
  mozilla::UniquePtr<UniqueJSONStrings> mUniqueStrings;
};

class UniqueStacks {
 public:
  struct FrameKey {
    explicit FrameKey(const char* aLocation)
        : mData(NormalFrameData{nsCString(aLocation), false, false, 0,
                                mozilla::Nothing(), mozilla::Nothing()}) {}

    FrameKey(nsCString&& aLocation, bool aRelevantForJS, bool aBaselineInterp,
             uint64_t aInnerWindowID, const mozilla::Maybe<unsigned>& aLine,
             const mozilla::Maybe<unsigned>& aColumn,
             const mozilla::Maybe<JS::ProfilingCategoryPair>& aCategoryPair)
        : mData(NormalFrameData{aLocation, aRelevantForJS, aBaselineInterp,
                                aInnerWindowID, aLine, aColumn,
                                aCategoryPair}) {}

    FrameKey(void* aJITAddress, uint32_t aJITDepth, uint32_t aRangeIndex)
        : mData(JITFrameData{aJITAddress, aJITDepth, aRangeIndex}) {}

    FrameKey(const FrameKey& aToCopy) = default;

    uint32_t Hash() const;
    bool operator==(const FrameKey& aOther) const {
      return mData == aOther.mData;
    }

    struct NormalFrameData {
      bool operator==(const NormalFrameData& aOther) const;

      nsCString mLocation;
      bool mRelevantForJS;
      bool mBaselineInterp;
      uint64_t mInnerWindowID;
      mozilla::Maybe<unsigned> mLine;
      mozilla::Maybe<unsigned> mColumn;
      mozilla::Maybe<JS::ProfilingCategoryPair> mCategoryPair;
    };
    struct JITFrameData {
      bool operator==(const JITFrameData& aOther) const;

      void* mCanonicalAddress;
      uint32_t mDepth;
      uint32_t mRangeIndex;
    };
    mozilla::Variant<NormalFrameData, JITFrameData> mData;
  };

  struct FrameKeyHasher {
    using Lookup = FrameKey;

    static mozilla::HashNumber hash(const FrameKey& aLookup) {
      mozilla::HashNumber hash = 0;
      if (aLookup.mData.is<FrameKey::NormalFrameData>()) {
        const FrameKey::NormalFrameData& data =
            aLookup.mData.as<FrameKey::NormalFrameData>();
        if (!data.mLocation.IsEmpty()) {
          hash = mozilla::AddToHash(hash,
                                    mozilla::HashString(data.mLocation.get()));
        }
        hash = mozilla::AddToHash(hash, data.mRelevantForJS);
        hash = mozilla::AddToHash(hash, data.mBaselineInterp);
        hash = mozilla::AddToHash(hash, data.mInnerWindowID);
        if (data.mLine.isSome()) {
          hash = mozilla::AddToHash(hash, *data.mLine);
        }
        if (data.mColumn.isSome()) {
          hash = mozilla::AddToHash(hash, *data.mColumn);
        }
        if (data.mCategoryPair.isSome()) {
          hash = mozilla::AddToHash(hash,
                                    static_cast<uint32_t>(*data.mCategoryPair));
        }
      } else {
        const FrameKey::JITFrameData& data =
            aLookup.mData.as<FrameKey::JITFrameData>();
        hash = mozilla::AddToHash(hash, data.mCanonicalAddress);
        hash = mozilla::AddToHash(hash, data.mDepth);
        hash = mozilla::AddToHash(hash, data.mRangeIndex);
      }
      return hash;
    }

    static bool match(const FrameKey& aKey, const FrameKey& aLookup) {
      return aKey == aLookup;
    }

    static void rekey(FrameKey& aKey, const FrameKey& aNewKey) {
      aKey = aNewKey;
    }
  };

  struct StackKey {
    mozilla::Maybe<uint32_t> mPrefixStackIndex;
    uint32_t mFrameIndex;

    explicit StackKey(uint32_t aFrame)
        : mFrameIndex(aFrame), mHash(mozilla::HashGeneric(aFrame)) {}

    StackKey(const StackKey& aPrefix, uint32_t aPrefixStackIndex,
             uint32_t aFrame)
        : mPrefixStackIndex(mozilla::Some(aPrefixStackIndex)),
          mFrameIndex(aFrame),
          mHash(mozilla::AddToHash(aPrefix.mHash, aFrame)) {}

    mozilla::HashNumber Hash() const { return mHash; }

    bool operator==(const StackKey& aOther) const {
      return mPrefixStackIndex == aOther.mPrefixStackIndex &&
             mFrameIndex == aOther.mFrameIndex;
    }

   private:
    mozilla::HashNumber mHash;
  };

  struct StackKeyHasher {
    using Lookup = StackKey;

    static mozilla::HashNumber hash(const StackKey& aLookup) {
      return aLookup.Hash();
    }

    static bool match(const StackKey& aKey, const StackKey& aLookup) {
      return aKey == aLookup;
    }

    static void rekey(StackKey& aKey, const StackKey& aNewKey) {
      aKey = aNewKey;
    }
  };

  explicit UniqueStacks(JITFrameInfo&& aJITFrameInfo);

  // Return a StackKey for aFrame as the stack's root frame (no prefix).
  [[nodiscard]] StackKey BeginStack(const FrameKey& aFrame);

  // Return a new StackKey that is obtained by appending aFrame to aStack.
  [[nodiscard]] StackKey AppendFrame(const StackKey& aStack,
                                     const FrameKey& aFrame);

  // Look up frame keys for the given JIT address, and ensure that our frame
  // table has entries for the returned frame keys. The JSON for these frames
  // is taken from mJITInfoRanges.
  // aBufferPosition is needed in order to look up the correct JIT frame info
  // object in mJITInfoRanges.
  [[nodiscard]] mozilla::Maybe<mozilla::Vector<UniqueStacks::FrameKey>>
  LookupFramesForJITAddressFromBufferPos(void* aJITAddress,
                                         uint64_t aBufferPosition);

  [[nodiscard]] uint32_t GetOrAddFrameIndex(const FrameKey& aFrame);
  [[nodiscard]] uint32_t GetOrAddStackIndex(const StackKey& aStack);

  void SpliceFrameTableElements(SpliceableJSONWriter& aWriter);
  void SpliceStackTableElements(SpliceableJSONWriter& aWriter);

 private:
  void StreamNonJITFrame(const FrameKey& aFrame);
  void StreamStack(const StackKey& aStack);

 public:
  mozilla::UniquePtr<UniqueJSONStrings> mUniqueStrings;

  ProfilerCodeAddressService* mCodeAddressService = nullptr;

 private:
  SpliceableChunkedJSONWriter mFrameTableWriter;
  mozilla::HashMap<FrameKey, uint32_t, FrameKeyHasher> mFrameToIndexMap;

  SpliceableChunkedJSONWriter mStackTableWriter;
  mozilla::HashMap<StackKey, uint32_t, StackKeyHasher> mStackToIndexMap;

  mozilla::Vector<JITFrameInfoForBufferRange> mJITInfoRanges;
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
//       "stack": 0,          /* index into stackTable */
//       "time": 1,           /* number */
//       "eventDelay": 2,     /* number */
//       "ThreadCPUDelta": 3, /* optional number */
//     },
//     "data":
//     [
//       [ 1, 0.0, 0.0 ]      /* { stack: 1, time: 0.0, eventDelay: 0.0 } */
//     ]
//   },
//
//   "markers":
//   {
//     "schema":
//     {
//       "name": 0,           /* index into stringTable */
//       "time": 1,           /* number */
//       "data": 2            /* arbitrary JSON */
//     },
//     "data":
//     [
//       [ 3, 0.1 ]           /* { name: 'example marker', time: 0.1 } */
//     ]
//   },
//
//   "stackTable":
//   {
//     "schema":
//     {
//       "prefix": 0,         /* index into stackTable */
//       "frame": 1           /* index into frameTable */
//     },
//     "data":
//     [
//       [ null, 0 ],         /* (root) */
//       [ 0,    1 ]          /* (root) > foo.js */
//     ]
//   },
//
//   "frameTable":
//   {
//     "schema":
//     {
//       "location": 0,       /* index into stringTable */
//       "relevantForJS": 1,  /* bool */
//       "innerWindowID": 2,  /* inner window ID of global JS `window` object */
//       "implementation": 3, /* index into stringTable */
//       "optimizations": 4,  /* arbitrary JSON */
//       "line": 5,           /* number */
//       "column": 6,         /* number */
//       "category": 7,       /* index into profile.meta.categories */
//       "subcategory": 8     /* index into
//       profile.meta.categories[category].subcategories */
//     },
//     "data":
//     [
//       [ 0 ],               /* { location: '(root)' } */
//       [ 1, null, null, 2 ] /* { location: 'foo.js',
//                                 implementation: 'baseline' } */
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
// Process:
// {
//   "name": "Bar",
//   "pid": 24,
//   "threads":
//   [
//     <0-N threads from above>
//   ],
//   "counters": /* includes the memory counter */
//   [
//     {
//       "name": "qwerty",
//       "category": "uiop",
//       "description": "this is qwerty uiop",
//       "sample_groups:
//       [
//         {
//           "id": 42, /* number (thread id, or object identifier (tab), etc) */
//           "samples:
//           {
//             "schema":
//             {
//               "time": 1,   /* number */
//               "number": 2, /* number (of times the counter was touched) */
//               "count": 3   /* number (total for the counter) */
//             },
//             "data":
//             [
//               [ 0.1, 1824,
//                 454622 ]   /* { time: 0.1, number: 1824, count: 454622 } */
//             ]
//           },
//         },
//         /* more sample-group objects with different id's */
//       ]
//     },
//     /* more counters */
//   ],
// }
//
#endif /* ndef ProfileBufferEntry_h */
