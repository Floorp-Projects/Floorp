/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfileBufferEntry.h"

#include "mozilla/ProfilerMarkers.h"
#include "platform.h"
#include "ProfileBuffer.h"
#include "ProfiledThreadData.h"
#include "ProfilerBacktrace.h"
#include "ProfilerRustBindings.h"

#include "js/ProfilingFrameIterator.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "mozilla/Logging.h"
#include "mozilla/JSONStringWriteFuncs.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StackWalk.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "ProfilerCodeAddressService.h"

#include <ostream>
#include <type_traits>

using namespace mozilla;
using namespace mozilla::literals::ProportionValue_literals;

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

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, ProfilerThreadId aThreadId)
    : mKind(aKind) {
  static_assert(std::is_trivially_copyable_v<ProfilerThreadId>);
  static_assert(sizeof(aThreadId) <= sizeof(mStorage));
  memcpy(mStorage, &aThreadId, sizeof(aThreadId));
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

ProfilerThreadId ProfileBufferEntry::GetThreadId() const {
  ProfilerThreadId result;
  static_assert(std::is_trivially_copyable_v<ProfilerThreadId>);
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
  explicit AutoArraySchemaWriter(SpliceableJSONWriter& aWriter)
      : mJSONWriter(aWriter), mNextFreeIndex(0) {
    mJSONWriter.StartArrayElement();
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

  void TimeMsElement(uint32_t aIndex, double aTime_ms) {
    FillUpTo(aIndex);
    mJSONWriter.TimeDoubleMsElement(aTime_ms);
  }

  void BoolElement(uint32_t aIndex, bool aValue) {
    FillUpTo(aIndex);
    mJSONWriter.BoolElement(aValue);
  }

 protected:
  SpliceableJSONWriter& Writer() { return mJSONWriter; }

  void FillUpTo(uint32_t aIndex) {
    MOZ_ASSERT(aIndex >= mNextFreeIndex);
    mJSONWriter.NullElements(aIndex - mNextFreeIndex);
    mNextFreeIndex = aIndex + 1;
  }

 private:
  SpliceableJSONWriter& mJSONWriter;
  uint32_t mNextFreeIndex;
};

// Same as AutoArraySchemaWriter, but this can also write strings (output as
// indexes into the table of unique strings).
class MOZ_RAII AutoArraySchemaWithStringsWriter : public AutoArraySchemaWriter {
 public:
  AutoArraySchemaWithStringsWriter(SpliceableJSONWriter& aWriter,
                                   UniqueJSONStrings& aStrings)
      : AutoArraySchemaWriter(aWriter), mStrings(aStrings) {}

  void StringElement(uint32_t aIndex, const Span<const char>& aValue) {
    FillUpTo(aIndex);
    mStrings.WriteElement(Writer(), aValue);
  }

 private:
  UniqueJSONStrings& mStrings;
};

Maybe<UniqueStacks::StackKey> UniqueStacks::BeginStack(const FrameKey& aFrame) {
  if (Maybe<uint32_t> frameIndex = GetOrAddFrameIndex(aFrame); frameIndex) {
    return Some(StackKey(*frameIndex));
  }
  return Nothing{};
}

Vector<JITFrameInfoForBufferRange>&&
JITFrameInfo::MoveRangesWithNewFailureLatch(FailureLatch& aFailureLatch) && {
  aFailureLatch.SetFailureFrom(mLocalFailureLatchSource);
  return std::move(mRanges);
}

UniquePtr<UniqueJSONStrings>&&
JITFrameInfo::MoveUniqueStringsWithNewFailureLatch(
    FailureLatch& aFailureLatch) && {
  if (mUniqueStrings) {
    mUniqueStrings->ChangeFailureLatchAndForwardState(aFailureLatch);
  } else {
    aFailureLatch.SetFailureFrom(mLocalFailureLatchSource);
  }
  return std::move(mUniqueStrings);
}

Maybe<UniqueStacks::StackKey> UniqueStacks::AppendFrame(
    const StackKey& aStack, const FrameKey& aFrame) {
  if (Maybe<uint32_t> stackIndex = GetOrAddStackIndex(aStack); stackIndex) {
    if (Maybe<uint32_t> frameIndex = GetOrAddFrameIndex(aFrame); frameIndex) {
      return Some(StackKey(aStack, *stackIndex, *frameIndex));
    }
  }
  return Nothing{};
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

JITFrameInfo::JITFrameInfo(const JITFrameInfo& aOther,
                           mozilla::ProgressLogger aProgressLogger)
    : mUniqueStrings(MakeUniqueFallible<UniqueJSONStrings>(
          mLocalFailureLatchSource, *aOther.mUniqueStrings,
          aProgressLogger.CreateSubLoggerFromTo(
              0_pc, "Creating JIT frame info unique strings...", 49_pc,
              "Created JIT frame info unique strings"))) {
  if (!mUniqueStrings) {
    mLocalFailureLatchSource.SetFailure(
        "OOM in JITFrameInfo allocating mUniqueStrings");
    return;
  }

  if (mRanges.reserve(aOther.mRanges.length())) {
    for (auto&& [i, progressLogger] :
         aProgressLogger.CreateLoopSubLoggersFromTo(50_pc, 100_pc,
                                                    aOther.mRanges.length(),
                                                    "Copying JIT frame info")) {
      mRanges.infallibleAppend(aOther.mRanges[i].Clone());
    }
  } else {
    mLocalFailureLatchSource.SetFailure("OOM in JITFrameInfo resizing mRanges");
  }
}

bool UniqueStacks::FrameKey::NormalFrameData::operator==(
    const NormalFrameData& aOther) const {
  return mLocation == aOther.mLocation &&
         mRelevantForJS == aOther.mRelevantForJS &&
         mBaselineInterp == aOther.mBaselineInterp &&
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
UniqueStacks::UniqueStacks(
    FailureLatch& aFailureLatch, JITFrameInfo&& aJITFrameInfo,
    ProfilerCodeAddressService* aCodeAddressService /* = nullptr */)
    : mUniqueStrings(std::move(aJITFrameInfo)
                         .MoveUniqueStringsWithNewFailureLatch(aFailureLatch)),
      mCodeAddressService(aCodeAddressService),
      mFrameTableWriter(aFailureLatch),
      mStackTableWriter(aFailureLatch),
      mJITInfoRanges(std::move(aJITFrameInfo)
                         .MoveRangesWithNewFailureLatch(aFailureLatch)) {
  if (!mUniqueStrings) {
    SetFailure("Did not get mUniqueStrings from JITFrameInfo");
    return;
  }

  mFrameTableWriter.StartBareList();
  mStackTableWriter.StartBareList();
}

Maybe<uint32_t> UniqueStacks::GetOrAddStackIndex(const StackKey& aStack) {
  if (Failed()) {
    return Nothing{};
  }

  uint32_t count = mStackToIndexMap.count();
  auto entry = mStackToIndexMap.lookupForAdd(aStack);
  if (entry) {
    MOZ_ASSERT(entry->value() < count);
    return Some(entry->value());
  }

  if (!mStackToIndexMap.add(entry, aStack, count)) {
    SetFailure("OOM in UniqueStacks::GetOrAddStackIndex");
    return Nothing{};
  }
  StreamStack(aStack);
  return Some(count);
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
      mFrameTableWriter.Splice(frameJSON->value());
      MOZ_RELEASE_ASSERT(mFrameToIndexMap.add(entry, frameKey, index));
    }
    MOZ_RELEASE_ASSERT(frameKeys.append(std::move(frameKey)));
  }
  return Some(std::move(frameKeys));
}

Maybe<uint32_t> UniqueStacks::GetOrAddFrameIndex(const FrameKey& aFrame) {
  if (Failed()) {
    return Nothing{};
  }

  uint32_t count = mFrameToIndexMap.count();
  auto entry = mFrameToIndexMap.lookupForAdd(aFrame);
  if (entry) {
    MOZ_ASSERT(entry->value() < count);
    return Some(entry->value());
  }

  if (!mFrameToIndexMap.add(entry, aFrame, count)) {
    SetFailure("OOM in UniqueStacks::GetOrAddFrameIndex");
    return Nothing{};
  }
  StreamNonJITFrame(aFrame);
  return Some(count);
}

void UniqueStacks::SpliceFrameTableElements(SpliceableJSONWriter& aWriter) {
  mFrameTableWriter.EndBareList();
  aWriter.TakeAndSplice(mFrameTableWriter.TakeChunkedWriteFunc());
}

void UniqueStacks::SpliceStackTableElements(SpliceableJSONWriter& aWriter) {
  mStackTableWriter.EndBareList();
  aWriter.TakeAndSplice(mStackTableWriter.TakeChunkedWriteFunc());
}

[[nodiscard]] nsAutoCString UniqueStacks::FunctionNameOrAddress(void* aPC) {
  nsAutoCString nameOrAddress;

  if (!mCodeAddressService ||
      !mCodeAddressService->GetFunction(aPC, nameOrAddress) ||
      nameOrAddress.IsEmpty()) {
    nameOrAddress.AppendASCII("0x");
    // `AppendInt` only knows `uint32_t` or `uint64_t`, but because these are
    // just aliases for *two* of (`unsigned`, `unsigned long`, and `unsigned
    // long long`), a call with `uintptr_t` could use the third type and
    // therefore would be ambiguous.
    // So we want to force using exactly `uint32_t` or `uint64_t`, whichever
    // matches the size of `uintptr_t`.
    // (The outer cast to `uint` should then be a no-op.)
    using uint = std::conditional_t<sizeof(uintptr_t) <= sizeof(uint32_t),
                                    uint32_t, uint64_t>;
    nameOrAddress.AppendInt(static_cast<uint>(reinterpret_cast<uintptr_t>(aPC)),
                            16);
  }

  return nameOrAddress;
}

void UniqueStacks::StreamStack(const StackKey& aStack) {
  enum Schema : uint32_t { PREFIX = 0, FRAME = 1 };

  AutoArraySchemaWriter writer(mStackTableWriter);
  if (aStack.mPrefixStackIndex.isSome()) {
    writer.IntElement(PREFIX, *aStack.mPrefixStackIndex);
  }
  writer.IntElement(FRAME, aStack.mFrameIndex);
}

void UniqueStacks::StreamNonJITFrame(const FrameKey& aFrame) {
  if (Failed()) {
    return;
  }

  using NormalFrameData = FrameKey::NormalFrameData;

  enum Schema : uint32_t {
    LOCATION = 0,
    RELEVANT_FOR_JS = 1,
    INNER_WINDOW_ID = 2,
    IMPLEMENTATION = 3,
    LINE = 4,
    COLUMN = 5,
    CATEGORY = 6,
    SUBCATEGORY = 7
  };

  AutoArraySchemaWithStringsWriter writer(mFrameTableWriter, *mUniqueStrings);

  const NormalFrameData& data = aFrame.mData.as<NormalFrameData>();
  writer.StringElement(LOCATION, data.mLocation);
  writer.BoolElement(RELEVANT_FOR_JS, data.mRelevantForJS);

  // It's okay to convert uint64_t to double here because DOM always creates IDs
  // that are convertible to double.
  writer.DoubleElement(INNER_WINDOW_ID, data.mInnerWindowID);

  // The C++ interpreter is the default implementation so we only emit element
  // for Baseline Interpreter frames.
  if (data.mBaselineInterp) {
    writer.StringElement(IMPLEMENTATION, MakeStringSpan("blinterp"));
  }

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
    LINE = 4,
    COLUMN = 5,
    CATEGORY = 6,
    SUBCATEGORY = 7
  };

  AutoArraySchemaWithStringsWriter writer(aWriter, aUniqueStrings);

  writer.StringElement(LOCATION, MakeStringSpan(aJITFrame.label()));
  writer.BoolElement(RELEVANT_FOR_JS, false);

  // It's okay to convert uint64_t to double here because DOM always creates IDs
  // that are convertible to double.
  // Realm ID is the name of innerWindowID inside JS code.
  writer.DoubleElement(INNER_WINDOW_ID, aJITFrame.realmID());

  JS::ProfilingFrameIterator::FrameKind frameKind = aJITFrame.frameKind();
  MOZ_ASSERT(frameKind == JS::ProfilingFrameIterator::Frame_Ion ||
             frameKind == JS::ProfilingFrameIterator::Frame_Baseline);
  writer.StringElement(IMPLEMENTATION,
                       frameKind == JS::ProfilingFrameIterator::Frame_Ion
                           ? MakeStringSpan("ion")
                           : MakeStringSpan("baseline"));

  const JS::ProfilingCategoryPairInfo& info = JS::GetProfilingCategoryPairInfo(
      frameKind == JS::ProfilingFrameIterator::Frame_Ion
          ? JS::ProfilingCategoryPair::JS_IonMonkey
          : JS::ProfilingCategoryPair::JS_Baseline);
  writer.IntElement(CATEGORY, uint32_t(info.mCategory));
  writer.IntElement(SUBCATEGORY, info.mSubcategoryIndex);
}

static nsCString JSONForJITFrame(JSContext* aContext,
                                 const JS::ProfiledFrameHandle& aJITFrame,
                                 UniqueJSONStrings& aUniqueStrings) {
  nsCString json;
  JSONStringRefWriteFunc jw(json);
  SpliceableJSONWriter writer(jw, aUniqueStrings.SourceFailureLatch());
  StreamJITFrame(aContext, writer, aUniqueStrings, aJITFrame);
  return json;
}

void JITFrameInfo::AddInfoForRange(
    uint64_t aRangeStart, uint64_t aRangeEnd, JSContext* aCx,
    const std::function<void(const std::function<void(void*)>&)>&
        aJITAddressProvider) {
  if (mLocalFailureLatchSource.Failed()) {
    return;
  }

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
          if (!jitFrameToFrameJSONMap.add(
                  frameEntry, jitFrameKey,
                  JSONForJITFrame(aCx, handle, *mUniqueStrings))) {
            mLocalFailureLatchSource.SetFailure(
                "OOM in JITFrameInfo::AddInfoForRange adding jit->frame map");
            return;
          }
        }
        if (!jitFrameKeys.append(jitFrameKey)) {
          mLocalFailureLatchSource.SetFailure(
              "OOM in JITFrameInfo::AddInfoForRange adding jit frame key");
          return;
        }
      }
      if (!jitAddressToJITFrameMap.add(addressEntry, aJITAddress,
                                       std::move(jitFrameKeys))) {
        mLocalFailureLatchSource.SetFailure(
            "OOM in JITFrameInfo::AddInfoForRange adding addr->jit map");
        return;
      }
    }
  });

  if (!mRanges.append(JITFrameInfoForBufferRange{
          aRangeStart, aRangeEnd, std::move(jitAddressToJITFrameMap),
          std::move(jitFrameToFrameJSONMap)})) {
    mLocalFailureLatchSource.SetFailure(
        "OOM in JITFrameInfo::AddInfoForRange adding range");
    return;
  }
}

