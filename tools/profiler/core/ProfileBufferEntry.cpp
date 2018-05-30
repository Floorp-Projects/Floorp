/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ostream>
#include "platform.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/Sprintf.h"

#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

// JS
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/TrackedOptimizationInfo.h"

// Self
#include "ProfileBufferEntry.h"

using namespace mozilla;

////////////////////////////////////////////////////////////////////////
// BEGIN ProfileBufferEntry

ProfileBufferEntry::ProfileBufferEntry()
  : mKind(Kind::INVALID)
{
  u.mString = nullptr;
}

// aString must be a static string.
ProfileBufferEntry::ProfileBufferEntry(Kind aKind, const char *aString)
  : mKind(aKind)
{
  u.mString = aString;
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, char aChars[kNumChars])
  : mKind(aKind)
{
  memcpy(u.mChars, aChars, kNumChars);
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, void* aPtr)
  : mKind(aKind)
{
  u.mPtr = aPtr;
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, ProfilerMarker* aMarker)
  : mKind(aKind)
{
  u.mMarker = aMarker;
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, double aDouble)
  : mKind(aKind)
{
  u.mDouble = aDouble;
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, int aInt)
  : mKind(aKind)
{
  u.mInt = aInt;
}

// END ProfileBufferEntry
////////////////////////////////////////////////////////////////////////

class JSONSchemaWriter
{
  JSONWriter& mWriter;
  uint32_t mIndex;

public:
  explicit JSONSchemaWriter(JSONWriter& aWriter)
   : mWriter(aWriter)
   , mIndex(0)
  {
    aWriter.StartObjectProperty("schema",
                                SpliceableJSONWriter::SingleLineStyle);
  }

  void WriteField(const char* aName) {
    mWriter.IntProperty(aName, mIndex++);
  }

  ~JSONSchemaWriter() {
    mWriter.EndObject();
  }
};

struct TypeInfo
{
  Maybe<nsCString> mKeyedBy;
  Maybe<nsCString> mName;
  Maybe<nsCString> mLocation;
  Maybe<unsigned> mLineNumber;
};

template<typename LambdaT>
class ForEachTrackedOptimizationTypeInfoLambdaOp : public JS::ForEachTrackedOptimizationTypeInfoOp
{
public:
  // aLambda needs to be a function with the following signature:
  // void lambda(JS::TrackedTypeSite site, const char* mirType,
  //             const nsTArray<TypeInfo>& typeset)
  // aLambda will be called once per entry.
  explicit ForEachTrackedOptimizationTypeInfoLambdaOp(LambdaT&& aLambda)
    : mLambda(aLambda)
  {}

  // This is called 0 or more times per entry, *before* operator() is called
  // for that entry.
  void readType(const char* keyedBy, const char* name,
                const char* location, const Maybe<unsigned>& lineno) override
  {
    TypeInfo info = {
      keyedBy ? Some(nsCString(keyedBy)) : Nothing(),
      name ? Some(nsCString(name)) : Nothing(),
      location ? Some(nsCString(location)) : Nothing(),
      lineno
    };
    mTypesetForUpcomingEntry.AppendElement(std::move(info));
  }

  void operator()(JS::TrackedTypeSite site, const char* mirType) override
  {
    nsTArray<TypeInfo> typeset(std::move(mTypesetForUpcomingEntry));
    mLambda(site, mirType, typeset);
  }

private:
  nsTArray<TypeInfo> mTypesetForUpcomingEntry;
  LambdaT mLambda;
};

template<typename LambdaT> ForEachTrackedOptimizationTypeInfoLambdaOp<LambdaT>
MakeForEachTrackedOptimizationTypeInfoLambdaOp(LambdaT&& aLambda)
{
  return ForEachTrackedOptimizationTypeInfoLambdaOp<LambdaT>(std::move(aLambda));
}

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
class MOZ_RAII AutoArraySchemaWriter
{
public:
  AutoArraySchemaWriter(SpliceableJSONWriter& aWriter, UniqueJSONStrings& aStrings)
    : mJSONWriter(aWriter)
    , mStrings(aStrings)
    , mNextFreeIndex(0)
  {
    mJSONWriter.StartArrayElement(SpliceableJSONWriter::SingleLineStyle);
  }

  ~AutoArraySchemaWriter() {
    mJSONWriter.EndArray();
  }

  void IntElement(uint32_t aIndex, uint32_t aValue) {
    FillUpTo(aIndex);
    mJSONWriter.IntElement(aValue);
  }

  void DoubleElement(uint32_t aIndex, double aValue) {
    FillUpTo(aIndex);
    mJSONWriter.DoubleElement(aValue);
  }

  void StringElement(uint32_t aIndex, const char* aValue) {
    FillUpTo(aIndex);
    mStrings.WriteElement(mJSONWriter, aValue);
  }

  // Write an element using a callback that takes a JSONWriter& and a
  // UniqueJSONStrings&.
  template<typename LambdaT>
  void FreeFormElement(uint32_t aIndex, LambdaT aCallback) {
    FillUpTo(aIndex);
    aCallback(mJSONWriter, mStrings);
  }

private:
  void FillUpTo(uint32_t aIndex) {
    MOZ_ASSERT(aIndex >= mNextFreeIndex);
    mJSONWriter.NullElements(aIndex - mNextFreeIndex);
    mNextFreeIndex = aIndex + 1;
  }

