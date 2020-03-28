/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfileBufferEntry.h"

#include "platform.h"
#include "ProfileBuffer.h"
#include "ProfilerMarkerPayload.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "mozilla/Logging.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StackWalk.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "ProfilerCodeAddressService.h"

#include <ostream>
#include <type_traits>

using namespace mozilla;

////////////////////////////////////////////////////////////////////////
// BEGIN ProfileBufferEntry

ProfileBufferEntry::ProfileBufferEntry()
    : mKind(Kind::INVALID), mStorage{0, 0, 0, 0, 0, 0, 0, 0} {}

// aString must be a static string.
ProfileBufferEntry::ProfileBufferEntry(Kind aKind, const char* aString)
    : mKind(aKind) {
  MOZ_ASSERT(aKind == Kind::Label);
  memcpy(mStorage, &aString, sizeof(aString));
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, char aChars[kNumChars])
    : mKind(aKind) {
  MOZ_ASSERT(aKind == Kind::DynamicStringFragment);
  memcpy(mStorage, aChars, kNumChars);
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, void* aPtr) : mKind(aKind) {
  memcpy(mStorage, &aPtr, sizeof(aPtr));
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, double aDouble)
    : mKind(aKind) {
  memcpy(mStorage, &aDouble, sizeof(aDouble));
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, int aInt) : mKind(aKind) {
  memcpy(mStorage, &aInt, sizeof(aInt));
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, int64_t aInt64)
    : mKind(aKind) {
  memcpy(mStorage, &aInt64, sizeof(aInt64));
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, uint64_t aUint64)
    : mKind(aKind) {
  memcpy(mStorage, &aUint64, sizeof(aUint64));
}

const char* ProfileBufferEntry::GetString() const {
  const char* result;
  memcpy(&result, mStorage, sizeof(result));
  return result;
}

void* ProfileBufferEntry::GetPtr() const {
  void* result;
  memcpy(&result, mStorage, sizeof(result));
  return result;
}

double ProfileBufferEntry::GetDouble() const {
  double result;
  memcpy(&result, mStorage, sizeof(result));
  return result;
}

int ProfileBufferEntry::GetInt() const {
  int result;
  memcpy(&result, mStorage, sizeof(result));
  return result;
}

int64_t ProfileBufferEntry::GetInt64() const {
  int64_t result;
  memcpy(&result, mStorage, sizeof(result));
  return result;
}

uint64_t ProfileBufferEntry::GetUint64() const {
  uint64_t result;
  memcpy(&result, mStorage, sizeof(result));
  return result;
}

void ProfileBufferEntry::CopyCharsInto(char (&aOutArray)[kNumChars]) const {
  memcpy(aOutArray, mStorage, kNumChars);
}

// END ProfileBufferEntry
////////////////////////////////////////////////////////////////////////

struct TypeInfo {
  Maybe<nsCString> mKeyedBy;
  Maybe<nsCString> mName;
  Maybe<nsCString> mLocation;
  Maybe<unsigned> mLineNumber;
};

// As mentioned in ProfileBufferEntry.h, the JSON format contains many
// arrays whose elements are laid out according to various schemas to help
// de-duplication. This RAII class helps write these arrays by keeping track of
// the last non-null element written and adding the appropriate number of null
// elements when writing new non-null elements. It also automatically opens and
// closes an array element on the given JSON writer.
//
// You grant the AutoArraySchemaWriter exclusive access to the JSONWriter and
// the UniqueJSONStrings objects for the lifetime of AutoArraySchemaWriter. Do
// not access them independently while the AutoArraySchemaWriter is alive.
// If you need to add complex objects, call FreeFormElement(), which will give
// you temporary access to the writer.
//
// Example usage:
//
//     // Define the schema of elements in this type of array: [FOO, BAR, BAZ]
//     enum Schema : uint32_t {
//       FOO = 0,
//       BAR = 1,
//       BAZ = 2
//     };
//
//     AutoArraySchemaWriter writer(someJsonWriter, someUniqueStrings);
//     if (shouldWriteFoo) {
//       writer.IntElement(FOO, getFoo());
//     }
//     ... etc ...
//
//     The elements need to be added in-order.
class MOZ_RAII AutoArraySchemaWriter {
 public:
  AutoArraySchemaWriter(SpliceableJSONWriter& aWriter,
                        UniqueJSONStrings& aStrings)
      : mJSONWriter(aWriter), mStrings(&aStrings), mNextFreeIndex(0) {
    mJSONWriter.StartArrayElement(SpliceableJSONWriter::SingleLineStyle);
  }

  explicit AutoArraySchemaWriter(SpliceableJSONWriter& aWriter)
      : mJSONWriter(aWriter), mStrings(nullptr), mNextFreeIndex(0) {
    mJSONWriter.StartArrayElement(SpliceableJSONWriter::SingleLineStyle);
  }

  ~AutoArraySchemaWriter() { mJSONWriter.EndArray(); }

  template <typename T>
  void IntElement(uint32_t aIndex, T aValue) {
    static_assert(!std::is_same_v<T, uint64_t>,
                  "Narrowing uint64 -> int64 conversion not allowed");
    FillUpTo(aIndex);
    mJSONWriter.IntElement(static_cast<int64_t>(aValue));
  }

  void DoubleElement(uint32_t aIndex, double aValue) {
    FillUpTo(aIndex);
    mJSONWriter.DoubleElement(aValue);
  }

  void BoolElement(uint32_t aIndex, bool aValue) {
    FillUpTo(aIndex);
    mJSONWriter.BoolElement(aValue);
  }

  void StringElement(uint32_t aIndex, const char* aValue) {
    MOZ_RELEASE_ASSERT(mStrings);
    FillUpTo(aIndex);
    mStrings->WriteElement(mJSONWriter, aValue);
  }

  // Write an element using a callback that takes a JSONWriter& and a
  // UniqueJSONStrings&.
  template <typename LambdaT>
  void FreeFormElement(uint32_t aIndex, LambdaT aCallback) {
    MOZ_RELEASE_ASSERT(mStrings);
    FillUpTo(aIndex);
    aCallback(mJSONWriter, *mStrings);
  }

 private:
  void FillUpTo(uint32_t aIndex) {
    MOZ_ASSERT(aIndex >= mNextFreeIndex);
    mJSONWriter.NullElements(aIndex - mNextFreeIndex);
    mNextFreeIndex = aIndex + 1;
  }

  SpliceableJSONWriter& mJSONWriter;
  UniqueJSONStrings* mStrings;
  uint32_t mNextFreeIndex;
};

UniqueJSONStrings::UniqueJSONStrings() { mStringTableWriter.StartBareList(); }

UniqueJSONStrings::UniqueJSONStrings(const UniqueJSONStrings& aOther) {
  mStringTableWriter.StartBareList();
  uint32_t count = mStringHashToIndexMap.count();
  if (count != 0) {
    MOZ_RELEASE_ASSERT(mStringHashToIndexMap.reserve(count));
    for (auto iter = aOther.mStringHashToIndexMap.iter(); !iter.done();
         iter.next()) {
      mStringHashToIndexMap.putNewInfallible(iter.get().key(),
                                             iter.get().value());
    }
    UniquePtr<char[]> stringTableJSON =
        aOther.mStringTableWriter.WriteFunc()->CopyData();
    mStringTableWriter.Splice(stringTableJSON.get());
  }
}

uint32_t UniqueJSONStrings::GetOrAddIndex(const char* aStr) {
  uint32_t count = mStringHashToIndexMap.count();
  HashNumber hash = HashString(aStr);
  auto entry = mStringHashToIndexMap.lookupForAdd(hash);
  if (entry) {
    MOZ_ASSERT(entry->value() < count);
    return entry->value();
  }

  MOZ_RELEASE_ASSERT(mStringHashToIndexMap.add(entry, hash, count));
  mStringTableWriter.StringElement(aStr);
  return count;
}

UniqueStacks::StackKey UniqueStacks::BeginStack(const FrameKey& aFrame) {
  return StackKey(GetOrAddFrameIndex(aFrame));
}

UniqueStacks::StackKey UniqueStacks::AppendFrame(const StackKey& aStack,
                                                 const FrameKey& aFrame) {
  return StackKey(aStack, GetOrAddStackIndex(aStack),
                  GetOrAddFrameIndex(aFrame));
}