struct ProfileSample {
  uint32_t mStack = 0;
  double mTime = 0.0;
  Maybe<double> mResponsiveness;
  RunningTimes mRunningTimes;
};

// Write CPU measurements with "Delta" unit, which is some amount of work that
// happened since the previous sample.
static void WriteDelta(AutoArraySchemaWriter& aSchemaWriter, uint32_t aProperty,
                       uint64_t aDelta) {
  aSchemaWriter.IntElement(aProperty, int64_t(aDelta));
}

static void WriteSample(SpliceableJSONWriter& aWriter,
                        const ProfileSample& aSample) {
  enum Schema : uint32_t {
    STACK = 0,
    TIME = 1,
    EVENT_DELAY = 2
#define RUNNING_TIME_SCHEMA(index, name, unit, jsonProperty) , name
    PROFILER_FOR_EACH_RUNNING_TIME(RUNNING_TIME_SCHEMA)
#undef RUNNING_TIME_SCHEMA
  };

  AutoArraySchemaWriter writer(aWriter);

  writer.IntElement(STACK, aSample.mStack);

  writer.TimeMsElement(TIME, aSample.mTime);

  if (aSample.mResponsiveness.isSome()) {
    writer.DoubleElement(EVENT_DELAY, *aSample.mResponsiveness);
  }

#define RUNNING_TIME_STREAM(index, name, unit, jsonProperty) \
  aSample.mRunningTimes.GetJson##name##unit().apply(         \
      [&writer](const uint64_t& aValue) {                    \
        Write##unit(writer, name, aValue);                   \
      });

  PROFILER_FOR_EACH_RUNNING_TIME(RUNNING_TIME_STREAM)