  SpliceableJSONWriter& mJSONWriter;
  UniqueJSONStrings& mStrings;
  uint32_t mNextFreeIndex;
};

template<typename LambdaT>
class ForEachTrackedOptimizationAttemptsLambdaOp
  : public JS::ForEachTrackedOptimizationAttemptOp
{
public:
  explicit ForEachTrackedOptimizationAttemptsLambdaOp(LambdaT&& aLambda)
    : mLambda(std::move(aLambda))
  {}
  void operator()(JS::TrackedStrategy aStrategy, JS::TrackedOutcome aOutcome) override {
    mLambda(aStrategy, aOutcome);
  }
private:
  LambdaT mLambda;
};

template<typename LambdaT> ForEachTrackedOptimizationAttemptsLambdaOp<LambdaT>
MakeForEachTrackedOptimizationAttemptsLambdaOp(LambdaT&& aLambda)
{
  return ForEachTrackedOptimizationAttemptsLambdaOp<LambdaT>(std::move(aLambda));
}

UniqueJSONStrings::UniqueJSONStrings()
{
  mStringTableWriter.StartBareList();
}

UniqueJSONStrings::UniqueJSONStrings(const UniqueJSONStrings& aOther)
{
  mStringTableWriter.StartBareList();
  if (aOther.mStringToIndexMap.Count() > 0) {
    for (auto iter = aOther.mStringToIndexMap.ConstIter();
         !iter.Done(); iter.Next()) {
      mStringToIndexMap.Put(iter.Key(), iter.Data());
    }
    UniquePtr<char[]> stringTableJSON =
      aOther.mStringTableWriter.WriteFunc()->CopyData();
    mStringTableWriter.Splice(stringTableJSON.get());
  }
}

uint32_t
UniqueJSONStrings::GetOrAddIndex(const char* aStr)
{
  nsDependentCString str(aStr);

  uint32_t index;
  if (mStringToIndexMap.Get(str, &index)) {
    return index;
  }

  index = mStringToIndexMap.Count();
  mStringToIndexMap.Put(str, index);
  mStringTableWriter.StringElement(aStr);
  return index;
}

UniqueStacks::StackKey
UniqueStacks::BeginStack(const FrameKey& aFrame)
{
  return StackKey(GetOrAddFrameIndex(aFrame));
}

UniqueStacks::StackKey
UniqueStacks::AppendFrame(const StackKey& aStack, const FrameKey& aFrame)
{
  return StackKey(aStack, GetOrAddStackIndex(aStack), GetOrAddFrameIndex(aFrame));
}

uint32_t
JITFrameInfoForBufferRange::JITFrameKey::Hash() const
{
  uint32_t hash = 0;
  hash = AddToHash(hash, mCanonicalAddress);
  hash = AddToHash(hash, mDepth);
  return hash;
}

bool
JITFrameInfoForBufferRange::JITFrameKey::operator==(const JITFrameKey& aOther) const
{
  return mCanonicalAddress == aOther.mCanonicalAddress &&
         mDepth == aOther.mDepth;
}

template<class KeyClass, class T> void
CopyClassHashtable(nsClassHashtable<KeyClass, T>& aDest,
                   const nsClassHashtable<KeyClass, T>& aSrc)
{
  for (auto iter = aSrc.ConstIter(); !iter.Done(); iter.Next()) {
    const T& objRef = *iter.Data();
    aDest.LookupOrAdd(iter.Key(), objRef);
  }
}

JITFrameInfoForBufferRange
JITFrameInfoForBufferRange::Clone() const
{
  nsClassHashtable<nsPtrHashKey<void>, nsTArray<JITFrameKey>> jitAddressToJITFramesMap;
  nsClassHashtable<nsGenericHashKey<JITFrameKey>, nsCString> jitFrameToFrameJSONMap;
  CopyClassHashtable(jitAddressToJITFramesMap, mJITAddressToJITFramesMap);
  CopyClassHashtable(jitFrameToFrameJSONMap, mJITFrameToFrameJSONMap);
  return JITFrameInfoForBufferRange{
    mRangeStart, mRangeEnd,
    std::move(jitAddressToJITFramesMap), std::move(jitFrameToFrameJSONMap) };
}

JITFrameInfo::JITFrameInfo(const JITFrameInfo& aOther)
  : mUniqueStrings(MakeUnique<UniqueJSONStrings>(*aOther.mUniqueStrings))
{
  for (const JITFrameInfoForBufferRange& range : aOther.mRanges) {
    mRanges.AppendElement(range.Clone());
  }
}

bool
UniqueStacks::FrameKey::NormalFrameData::operator==(const NormalFrameData& aOther) const
{
  return mLocation == aOther.mLocation &&
         mLine == aOther.mLine &&
         mCategory == aOther.mCategory;
}