JITFrameInfoForBufferRange JITFrameInfoForBufferRange::Clone() const {
  JITFrameInfoForBufferRange::JITAddressToJITFramesMap jitAddressToJITFramesMap;
  MOZ_RELEASE_ASSERT(
      jitAddressToJITFramesMap.reserve(mJITAddressToJITFramesMap.count()));
  for (auto iter = mJITAddressToJITFramesMap.iter(); !iter.done();
       iter.next()) {
    const mozilla::Vector<JITFrameKey>& srcKeys = iter.get().value();
    mozilla::Vector<JITFrameKey> destKeys;
    MOZ_RELEASE_ASSERT(destKeys.appendAll(srcKeys));
    jitAddressToJITFramesMap.putNewInfallible(iter.get().key(),
                                              std::move(destKeys));
  }

  JITFrameInfoForBufferRange::JITFrameToFrameJSONMap jitFrameToFrameJSONMap;
  MOZ_RELEASE_ASSERT(
      jitFrameToFrameJSONMap.reserve(mJITFrameToFrameJSONMap.count()));
  for (auto iter = mJITFrameToFrameJSONMap.iter(); !iter.done(); iter.next()) {
    jitFrameToFrameJSONMap.putNewInfallible(iter.get().key(),
                                            iter.get().value());
  }

  return JITFrameInfoForBufferRange{mRangeStart, mRangeEnd,
                                    std::move(jitAddressToJITFramesMap),
                                    std::move(jitFrameToFrameJSONMap)};
}

JITFrameInfo::JITFrameInfo(const JITFrameInfo& aOther)
    : mUniqueStrings(MakeUnique<UniqueJSONStrings>(*aOther.mUniqueStrings)) {
  for (const JITFrameInfoForBufferRange& range : aOther.mRanges) {
    MOZ_RELEASE_ASSERT(mRanges.append(range.Clone()));
  }
}

bool UniqueStacks::FrameKey::NormalFrameData::operator==(
    const NormalFrameData& aOther) const {
  return mLocation == aOther.mLocation &&
         mRelevantForJS == aOther.mRelevantForJS &&
         mInnerWindowID == aOther.mInnerWindowID && mLine == aOther.mLine &&
         mColumn == aOther.mColumn && mCategoryPair == aOther.mCategoryPair;
}

bool UniqueStacks::FrameKey::JITFrameData::operator==(
    const JITFrameData& aOther) const {
  return mCanonicalAddress == aOther.mCanonicalAddress &&
         mDepth == aOther.mDepth && mRangeIndex == aOther.mRangeIndex;
}

// Consume aJITFrameInfo by stealing its string table and its JIT frame info
// ranges. The JIT frame info contains JSON which refers to strings from the
// JIT frame info's string table, so our string table needs to have the same
// strings at the same indices.
UniqueStacks::UniqueStacks(JITFrameInfo&& aJITFrameInfo)
    : mUniqueStrings(std::move(aJITFrameInfo.mUniqueStrings)),
      mJITInfoRanges(std::move(aJITFrameInfo.mRanges)) {
  mFrameTableWriter.StartBareList();
  mStackTableWriter.StartBareList();
}

uint32_t UniqueStacks::GetOrAddStackIndex(const StackKey& aStack) {
  uint32_t count = mStackToIndexMap.count();
  auto entry = mStackToIndexMap.lookupForAdd(aStack);
  if (entry) {
    MOZ_ASSERT(entry->value() < count);
    return entry->value();
  }

  MOZ_RELEASE_ASSERT(mStackToIndexMap.add(entry, aStack, count));
  StreamStack(aStack);
  return count;
}

Maybe<Vector<UniqueStacks::FrameKey>>
UniqueStacks::LookupFramesForJITAddressFromBufferPos(void* aJITAddress,
                                                     uint64_t aBufferPos) {
  JITFrameInfoForBufferRange* rangeIter =
      std::lower_bound(mJITInfoRanges.begin(), mJITInfoRanges.end(), aBufferPos,
                       [](const JITFrameInfoForBufferRange& aRange,
                          uint64_t aPos) { return aRange.mRangeEnd < aPos; });
  MOZ_RELEASE_ASSERT(
      rangeIter != mJITInfoRanges.end() &&
          rangeIter->mRangeStart <= aBufferPos &&
          aBufferPos < rangeIter->mRangeEnd,
      "Buffer position of jit address needs to be in one of the ranges");

  using JITFrameKey = JITFrameInfoForBufferRange::JITFrameKey;

  const JITFrameInfoForBufferRange& jitFrameInfoRange = *rangeIter;
  auto jitFrameKeys =
      jitFrameInfoRange.mJITAddressToJITFramesMap.lookup(aJITAddress);
  if (!jitFrameKeys) {
    return Nothing();
  }

  // Map the array of JITFrameKeys to an array of FrameKeys, and ensure that
  // each of the FrameKeys exists in mFrameToIndexMap.
  Vector<FrameKey> frameKeys;
  MOZ_RELEASE_ASSERT(frameKeys.initCapacity(jitFrameKeys->value().length()));
  for (const JITFrameKey& jitFrameKey : jitFrameKeys->value()) {
    FrameKey frameKey(jitFrameKey.mCanonicalAddress, jitFrameKey.mDepth,
                      rangeIter - mJITInfoRanges.begin());
    uint32_t index = mFrameToIndexMap.count();
    auto entry = mFrameToIndexMap.lookupForAdd(frameKey);
    if (!entry) {
      // We need to add this frame to our frame table. The JSON for this frame
      // already exists in jitFrameInfoRange, we just need to splice it into
      // the frame table and give it an index.
      auto frameJSON =
          jitFrameInfoRange.mJITFrameToFrameJSONMap.lookup(jitFrameKey);
      MOZ_RELEASE_ASSERT(frameJSON, "Should have cached JSON for this frame");
      mFrameTableWriter.Splice(frameJSON->value().get());
      MOZ_RELEASE_ASSERT(mFrameToIndexMap.add(entry, frameKey, index));
    }
    MOZ_RELEASE_ASSERT(frameKeys.append(std::move(frameKey)));
  }
  return Some(std::move(frameKeys));
}

uint32_t UniqueStacks::GetOrAddFrameIndex(const FrameKey& aFrame) {
  uint32_t count = mFrameToIndexMap.count();
  auto entry = mFrameToIndexMap.lookupForAdd(aFrame);
  if (entry) {
    MOZ_ASSERT(entry->value() < count);
    return entry->value();
  }

  MOZ_RELEASE_ASSERT(mFrameToIndexMap.add(entry, aFrame, count));
  StreamNonJITFrame(aFrame);
  return count;
}

void UniqueStacks::SpliceFrameTableElements(SpliceableJSONWriter& aWriter) {
  mFrameTableWriter.EndBareList();
  aWriter.TakeAndSplice(mFrameTableWriter.WriteFunc());
}

void UniqueStacks::SpliceStackTableElements(SpliceableJSONWriter& aWriter) {
  mStackTableWriter.EndBareList();
  aWriter.TakeAndSplice(mStackTableWriter.WriteFunc());
}

void UniqueStacks::StreamStack(const StackKey& aStack) {
  enum Schema : uint32_t { PREFIX = 0, FRAME = 1 };

  AutoArraySchemaWriter writer(mStackTableWriter, *mUniqueStrings);
  if (aStack.mPrefixStackIndex.isSome()) {
    writer.IntElement(PREFIX, *aStack.mPrefixStackIndex);
  }
  writer.IntElement(FRAME, aStack.mFrameIndex);
}

void UniqueStacks::StreamNonJITFrame(const FrameKey& aFrame) {
  using NormalFrameData = FrameKey::NormalFrameData;

  enum Schema : uint32_t {
    LOCATION = 0,
    RELEVANT_FOR_JS = 1,
    INNER_WINDOW_ID = 2,
    IMPLEMENTATION = 3,
    OPTIMIZATIONS = 4,
    LINE = 5,
    COLUMN = 6,
    CATEGORY = 7,
    SUBCATEGORY = 8
  };

  AutoArraySchemaWriter writer(mFrameTableWriter, *mUniqueStrings);

  const NormalFrameData& data = aFrame.mData.as<NormalFrameData>();
  writer.StringElement(LOCATION, data.mLocation.get());
  writer.BoolElement(RELEVANT_FOR_JS, data.mRelevantForJS);

  // It's okay to convert uint64_t to double here because DOM always creates IDs
  // that are convertible to double.
  writer.DoubleElement(INNER_WINDOW_ID, data.mInnerWindowID);

  if (data.mLine.isSome()) {
    writer.IntElement(LINE, *data.mLine);
  }
  if (data.mColumn.isSome()) {
    writer.IntElement(COLUMN, *data.mColumn);
  }
  if (data.mCategoryPair.isSome()) {
    const JS::ProfilingCategoryPairInfo& info =
        JS::GetProfilingCategoryPairInfo(*data.mCategoryPair);
    writer.IntElement(CATEGORY, uint32_t(info.mCategory));
    writer.IntElement(SUBCATEGORY, info.mSubcategoryIndex);
  }
}