#undef RUNNING_TIME_STREAM
}

static void StreamMarkerAfterKind(
    ProfileBufferEntryReader& aER,
    ProcessStreamingContext& aProcessStreamingContext) {
  ThreadStreamingContext* threadData = nullptr;
  mozilla::base_profiler_markers_detail::DeserializeAfterKindAndStream(
      aER,
      [&](ProfilerThreadId aThreadId) -> baseprofiler::SpliceableJSONWriter* {
        threadData =
            aProcessStreamingContext.GetThreadStreamingContext(aThreadId);
        return threadData ? &threadData->mMarkersDataWriter : nullptr;
      },
      [&](ProfileChunkedBuffer& aChunkedBuffer) {
        ProfilerBacktrace backtrace("", &aChunkedBuffer);
        MOZ_ASSERT(threadData,
                   "threadData should have been set before calling here");
        backtrace.StreamJSON(threadData->mMarkersDataWriter,
                             aProcessStreamingContext.ProcessStartTime(),
                             *threadData->mUniqueStacks);
      },
      [&](mozilla::base_profiler_markers_detail::Streaming::DeserializerTag
              aTag) {
        MOZ_ASSERT(threadData,
                   "threadData should have been set before calling here");

        size_t payloadSize = aER.RemainingBytes();

        ProfileBufferEntryReader::DoubleSpanOfConstBytes spans =
            aER.ReadSpans(payloadSize);
        if (MOZ_LIKELY(spans.IsSingleSpan())) {
          // Only a single span, we can just refer to it directly
          // instead of copying it.
          profiler::ffi::gecko_profiler_serialize_marker_for_tag(
              aTag, spans.mFirstOrOnly.Elements(), payloadSize,
              &threadData->mMarkersDataWriter);
        } else {
          // Two spans, we need to concatenate them by copying.
          uint8_t* payloadBuffer = new uint8_t[payloadSize];
          spans.CopyBytesTo(payloadBuffer);
          profiler::ffi::gecko_profiler_serialize_marker_for_tag(
              aTag, payloadBuffer, payloadSize,
              &threadData->mMarkersDataWriter);
          delete[] payloadBuffer;
        }
      });
}

class EntryGetter {
 public:
  explicit EntryGetter(
      ProfileChunkedBuffer::Reader& aReader,
      mozilla::FailureLatch& aFailureLatch,
      mozilla::ProgressLogger aProgressLogger = {},
      uint64_t aInitialReadPos = 0,
      ProcessStreamingContext* aStreamingContextForMarkers = nullptr)
      : mFailureLatch(aFailureLatch),
        mStreamingContextForMarkers(aStreamingContextForMarkers),
        mBlockIt(
            aReader.At(ProfileBufferBlockIndex::CreateFromProfileBufferIndex(
                aInitialReadPos))),
        mBlockItEnd(aReader.end()),
        mRangeStart(mBlockIt.BufferRangeStart().ConvertToProfileBufferIndex()),
        mRangeSize(
            double(mBlockIt.BufferRangeEnd().ConvertToProfileBufferIndex() -
                   mRangeStart)),
        mProgressLogger(std::move(aProgressLogger)) {
    SetLocalProgress(ProgressLogger::NO_LOCATION_UPDATE);
    if (!ReadLegacyOrEnd()) {
      // Find and read the next non-legacy entry.
      Next();
    }
  }

  bool Has() const {
    return (!mFailureLatch.Failed()) && (mBlockIt != mBlockItEnd);
  }

  const ProfileBufferEntry& Get() const {
    MOZ_ASSERT(Has() || mFailureLatch.Failed(),
               "Caller should have checked `Has()` before `Get()`");
    return mEntry;
  }

  void Next() {
    MOZ_ASSERT(Has() || mFailureLatch.Failed(),
               "Caller should have checked `Has()` before `Next()`");
    ++mBlockIt;
    ReadUntilLegacyOrEnd();
  }

  // Hand off the current iterator to the caller, which may be used to read
  // any kind of entries (legacy or modern).
  ProfileChunkedBuffer::BlockIterator Iterator() const { return mBlockIt; }

  // After `Iterator()` was used, we can restart from *after* its updated
  // position.
  void RestartAfter(const ProfileChunkedBuffer::BlockIterator& it) {
    mBlockIt = it;
    if (!Has()) {
      return;
    }
    Next();
  }

  ProfileBufferBlockIndex CurBlockIndex() const {
    return mBlockIt.CurrentBlockIndex();
  }

  uint64_t CurPos() const {
    return CurBlockIndex().ConvertToProfileBufferIndex();
  }