bool
UniqueStacks::FrameKey::JITFrameData::operator==(const JITFrameData& aOther) const
{
  return mCanonicalAddress == aOther.mCanonicalAddress &&
         mDepth == aOther.mDepth &&
         mRangeIndex == aOther.mRangeIndex;
}

uint32_t
UniqueStacks::FrameKey::Hash() const
{
  uint32_t hash = 0;
  if (mData.is<NormalFrameData>()) {
    const NormalFrameData& data = mData.as<NormalFrameData>();
    if (!data.mLocation.IsEmpty()) {
      hash = AddToHash(hash, HashString(data.mLocation.get()));
    }
    if (data.mLine.isSome()) {
      hash = AddToHash(hash, *data.mLine);
    }
    if (data.mCategory.isSome()) {
      hash = AddToHash(hash, *data.mCategory);
    }
  } else {
    const JITFrameData& data = mData.as<JITFrameData>();
    hash = AddToHash(hash, data.mCanonicalAddress);
    hash = AddToHash(hash, data.mDepth);
    hash = AddToHash(hash, data.mRangeIndex);
  }
  return hash;
}

// Consume aJITFrameInfo by stealing its string table and its JIT frame info
// ranges. The JIT frame info contains JSON which refers to strings from the
// JIT frame info's string table, so our string table needs to have the same
// strings at the same indices.
UniqueStacks::UniqueStacks(JITFrameInfo&& aJITFrameInfo)
  : mUniqueStrings(std::move(aJITFrameInfo.mUniqueStrings))
  , mJITInfoRanges(std::move(aJITFrameInfo.mRanges))
{
  mFrameTableWriter.StartBareList();
  mStackTableWriter.StartBareList();
}

uint32_t UniqueStacks::GetOrAddStackIndex(const StackKey& aStack)
{
  uint32_t index;
  if (mStackToIndexMap.Get(aStack, &index)) {
    MOZ_ASSERT(index < mStackToIndexMap.Count());
    return index;
  }

  index = mStackToIndexMap.Count();
  mStackToIndexMap.Put(aStack, index);
  StreamStack(aStack);
  return index;
}

template<typename RangeT, typename PosT>
struct PositionInRangeComparator final
{
  bool Equals(const RangeT& aRange, PosT aPos) const
  {
    return aRange.mRangeStart <= aPos && aPos < aRange.mRangeEnd;
  }

  bool LessThan(const RangeT& aRange, PosT aPos) const
  {
    return aRange.mRangeEnd <= aPos;
  }
};

Maybe<nsTArray<UniqueStacks::FrameKey>>
UniqueStacks::LookupFramesForJITAddressFromBufferPos(void* aJITAddress,
                                                     uint64_t aBufferPos)
{
  size_t rangeIndex = mJITInfoRanges.BinaryIndexOf(aBufferPos,
    PositionInRangeComparator<JITFrameInfoForBufferRange, uint64_t>());
  MOZ_RELEASE_ASSERT(rangeIndex != mJITInfoRanges.NoIndex,
                     "Buffer position of jit address needs to be in one of the ranges");

  using JITFrameKey = JITFrameInfoForBufferRange::JITFrameKey;

  const JITFrameInfoForBufferRange& jitFrameInfoRange = mJITInfoRanges[rangeIndex];
  const nsTArray<JITFrameKey>* jitFrameKeys =
    jitFrameInfoRange.mJITAddressToJITFramesMap.Get(aJITAddress);
  if (!jitFrameKeys) {
    return Nothing();
  }

  // Map the array of JITFrameKeys to an array of FrameKeys, and ensure that
  // each of the FrameKeys exists in mFrameToIndexMap.
  nsTArray<FrameKey> frameKeys;
  for (const JITFrameKey& jitFrameKey : *jitFrameKeys) {
    FrameKey frameKey(jitFrameKey.mCanonicalAddress, jitFrameKey.mDepth, rangeIndex);
    if (!mFrameToIndexMap.Contains(frameKey)) {
      // We need to add this frame to our frame table. The JSON for this frame
      // already exists in jitFrameInfoRange, we just need to splice it into
      // the frame table and give it an index.
      uint32_t index = mFrameToIndexMap.Count();
      const nsCString* frameJSON =
        jitFrameInfoRange.mJITFrameToFrameJSONMap.Get(jitFrameKey);
      MOZ_RELEASE_ASSERT(frameJSON, "Should have cached JSON for this frame");
      mFrameTableWriter.Splice(frameJSON->get());
      mFrameToIndexMap.Put(frameKey, index);
    }
    frameKeys.AppendElement(std::move(frameKey));
  }
  return Some(std::move(frameKeys));
}

uint32_t
UniqueStacks::GetOrAddFrameIndex(const FrameKey& aFrame)
{
  uint32_t index;
  if (mFrameToIndexMap.Get(aFrame, &index)) {
    MOZ_ASSERT(index < mFrameToIndexMap.Count());
    return index;
  }

  index = mFrameToIndexMap.Count();
  mFrameToIndexMap.Put(aFrame, index);
  StreamNonJITFrame(aFrame);
  return index;
}