static void StreamJITFrame(JSContext* aContext, SpliceableJSONWriter& aWriter,
                           UniqueJSONStrings& aUniqueStrings,
                           const JS::ProfiledFrameHandle& aJITFrame) {
  enum Schema : uint32_t {
    LOCATION = 0,
    RELEVANT_FOR_JS = 1,
    INNER_WINDOW_ID = 2,
    IMPLEMENTATION = 3,
    OPTIMIZATIONS = 4,
    LINE = 5,
    COLUMN = 6,
    CATEGORY = 7,
    SUBCATEGORY = 8
  };

  AutoArraySchemaWriter writer(aWriter, aUniqueStrings);

  writer.StringElement(LOCATION, aJITFrame.label());
  writer.BoolElement(RELEVANT_FOR_JS, false);

  // It's okay to convert uint64_t to double here because DOM always creates IDs
  // that are convertible to double.
  // Realm ID is the name of innerWindowID inside JS code.
  writer.DoubleElement(INNER_WINDOW_ID, aJITFrame.realmID());

  JS::ProfilingFrameIterator::FrameKind frameKind = aJITFrame.frameKind();
  MOZ_ASSERT(frameKind == JS::ProfilingFrameIterator::Frame_Ion ||
             frameKind == JS::ProfilingFrameIterator::Frame_Baseline);
  writer.StringElement(
      IMPLEMENTATION,
      frameKind == JS::ProfilingFrameIterator::Frame_Ion ? "ion" : "baseline");

  const JS::ProfilingCategoryPairInfo& info =
      JS::GetProfilingCategoryPairInfo(JS::ProfilingCategoryPair::JS);
  writer.IntElement(CATEGORY, uint32_t(info.mCategory));
  writer.IntElement(SUBCATEGORY, info.mSubcategoryIndex);
}

struct CStringWriteFunc : public JSONWriteFunc {
  nsACString& mBuffer;  // The struct must not outlive this buffer
  explicit CStringWriteFunc(nsACString& aBuffer) : mBuffer(aBuffer) {}

  void Write(const char* aStr) override { mBuffer.Append(aStr); }
};

static nsCString JSONForJITFrame(JSContext* aContext,
                                 const JS::ProfiledFrameHandle& aJITFrame,
                                 UniqueJSONStrings& aUniqueStrings) {
  nsCString json;
  SpliceableJSONWriter writer(MakeUnique<CStringWriteFunc>(json));
  StreamJITFrame(aContext, writer, aUniqueStrings, aJITFrame);
  return json;
}

void JITFrameInfo::AddInfoForRange(
    uint64_t aRangeStart, uint64_t aRangeEnd, JSContext* aCx,
    const std::function<void(const std::function<void(void*)>&)>&
        aJITAddressProvider) {
  if (aRangeStart == aRangeEnd) {
    return;
  }

  MOZ_RELEASE_ASSERT(aRangeStart < aRangeEnd);

  if (!mRanges.empty()) {
    const JITFrameInfoForBufferRange& prevRange = mRanges.back();
    MOZ_RELEASE_ASSERT(prevRange.mRangeEnd <= aRangeStart,
                       "Ranges must be non-overlapping and added in-order.");
  }

  using JITFrameKey = JITFrameInfoForBufferRange::JITFrameKey;

  JITFrameInfoForBufferRange::JITAddressToJITFramesMap jitAddressToJITFrameMap;
  JITFrameInfoForBufferRange::JITFrameToFrameJSONMap jitFrameToFrameJSONMap;

  aJITAddressProvider([&](void* aJITAddress) {
    // Make sure that we have cached data for aJITAddress.
    auto addressEntry = jitAddressToJITFrameMap.lookupForAdd(aJITAddress);
    if (!addressEntry) {
      Vector<JITFrameKey> jitFrameKeys;
      for (JS::ProfiledFrameHandle handle :
           JS::GetProfiledFrames(aCx, aJITAddress)) {
        uint32_t depth = jitFrameKeys.length();
        JITFrameKey jitFrameKey{handle.canonicalAddress(), depth};
        auto frameEntry = jitFrameToFrameJSONMap.lookupForAdd(jitFrameKey);
        if (!frameEntry) {
          MOZ_RELEASE_ASSERT(jitFrameToFrameJSONMap.add(
              frameEntry, jitFrameKey,
              JSONForJITFrame(aCx, handle, *mUniqueStrings)));
        }
        MOZ_RELEASE_ASSERT(jitFrameKeys.append(jitFrameKey));
      }
      MOZ_RELEASE_ASSERT(jitAddressToJITFrameMap.add(addressEntry, aJITAddress,
                                                     std::move(jitFrameKeys)));
    }
  });

  MOZ_RELEASE_ASSERT(mRanges.append(JITFrameInfoForBufferRange{
      aRangeStart, aRangeEnd, std::move(jitAddressToJITFrameMap),
      std::move(jitFrameToFrameJSONMap)}));
}

struct ProfileSample {
  uint32_t mStack;
  double mTime;
  Maybe<double> mResponsiveness;
};

static void WriteSample(SpliceableJSONWriter& aWriter,
                        UniqueJSONStrings& aUniqueStrings,
                        const ProfileSample& aSample) {
  enum Schema : uint32_t {
    STACK = 0,
    TIME = 1,
    EVENT_DELAY = 2,
  };

  AutoArraySchemaWriter writer(aWriter, aUniqueStrings);

  writer.IntElement(STACK, aSample.mStack);

  writer.DoubleElement(TIME, aSample.mTime);

  if (aSample.mResponsiveness.isSome()) {
    writer.DoubleElement(EVENT_DELAY, *aSample.mResponsiveness);
  }
}

class EntryGetter {
 public:
  explicit EntryGetter(BlocksRingBuffer::Reader& aReader,
                       uint64_t aInitialReadPos = 0)
      : mBlockIt(
            aReader.At(ProfileBufferBlockIndex::CreateFromProfileBufferIndex(
                aInitialReadPos))),
        mBlockItEnd(aReader.end()) {
    if (!ReadLegacyOrEnd()) {
      // Find and read the next non-legacy entry.
      Next();
    }
  }

  bool Has() const { return mBlockIt != mBlockItEnd; }

  const ProfileBufferEntry& Get() const {
    MOZ_ASSERT(Has(), "Caller should have checked `Has()` before `Get()`");
    return mEntry;
  }

  void Next() {
    MOZ_ASSERT(Has(), "Caller should have checked `Has()` before `Get()`");
    for (;;) {
      ++mBlockIt;
      if (ReadLegacyOrEnd()) {
        // Either we're at the end, or we could read a legacy entry -> Done.
        break;
      }
      // Otherwise loop around until we hit a legacy entry or the end.
    }
  }

  BlocksRingBuffer::BlockIterator Iterator() const { return mBlockIt; }

  ProfileBufferBlockIndex CurBlockIndex() const {
    return mBlockIt.CurrentBlockIndex();
  }

  uint64_t CurPos() const {
    return CurBlockIndex().ConvertToProfileBufferIndex();
  }

 private:
  // Try to read the entry at the current `mBlockIt` position.
  // * If we're at the end of the buffer, just return `true`.
  // * If there is a "legacy" entry (containing a real `ProfileBufferEntry`),
  //   read it into `mEntry`, and return `true` as well.
  // * Otherwise the entry contains a "modern" type that cannot be read into
  // `mEntry`, return `false` (so `EntryGetter` can skip to another entry).
  bool ReadLegacyOrEnd() {
    if (!Has()) {
      return true;
    }
    // Read the entry "kind", which is always at the start of all entries.
    ProfileBufferEntryReader aER = *mBlockIt;
    auto type = static_cast<ProfileBufferEntry::Kind>(
        aER.ReadObject<ProfileBufferEntry::KindUnderlyingType>());
    MOZ_ASSERT(static_cast<ProfileBufferEntry::KindUnderlyingType>(type) <
               static_cast<ProfileBufferEntry::KindUnderlyingType>(
                   ProfileBufferEntry::Kind::MODERN_LIMIT));
    if (type >= ProfileBufferEntry::Kind::LEGACY_LIMIT) {
      return false;
    }
    // Here, we have a legacy item, we need to read it from the start.
    // Because the above `ReadObject` moved the reader, we ned to reset it to
    // the start of the entry before reading the whole entry.
    aER = *mBlockIt;
    aER.ReadBytes(&mEntry, aER.RemainingBytes());
    return true;
  }

