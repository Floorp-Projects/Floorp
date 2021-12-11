/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfileBufferEntry_h
#define ProfileBufferEntry_h

#include "BaseProfilingCategory.h"
#include "gtest/MozGtestFriend.h"
#include "mozilla/BaseProfileJSONWriter.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/HashTable.h"
#include "mozilla/Maybe.h"
#include "mozilla/ProfileBufferEntryKinds.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Variant.h"
#include "mozilla/Vector.h"

#include <string>

namespace mozilla {
namespace baseprofiler {

class ProfileBufferEntry {
 public:
  using KindUnderlyingType = ::mozilla::ProfileBufferEntryKindUnderlyingType;
  using Kind = ::mozilla::ProfileBufferEntryKind;

  ProfileBufferEntry();

  static constexpr size_t kNumChars = ::mozilla::ProfileBufferEntryNumChars;

 private:
  // aString must be a static string.
  ProfileBufferEntry(Kind aKind, const char* aString);
  ProfileBufferEntry(Kind aKind, char aChars[kNumChars]);
  ProfileBufferEntry(Kind aKind, void* aPtr);
  ProfileBufferEntry(Kind aKind, double aDouble);
  ProfileBufferEntry(Kind aKind, int64_t aInt64);
  ProfileBufferEntry(Kind aKind, uint64_t aUint64);
  ProfileBufferEntry(Kind aKind, int aInt);
  ProfileBufferEntry(Kind aKind, BaseProfilerThreadId aThreadId);

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
  BaseProfilerThreadId GetThreadId() const;
  void CopyCharsInto(char (&aOutArray)[kNumChars]) const;
};

// Packed layout: 1 byte for the tag + 8 bytes for the value.
static_assert(sizeof(ProfileBufferEntry) == 9, "bad ProfileBufferEntry size");

class UniqueStacks {
 public:
  struct FrameKey {
    explicit FrameKey(const char* aLocation)
        : mData(NormalFrameData{std::string(aLocation), false, 0, Nothing(),
                                Nothing()}) {}

    FrameKey(std::string&& aLocation, bool aRelevantForJS,
             uint64_t aInnerWindowID, const Maybe<unsigned>& aLine,
             const Maybe<unsigned>& aColumn,
             const Maybe<ProfilingCategoryPair>& aCategoryPair)
        : mData(NormalFrameData{aLocation, aRelevantForJS, aInnerWindowID,
                                aLine, aColumn, aCategoryPair}) {}

    FrameKey(const FrameKey& aToCopy) = default;

    uint32_t Hash() const;
    bool operator==(const FrameKey& aOther) const {
      return mData == aOther.mData;
    }

    struct NormalFrameData {
      bool operator==(const NormalFrameData& aOther) const;

      std::string mLocation;
      bool mRelevantForJS;
      uint64_t mInnerWindowID;
      Maybe<unsigned> mLine;
      Maybe<unsigned> mColumn;
      Maybe<ProfilingCategoryPair> mCategoryPair;
    };
    Variant<NormalFrameData> mData;
  };

  struct FrameKeyHasher {
    using Lookup = FrameKey;

    static HashNumber hash(const FrameKey& aLookup) {
      HashNumber hash = 0;
      if (aLookup.mData.is<FrameKey::NormalFrameData>()) {
        const FrameKey::NormalFrameData& data =
            aLookup.mData.as<FrameKey::NormalFrameData>();
        if (!data.mLocation.empty()) {
          hash = AddToHash(hash, HashString(data.mLocation.c_str()));
        }
        hash = AddToHash(hash, data.mRelevantForJS);
        hash = mozilla::AddToHash(hash, data.mInnerWindowID);
        if (data.mLine.isSome()) {
          hash = AddToHash(hash, *data.mLine);
        }
        if (data.mColumn.isSome()) {
          hash = AddToHash(hash, *data.mColumn);
        }
        if (data.mCategoryPair.isSome()) {
          hash = AddToHash(hash, static_cast<uint32_t>(*data.mCategoryPair));
        }
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
    Maybe<uint32_t> mPrefixStackIndex;
    uint32_t mFrameIndex;

    explicit StackKey(uint32_t aFrame)
        : mFrameIndex(aFrame), mHash(HashGeneric(aFrame)) {}

    StackKey(const StackKey& aPrefix, uint32_t aPrefixStackIndex,
             uint32_t aFrame)
        : mPrefixStackIndex(Some(aPrefixStackIndex)),
          mFrameIndex(aFrame),
          mHash(AddToHash(aPrefix.mHash, aFrame)) {}

    HashNumber Hash() const { return mHash; }

    bool operator==(const StackKey& aOther) const {
      return mPrefixStackIndex == aOther.mPrefixStackIndex &&
             mFrameIndex == aOther.mFrameIndex;
    }

   private:
    HashNumber mHash;
  };

  struct StackKeyHasher {
    using Lookup = StackKey;

    static HashNumber hash(const StackKey& aLookup) { return aLookup.Hash(); }

    static bool match(const StackKey& aKey, const StackKey& aLookup) {
      return aKey == aLookup;
    }

    static void rekey(StackKey& aKey, const StackKey& aNewKey) {
      aKey = aNewKey;
    }
  };

  UniqueStacks();

  // Return a StackKey for aFrame as the stack's root frame (no prefix).
  [[nodiscard]] StackKey BeginStack(const FrameKey& aFrame);

  // Return a new StackKey that is obtained by appending aFrame to aStack.
  [[nodiscard]] StackKey AppendFrame(const StackKey& aStack,
                                     const FrameKey& aFrame);

  [[nodiscard]] uint32_t GetOrAddFrameIndex(const FrameKey& aFrame);
  [[nodiscard]] uint32_t GetOrAddStackIndex(const StackKey& aStack);

  void SpliceFrameTableElements(SpliceableJSONWriter& aWriter);
  void SpliceStackTableElements(SpliceableJSONWriter& aWriter);

 private:
  void StreamNonJITFrame(const FrameKey& aFrame);
  void StreamStack(const StackKey& aStack);

 public:
  UniquePtr<UniqueJSONStrings> mUniqueStrings;

 private:
  SpliceableChunkedJSONWriter mFrameTableWriter;
  HashMap<FrameKey, uint32_t, FrameKeyHasher> mFrameToIndexMap;

  SpliceableChunkedJSONWriter mStackTableWriter;
  HashMap<StackKey, uint32_t, StackKeyHasher> mStackToIndexMap;
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
//       "eventDelay": 2, /* number */
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
//       [ 1, 2 ]             /* { location: 'foo.js',
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

}  // namespace baseprofiler
}  // namespace mozilla

#endif /* ndef ProfileBufferEntry_h */