  void SetLocalProgress(const char* aLocation) {
    mProgressLogger.SetLocalProgress(
        ProportionValue{double(CurBlockIndex().ConvertToProfileBufferIndex() -
                               mRangeStart) /
                        mRangeSize},
        aLocation);
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
    ProfileBufferEntryReader er = *mBlockIt;
    auto type = static_cast<ProfileBufferEntry::Kind>(
        er.ReadObject<ProfileBufferEntry::KindUnderlyingType>());
    MOZ_ASSERT(static_cast<ProfileBufferEntry::KindUnderlyingType>(type) <
               static_cast<ProfileBufferEntry::KindUnderlyingType>(
                   ProfileBufferEntry::Kind::MODERN_LIMIT));
    if (type >= ProfileBufferEntry::Kind::LEGACY_LIMIT) {
      if (type == ProfileBufferEntry::Kind::Marker &&
          mStreamingContextForMarkers) {
        StreamMarkerAfterKind(er, *mStreamingContextForMarkers);
        if (!Has()) {
          return true;
        }
        SetLocalProgress("Processed marker");
      }
      er.SetRemainingBytes(0);
      return false;
    }
    // Here, we have a legacy item, we need to read it from the start.
    // Because the above `ReadObject` moved the reader, we ned to reset it to
    // the start of the entry before reading the whole entry.
    er = *mBlockIt;
    er.ReadBytes(&mEntry, er.RemainingBytes());
    return true;
  }

  void ReadUntilLegacyOrEnd() {
    for (;;) {
      if (ReadLegacyOrEnd()) {
        // Either we're at the end, or we could read a legacy entry -> Done.
        break;
      }
      // Otherwise loop around until we hit a legacy entry or the end.
      ++mBlockIt;
    }
    SetLocalProgress(ProgressLogger::NO_LOCATION_UPDATE);
  }

  mozilla::FailureLatch& mFailureLatch;

  ProcessStreamingContext* const mStreamingContextForMarkers;

  ProfileBufferEntry mEntry;
  ProfileChunkedBuffer::BlockIterator mBlockIt;
  const ProfileChunkedBuffer::BlockIterator mBlockItEnd;

  // Progress logger, and the data needed to compute the current relative
  // position in the buffer.
  const mozilla::ProfileBufferIndex mRangeStart;
  const double mRangeSize;
  mozilla::ProgressLogger mProgressLogger;
};

// The following grammar shows legal sequences of profile buffer entries.
// The sequences beginning with a ThreadId entry are known as "samples".
//
// (
//   ( /* Samples */
//     ThreadId
//     TimeBeforeCompactStack
//     RunningTimes?
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
//   | ( /* Reference to a previous identical sample */
//       ThreadId
//       TimeBeforeSameSample
//       RunningTimes?
//       SameSample
//     )
//   | Marker
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

struct StreamingParametersForThread {
  SpliceableJSONWriter& mWriter;
  UniqueStacks& mUniqueStacks;
  ThreadStreamingContext::PreviousStackState& mPreviousStackState;
  uint32_t& mPreviousStack;

  StreamingParametersForThread(
      SpliceableJSONWriter& aWriter, UniqueStacks& aUniqueStacks,
      ThreadStreamingContext::PreviousStackState& aPreviousStackState,
      uint32_t& aPreviousStack)
      : mWriter(aWriter),
        mUniqueStacks(aUniqueStacks),
        mPreviousStackState(aPreviousStackState),
        mPreviousStack(aPreviousStack) {}
};