  ProfileBufferEntry mEntry;
  BlocksRingBuffer::BlockIterator mBlockIt;
  const BlocksRingBuffer::BlockIterator mBlockItEnd;
};

// The following grammar shows legal sequences of profile buffer entries.
// The sequences beginning with a ThreadId entry are known as "samples".
//
// (
//   ( /* Samples */
//     ThreadId
//     TimeBeforeCompactStack
//     UnresponsivenessDurationMs?
//     CompactStack
//         /* internally including:
//           ( NativeLeafAddr
//           | Label FrameFlags? DynamicStringFragment*
//             LineNumber? CategoryPair?
//           | JitReturnAddr
//           )+
//         */
//   )
//   | MarkerData
//   | ( /* Counters */
//       CounterId
//       Time
//       (
//         CounterKey
//         Count
//         Number?
//       )*
//     )
//   | CollectionStart
//   | CollectionEnd
//   | Pause
//   | Resume
//   | ( ProfilerOverheadTime /* Sampling start timestamp */
//       ProfilerOverheadDuration /* Lock acquisition */
//       ProfilerOverheadDuration /* Expired markers cleaning */
//       ProfilerOverheadDuration /* Counters */
//       ProfilerOverheadDuration /* Threads */
//     )
// )*
//
// The most complicated part is the stack entry sequence that begins with
// Label. Here are some examples.
//
// - ProfilingStack frames without a dynamic string:
//
//     Label("js::RunScript")
//     CategoryPair(JS::ProfilingCategoryPair::JS)
//
//     Label("XREMain::XRE_main")
//     LineNumber(4660)
//     CategoryPair(JS::ProfilingCategoryPair::OTHER)
//
//     Label("ElementRestyler::ComputeStyleChangeFor")
//     LineNumber(3003)
//     CategoryPair(JS::ProfilingCategoryPair::CSS)
//
// - ProfilingStack frames with a dynamic string:
//
//     Label("nsObserverService::NotifyObservers")
//     FrameFlags(uint64_t(ProfilingStackFrame::Flags::IS_LABEL_FRAME))
//     DynamicStringFragment("domwindo")
//     DynamicStringFragment("wopened")
//     LineNumber(291)
//     CategoryPair(JS::ProfilingCategoryPair::OTHER)
//
//     Label("")
//     FrameFlags(uint64_t(ProfilingStackFrame::Flags::IS_JS_FRAME))
//     DynamicStringFragment("closeWin")
//     DynamicStringFragment("dow (chr")
//     DynamicStringFragment("ome://gl")
//     DynamicStringFragment("obal/con")
//     DynamicStringFragment("tent/glo")
//     DynamicStringFragment("balOverl")
//     DynamicStringFragment("ay.js:5)")
//     DynamicStringFragment("")          # this string holds the closing '\0'
//     LineNumber(25)
//     CategoryPair(JS::ProfilingCategoryPair::JS)
//
//     Label("")
//     FrameFlags(uint64_t(ProfilingStackFrame::Flags::IS_JS_FRAME))
//     DynamicStringFragment("bound (s")
//     DynamicStringFragment("elf-host")
//     DynamicStringFragment("ed:914)")
//     LineNumber(945)
//     CategoryPair(JS::ProfilingCategoryPair::JS)
//
// - A profiling stack frame with a dynamic string, but with privacy enabled:
//
//     Label("nsObserverService::NotifyObservers")
//     FrameFlags(uint64_t(ProfilingStackFrame::Flags::IS_LABEL_FRAME))
//     DynamicStringFragment("(private")
//     DynamicStringFragment(")")
//     LineNumber(291)
//     CategoryPair(JS::ProfilingCategoryPair::OTHER)
//
// - A profiling stack frame with an overly long dynamic string:
//
//     Label("")
//     FrameFlags(uint64_t(ProfilingStackFrame::Flags::IS_LABEL_FRAME))
//     DynamicStringFragment("(too lon")
//     DynamicStringFragment("g)")
//     LineNumber(100)
//     CategoryPair(JS::ProfilingCategoryPair::NETWORK)
//
// - A wasm JIT frame:
//
//     Label("")
//     FrameFlags(uint64_t(0))
//     DynamicStringFragment("wasm-fun")
//     DynamicStringFragment("ction[87")
//     DynamicStringFragment("36] (blo")
//     DynamicStringFragment("b:http:/")
//     DynamicStringFragment("/webasse")
//     DynamicStringFragment("mbly.org")
//     DynamicStringFragment("/3dc5759")
//     DynamicStringFragment("4-ce58-4")
//     DynamicStringFragment("626-975b")
//     DynamicStringFragment("-08ad116")
//     DynamicStringFragment("30bc1:38")
//     DynamicStringFragment("29856)")
//
// - A JS frame in a synchronous sample:
//
//     Label("")
//     FrameFlags(uint64_t(ProfilingStackFrame::Flags::IS_LABEL_FRAME))
//     DynamicStringFragment("u (https")
//     DynamicStringFragment("://perf-")
//     DynamicStringFragment("html.io/")
//     DynamicStringFragment("ac0da204")
//     DynamicStringFragment("aaa44d75")
//     DynamicStringFragment("a800.bun")
//     DynamicStringFragment("dle.js:2")
//     DynamicStringFragment("5)")

// Because this is a format entirely internal to the Profiler, any parsing
// error indicates a bug in the ProfileBuffer writing or the parser itself,
// or possibly flaky hardware.
#define ERROR_AND_CONTINUE(msg)                            \
  {                                                        \
    fprintf(stderr, "ProfileBuffer parse error: %s", msg); \
    MOZ_ASSERT(false, msg);                                \
    continue;                                              \
  }