void UniqueStacks::SpliceFrameTableElements(SpliceableJSONWriter& aWriter)
{
  mFrameTableWriter.EndBareList();
  aWriter.TakeAndSplice(mFrameTableWriter.WriteFunc());
}

void UniqueStacks::SpliceStackTableElements(SpliceableJSONWriter& aWriter)
{
  mStackTableWriter.EndBareList();
  aWriter.TakeAndSplice(mStackTableWriter.WriteFunc());
}

void UniqueStacks::StreamStack(const StackKey& aStack)
{
  enum Schema : uint32_t {
    PREFIX = 0,
    FRAME = 1
  };

  AutoArraySchemaWriter writer(mStackTableWriter, *mUniqueStrings);
  if (aStack.mPrefixStackIndex.isSome()) {
    writer.IntElement(PREFIX, *aStack.mPrefixStackIndex);
  }
  writer.IntElement(FRAME, aStack.mFrameIndex);
}

void
UniqueStacks::StreamNonJITFrame(const FrameKey& aFrame)
{
  using NormalFrameData = FrameKey::NormalFrameData;

  enum Schema : uint32_t {
    LOCATION = 0,
    IMPLEMENTATION = 1,
    OPTIMIZATIONS = 2,
    LINE = 3,
    CATEGORY = 4
  };

  AutoArraySchemaWriter writer(mFrameTableWriter, *mUniqueStrings);

  const NormalFrameData& data = aFrame.mData.as<NormalFrameData>();
  writer.StringElement(LOCATION, data.mLocation.get());
  if (data.mLine.isSome()) {
    writer.IntElement(LINE, *data.mLine);
  }
  if (data.mCategory.isSome()) {
    writer.IntElement(CATEGORY, *data.mCategory);
  }
}

static void
StreamJITFrameOptimizations(SpliceableJSONWriter& aWriter,
                            UniqueJSONStrings& aUniqueStrings,
                            JSContext* aContext,
                            const JS::ProfiledFrameHandle& aJITFrame)
{
  aWriter.StartObjectElement();
  {
    aWriter.StartArrayProperty("types");
    {
      auto op = MakeForEachTrackedOptimizationTypeInfoLambdaOp(
        [&](JS::TrackedTypeSite site, const char* mirType,
            const nsTArray<TypeInfo>& typeset) {
          aWriter.StartObjectElement();
          {
            aUniqueStrings.WriteProperty(aWriter, "site",
                                        JS::TrackedTypeSiteString(site));
            aUniqueStrings.WriteProperty(aWriter, "mirType", mirType);

            if (!typeset.IsEmpty()) {
              aWriter.StartArrayProperty("typeset");
              for (const TypeInfo& typeInfo : typeset) {
                aWriter.StartObjectElement();
                {
                  aUniqueStrings.WriteProperty(aWriter, "keyedBy",
                                              typeInfo.mKeyedBy->get());
                  if (typeInfo.mName) {
                    aUniqueStrings.WriteProperty(aWriter, "name",
                                                typeInfo.mName->get());
                  }
                  if (typeInfo.mLocation) {
                    aUniqueStrings.WriteProperty(aWriter, "location",
                                                typeInfo.mLocation->get());
                  }
                  if (typeInfo.mLineNumber.isSome()) {
                    aWriter.IntProperty("line", *typeInfo.mLineNumber);
                  }
                }
                aWriter.EndObject();
              }
              aWriter.EndArray();
            }

          }
          aWriter.EndObject();
        });
      aJITFrame.forEachOptimizationTypeInfo(op);
    }
    aWriter.EndArray();

    JS::Rooted<JSScript*> script(aContext);
    jsbytecode* pc;
    aWriter.StartObjectProperty("attempts");
    {
      {
        JSONSchemaWriter schema(aWriter);
        schema.WriteField("strategy");
        schema.WriteField("outcome");
      }

      aWriter.StartArrayProperty("data");
      {
        auto op = MakeForEachTrackedOptimizationAttemptsLambdaOp(
          [&](JS::TrackedStrategy strategy, JS::TrackedOutcome outcome) {
            enum Schema : uint32_t {
              STRATEGY = 0,
              OUTCOME = 1
            };

            AutoArraySchemaWriter writer(aWriter, aUniqueStrings);
            writer.StringElement(STRATEGY, JS::TrackedStrategyString(strategy));
            writer.StringElement(OUTCOME, JS::TrackedOutcomeString(outcome));
          });
        aJITFrame.forEachOptimizationAttempt(op, script.address(), &pc);
      }
      aWriter.EndArray();
    }
    aWriter.EndObject();

    if (JSAtom* name = js::GetPropertyNameFromPC(script, pc)) {
      char buf[512];
      JS_PutEscapedFlatString(buf, ArrayLength(buf), js::AtomToFlatString(name), 0);
      aUniqueStrings.WriteProperty(aWriter, "propertyName", buf);
    }

    unsigned line, column;
    line = JS_PCToLineNumber(script, pc, &column);
    aWriter.IntProperty("line", line);
    aWriter.IntProperty("column", column);
  }
  aWriter.EndObject();
}