// GetStreamingParametersForThreadCallback:
//   (ProfilerThreadId) -> Maybe<StreamingParametersForThread>
template <typename GetStreamingParametersForThreadCallback>
ProfilerThreadId ProfileBuffer::DoStreamSamplesAndMarkersToJSON(
    mozilla::FailureLatch& aFailureLatch,
    GetStreamingParametersForThreadCallback&&
        aGetStreamingParametersForThreadCallback,
    double aSinceTime, ProcessStreamingContext* aStreamingContextForMarkers,
    mozilla::ProgressLogger aProgressLogger) const {
  UniquePtr<char[]> dynStrBuf = MakeUnique<char[]>(kMaxFrameKeyLength);

  return mEntries.Read([&](ProfileChunkedBuffer::Reader* aReader) {
    MOZ_ASSERT(aReader,
               "ProfileChunkedBuffer cannot be out-of-session when sampler is "
               "running");

    ProfilerThreadId processedThreadId;

    EntryGetter e(*aReader, aFailureLatch, std::move(aProgressLogger),
                  /* aInitialReadPos */ 0, aStreamingContextForMarkers);

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

      // Due to the skip_to_next_sample block above, if we have an entry here it
      // must be a ThreadId entry.
      MOZ_ASSERT(e.Get().IsThreadId());

      ProfilerThreadId threadId = e.Get().GetThreadId();
      e.Next();

      Maybe<StreamingParametersForThread> streamingParameters =
          std::forward<GetStreamingParametersForThreadCallback>(
              aGetStreamingParametersForThreadCallback)(threadId);

      // Ignore samples that are for the wrong thread.
      if (!streamingParameters) {
        continue;
      }

      SpliceableJSONWriter& writer = streamingParameters->mWriter;
      UniqueStacks& uniqueStacks = streamingParameters->mUniqueStacks;
      ThreadStreamingContext::PreviousStackState& previousStackState =
          streamingParameters->mPreviousStackState;
      uint32_t& previousStack = streamingParameters->mPreviousStack;

      auto ReadStack = [&](EntryGetter& e, double time, uint64_t entryPosition,
                           const Maybe<double>& unresponsiveDuration,
                           const RunningTimes& runningTimes) {
        if (writer.Failed()) {
          return;
        }

        Maybe<UniqueStacks::StackKey> maybeStack =
            uniqueStacks.BeginStack(UniqueStacks::FrameKey("(root)"));
        if (!maybeStack) {
          writer.SetFailure("BeginStack failure");
          return;
        }

        UniqueStacks::StackKey stack = *maybeStack;

        int numFrames = 0;
        while (e.Has()) {
          if (e.Get().IsNativeLeafAddr()) {
            numFrames++;

            void* pc = e.Get().GetPtr();
            e.Next();

            nsAutoCString functionNameOrAddress =
                uniqueStacks.FunctionNameOrAddress(pc);

            maybeStack = uniqueStacks.AppendFrame(
                stack, UniqueStacks::FrameKey(functionNameOrAddress.get()));
            if (!maybeStack) {
              writer.SetFailure("AppendFrame failure");
              return;
            }
            stack = *maybeStack;

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

            bool isBaselineInterp =
                frameFlags & uint32_t(FrameFlags::IS_BLINTERP_FRAME);

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

            maybeStack = uniqueStacks.AppendFrame(
                stack,
                UniqueStacks::FrameKey(std::move(frameLabel), relevantForJS,
                                       isBaselineInterp, innerWindowID, line,
                                       column, categoryPair));
            if (!maybeStack) {
              writer.SetFailure("AppendFrame failure");
              return;
            }
            stack = *maybeStack;

          } else if (e.Get().IsJitReturnAddr()) {
            numFrames++;

            // A JIT frame may expand to multiple frames due to inlining.
            void* pc = e.Get().GetPtr();
            const Maybe<Vector<UniqueStacks::FrameKey>>& frameKeys =
                uniqueStacks.LookupFramesForJITAddressFromBufferPos(
                    pc, entryPosition ? entryPosition : e.CurPos());
            MOZ_RELEASE_ASSERT(
                frameKeys,
                "Attempting to stream samples for a buffer range "
                "for which we don't have JITFrameInfo?");
            for (const UniqueStacks::FrameKey& frameKey : *frameKeys) {
              maybeStack = uniqueStacks.AppendFrame(stack, frameKey);
              if (!maybeStack) {
                writer.SetFailure("AppendFrame failure");
                return;
              }
              stack = *maybeStack;
            }

            e.Next();

          } else {
            break;
          }
        }

        // Even if this stack is considered empty, it contains the root frame,
        // which needs to be in the JSON output because following "same samples"
        // may refer to it when reusing this sample.mStack.
        const Maybe<uint32_t> stackIndex =
            uniqueStacks.GetOrAddStackIndex(stack);
        if (!stackIndex) {
          writer.SetFailure("Can't add unique string for stack");
          return;
        }

        // And store that possibly-empty stack in case it's followed by "same
        // sample" entries.
        previousStack = *stackIndex;
        previousStackState = (numFrames == 0)
                                 ? ThreadStreamingContext::eStackWasEmpty
                                 : ThreadStreamingContext::eStackWasNotEmpty;

        // Even if too old or empty, we did process a sample for this thread id.
        processedThreadId = threadId;

        // Discard samples that are too old.
        if (time < aSinceTime) {
          return;
        }

        if (numFrames == 0 && runningTimes.IsEmpty()) {
          // It is possible to have empty stacks if native stackwalking is
          // disabled. Skip samples with empty stacks, unless we have useful
          // running times.
          return;
        }

        WriteSample(writer, ProfileSample{*stackIndex, time,
                                          unresponsiveDuration, runningTimes});
      };  // End of `ReadStack(EntryGetter&)` lambda.

      if (e.Has() && e.Get().IsTime()) {
        double time = e.Get().GetDouble();
        e.Next();
        // Note: Even if this sample is too old (before aSinceTime), we still
        // need to read it, so that its frames are in the tables, in case there
        // is a same-sample following it that would be after aSinceTime, which
        // would need these frames to be present.

        ReadStack(e, time, 0, Nothing{}, RunningTimes{});

        e.SetLocalProgress("Processed sample");
      } else if (e.Has() && e.Get().IsTimeBeforeCompactStack()) {
        double time = e.Get().GetDouble();
        // Note: Even if this sample is too old (before aSinceTime), we still
        // need to read it, so that its frames are in the tables, in case there
        // is a same-sample following it that would be after aSinceTime, which
        // would need these frames to be present.

        RunningTimes runningTimes;
        Maybe<double> unresponsiveDuration;

        ProfileChunkedBuffer::BlockIterator it = e.Iterator();
        for (;;) {
          ++it;
          if (it.IsAtEnd()) {
            break;
          }
          ProfileBufferEntryReader er = *it;
          ProfileBufferEntry::Kind kind =
              er.ReadObject<ProfileBufferEntry::Kind>();

          // There may be running times before the CompactStack.
          if (kind == ProfileBufferEntry::Kind::RunningTimes) {
            er.ReadIntoObject(runningTimes);
            continue;
          }

          // There may be an UnresponsiveDurationMs before the CompactStack.
          if (kind == ProfileBufferEntry::Kind::UnresponsiveDurationMs) {
            unresponsiveDuration = Some(er.ReadObject<double>());
            continue;
          }

          if (kind == ProfileBufferEntry::Kind::CompactStack) {
            ProfileChunkedBuffer tempBuffer(
                ProfileChunkedBuffer::ThreadSafety::WithoutMutex,
                WorkerChunkManager());
            er.ReadIntoObject(tempBuffer);
            tempBuffer.Read([&](ProfileChunkedBuffer::Reader* aReader) {
              MOZ_ASSERT(aReader,
                         "Local ProfileChunkedBuffer cannot be out-of-session");
              // This is a compact stack, it should only contain one sample.
              EntryGetter stackEntryGetter(*aReader, aFailureLatch);
              ReadStack(stackEntryGetter, time,
                        it.CurrentBlockIndex().ConvertToProfileBufferIndex(),
                        unresponsiveDuration, runningTimes);
            });
            WorkerChunkManager().Reset(tempBuffer.GetAllChunks());
            break;
          }

          if (kind == ProfileBufferEntry::Kind::Marker &&
              aStreamingContextForMarkers) {
            StreamMarkerAfterKind(er, *aStreamingContextForMarkers);
            continue;
          }

          MOZ_ASSERT(kind >= ProfileBufferEntry::Kind::LEGACY_LIMIT,
                     "There should be no legacy entries between "
                     "TimeBeforeCompactStack and CompactStack");
          er.SetRemainingBytes(0);
        }

        e.RestartAfter(it);

        e.SetLocalProgress("Processed compact sample");
      } else if (e.Has() && e.Get().IsTimeBeforeSameSample()) {
        if (previousStackState == ThreadStreamingContext::eNoStackYet) {
          // We don't have any full sample yet, we cannot duplicate a "previous"
          // one. This should only happen at most once per thread, for the very
          // first sample.
          continue;
        }

        ProfileSample sample;

        // Keep the same `mStack` as previously output.
        // Note that it may be empty, this is checked below before writing it.
        sample.mStack = previousStack;

        sample.mTime = e.Get().GetDouble();

        // Ignore samples that are too old.
        if (sample.mTime < aSinceTime) {
          e.Next();
          continue;
        }

        sample.mResponsiveness = Nothing{};

        sample.mRunningTimes.Clear();

        ProfileChunkedBuffer::BlockIterator it = e.Iterator();
        for (;;) {
          ++it;
          if (it.IsAtEnd()) {
            break;
          }
          ProfileBufferEntryReader er = *it;
          ProfileBufferEntry::Kind kind =
              er.ReadObject<ProfileBufferEntry::Kind>();

          // There may be running times before the SameSample.
          if (kind == ProfileBufferEntry::Kind::RunningTimes) {
            er.ReadIntoObject(sample.mRunningTimes);
            continue;
          }

          if (kind == ProfileBufferEntry::Kind::SameSample) {
            if (previousStackState == ThreadStreamingContext::eStackWasEmpty &&
                sample.mRunningTimes.IsEmpty()) {
              // Skip samples with empty stacks, unless we have useful running
              // times.
              break;
            }
            WriteSample(writer, sample);
            break;
          }

          if (kind == ProfileBufferEntry::Kind::Marker &&
              aStreamingContextForMarkers) {
            StreamMarkerAfterKind(er, *aStreamingContextForMarkers);
            continue;
          }

          MOZ_ASSERT(kind >= ProfileBufferEntry::Kind::LEGACY_LIMIT,
                     "There should be no legacy entries between "
                     "TimeBeforeSameSample and SameSample");
          er.SetRemainingBytes(0);
        }

        e.RestartAfter(it);

        e.SetLocalProgress("Processed repeated sample");
      } else {
        ERROR_AND_CONTINUE("expected a Time entry");
      }
    }

    return processedThreadId;
  });
}