void ProfileBuffer::StreamSamplesToJSON(SpliceableJSONWriter& aWriter,
                                        int aThreadId, double aSinceTime,
                                        UniqueStacks& aUniqueStacks) const {
  UniquePtr<char[]> dynStrBuf = MakeUnique<char[]>(kMaxFrameKeyLength);

  mEntries.Read([&](BlocksRingBuffer::Reader* aReader) {
    MOZ_ASSERT(
        aReader,
        "BlocksRingBuffer cannot be out-of-session when sampler is running");

    EntryGetter e(*aReader);

    for (;;) {
      // This block skips entries until we find the start of the next sample.
      // This is useful in three situations.
      //
      // - The circular buffer overwrites old entries, so when we start parsing
      //   we might be in the middle of a sample, and we must skip forward to
      //   the start of the next sample.
      //
      // - We skip samples that don't have an appropriate ThreadId or Time.
      //
      // - We skip range Pause, Resume, CollectionStart, Marker, Counter
      //   and CollectionEnd entries between samples.
      while (e.Has()) {
        if (e.Get().IsThreadId()) {
          break;
        }
        e.Next();
      }

      if (!e.Has()) {
        break;
      }

      if (e.Get().IsThreadId()) {
        int threadId = e.Get().GetInt();
        e.Next();

        // Ignore samples that are for the wrong thread.
        if (threadId != aThreadId) {
          continue;
        }
      } else {
        // Due to the skip_to_next_sample block above, if we have an entry here
        // it must be a ThreadId entry.
        MOZ_CRASH();
      }

      ProfileSample sample;

      auto ReadStack = [&](EntryGetter& e, uint64_t entryPosition,
                           const Maybe<double>& unresponsiveDuration) {
        UniqueStacks::StackKey stack =
            aUniqueStacks.BeginStack(UniqueStacks::FrameKey("(root)"));

        int numFrames = 0;
        while (e.Has()) {
          if (e.Get().IsNativeLeafAddr()) {
            numFrames++;

            void* pc = e.Get().GetPtr();
            e.Next();

            nsAutoCString buf;

            if (!aUniqueStacks.mCodeAddressService ||
                !aUniqueStacks.mCodeAddressService->GetFunction(pc, buf) ||
                buf.IsEmpty()) {
              buf.AppendASCII("0x");
              // `AppendInt` only knows `uint32_t` or `uint64_t`, but because
              // these are just aliases for *two* of (`unsigned`, `unsigned
              // long`, and `unsigned long long`), a call with `uintptr_t` could
              // use the third type and therefore would be ambiguous.
              // So we want to force using exactly `uint32_t` or `uint64_t`,
              // whichever matches the size of `uintptr_t`.
              // (The outer cast to `uint` should then be a no-op.)
              using uint =
                  std::conditional_t<sizeof(uintptr_t) <= sizeof(uint32_t),
                                     uint32_t, uint64_t>;
              buf.AppendInt(static_cast<uint>(reinterpret_cast<uintptr_t>(pc)),
                            16);
            }

            stack = aUniqueStacks.AppendFrame(
                stack, UniqueStacks::FrameKey(buf.get()));

          } else if (e.Get().IsLabel()) {
            numFrames++;

            const char* label = e.Get().GetString();
            e.Next();

            using FrameFlags = js::ProfilingStackFrame::Flags;
            uint32_t frameFlags = 0;
            if (e.Has() && e.Get().IsFrameFlags()) {
              frameFlags = uint32_t(e.Get().GetUint64());
              e.Next();
            }

            bool relevantForJS =
                frameFlags & uint32_t(FrameFlags::RELEVANT_FOR_JS);

            // Copy potential dynamic string fragments into dynStrBuf, so that
            // dynStrBuf will then contain the entire dynamic string.
            size_t i = 0;
            dynStrBuf[0] = '\0';
            while (e.Has()) {
              if (e.Get().IsDynamicStringFragment()) {
                char chars[ProfileBufferEntry::kNumChars];
                e.Get().CopyCharsInto(chars);
                for (char c : chars) {
                  if (i < kMaxFrameKeyLength) {
                    dynStrBuf[i] = c;
                    i++;
                  }
                }
                e.Next();
              } else {
                break;
              }
            }
            dynStrBuf[kMaxFrameKeyLength - 1] = '\0';
            bool hasDynamicString = (i != 0);

            nsAutoCStringN<1024> frameLabel;
            if (label[0] != '\0' && hasDynamicString) {
              if (frameFlags & uint32_t(FrameFlags::STRING_TEMPLATE_METHOD)) {
                frameLabel.AppendPrintf("%s.%s", label, dynStrBuf.get());
              } else if (frameFlags &
                         uint32_t(FrameFlags::STRING_TEMPLATE_GETTER)) {
                frameLabel.AppendPrintf("get %s.%s", label, dynStrBuf.get());
              } else if (frameFlags &
                         uint32_t(FrameFlags::STRING_TEMPLATE_SETTER)) {
                frameLabel.AppendPrintf("set %s.%s", label, dynStrBuf.get());
              } else {
                frameLabel.AppendPrintf("%s %s", label, dynStrBuf.get());
              }
            } else if (hasDynamicString) {
              frameLabel.Append(dynStrBuf.get());
            } else {
              frameLabel.Append(label);
            }

            uint64_t innerWindowID = 0;
            if (e.Has() && e.Get().IsInnerWindowID()) {
              innerWindowID = uint64_t(e.Get().GetUint64());
              e.Next();
            }

            Maybe<unsigned> line;
            if (e.Has() && e.Get().IsLineNumber()) {
              line = Some(unsigned(e.Get().GetInt()));
              e.Next();
            }

            Maybe<unsigned> column;
            if (e.Has() && e.Get().IsColumnNumber()) {
              column = Some(unsigned(e.Get().GetInt()));
              e.Next();
            }

            Maybe<JS::ProfilingCategoryPair> categoryPair;
            if (e.Has() && e.Get().IsCategoryPair()) {
              categoryPair =
                  Some(JS::ProfilingCategoryPair(uint32_t(e.Get().GetInt())));
              e.Next();
            }

            stack = aUniqueStacks.AppendFrame(
                stack, UniqueStacks::FrameKey(std::move(frameLabel),
                                              relevantForJS, innerWindowID,
                                              line, column, categoryPair));

          } else if (e.Get().IsJitReturnAddr()) {
            numFrames++;

            // A JIT frame may expand to multiple frames due to inlining.
            void* pc = e.Get().GetPtr();
            const Maybe<Vector<UniqueStacks::FrameKey>>& frameKeys =
                aUniqueStacks.LookupFramesForJITAddressFromBufferPos(
                    pc, entryPosition ? entryPosition : e.CurPos());
            MOZ_RELEASE_ASSERT(
                frameKeys,
                "Attempting to stream samples for a buffer range "
                "for which we don't have JITFrameInfo?");
            for (const UniqueStacks::FrameKey& frameKey : *frameKeys) {
              stack = aUniqueStacks.AppendFrame(stack, frameKey);
            }

            e.Next();

          } else {
            break;
          }
        }

        if (numFrames == 0) {
          // It is possible to have empty stacks if native stackwalking is
          // disabled. Skip samples with empty stacks. (See Bug 1497985).
          // Thus, don't use ERROR_AND_CONTINUE, but just continue by returning
          // from this lambda.
          return;
        }

        sample.mStack = aUniqueStacks.GetOrAddStackIndex(stack);

        if (unresponsiveDuration.isSome()) {
          sample.mResponsiveness = unresponsiveDuration;
        }

        WriteSample(aWriter, *aUniqueStacks.mUniqueStrings, sample);
      };  // End of `ReadStack(EntryGetter&)` lambda.

      if (e.Has() && e.Get().IsTime()) {
        sample.mTime = e.Get().GetDouble();
        e.Next();

        // Ignore samples that are too old.
        if (sample.mTime < aSinceTime) {
          continue;
        }

        ReadStack(e, 0, Nothing{});
      } else if (e.Has() && e.Get().IsTimeBeforeCompactStack()) {
        sample.mTime = e.Get().GetDouble();

        // Ignore samples that are too old.
        if (sample.mTime < aSinceTime) {
          e.Next();
          continue;
        }

        Maybe<double> unresponsiveDuration;

        BlocksRingBuffer::BlockIterator it = e.Iterator();
        for (;;) {
          ++it;
          if (it.IsAtEnd()) {
            break;
          }
          ProfileBufferEntryReader er = *it;
          ProfileBufferEntry::Kind kind =
              er.ReadObject<ProfileBufferEntry::Kind>();

          // There may be an UnresponsiveDurationMs before the CompactStack.
          if (kind == ProfileBufferEntry::Kind::UnresponsiveDurationMs) {
            unresponsiveDuration = Some(er.ReadObject<double>());
            continue;
          }

          if (kind == ProfileBufferEntry::Kind::CompactStack) {
            BlocksRingBuffer tempBuffer(
                BlocksRingBuffer::ThreadSafety::WithoutMutex,
                mWorkerBuffer.get(), WorkerBufferBytes);
            er.ReadIntoObject(tempBuffer);
            tempBuffer.Read([&](BlocksRingBuffer::Reader* aReader) {
              MOZ_ASSERT(aReader,
                         "Local BlocksRingBuffer cannot be out-of-session");
              EntryGetter stackEntryGetter(*aReader);
              if (stackEntryGetter.Has()) {
                ReadStack(stackEntryGetter,
                          it.CurrentBlockIndex().ConvertToProfileBufferIndex(),
                          unresponsiveDuration);
              }
            });
            break;
          }

          MOZ_ASSERT(kind >= ProfileBufferEntry::Kind::LEGACY_LIMIT,
                     "There should be no legacy entries between "
                     "TimeBeforeCompactStack and CompactStack");
        }

        e.Next();
      } else {
        ERROR_AND_CONTINUE("expected a Time entry");
      }
    }
  });
}