static void
StreamJITFrame(JSContext* aContext, SpliceableJSONWriter& aWriter,
               UniqueJSONStrings& aUniqueStrings,
               const JS::ProfiledFrameHandle& aJITFrame)
{
  enum Schema : uint32_t {
    LOCATION = 0,
    IMPLEMENTATION = 1,
    OPTIMIZATIONS = 2,
    LINE = 3,
    CATEGORY = 4
  };

  AutoArraySchemaWriter writer(aWriter, aUniqueStrings);

  writer.StringElement(LOCATION, aJITFrame.label());

  JS::ProfilingFrameIterator::FrameKind frameKind = aJITFrame.frameKind();
  MOZ_ASSERT(frameKind == JS::ProfilingFrameIterator::Frame_Ion ||
              frameKind == JS::ProfilingFrameIterator::Frame_Baseline);
  writer.StringElement(IMPLEMENTATION,
                        frameKind == JS::ProfilingFrameIterator::Frame_Ion
                        ? "ion"
                        : "baseline");

  if (aJITFrame.hasTrackedOptimizations()) {
    writer.FreeFormElement(OPTIMIZATIONS,
      [&](SpliceableJSONWriter& aWriter, UniqueJSONStrings& aUniqueStrings) {
        StreamJITFrameOptimizations(aWriter, aUniqueStrings, aContext,
                                    aJITFrame);
      });
  }
}

struct CStringWriteFunc : public JSONWriteFunc
{
  nsACString& mBuffer; // The struct must not outlive this buffer
  explicit CStringWriteFunc(nsACString& aBuffer) : mBuffer(aBuffer) {}

  void Write(const char* aStr) override
  {
    mBuffer.Append(aStr);
  }
};

static nsCString
JSONForJITFrame(JSContext* aContext, const JS::ProfiledFrameHandle& aJITFrame,
                UniqueJSONStrings& aUniqueStrings)
{
  nsCString json;
  SpliceableJSONWriter writer(MakeUnique<CStringWriteFunc>(json));
  StreamJITFrame(aContext, writer, aUniqueStrings, aJITFrame);
  return json;
}

void
JITFrameInfo::AddInfoForRange(uint64_t aRangeStart, uint64_t aRangeEnd,
                              JSContext* aCx,
                              const std::function<void(const std::function<void(void*)>&)>& aJITAddressProvider)
{
  if (aRangeStart == aRangeEnd) {
    return;
  }

  MOZ_RELEASE_ASSERT(aRangeStart < aRangeEnd);

  if (!mRanges.IsEmpty()) {
    const JITFrameInfoForBufferRange& prevRange = mRanges.LastElement();
    MOZ_RELEASE_ASSERT(prevRange.mRangeEnd <= aRangeStart,
                        "Ranges must be non-overlapping and added in-order.");
  }

  using JITFrameKey = JITFrameInfoForBufferRange::JITFrameKey;

  nsClassHashtable<nsPtrHashKey<void>, nsTArray<JITFrameKey>> jitAddressToJITFrameMap;
  nsClassHashtable<nsGenericHashKey<JITFrameKey>, nsCString> jitFrameToFrameJSONMap;

  aJITAddressProvider([&](void* aJITAddress) {
    // Make sure that we have cached data for aJITAddress.
    if (!jitAddressToJITFrameMap.Contains(aJITAddress)) {
      nsTArray<JITFrameKey>& jitFrameKeys =
        *jitAddressToJITFrameMap.LookupOrAdd(aJITAddress);
      for (JS::ProfiledFrameHandle handle : JS::GetProfiledFrames(aCx, aJITAddress)) {
        uint32_t depth = jitFrameKeys.Length();
        JITFrameKey jitFrameKey{ handle.canonicalAddress(), depth };
        if (!jitFrameToFrameJSONMap.Contains(jitFrameKey)) {
          nsCString& json = *jitFrameToFrameJSONMap.LookupOrAdd(jitFrameKey);
          json = JSONForJITFrame(aCx, handle, *mUniqueStrings);
        }
        jitFrameKeys.AppendElement(jitFrameKey);
      }
    }
  });

  mRanges.AppendElement(JITFrameInfoForBufferRange{
    aRangeStart, aRangeEnd,
    std::move(jitAddressToJITFrameMap), std::move(jitFrameToFrameJSONMap)
  });
}

struct ProfileSample
{
  uint32_t mStack;
  double mTime;
  Maybe<double> mResponsiveness;
  Maybe<double> mRSS;
  Maybe<double> mUSS;
};

static void
WriteSample(SpliceableJSONWriter& aWriter, UniqueJSONStrings& aUniqueStrings,
            const ProfileSample& aSample)
{
  enum Schema : uint32_t {
    STACK = 0,
    TIME = 1,
    RESPONSIVENESS = 2,
    RSS = 3,
    USS = 4
  };

  AutoArraySchemaWriter writer(aWriter, aUniqueStrings);

  writer.IntElement(STACK, aSample.mStack);

  writer.DoubleElement(TIME, aSample.mTime);

  if (aSample.mResponsiveness.isSome()) {
    writer.DoubleElement(RESPONSIVENESS, *aSample.mResponsiveness);
  }

  if (aSample.mRSS.isSome()) {
    writer.DoubleElement(RSS, *aSample.mRSS);
  }

  if (aSample.mUSS.isSome()) {
    writer.DoubleElement(USS, *aSample.mUSS);
  }
}