ProfilerThreadId ProfileBuffer::StreamSamplesToJSON(
    SpliceableJSONWriter& aWriter, ProfilerThreadId aThreadId,
    double aSinceTime, UniqueStacks& aUniqueStacks,
    mozilla::ProgressLogger aProgressLogger) const {
  ThreadStreamingContext::PreviousStackState previousStackState =
      ThreadStreamingContext::eNoStackYet;
  uint32_t stack = 0u;
#ifdef DEBUG
  int processedCount = 0;
#endif  // DEBUG
  return DoStreamSamplesAndMarkersToJSON(
      aWriter.SourceFailureLatch(),
      [&](ProfilerThreadId aReadThreadId) {
        Maybe<StreamingParametersForThread> streamingParameters;
#ifdef DEBUG
        ++processedCount;
        MOZ_ASSERT(
            aThreadId.IsSpecified() ||
                (processedCount == 1 && aReadThreadId.IsSpecified()),
            "Unspecified aThreadId should only be used with 1-sample buffer");
#endif  // DEBUG
        if (!aThreadId.IsSpecified() || aThreadId == aReadThreadId) {
          streamingParameters.emplace(aWriter, aUniqueStacks,
                                      previousStackState, stack);
        }
        return streamingParameters;
      },
      aSinceTime, /* aStreamingContextForMarkers */ nullptr,
      std::move(aProgressLogger));
}

void ProfileBuffer::StreamSamplesAndMarkersToJSON(
    ProcessStreamingContext& aProcessStreamingContext,
    mozilla::ProgressLogger aProgressLogger) const {
  (void)DoStreamSamplesAndMarkersToJSON(
      aProcessStreamingContext.SourceFailureLatch(),
      [&](ProfilerThreadId aReadThreadId) {
        Maybe<StreamingParametersForThread> streamingParameters;
        ThreadStreamingContext* threadData =
            aProcessStreamingContext.GetThreadStreamingContext(aReadThreadId);
        if (threadData) {
          streamingParameters.emplace(
              threadData->mSamplesDataWriter, *threadData->mUniqueStacks,
              threadData->mPreviousStackState, threadData->mPreviousStack);
        }
        return streamingParameters;
      },
      aProcessStreamingContext.GetSinceTime(), &aProcessStreamingContext,
      std::move(aProgressLogger));
}

void ProfileBuffer::AddJITInfoForRange(
    uint64_t aRangeStart, ProfilerThreadId aThreadId, JSContext* aContext,
    JITFrameInfo& aJITFrameInfo,
    mozilla::ProgressLogger aProgressLogger) const {
  // We can only process JitReturnAddr entries if we have a JSContext.
  MOZ_RELEASE_ASSERT(aContext);

  aRangeStart = std::max(aRangeStart, BufferRangeStart());
  aJITFrameInfo.AddInfoForRange(
      aRangeStart, BufferRangeEnd(), aContext,
      [&](const std::function<void(void*)>& aJITAddressConsumer) {
        // Find all JitReturnAddr entries in the given range for the given
        // thread, and call aJITAddressConsumer with those addresses.

        mEntries.Read([&](ProfileChunkedBuffer::Reader* aReader) {
          MOZ_ASSERT(aReader,
                     "ProfileChunkedBuffer cannot be out-of-session when "
                     "sampler is running");

          EntryGetter e(*aReader, aJITFrameInfo.LocalFailureLatchSource(),
                        std::move(aProgressLogger), aRangeStart);

          while (true) {
            // Advance to the next ThreadId entry.
            while (e.Has() && !e.Get().IsThreadId()) {
              e.Next();
            }
            if (!e.Has()) {
              break;
            }

            MOZ_ASSERT(e.Get().IsThreadId());
            ProfilerThreadId threadId = e.Get().GetThreadId();
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
              ProfileChunkedBuffer::BlockIterator it = e.Iterator();
              for (;;) {
                ++it;
                if (it.IsAtEnd()) {
                  break;
                }
                ProfileBufferEntryReader er = *it;
                ProfileBufferEntry::Kind kind =
                    er.ReadObject<ProfileBufferEntry::Kind>();
                if (kind == ProfileBufferEntry::Kind::CompactStack) {
                  ProfileChunkedBuffer tempBuffer(
                      ProfileChunkedBuffer::ThreadSafety::WithoutMutex,
                      WorkerChunkManager());
                  er.ReadIntoObject(tempBuffer);
                  tempBuffer.Read([&](ProfileChunkedBuffer::Reader* aReader) {
                    MOZ_ASSERT(
                        aReader,
                        "Local ProfileChunkedBuffer cannot be out-of-session");
                    EntryGetter stackEntryGetter(
                        *aReader, aJITFrameInfo.LocalFailureLatchSource());
                    while (stackEntryGetter.Has()) {
                      if (stackEntryGetter.Get().IsJitReturnAddr()) {
                        aJITAddressConsumer(stackEntryGetter.Get().GetPtr());
                      }
                      stackEntryGetter.Next();
                    }
                  });
                  WorkerChunkManager().Reset(tempBuffer.GetAllChunks());
                  break;
                }

                MOZ_ASSERT(kind >= ProfileBufferEntry::Kind::LEGACY_LIMIT,
                           "There should be no legacy entries between "
                           "TimeBeforeCompactStack and CompactStack");
                er.SetRemainingBytes(0);
              }

              e.Next();
            } else if (e.Has() && e.Get().IsTimeBeforeSameSample()) {
              // Sample index, nothing to do.

            } else {
              ERROR_AND_CONTINUE("expected a Time entry");
            }
          }
        });
      });
}

void ProfileBuffer::StreamMarkersToJSON(
    SpliceableJSONWriter& aWriter, ProfilerThreadId aThreadId,
    const TimeStamp& aProcessStartTime, double aSinceTime,
    UniqueStacks& aUniqueStacks,
    mozilla::ProgressLogger aProgressLogger) const {
  mEntries.ReadEach([&](ProfileBufferEntryReader& aER) {
    auto type = static_cast<ProfileBufferEntry::Kind>(
        aER.ReadObject<ProfileBufferEntry::KindUnderlyingType>());
    MOZ_ASSERT(static_cast<ProfileBufferEntry::KindUnderlyingType>(type) <
               static_cast<ProfileBufferEntry::KindUnderlyingType>(
                   ProfileBufferEntry::Kind::MODERN_LIMIT));
    if (type == ProfileBufferEntry::Kind::Marker) {
      mozilla::base_profiler_markers_detail::DeserializeAfterKindAndStream(
          aER,
          [&](const ProfilerThreadId& aMarkerThreadId) {
            return (!aThreadId.IsSpecified() || aMarkerThreadId == aThreadId)
                       ? &aWriter
                       : nullptr;
          },
          [&](ProfileChunkedBuffer& aChunkedBuffer) {
            ProfilerBacktrace backtrace("", &aChunkedBuffer);
            backtrace.StreamJSON(aWriter, aProcessStartTime, aUniqueStacks);
          },
          [&](mozilla::base_profiler_markers_detail::Streaming::DeserializerTag
                  aTag) {
            size_t payloadSize = aER.RemainingBytes();

            ProfileBufferEntryReader::DoubleSpanOfConstBytes spans =
                aER.ReadSpans(payloadSize);
            if (MOZ_LIKELY(spans.IsSingleSpan())) {
              // Only a single span, we can just refer to it directly
              // instead of copying it.
              profiler::ffi::gecko_profiler_serialize_marker_for_tag(
                  aTag, spans.mFirstOrOnly.Elements(), payloadSize, &aWriter);
            } else {
              // Two spans, we need to concatenate them by copying.
              uint8_t* payloadBuffer = new uint8_t[payloadSize];
              spans.CopyBytesTo(payloadBuffer);
              profiler::ffi::gecko_profiler_serialize_marker_for_tag(
                  aTag, payloadBuffer, payloadSize, &aWriter);
              delete[] payloadBuffer;
            }
          });
    } else {
      // The entry was not a marker, we need to skip to the end.
      aER.SetRemainingBytes(0);
    }
  });
}