void ProfileBuffer::AddJITInfoForRange(uint64_t aRangeStart, int aThreadId,
                                       JSContext* aContext,
                                       JITFrameInfo& aJITFrameInfo) const {
  // We can only process JitReturnAddr entries if we have a JSContext.
  MOZ_RELEASE_ASSERT(aContext);

  aRangeStart = std::max(aRangeStart, BufferRangeStart());
  aJITFrameInfo.AddInfoForRange(
      aRangeStart, BufferRangeEnd(), aContext,
      [&](const std::function<void(void*)>& aJITAddressConsumer) {
        // Find all JitReturnAddr entries in the given range for the given
        // thread, and call aJITAddressConsumer with those addresses.

        mEntries.Read([&](BlocksRingBuffer::Reader* aReader) {
          MOZ_ASSERT(aReader,
                     "BlocksRingBuffer cannot be out-of-session when sampler "
                     "is running");

          EntryGetter e(*aReader, aRangeStart);

          while (true) {
            // Advance to the next ThreadId entry.
            while (e.Has() && !e.Get().IsThreadId()) {
              e.Next();
            }
            if (!e.Has()) {
              break;
            }

            MOZ_ASSERT(e.Get().IsThreadId());
            int threadId = e.Get().GetInt();
            e.Next();

            // Ignore samples that are for a different thread.
            if (threadId != aThreadId) {
              continue;
            }

            if (e.Has() && e.Get().IsTime()) {
              // Legacy stack.
              e.Next();
              while (e.Has() && !e.Get().IsThreadId()) {
                if (e.Get().IsJitReturnAddr()) {
                  aJITAddressConsumer(e.Get().GetPtr());
                }
                e.Next();
              }
            } else if (e.Has() && e.Get().IsTimeBeforeCompactStack()) {
              // Compact stack.
              BlocksRingBuffer::BlockIterator it = e.Iterator();
              for (;;) {
                ++it;
                if (it.IsAtEnd()) {
                  break;
                }
                ProfileBufferEntryReader er = *it;
                ProfileBufferEntry::Kind kind =
                    er.ReadObject<ProfileBufferEntry::Kind>();
                if (kind == ProfileBufferEntry::Kind::CompactStack) {
                  BlocksRingBuffer tempBuffer(
                      BlocksRingBuffer::ThreadSafety::WithoutMutex,
                      mWorkerBuffer.get(), WorkerBufferBytes);
                  er.ReadIntoObject(tempBuffer);
                  tempBuffer.Read([&](BlocksRingBuffer::Reader* aReader) {
                    MOZ_ASSERT(
                        aReader,
                        "Local BlocksRingBuffer cannot be out-of-session");
                    EntryGetter stackEntryGetter(*aReader);
                    while (stackEntryGetter.Has()) {
                      if (stackEntryGetter.Get().IsJitReturnAddr()) {
                        aJITAddressConsumer(stackEntryGetter.Get().GetPtr());
                      }
                      stackEntryGetter.Next();
                    }
                  });
                  break;
                }
                MOZ_ASSERT(kind >= ProfileBufferEntry::Kind::LEGACY_LIMIT,
                           "There should be no legacy entries between "
                           "TimeBeforeCompactStack and CompactStack");
              }

              e.Next();
            } else {
              ERROR_AND_CONTINUE("expected a Time entry");
            }
          }
        });
      });
}

void ProfileBuffer::StreamMarkersToJSON(SpliceableJSONWriter& aWriter,
                                        int aThreadId,
                                        const TimeStamp& aProcessStartTime,
                                        double aSinceTime,
                                        UniqueStacks& aUniqueStacks) const {
  mEntries.ReadEach([&](ProfileBufferEntryReader& aER) {
    auto type = static_cast<ProfileBufferEntry::Kind>(
        aER.ReadObject<ProfileBufferEntry::KindUnderlyingType>());
    MOZ_ASSERT(static_cast<ProfileBufferEntry::KindUnderlyingType>(type) <
               static_cast<ProfileBufferEntry::KindUnderlyingType>(
                   ProfileBufferEntry::Kind::MODERN_LIMIT));
    if (type == ProfileBufferEntry::Kind::MarkerData &&
        aER.ReadObject<int>() == aThreadId) {
      // Schema:
      //   [name, time, category, data]

      aWriter.StartArrayElement();
      {
        std::string name = aER.ReadObject<std::string>();
        const JS::ProfilingCategoryPairInfo& info =
            GetProfilingCategoryPairInfo(static_cast<JS::ProfilingCategoryPair>(
                aER.ReadObject<uint32_t>()));
        auto payload = aER.ReadObject<UniquePtr<ProfilerMarkerPayload>>();
        double time = aER.ReadObject<double>();
        MOZ_ASSERT(aER.RemainingBytes() == 0);

        aUniqueStacks.mUniqueStrings->WriteElement(aWriter, name.c_str());
        aWriter.DoubleElement(time);
        aWriter.IntElement(unsigned(info.mCategory));
        if (payload) {
          aWriter.StartObjectElement(SpliceableJSONWriter::SingleLineStyle);
          { payload->StreamPayload(aWriter, aProcessStartTime, aUniqueStacks); }
          aWriter.EndObject();
        }
      }
      aWriter.EndArray();
    }
  });
}

void ProfileBuffer::StreamProfilerOverheadToJSON(
    SpliceableJSONWriter& aWriter, const TimeStamp& aProcessStartTime,
    double aSinceTime) const {
  mEntries.Read([&](BlocksRingBuffer::Reader* aReader) {
    MOZ_ASSERT(
        aReader,
        "BlocksRingBuffer cannot be out-of-session when sampler is running");

    EntryGetter e(*aReader);

    enum Schema : uint32_t {
      TIME = 0,
      LOCKING = 1,
      MARKER_CLEANING = 2,
      COUNTERS = 3,
      THREADS = 4
    };

    aWriter.StartObjectProperty("profilerOverhead");
    aWriter.StartObjectProperty("samples");
    // Stream all sampling overhead data. We skip other entries, because we
    // process them in StreamSamplesToJSON()/etc.
    {
      JSONSchemaWriter schema(aWriter);
      schema.WriteField("time");
      schema.WriteField("locking");
      schema.WriteField("expiredMarkerCleaning");
      schema.WriteField("counters");
      schema.WriteField("threads");
    }

    aWriter.StartArrayProperty("data");
    double firstTime = 0.0;
    double lastTime = 0.0;
    ProfilerStats intervals, overheads, lockings, cleanings, counters, threads;
    while (e.Has()) {
      // valid sequence: ProfilerOverheadTime, ProfilerOverheadDuration * 4
      if (e.Get().IsProfilerOverheadTime()) {
        double time = e.Get().GetDouble();
        if (time >= aSinceTime) {
          e.Next();
          if (!e.Has() || !e.Get().IsProfilerOverheadDuration()) {
            ERROR_AND_CONTINUE(
                "expected a ProfilerOverheadDuration entry after "
                "ProfilerOverheadTime");
          }
          double locking = e.Get().GetDouble();
          e.Next();
          if (!e.Has() || !e.Get().IsProfilerOverheadDuration()) {
            ERROR_AND_CONTINUE(
                "expected a ProfilerOverheadDuration entry after "
                "ProfilerOverheadTime,ProfilerOverheadDuration");
          }
          double cleaning = e.Get().GetDouble();
          e.Next();
          if (!e.Has() || !e.Get().IsProfilerOverheadDuration()) {
            ERROR_AND_CONTINUE(
                "expected a ProfilerOverheadDuration entry after "
                "ProfilerOverheadTime,ProfilerOverheadDuration*2");
          }
          double counter = e.Get().GetDouble();
          e.Next();
          if (!e.Has() || !e.Get().IsProfilerOverheadDuration()) {
            ERROR_AND_CONTINUE(
                "expected a ProfilerOverheadDuration entry after "
                "ProfilerOverheadTime,ProfilerOverheadDuration*3");
          }
          double thread = e.Get().GetDouble();

          if (firstTime == 0.0) {
            firstTime = time;
          } else {
            // Note that we'll have 1 fewer interval than other numbers (because
            // we need both ends of an interval to know its duration). The final
            // difference should be insignificant over the expected many
            // thousands of iterations.
            intervals.Count(time - lastTime);
          }
          lastTime = time;
          overheads.Count(locking + cleaning + counter + thread);
          lockings.Count(locking);
          cleanings.Count(cleaning);
          counters.Count(counter);
          threads.Count(thread);

          AutoArraySchemaWriter writer(aWriter);
          writer.DoubleElement(TIME, time);
          writer.DoubleElement(LOCKING, locking);
          writer.DoubleElement(MARKER_CLEANING, cleaning);
          writer.DoubleElement(COUNTERS, counter);
          writer.DoubleElement(THREADS, thread);
        }
      }
      e.Next();
    }
    aWriter.EndArray();   // data
    aWriter.EndObject();  // samples

    // Only output statistics if there is at least one full interval (and
    // therefore at least two samplings.)
    if (intervals.n > 0) {
      aWriter.StartObjectProperty("statistics");
      aWriter.DoubleProperty("profiledDuration", lastTime - firstTime);
      aWriter.IntProperty("samplingCount", overheads.n);
      aWriter.DoubleProperty("overheadDurations", overheads.sum);
      aWriter.DoubleProperty("overheadPercentage",
                             overheads.sum / (lastTime - firstTime));
#define PROFILER_STATS(name, var)                           \
  aWriter.DoubleProperty("mean" name, (var).sum / (var).n); \
  aWriter.DoubleProperty("min" name, (var).min);            \
  aWriter.DoubleProperty("max" name, (var).max);
      PROFILER_STATS("Interval", intervals);
      PROFILER_STATS("Overhead", overheads);
      PROFILER_STATS("Lockings", lockings);
      PROFILER_STATS("Cleaning", cleanings);
      PROFILER_STATS("Counter", counters);
      PROFILER_STATS("Thread", threads);
#undef PROFILER_STATS
      aWriter.EndObject();  // statistics
    }
    aWriter.EndObject();  // profilerOverhead
  });
}