class EntryGetter
{
public:
  explicit EntryGetter(const ProfileBuffer& aBuffer,
                       uint64_t aInitialReadPos = 0)
    : mBuffer(aBuffer)
    , mReadPos(aBuffer.mRangeStart)
  {
    if (aInitialReadPos != 0) {
      MOZ_RELEASE_ASSERT(aInitialReadPos >= aBuffer.mRangeStart &&
                         aInitialReadPos <= aBuffer.mRangeEnd);
      mReadPos = aInitialReadPos;
    }
  }

  bool Has() const { return mReadPos != mBuffer.mRangeEnd; }
  const ProfileBufferEntry& Get() const { return mBuffer.GetEntry(mReadPos); }
  void Next() { mReadPos++; }
  uint64_t CurPos() { return mReadPos; }

private:
  const ProfileBuffer& mBuffer;
  uint64_t mReadPos;
};

// The following grammar shows legal sequences of profile buffer entries.
// The sequences beginning with a ThreadId entry are known as "samples".
//
// (
//   (
//     ThreadId
//     Time
//     ( NativeLeafAddr
//     | Label DynamicStringFragment* LineNumber? Category?
//     | JitReturnAddr
//     )+
//     Marker*
//     Responsiveness?
//     ResidentMemory?
//     UnsharedMemory?
//   )
//   | CollectionStart
//   | CollectionEnd
//   | Pause
//   | Resume
// )*
//
// The most complicated part is the stack entry sequence that begins with
// Label. Here are some examples.
//
// - ProfilingStack frames without a dynamic string:
//
//     Label("js::RunScript")
//     Category(ProfilingStackFrame::Category::JS)
//
//     Label("XREMain::XRE_main")
//     LineNumber(4660)
//     Category(ProfilingStackFrame::Category::OTHER)
//
//     Label("ElementRestyler::ComputeStyleChangeFor")
//     LineNumber(3003)
//     Category(ProfilingStackFrame::Category::CSS)
//
// - ProfilingStack frames with a dynamic string:
//
//     Label("nsObserverService::NotifyObservers")
//     DynamicStringFragment("domwindo")
//     DynamicStringFragment("wopened")
//     LineNumber(291)
//     Category(ProfilingStackFrame::Category::OTHER)
//
//     Label("")
//     DynamicStringFragment("closeWin")
//     DynamicStringFragment("dow (chr")
//     DynamicStringFragment("ome://gl")
//     DynamicStringFragment("obal/con")
//     DynamicStringFragment("tent/glo")
//     DynamicStringFragment("balOverl")
//     DynamicStringFragment("ay.js:5)")
//     DynamicStringFragment("")          # this string holds the closing '\0'
//     LineNumber(25)
//     Category(ProfilingStackFrame::Category::JS)
//
//     Label("")
//     DynamicStringFragment("bound (s")
//     DynamicStringFragment("elf-host")
//     DynamicStringFragment("ed:914)")
//     LineNumber(945)
//     Category(ProfilingStackFrame::Category::JS)
//
// - A profiling stack frame with a dynamic string, but with privacy enabled:
//
//     Label("nsObserverService::NotifyObservers")
//     DynamicStringFragment("(private")
//     DynamicStringFragment(")")
//     LineNumber(291)
//     Category(ProfilingStackFrame::Category::OTHER)
//
// - A profiling stack frame with an overly long dynamic string:
//
//     Label("")
//     DynamicStringFragment("(too lon")
//     DynamicStringFragment("g)")
//     LineNumber(100)
//     Category(ProfilingStackFrame::Category::NETWORK)
//
// - A wasm JIT frame:
//
//     Label("")
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
//     DynamicStringFragment("u (https")
//     DynamicStringFragment("://perf-")
//     DynamicStringFragment("html.io/")
//     DynamicStringFragment("ac0da204")
//     DynamicStringFragment("aaa44d75")
//     DynamicStringFragment("a800.bun")
//     DynamicStringFragment("dle.js:2")
//     DynamicStringFragment("5)")
void
ProfileBuffer::StreamSamplesToJSON(SpliceableJSONWriter& aWriter, int aThreadId,
                                   double aSinceTime,
                                   UniqueStacks& aUniqueStacks) const
{
  UniquePtr<char[]> strbuf = MakeUnique<char[]>(kMaxFrameKeyLength);

  // Because this is a format entirely internal to the Profiler, any parsing
  // error indicates a bug in the ProfileBuffer writing or the parser itself,
  // or possibly flaky hardware.
  #define ERROR_AND_CONTINUE(msg) \
    { \
      fprintf(stderr, "ProfileBuffer parse error: %s", msg); \
      MOZ_ASSERT(false, msg); \
      continue; \
    }

  EntryGetter e(*this);

  for (;;) {
    // This block skips entries until we find the start of the next sample.
    // This is useful in three situations.
    //
    // - The circular buffer overwrites old entries, so when we start parsing
    //   we might be in the middle of a sample, and we must skip forward to the
    //   start of the next sample.
    //
    // - We skip samples that don't have an appropriate ThreadId or Time.
    //
    // - We skip range Pause, Resume, CollectionStart, Marker, and CollectionEnd
    //   entries between samples.
    while (e.Has()) {
      if (e.Get().IsThreadId()) {
        break;
      } else {
        e.Next();
      }
    }

    if (!e.Has()) {
      break;
    }

    if (e.Get().IsThreadId()) {
      int threadId = e.Get().u.mInt;
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

    if (e.Has() && e.Get().IsTime()) {
      sample.mTime = e.Get().u.mDouble;
      e.Next();

      // Ignore samples that are too old.
      if (sample.mTime < aSinceTime) {
        continue;
      }
    } else {
      ERROR_AND_CONTINUE("expected a Time entry");
    }

    UniqueStacks::StackKey stack =
      aUniqueStacks.BeginStack(UniqueStacks::FrameKey("(root)"));

    int numFrames = 0;
    while (e.Has()) {
      if (e.Get().IsNativeLeafAddr()) {
        numFrames++;

        // Bug 753041: We need a double cast here to tell GCC that we don't
        // want to sign extend 32-bit addresses starting with 0xFXXXXXX.
        unsigned long long pc = (unsigned long long)(uintptr_t)e.Get().u.mPtr;
        char buf[20];
        SprintfLiteral(buf, "%#llx", pc);
        stack = aUniqueStacks.AppendFrame(stack, UniqueStacks::FrameKey(buf));
        e.Next();

      } else if (e.Get().IsLabel()) {
        numFrames++;

        // Copy the label into strbuf.
        const char* label = e.Get().u.mString;
        strncpy(strbuf.get(), label, kMaxFrameKeyLength);
        size_t i = strlen(label);
        e.Next();

        bool seenFirstDynamicStringFragment = false;
        while (e.Has()) {
          if (e.Get().IsDynamicStringFragment()) {
            // If this is the first dynamic string fragment and we have a
            // non-empty label, insert a ' ' after the label and before the
            // dynamic string.
            if (!seenFirstDynamicStringFragment) {
              if (i > 0 && i < kMaxFrameKeyLength) {
                strbuf[i] = ' ';
                i++;
              }
              seenFirstDynamicStringFragment = true;
            }

            for (size_t j = 0; j < ProfileBufferEntry::kNumChars; j++) {
              const char* chars = e.Get().u.mChars;
              if (i < kMaxFrameKeyLength) {
                strbuf[i] = chars[j];
                i++;
              }
            }
            e.Next();
          } else {
            break;
          }
        }
        strbuf[kMaxFrameKeyLength - 1] = '\0';

        Maybe<unsigned> line;
        if (e.Has() && e.Get().IsLineNumber()) {
          line = Some(unsigned(e.Get().u.mInt));
          e.Next();
        }

        Maybe<unsigned> category;
        if (e.Has() && e.Get().IsCategory()) {
          category = Some(unsigned(e.Get().u.mInt));
          e.Next();
        }

        stack = aUniqueStacks.AppendFrame(
          stack, UniqueStacks::FrameKey(strbuf.get(), line, category));

      } else if (e.Get().IsJitReturnAddr()) {
        numFrames++;

        // A JIT frame may expand to multiple frames due to inlining.
        void* pc = e.Get().u.mPtr;
        const Maybe<nsTArray<UniqueStacks::FrameKey>>& frameKeys =
          aUniqueStacks.LookupFramesForJITAddressFromBufferPos(pc, e.CurPos());
        MOZ_RELEASE_ASSERT(frameKeys,
          "Attempting to stream samples for a buffer range for which we don't have JITFrameInfo?");
        for (const UniqueStacks::FrameKey& frameKey : *frameKeys) {
          stack = aUniqueStacks.AppendFrame(stack, frameKey);
        }

        e.Next();

      } else {
        break;
      }
    }

    if (numFrames == 0) {
      ERROR_AND_CONTINUE("expected one or more frame entries");
    }

    sample.mStack = aUniqueStacks.GetOrAddStackIndex(stack);

    // Skip over the markers. We process them in StreamMarkersToJSON().
    while (e.Has()) {
      if (e.Get().IsMarker()) {
        e.Next();
      } else {
        break;
      }
    }

    if (e.Has() && e.Get().IsResponsiveness()) {
      sample.mResponsiveness = Some(e.Get().u.mDouble);
      e.Next();
    }

    if (e.Has() && e.Get().IsResidentMemory()) {
      sample.mRSS = Some(e.Get().u.mDouble);
      e.Next();
    }

    if (e.Has() && e.Get().IsUnsharedMemory()) {
      sample.mUSS = Some(e.Get().u.mDouble);
      e.Next();
    }

    WriteSample(aWriter, *aUniqueStacks.mUniqueStrings, sample);
  }

  #undef ERROR_AND_CONTINUE
}

void
ProfileBuffer::AddJITInfoForRange(uint64_t aRangeStart,
                                  int aThreadId, JSContext* aContext,
                                  JITFrameInfo& aJITFrameInfo) const
{
  // We can only process JitReturnAddr entries if we have a JSContext.
  MOZ_RELEASE_ASSERT(aContext);

  aRangeStart = std::max(aRangeStart, mRangeStart);
  aJITFrameInfo.AddInfoForRange(aRangeStart, mRangeEnd, aContext,
    [&](const std::function<void(void*)>& aJITAddressConsumer) {
      // Find all JitReturnAddr entries in the given range for the given thread,
      // and call aJITAddressConsumer with those addresses.

      EntryGetter e(*this, aRangeStart);
      while (true) {
        // Advance to the next ThreadId entry.
        while (e.Has() && !e.Get().IsThreadId()) {
          e.Next();
        }
        if (!e.Has()) {
          break;
        }

        MOZ_ASSERT(e.Get().IsThreadId());
        int threadId = e.Get().u.mInt;
        e.Next();

        // Ignore samples that are for a different thread.
        if (threadId != aThreadId) {
          continue;
        }

        while (e.Has() && !e.Get().IsThreadId()) {
          if (e.Get().IsJitReturnAddr()) {
            aJITAddressConsumer(e.Get().u.mPtr);
          }
          e.Next();
        }
      }
    });
}

void
ProfileBuffer::StreamMarkersToJSON(SpliceableJSONWriter& aWriter,
                                   int aThreadId,
                                   const TimeStamp& aProcessStartTime,
                                   double aSinceTime,
                                   UniqueStacks& aUniqueStacks) const
{
  EntryGetter e(*this);

  // Stream all markers whose threadId matches aThreadId. We skip other entries,
  // because we process them in StreamSamplesToJSON().
  //
  // NOTE: The ThreadId of a marker is determined by its GetThreadId() method,
  // rather than ThreadId buffer entries, as markers can be added outside of
  // samples.
  while (e.Has()) {
    if (e.Get().IsMarker()) {
      const ProfilerMarker* marker = e.Get().u.mMarker;
      if (marker->GetTime() >= aSinceTime &&
          marker->GetThreadId() == aThreadId) {
        marker->StreamJSON(aWriter, aProcessStartTime, aUniqueStacks);
      }
    }
    e.Next();
  }
}

static void
AddPausedRange(SpliceableJSONWriter& aWriter, const char* aReason,
               const Maybe<double>& aStartTime, const Maybe<double>& aEndTime)
{
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

void
ProfileBuffer::StreamPausedRangesToJSON(SpliceableJSONWriter& aWriter,
                                        double aSinceTime) const
{
  EntryGetter e(*this);

  Maybe<double> currentPauseStartTime;
  Maybe<double> currentCollectionStartTime;

  while (e.Has()) {
    if (e.Get().IsPause()) {
      currentPauseStartTime = Some(e.Get().u.mDouble);
    } else if (e.Get().IsResume()) {
      AddPausedRange(aWriter, "profiler-paused",
                     currentPauseStartTime, Some(e.Get().u.mDouble));
      currentPauseStartTime = Nothing();
    } else if (e.Get().IsCollectionStart()) {
      currentCollectionStartTime = Some(e.Get().u.mDouble);
    } else if (e.Get().IsCollectionEnd()) {
      AddPausedRange(aWriter, "collecting",
                     currentCollectionStartTime, Some(e.Get().u.mDouble));
      currentCollectionStartTime = Nothing();
    }
    e.Next();
  }

  if (currentPauseStartTime) {
    AddPausedRange(aWriter, "profiler-paused",
                   currentPauseStartTime, Nothing());
  }
  if (currentCollectionStartTime) {
    AddPausedRange(aWriter, "collecting",
                   currentCollectionStartTime, Nothing());
  }
}

bool
ProfileBuffer::DuplicateLastSample(int aThreadId,
                                   const TimeStamp& aProcessStartTime,
                                   Maybe<uint64_t>& aLastSample)
{
  if (aLastSample && *aLastSample < mRangeStart) {
    // The last sample is no longer within the buffer range, so we cannot use
    // it. Reset the stored buffer position to Nothing().
    aLastSample.reset();
  }

  if (!aLastSample) {
    return false;
  }

  uint64_t lastSampleStartPos = *aLastSample;

  MOZ_RELEASE_ASSERT(GetEntry(lastSampleStartPos).IsThreadId() &&
                     GetEntry(lastSampleStartPos).u.mInt == aThreadId);

  aLastSample = Some(AddThreadIdEntry(aThreadId));

  EntryGetter e(*this, lastSampleStartPos + 1);

  // Go through the whole entry and duplicate it, until we find the next one.
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
        AddEntry(ProfileBufferEntry::Time(
          (TimeStamp::Now() - aProcessStartTime).ToMilliseconds()));
        break;
      case ProfileBufferEntry::Kind::Marker:
        // Don't copy markers
        break;
      default: {
        // Copy anything else we don't know about.
        ProfileBufferEntry entry = e.Get();
        AddEntry(entry);
        break;
      }
    }
    e.Next();
  }
  return true;
}

// END ProfileBuffer
////////////////////////////////////////////////////////////////////////