void ProfileBuffer::StreamProfilerOverheadToJSON(
    SpliceableJSONWriter& aWriter, const TimeStamp& aProcessStartTime,
    double aSinceTime, mozilla::ProgressLogger aProgressLogger) const {
  mEntries.Read([&](ProfileChunkedBuffer::Reader* aReader) {
    MOZ_ASSERT(aReader,
               "ProfileChunkedBuffer cannot be out-of-session when sampler is "
               "running");

    EntryGetter e(*aReader, aWriter.SourceFailureLatch(),
                  std::move(aProgressLogger));

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
          writer.TimeMsElement(TIME, time);
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

struct CounterSample {
  double mTime;
  uint64_t mNumber;
  int64_t mCount;
};

using CounterSamples = Vector<CounterSample>;

static LazyLogModule sFuzzyfoxLog("Fuzzyfox");

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

void ProfileBuffer::StreamCountersToJSON(
    SpliceableJSONWriter& aWriter, const TimeStamp& aProcessStartTime,
    double aSinceTime, mozilla::ProgressLogger aProgressLogger) const {
  // Because this is a format entirely internal to the Profiler, any parsing
  // error indicates a bug in the ProfileBuffer writing or the parser itself,
  // or possibly flaky hardware.

  mEntries.Read([&](ProfileChunkedBuffer::Reader* aReader) {
    MOZ_ASSERT(aReader,
               "ProfileChunkedBuffer cannot be out-of-session when sampler is "
               "running");

    EntryGetter e(*aReader, aWriter.SourceFailureLatch(),
                  std::move(aProgressLogger));

    enum Schema : uint32_t { TIME = 0, COUNT = 1, NUMBER = 2 };

    // Stream all counters. We skip other entries, because we process them in
    // StreamSamplesToJSON()/etc.
    //
    // Valid sequence in the buffer:
    // CounterID
    // Time
    // ( Count Number? )*
    //
    // And the JSON (example):
    // "counters": {
    //  "name": "malloc",
    //  "category": "Memory",
    //  "description": "Amount of allocated memory",
    //  "samples": {
    //   "schema": {"time": 0, "count": 1, "number": 2},
    //   "data": [
    //    [
    //     16117.033968000002,
    //     2446216,
    //     6801320
    //    ],
    //    [
    //     16118.037638,
    //     2446216,
    //     6801320
    //    ],
    //   ],
    //  },
    // }

    // Build the map of counters and populate it
    HashMap<void*, CounterSamples> counters;

    while (e.Has()) {
      // skip all non-Counters, including if we start in the middle of a counter
      if (e.Get().IsCounterId()) {
        void* id = e.Get().GetPtr();
        CounterSamples& data = LookupOrAdd(counters, id);
        e.Next();
        if (!e.Has() || !e.Get().IsTime()) {
          ERROR_AND_CONTINUE("expected a Time entry");
        }
        double time = e.Get().GetDouble();
        e.Next();
        if (time >= aSinceTime) {
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
            e.Next();
          }
          CounterSample sample = {time, number, count};
          MOZ_RELEASE_ASSERT(data.append(sample));
        } else {
          // skip counter sample - only need to skip the initial counter
          // id, then let the loop at the top skip the rest
        }
      } else {
        e.Next();
      }
    }
    // we have a map of counter entries; dump them to JSON
    if (counters.count() == 0) {
      return;
    }

    aWriter.StartArrayProperty("counters");
    for (auto iter = counters.iter(); !iter.done(); iter.next()) {
      CounterSamples& samples = iter.get().value();
      size_t size = samples.length();
      if (size == 0) {
        continue;
      }
      const BaseProfilerCount* base_counter =
          static_cast<const BaseProfilerCount*>(iter.get().key());

      aWriter.Start();
      aWriter.StringProperty("name", MakeStringSpan(base_counter->mLabel));
      aWriter.StringProperty("category",
                             MakeStringSpan(base_counter->mCategory));
      aWriter.StringProperty("description",
                             MakeStringSpan(base_counter->mDescription));

      bool hasNumber = false;
      for (size_t i = 0; i < size; i++) {
        if (samples[i].mNumber != 0) {
          hasNumber = true;
          break;
        }
      }
      aWriter.StartObjectProperty("samples");
      {
        JSONSchemaWriter schema(aWriter);
        schema.WriteField("time");
        schema.WriteField("count");
        if (hasNumber) {
          schema.WriteField("number");
        }
      }

      aWriter.StartArrayProperty("data");
      double previousSkippedTime = 0.0;
      uint64_t previousNumber = 0;
      int64_t previousCount = 0;
      for (size_t i = 0; i < size; i++) {
        // Encode as deltas, and only encode if different than the previous
        // or next sample; Always write the first and last samples.
        if (i == 0 || i == size - 1 || samples[i].mNumber != previousNumber ||
            samples[i].mCount != previousCount ||
            // Ensure we ouput the first 0 before skipping samples.
            (i >= 2 && (samples[i - 2].mNumber != previousNumber ||
                        samples[i - 2].mCount != previousCount))) {
          if (i != 0 && samples[i].mTime >= samples[i - 1].mTime) {
            MOZ_LOG(sFuzzyfoxLog, mozilla::LogLevel::Error,
                    ("Fuzzyfox Profiler Assertion: %f >= %f", samples[i].mTime,
                     samples[i - 1].mTime));
          }
          MOZ_ASSERT(i == 0 || samples[i].mTime >= samples[i - 1].mTime);
          MOZ_ASSERT(samples[i].mNumber >= previousNumber);
          MOZ_ASSERT(samples[i].mNumber - previousNumber <=
                     uint64_t(std::numeric_limits<int64_t>::max()));

          int64_t numberDelta =
              static_cast<int64_t>(samples[i].mNumber - previousNumber);
          int64_t countDelta = samples[i].mCount - previousCount;

          if (previousSkippedTime != 0.0 &&
              (numberDelta != 0 || countDelta != 0)) {
            // Write the last skipped sample, unless the new one is all
            // zeroes (that'd be redundant) This is useful to know when a
            // certain value was last sampled, so that the front-end graph
            // will be more correct.
            AutoArraySchemaWriter writer(aWriter);
            writer.TimeMsElement(TIME, previousSkippedTime);
            // The deltas are effectively zeroes, since no change happened
            // between the last actually-written sample and the last skipped
            // one.
            writer.IntElement(COUNT, 0);
            if (hasNumber) {
              writer.IntElement(NUMBER, 0);
            }
          }

          AutoArraySchemaWriter writer(aWriter);
          writer.TimeMsElement(TIME, samples[i].mTime);
          writer.IntElement(COUNT, countDelta);
          if (hasNumber) {
            writer.IntElement(NUMBER, numberDelta);
          }

          previousSkippedTime = 0.0;
          previousNumber = samples[i].mNumber;
          previousCount = samples[i].mCount;
        } else {
          previousSkippedTime = samples[i].mTime;
        }
      }
      aWriter.EndArray();   // data
      aWriter.EndObject();  // samples
      aWriter.End();        // for each counter
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
    aWriter.TimeDoubleMsProperty("startTime", *aStartTime);
  } else {
    aWriter.NullProperty("startTime");
  }
  if (aEndTime) {
    aWriter.TimeDoubleMsProperty("endTime", *aEndTime);
  } else {
    aWriter.NullProperty("endTime");
  }
  aWriter.StringProperty("reason", MakeStringSpan(aReason));
  aWriter.End();
}

void ProfileBuffer::StreamPausedRangesToJSON(
    SpliceableJSONWriter& aWriter, double aSinceTime,
    mozilla::ProgressLogger aProgressLogger) const {
  mEntries.Read([&](ProfileChunkedBuffer::Reader* aReader) {
    MOZ_ASSERT(aReader,
               "ProfileChunkedBuffer cannot be out-of-session when sampler is "
               "running");

    EntryGetter e(*aReader, aWriter.SourceFailureLatch(),
                  aProgressLogger.CreateSubLoggerFromTo(
                      1_pc, "Streaming pauses...", 99_pc, "Streamed pauses"));

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

bool ProfileBuffer::DuplicateLastSample(ProfilerThreadId aThreadId,
                                        double aSampleTimeMs,
                                        Maybe<uint64_t>& aLastSample,
                                        const RunningTimes& aRunningTimes) {
  if (!aLastSample) {
    return false;
  }

  if (mEntries.IsIndexInCurrentChunk(ProfileBufferIndex{*aLastSample})) {
    // The last (fully-written) sample is in this chunk, we can refer to it.

    // Note that between now and when we write the SameSample below, another
    // chunk could have been started, so the SameSample will in fact refer to a
    // block in a previous chunk. This is okay, because:
    // - When serializing to JSON, if that chunk is still there, we'll still be
    //   able to find that old stack, so nothing will be lost.
    // - If unfortunately that chunk has been destroyed, we will lose this
    //   sample. But this will only happen to the first sample (per thread) in
    //   in the whole JSON output, because the next time we're here to duplicate
    //   the same sample again, IsIndexInCurrentChunk will say `false` and we
    //   will fall back to the normal copy or even re-sample. Losing the first
    //   sample out of many in a whole recording is acceptable.
    //
    // |---| = chunk, S = Sample, D = Duplicate, s = same sample
    // |---S-s-s--| |s-D--s--s-| |s-D--s---s|
    // Later, the first chunk is destroyed/recycled:
    //              |s-D--s--s-| |s-D--s---s| |-...
    // Output:       ^ ^  ^       ^
    //               `-|--|-------|--- Same but no previous -> lost.
    //                 `--|-------|--- Full duplicate sample.
    //                    `-------|--- Same with previous -> okay.
    //                            `--- Same but now we have a previous -> okay!

    AUTO_PROFILER_STATS(DuplicateLastSample_SameSample);

    // Add the thread id first. We don't update `aLastSample` because we are not
    // writing a full sample.
    (void)AddThreadIdEntry(aThreadId);

    // Copy the new time, to be followed by a SameSample.
    AddEntry(ProfileBufferEntry::TimeBeforeSameSample(aSampleTimeMs));

    // Add running times if they have data.
    if (!aRunningTimes.IsEmpty()) {
      mEntries.PutObjects(ProfileBufferEntry::Kind::RunningTimes,
                          aRunningTimes);
    }

    // Finish with a SameSample entry.
    mEntries.PutObjects(ProfileBufferEntry::Kind::SameSample);

    return true;
  }

  AUTO_PROFILER_STATS(DuplicateLastSample_copy);

  ProfileChunkedBuffer tempBuffer(
      ProfileChunkedBuffer::ThreadSafety::WithoutMutex, WorkerChunkManager());

  auto retrieveWorkerChunk = MakeScopeExit(
      [&]() { WorkerChunkManager().Reset(tempBuffer.GetAllChunks()); });

  const bool ok = mEntries.Read([&](ProfileChunkedBuffer::Reader* aReader) {
    MOZ_ASSERT(aReader,
               "ProfileChunkedBuffer cannot be out-of-session when sampler is "
               "running");

    // DuplicateLastSample is only called during profiling, so we don't need a
    // progress logger (only useful when capturing the final profile).
    EntryGetter e(*aReader, mozilla::FailureLatchInfallibleSource::Singleton(),
                  ProgressLogger{}, *aLastSample);

    if (e.CurPos() != *aLastSample) {
      // The last sample is no longer within the buffer range, so we cannot
      // use it. Reset the stored buffer position to Nothing().
      aLastSample.reset();
      return false;
    }

    MOZ_RELEASE_ASSERT(e.Has() && e.Get().IsThreadId() &&
                       e.Get().GetThreadId() == aThreadId);

    e.Next();

    // Go through the whole entry and duplicate it, until we find the next
    // one.
    while (e.Has()) {
      switch (e.Get().GetKind()) {
        case ProfileBufferEntry::Kind::Pause:
        case ProfileBufferEntry::Kind::Resume:
        case ProfileBufferEntry::Kind::PauseSampling:
        case ProfileBufferEntry::Kind::ResumeSampling:
        case ProfileBufferEntry::Kind::CollectionStart:
        case ProfileBufferEntry::Kind::CollectionEnd:
        case ProfileBufferEntry::Kind::ThreadId:
        case ProfileBufferEntry::Kind::TimeBeforeSameSample:
          // We're done.
          return true;
        case ProfileBufferEntry::Kind::Time:
          // Copy with new time
          AddEntry(tempBuffer, ProfileBufferEntry::Time(aSampleTimeMs));
          break;
        case ProfileBufferEntry::Kind::TimeBeforeCompactStack: {
          // Copy with new time, followed by a compact stack.
          AddEntry(tempBuffer,
                   ProfileBufferEntry::TimeBeforeCompactStack(aSampleTimeMs));

          // Add running times if they have data.
          if (!aRunningTimes.IsEmpty()) {
            tempBuffer.PutObjects(ProfileBufferEntry::Kind::RunningTimes,
                                  aRunningTimes);
          }

          // The `CompactStack` *must* be present afterwards, but may not
          // immediately follow `TimeBeforeCompactStack` (e.g., some markers
          // could be written in-between), so we need to look for it in the
          // following entries.
          ProfileChunkedBuffer::BlockIterator it = e.Iterator();
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
              MOZ_ASSERT(bytes <
                         ProfileBufferChunkManager::scExpectedMaximumStackSize);
              tempBuffer.Put(bytes, [&](Maybe<ProfileBufferEntryWriter>& aEW) {
                MOZ_ASSERT(aEW.isSome(), "tempBuffer cannot be out-of-session");
                aEW->WriteFromReader(er, bytes);
              });
              // CompactStack marks the end, we're done.
              break;
            }

            MOZ_ASSERT(kind >= ProfileBufferEntry::Kind::LEGACY_LIMIT,
                       "There should be no legacy entries between "
                       "TimeBeforeCompactStack and CompactStack");
            er.SetRemainingBytes(0);
            // Here, we have encountered a non-legacy entry that was not the
            // CompactStack we're looking for; just continue the search...
          }
          // We're done.
          return true;
        }
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
  // This function does nothing!
  // The duration limit will be removed from Firefox, see bug 1632365.
  Unused << aTime;
}

// END ProfileBuffer
////////////////////////////////////////////////////////////////////////