struct CounterKeyedSample {
  double mTime;
  uint64_t mNumber;
  int64_t mCount;
};

using CounterKeyedSamples = Vector<CounterKeyedSample>;

static LazyLogModule sFuzzyfoxLog("Fuzzyfox");

using CounterMap = HashMap<uint64_t, CounterKeyedSamples>;

// HashMap lookup, if not found, a default value is inserted.
// Returns reference to (existing or new) value inside the HashMap.
template <typename HashM, typename Key>
static auto& LookupOrAdd(HashM& aMap, Key&& aKey) {
  auto addPtr = aMap.lookupForAdd(aKey);
  if (!addPtr) {
    MOZ_RELEASE_ASSERT(aMap.add(addPtr, std::forward<Key>(aKey),
                                typename HashM::Entry::ValueType{}));
    MOZ_ASSERT(!!addPtr);
  }
  return addPtr->value();
}

void ProfileBuffer::StreamCountersToJSON(SpliceableJSONWriter& aWriter,
                                         const TimeStamp& aProcessStartTime,
                                         double aSinceTime) const {
  // Because this is a format entirely internal to the Profiler, any parsing
  // error indicates a bug in the ProfileBuffer writing or the parser itself,
  // or possibly flaky hardware.

  mEntries.Read([&](BlocksRingBuffer::Reader* aReader) {
    MOZ_ASSERT(
        aReader,
        "BlocksRingBuffer cannot be out-of-session when sampler is running");

    EntryGetter e(*aReader);

    enum Schema : uint32_t { TIME = 0, NUMBER = 1, COUNT = 2 };

    // Stream all counters. We skip other entries, because we process them in
    // StreamSamplesToJSON()/etc.
    //
    // Valid sequence in the buffer:
    // CounterID
    // Time
    // ( CounterKey Count Number? )*
    //
    // And the JSON (example):
    // "counters": {
    //  "name": "malloc",
    //  "category": "Memory",
    //  "description": "Amount of allocated memory",
    //  "sample_groups": {
    //   "id": 0,
    //   "samples": {
    //    "schema": {"time": 0, "number": 1, "count": 2},
    //    "data": [
    //     [
    //      16117.033968000002,
    //      2446216,
    //      6801320
    //     ],
    //     [
    //      16118.037638,
    //      2446216,
    //      6801320
    //     ],
    //    ],
    //   }
    //  }
    // },

    // Build the map of counters and populate it
    HashMap<void*, CounterMap> counters;

    while (e.Has()) {
      // skip all non-Counters, including if we start in the middle of a counter
      if (e.Get().IsCounterId()) {
        void* id = e.Get().GetPtr();
        CounterMap& counter = LookupOrAdd(counters, id);
        e.Next();
        if (!e.Has() || !e.Get().IsTime()) {
          ERROR_AND_CONTINUE("expected a Time entry");
        }
        double time = e.Get().GetDouble();
        if (time >= aSinceTime) {
          e.Next();
          while (e.Has() && e.Get().IsCounterKey()) {
            uint64_t key = e.Get().GetUint64();
            CounterKeyedSamples& data = LookupOrAdd(counter, key);
            e.Next();
            if (!e.Has() || !e.Get().IsCount()) {
              ERROR_AND_CONTINUE("expected a Count entry");
            }
            int64_t count = e.Get().GetUint64();
            e.Next();
            uint64_t number;
            if (!e.Has() || !e.Get().IsNumber()) {
              number = 0;
            } else {
              number = e.Get().GetInt64();
            }
            CounterKeyedSample sample = {time, number, count};
            MOZ_RELEASE_ASSERT(data.append(sample));
          }
        } else {
          // skip counter sample - only need to skip the initial counter
          // id, then let the loop at the top skip the rest
        }
      }
      e.Next();
    }
    // we have a map of a map of counter entries; dump them to JSON
    if (counters.count() == 0) {
      return;
    }

    aWriter.StartArrayProperty("counters");
    for (auto iter = counters.iter(); !iter.done(); iter.next()) {
      CounterMap& counter = iter.get().value();
      const BaseProfilerCount* base_counter =
          static_cast<const BaseProfilerCount*>(iter.get().key());

      aWriter.Start();
      aWriter.StringProperty("name", base_counter->mLabel);
      aWriter.StringProperty("category", base_counter->mCategory);
      aWriter.StringProperty("description", base_counter->mDescription);

      aWriter.StartArrayProperty("sample_groups");
      for (auto counter_iter = counter.iter(); !counter_iter.done();
           counter_iter.next()) {
        CounterKeyedSamples& samples = counter_iter.get().value();
        uint64_t key = counter_iter.get().key();

        size_t size = samples.length();
        if (size == 0) {
          continue;
        }

        aWriter.StartObjectElement();
        {
          aWriter.IntProperty("id", static_cast<int64_t>(key));
          aWriter.StartObjectProperty("samples");
          {
            // XXX Can we assume a missing count means 0?
            JSONSchemaWriter schema(aWriter);
            schema.WriteField("time");
            schema.WriteField("number");
            schema.WriteField("count");
          }

          aWriter.StartArrayProperty("data");
          uint64_t previousNumber = 0;
          int64_t previousCount = 0;
          for (size_t i = 0; i < size; i++) {
            // Encode as deltas, and only encode if different than the last
            // sample
            if (i == 0 || samples[i].mNumber != previousNumber ||
                samples[i].mCount != previousCount) {
              if (i != 0 && samples[i].mTime >= samples[i - 1].mTime) {
                MOZ_LOG(sFuzzyfoxLog, mozilla::LogLevel::Error,
                        ("Fuzzyfox Profiler Assertion: %f >= %f",
                         samples[i].mTime, samples[i - 1].mTime));
              }
              MOZ_ASSERT(i == 0 || samples[i].mTime >= samples[i - 1].mTime);
              MOZ_ASSERT(samples[i].mNumber >= previousNumber);
              MOZ_ASSERT(samples[i].mNumber - previousNumber <=
                         uint64_t(std::numeric_limits<int64_t>::max()));

              AutoArraySchemaWriter writer(aWriter);
              writer.DoubleElement(TIME, samples[i].mTime);
              writer.IntElement(
                  NUMBER,
                  static_cast<int64_t>(samples[i].mNumber - previousNumber));
              writer.IntElement(COUNT, samples[i].mCount - previousCount);
              previousNumber = samples[i].mNumber;
              previousCount = samples[i].mCount;
            }
          }
          aWriter.EndArray();   // data
          aWriter.EndObject();  // samples
        }
        aWriter.EndObject();  // sample_groups item
      }
      aWriter.EndArray();  // sample groups
      aWriter.End();       // for each counter
    }
    aWriter.EndArray();  // counters
  });
}

#undef ERROR_AND_CONTINUE

static void AddPausedRange(SpliceableJSONWriter& aWriter, const char* aReason,
                           const Maybe<double>& aStartTime,
                           const Maybe<double>& aEndTime) {
  aWriter.Start();
  if (aStartTime) {
    aWriter.DoubleProperty("startTime", *aStartTime);
  } else {
    aWriter.NullProperty("startTime");
  }
  if (aEndTime) {
    aWriter.DoubleProperty("endTime", *aEndTime);
  } else {
    aWriter.NullProperty("endTime");
  }
  aWriter.StringProperty("reason", aReason);
  aWriter.End();
}

void ProfileBuffer::StreamPausedRangesToJSON(SpliceableJSONWriter& aWriter,
                                             double aSinceTime) const {
  mEntries.Read([&](BlocksRingBuffer::Reader* aReader) {
    MOZ_ASSERT(
        aReader,
        "BlocksRingBuffer cannot be out-of-session when sampler is running");

    EntryGetter e(*aReader);

    Maybe<double> currentPauseStartTime;
    Maybe<double> currentCollectionStartTime;

    while (e.Has()) {
      if (e.Get().IsPause()) {
        currentPauseStartTime = Some(e.Get().GetDouble());
      } else if (e.Get().IsResume()) {
        AddPausedRange(aWriter, "profiler-paused", currentPauseStartTime,
                       Some(e.Get().GetDouble()));
        currentPauseStartTime = Nothing();
      } else if (e.Get().IsCollectionStart()) {
        currentCollectionStartTime = Some(e.Get().GetDouble());
      } else if (e.Get().IsCollectionEnd()) {
        AddPausedRange(aWriter, "collecting", currentCollectionStartTime,
                       Some(e.Get().GetDouble()));
        currentCollectionStartTime = Nothing();
      }
      e.Next();
    }

    if (currentPauseStartTime) {
      AddPausedRange(aWriter, "profiler-paused", currentPauseStartTime,
                     Nothing());
    }
    if (currentCollectionStartTime) {
      AddPausedRange(aWriter, "collecting", currentCollectionStartTime,
                     Nothing());
    }
  });
}

bool ProfileBuffer::DuplicateLastSample(int aThreadId,
                                        const TimeStamp& aProcessStartTime,
                                        Maybe<uint64_t>& aLastSample) {
  if (!aLastSample) {
    return false;
  }

  BlocksRingBuffer tempBuffer(BlocksRingBuffer::ThreadSafety::WithoutMutex,
                              mWorkerBuffer.get(), WorkerBufferBytes);

  const bool ok = mEntries.Read([&](BlocksRingBuffer::Reader* aReader) {
    MOZ_ASSERT(
        aReader,
        "BlocksRingBuffer cannot be out-of-session when sampler is running");

    EntryGetter e(*aReader, *aLastSample);

    if (e.CurPos() != *aLastSample) {
      // The last sample is no longer within the buffer range, so we cannot
      // use it. Reset the stored buffer position to Nothing().
      aLastSample.reset();
      return false;
    }

    MOZ_RELEASE_ASSERT(e.Has() && e.Get().IsThreadId() &&
                       e.Get().GetInt() == aThreadId);

    e.Next();

    // Go through the whole entry and duplicate it, until we find the next
    // one.
    while (e.Has()) {
      switch (e.Get().GetKind()) {
        case ProfileBufferEntry::Kind::Pause:
        case ProfileBufferEntry::Kind::Resume:
        case ProfileBufferEntry::Kind::CollectionStart:
        case ProfileBufferEntry::Kind::CollectionEnd:
        case ProfileBufferEntry::Kind::ThreadId:
          // We're done.
          return true;
        case ProfileBufferEntry::Kind::Time:
          // Copy with new time
          AddEntry(tempBuffer,
                   ProfileBufferEntry::Time(
                       (TimeStamp::NowUnfuzzed() - aProcessStartTime)
                           .ToMilliseconds()));
          break;
        case ProfileBufferEntry::Kind::TimeBeforeCompactStack: {
          // Copy with new time, followed by a compact stack.
          AddEntry(tempBuffer,
                   ProfileBufferEntry::TimeBeforeCompactStack(
                       (TimeStamp::NowUnfuzzed() - aProcessStartTime)
                           .ToMilliseconds()));

          // The `CompactStack` *must* be present afterwards, but may not
          // immediately follow `TimeBeforeCompactStack` (e.g., some markers
          // could be written in-between), so we need to look for it in the
          // following entries.
          BlocksRingBuffer::BlockIterator it = e.Iterator();
          for (;;) {
            ++it;
            if (it.IsAtEnd()) {
              break;
            }
            ProfileBufferEntryReader er = *it;
            auto kind = static_cast<ProfileBufferEntry::Kind>(
                er.ReadObject<ProfileBufferEntry::KindUnderlyingType>());
            MOZ_ASSERT(
                static_cast<ProfileBufferEntry::KindUnderlyingType>(kind) <
                static_cast<ProfileBufferEntry::KindUnderlyingType>(
                    ProfileBufferEntry::Kind::MODERN_LIMIT));
            if (kind == ProfileBufferEntry::Kind::CompactStack) {
              // Found our CompactStack, just make a copy of the whole entry.
              er = *it;
              auto bytes = er.RemainingBytes();
              MOZ_ASSERT(bytes < 65536);
              tempBuffer.Put(bytes, [&](ProfileBufferEntryWriter* aEW) {
                MOZ_ASSERT(aEW, "tempBuffer cannot be out-of-session");
                aEW->WriteFromReader(er, bytes);
              });
              break;
            }
            MOZ_ASSERT(kind >= ProfileBufferEntry::Kind::LEGACY_LIMIT,
                       "There should be no legacy entries between "
                       "TimeBeforeCompactStack and CompactStack");
            // Here, we have encountered a non-legacy entry that was not the
            // CompactStack we're looking for; just continue the search...
          }
          // We're done.
          return true;
        }
        case ProfileBufferEntry::Kind::CounterKey:
        case ProfileBufferEntry::Kind::Number:
        case ProfileBufferEntry::Kind::Count:
          // Don't copy anything not part of a thread's stack sample
          break;
        case ProfileBufferEntry::Kind::CounterId:
          // CounterId is normally followed by Time - if so, we'd like
          // to skip it.  If we duplicate Time, it won't hurt anything, just
          // waste buffer space (and this can happen if the CounterId has
          // fallen off the end of the buffer, but Time (and Number/Count)
          // are still in the buffer).
          e.Next();
          if (e.Has() && e.Get().GetKind() != ProfileBufferEntry::Kind::Time) {
            // this would only happen if there was an invalid sequence
            // in the buffer.  Don't skip it.
            continue;
          }
          // we've skipped Time
          break;
        case ProfileBufferEntry::Kind::ProfilerOverheadTime:
          // ProfilerOverheadTime is normally followed by
          // ProfilerOverheadDuration*4 - if so, we'd like to skip it. Don't
          // duplicate, as we are in the middle of a sampling and will soon
          // capture its own overhead.
          e.Next();
          // A missing Time would only happen if there was an invalid
          // sequence in the buffer. Don't skip unexpected entry.
          if (e.Has() &&
              e.Get().GetKind() !=
                  ProfileBufferEntry::Kind::ProfilerOverheadDuration) {
            continue;
          }
          e.Next();
          if (e.Has() &&
              e.Get().GetKind() !=
                  ProfileBufferEntry::Kind::ProfilerOverheadDuration) {
            continue;
          }
          e.Next();
          if (e.Has() &&
              e.Get().GetKind() !=
                  ProfileBufferEntry::Kind::ProfilerOverheadDuration) {
            continue;
          }
          e.Next();
          if (e.Has() &&
              e.Get().GetKind() !=
                  ProfileBufferEntry::Kind::ProfilerOverheadDuration) {
            continue;
          }
          // we've skipped ProfilerOverheadTime and
          // ProfilerOverheadDuration*4.
          break;
        default: {
          // Copy anything else we don't know about.
          AddEntry(tempBuffer, e.Get());
          break;
        }
      }
      e.Next();
    }
    return true;
  });

  if (!ok) {
    return false;
  }

  // If the buffer was big enough, there won't be any cleared blocks.
  if (tempBuffer.GetState().mClearedBlockCount != 0) {
    // No need to try to read stack again as it won't fit. Reset the stored
    // buffer position to Nothing().
    aLastSample.reset();
    return false;
  }

  aLastSample = Some(AddThreadIdEntry(aThreadId));

  mEntries.AppendContents(tempBuffer);

  return true;
}

void ProfileBuffer::DiscardSamplesBeforeTime(double aTime) {
  const ProfileBufferBlockIndex firstBlockToKeep =
      mEntries.Read([&](BlocksRingBuffer::Reader* aReader) {
        MOZ_ASSERT(aReader,
                   "BlocksRingBuffer cannot be out-of-session when sampler is "
                   "running");

        EntryGetter e(*aReader);

        const ProfileBufferBlockIndex bufferStartPos = e.CurBlockIndex();
        for (;;) {
          // This block skips entries until we find the start of the next
          // sample. This is useful in three situations.
          //
          // - The circular buffer overwrites old entries, so when we start
          //   parsing we might be in the middle of a sample, and we must skip
          //   forward to the start of the next sample.
          //
          // - We skip samples that don't have an appropriate ThreadId or Time.
          //
          // - We skip range Pause, Resume, CollectionStart, Marker, and
          //   CollectionEnd entries between samples.
          while (e.Has()) {
            if (e.Get().IsThreadId()) {
              break;
            }
            e.Next();
          }

          if (!e.Has()) {
            return bufferStartPos;
          }

          MOZ_RELEASE_ASSERT(e.Get().IsThreadId());
          const ProfileBufferBlockIndex sampleStartPos = e.CurBlockIndex();
          e.Next();

          if (e.Has() &&
              (e.Get().IsTime() || e.Get().IsTimeBeforeCompactStack())) {
            double sampleTime = e.Get().GetDouble();

            if (sampleTime >= aTime) {
              // This is the first sample within the window of time that we want
              // to keep. Throw away all samples before sampleStartPos and
              // return.
              return sampleStartPos;
            }
          }
        }
      });
  mEntries.ClearBefore(firstBlockToKeep);
}

// END ProfileBuffer
////////////////////////////////////////////////////////////////////////
