/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ostream>
#include "platform.h"
#include "mozilla/HashFunctions.h"

#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

// JS
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/TrackedOptimizationInfo.h"

// Self
#include "ProfileBufferEntry.h"

using mozilla::JSONWriter;
using mozilla::MakeUnique;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;
using mozilla::TimeStamp;
using mozilla::UniquePtr;

////////////////////////////////////////////////////////////////////////
// BEGIN ProfileBufferEntry

ProfileBufferEntry::ProfileBufferEntry()
  : mTagData(nullptr)
  , mKind(Kind::INVALID)
{ }

// aTagData must not need release (i.e. be a string from the text segment)
ProfileBufferEntry::ProfileBufferEntry(Kind aKind, const char *aTagData)
  : mTagData(aTagData)
  , mKind(aKind)
{ }

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, ProfilerMarker *aTagMarker)
  : mTagMarker(aTagMarker)
  , mKind(aKind)
{ }

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, void *aTagPtr)
  : mTagPtr(aTagPtr)
  , mKind(aKind)
{ }

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, double aTagDouble)
  : mTagDouble(aTagDouble)
  , mKind(aKind)
{ }

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, uintptr_t aTagOffset)
  : mTagOffset(aTagOffset)
  , mKind(aKind)
{ }

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, Address aTagAddress)
  : mTagAddress(aTagAddress)
  , mKind(aKind)
{ }

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, int aTagInt)
  : mTagInt(aTagInt)
  , mKind(aKind)
{ }

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, char aTagChar)
  : mTagChar(aTagChar)
  , mKind(aKind)
{ }

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
    aWriter.StartObjectProperty("schema");
  }

  void WriteField(const char* aName) {
    mWriter.IntProperty(aName, mIndex++);
  }

  ~JSONSchemaWriter() {
    mWriter.EndObject();
  }
};

class StreamOptimizationTypeInfoOp : public JS::ForEachTrackedOptimizationTypeInfoOp
{
  JSONWriter& mWriter;
  UniqueJSONStrings& mUniqueStrings;
  bool mStartedTypeList;

public:
  StreamOptimizationTypeInfoOp(JSONWriter& aWriter, UniqueJSONStrings& aUniqueStrings)
    : mWriter(aWriter)
    , mUniqueStrings(aUniqueStrings)
    , mStartedTypeList(false)
  { }

  void readType(const char* keyedBy, const char* name,
                const char* location, const Maybe<unsigned>& lineno) override {
    if (!mStartedTypeList) {
      mStartedTypeList = true;
      mWriter.StartObjectElement();
      mWriter.StartArrayProperty("typeset");
    }

    mWriter.StartObjectElement();
    {
      mUniqueStrings.WriteProperty(mWriter, "keyedBy", keyedBy);
      if (name) {
        mUniqueStrings.WriteProperty(mWriter, "name", name);
      }
      if (location) {
        mUniqueStrings.WriteProperty(mWriter, "location", location);
      }
      if (lineno.isSome()) {
        mWriter.IntProperty("line", *lineno);
      }
    }
    mWriter.EndObject();
  }

  void operator()(JS::TrackedTypeSite site, const char* mirType) override {
    if (mStartedTypeList) {
      mWriter.EndArray();
      mStartedTypeList = false;
    } else {
      mWriter.StartObjectElement();
    }

    {
      mUniqueStrings.WriteProperty(mWriter, "site", JS::TrackedTypeSiteString(site));
      mUniqueStrings.WriteProperty(mWriter, "mirType", mirType);
    }
    mWriter.EndObject();
  }
};

// As mentioned in ProfileBufferEntry.h, the JSON format contains many
// arrays whose elements are laid out according to various schemas to help
// de-duplication. This RAII class helps write these arrays by keeping track of
// the last non-null element written and adding the appropriate number of null
// elements when writing new non-null elements. It also automatically opens and
// closes an array element on the given JSON writer.
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
class MOZ_RAII AutoArraySchemaWriter
{
  friend class AutoObjectWriter;

  SpliceableJSONWriter& mJSONWriter;
  UniqueJSONStrings*    mStrings;
  uint32_t              mNextFreeIndex;

public:
  AutoArraySchemaWriter(SpliceableJSONWriter& aWriter, UniqueJSONStrings& aStrings)
    : mJSONWriter(aWriter)
    , mStrings(&aStrings)
    , mNextFreeIndex(0)
  {
    mJSONWriter.StartArrayElement();
  }

  // If you don't have access to a UniqueStrings, you had better not try and
  // write a string element down the line!
  explicit AutoArraySchemaWriter(SpliceableJSONWriter& aWriter)
    : mJSONWriter(aWriter)
    , mStrings(nullptr)
    , mNextFreeIndex(0)
  {
    mJSONWriter.StartArrayElement();
  }

  ~AutoArraySchemaWriter() {
    mJSONWriter.EndArray();
  }

  void FillUpTo(uint32_t aIndex) {
    MOZ_ASSERT(aIndex >= mNextFreeIndex);
    mJSONWriter.NullElements(aIndex - mNextFreeIndex);
    mNextFreeIndex = aIndex + 1;
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
    MOZ_RELEASE_ASSERT(mStrings);
    FillUpTo(aIndex);
    mStrings->WriteElement(mJSONWriter, aValue);
  }
};

class StreamOptimizationAttemptsOp : public JS::ForEachTrackedOptimizationAttemptOp
{
  SpliceableJSONWriter& mWriter;
  UniqueJSONStrings& mUniqueStrings;

public:
  StreamOptimizationAttemptsOp(SpliceableJSONWriter& aWriter, UniqueJSONStrings& aUniqueStrings)
    : mWriter(aWriter),
      mUniqueStrings(aUniqueStrings)
  { }

  void operator()(JS::TrackedStrategy strategy, JS::TrackedOutcome outcome) override {
    enum Schema : uint32_t {
      STRATEGY = 0,
      OUTCOME = 1
    };

    AutoArraySchemaWriter writer(mWriter, mUniqueStrings);
    writer.StringElement(STRATEGY, JS::TrackedStrategyString(strategy));
    writer.StringElement(OUTCOME, JS::TrackedOutcomeString(outcome));
  }
};

class StreamJSFramesOp : public JS::ForEachProfiledFrameOp
{
  void* mReturnAddress;
  UniqueStacks::Stack& mStack;
  unsigned mDepth;

public:
  StreamJSFramesOp(void* aReturnAddr, UniqueStacks::Stack& aStack)
   : mReturnAddress(aReturnAddr)
   , mStack(aStack)
   , mDepth(0)
  { }

  unsigned depth() const {
    MOZ_ASSERT(mDepth > 0);
    return mDepth;
  }

  void operator()(const JS::ForEachProfiledFrameOp::FrameHandle& aFrameHandle) override {
    UniqueStacks::OnStackFrameKey frameKey(mReturnAddress, mDepth, aFrameHandle);
    mStack.AppendFrame(frameKey);
    mDepth++;
  }
};

uint32_t UniqueJSONStrings::GetOrAddIndex(const char* aStr)
{
  uint32_t index;
  StringKey key(aStr);

  auto it = mStringToIndexMap.find(key);

  if (it != mStringToIndexMap.end()) {
    return it->second;
  }
  index = mStringToIndexMap.size();
  mStringToIndexMap[key] = index;
  mStringTableWriter.StringElement(aStr);
  return index;
}

bool UniqueStacks::FrameKey::operator==(const FrameKey& aOther) const
{
  return mLocation == aOther.mLocation &&
         mLine == aOther.mLine &&
         mCategory == aOther.mCategory &&
         mJITAddress == aOther.mJITAddress &&
         mJITDepth == aOther.mJITDepth;
}

bool UniqueStacks::StackKey::operator==(const StackKey& aOther) const
{
  MOZ_ASSERT_IF(mPrefix == aOther.mPrefix, mPrefixHash == aOther.mPrefixHash);
  return mPrefix == aOther.mPrefix && mFrame == aOther.mFrame;
}

UniqueStacks::Stack::Stack(UniqueStacks& aUniqueStacks, const OnStackFrameKey& aRoot)
 : mUniqueStacks(aUniqueStacks)
 , mStack(aUniqueStacks.GetOrAddFrameIndex(aRoot))
{
}

void UniqueStacks::Stack::AppendFrame(const OnStackFrameKey& aFrame)
{
  // Compute the prefix hash and index before mutating mStack.
  uint32_t prefixHash = mStack.Hash();
  uint32_t prefix = mUniqueStacks.GetOrAddStackIndex(mStack);
  mStack.UpdateHash(prefixHash, prefix, mUniqueStacks.GetOrAddFrameIndex(aFrame));
}

uint32_t UniqueStacks::Stack::GetOrAddIndex() const
{
  return mUniqueStacks.GetOrAddStackIndex(mStack);
}

uint32_t UniqueStacks::FrameKey::Hash() const
{
  uint32_t hash = 0;
  if (!mLocation.IsEmpty()) {
    hash = mozilla::HashString(mLocation.get());
  }
  if (mLine.isSome()) {
    hash = mozilla::AddToHash(hash, *mLine);
  }
  if (mCategory.isSome()) {
    hash = mozilla::AddToHash(hash, *mCategory);
  }
  if (mJITAddress.isSome()) {
    hash = mozilla::AddToHash(hash, *mJITAddress);
    if (mJITDepth.isSome()) {
      hash = mozilla::AddToHash(hash, *mJITDepth);
    }
  }
  return hash;
}

uint32_t UniqueStacks::StackKey::Hash() const
{
  if (mPrefix.isNothing()) {
    return mozilla::HashGeneric(mFrame);
  }
  return mozilla::AddToHash(*mPrefixHash, mFrame);
}

UniqueStacks::Stack UniqueStacks::BeginStack(const OnStackFrameKey& aRoot)
{
  return Stack(*this, aRoot);
}

UniqueStacks::UniqueStacks(JSContext* aContext)
 : mContext(aContext)
 , mFrameCount(0)
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

uint32_t UniqueStacks::GetOrAddFrameIndex(const OnStackFrameKey& aFrame)
{
  uint32_t index;
  if (mFrameToIndexMap.Get(aFrame, &index)) {
    MOZ_ASSERT(index < mFrameCount);
    return index;
  }

  // If aFrame isn't canonical, forward it to the canonical frame's index.
  if (aFrame.mJITFrameHandle) {
    void* canonicalAddr = aFrame.mJITFrameHandle->canonicalAddress();
    if (canonicalAddr != *aFrame.mJITAddress) {
      OnStackFrameKey canonicalKey(canonicalAddr, *aFrame.mJITDepth, *aFrame.mJITFrameHandle);
      uint32_t canonicalIndex = GetOrAddFrameIndex(canonicalKey);
      mFrameToIndexMap.Put(aFrame, canonicalIndex);
      return canonicalIndex;
    }
  }

  // A manual count is used instead of mFrameToIndexMap.Count() due to
  // forwarding of canonical JIT frames above.
  index = mFrameCount++;
  mFrameToIndexMap.Put(aFrame, index);
  StreamFrame(aFrame);
  return index;
}

uint32_t UniqueStacks::LookupJITFrameDepth(void* aAddr)
{
  uint32_t depth;

  auto it = mJITFrameDepthMap.find(aAddr);
  if (it != mJITFrameDepthMap.end()) {
    depth = it->second;
    MOZ_ASSERT(depth > 0);
    return depth;
  }
  return 0;
}

void UniqueStacks::AddJITFrameDepth(void* aAddr, unsigned depth)
{
  mJITFrameDepthMap[aAddr] = depth;
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

  AutoArraySchemaWriter writer(mStackTableWriter, mUniqueStrings);
  if (aStack.mPrefix.isSome()) {
    writer.IntElement(PREFIX, *aStack.mPrefix);
  }
  writer.IntElement(FRAME, aStack.mFrame);
}

void UniqueStacks::StreamFrame(const OnStackFrameKey& aFrame)
{
  enum Schema : uint32_t {
    LOCATION = 0,
    IMPLEMENTATION = 1,
    OPTIMIZATIONS = 2,
    LINE = 3,
    CATEGORY = 4
  };

  AutoArraySchemaWriter writer(mFrameTableWriter, mUniqueStrings);

  if (!aFrame.mJITFrameHandle) {
    writer.StringElement(LOCATION, aFrame.mLocation.get());
    if (aFrame.mLine.isSome()) {
      writer.IntElement(LINE, *aFrame.mLine);
    }
    if (aFrame.mCategory.isSome()) {
      writer.IntElement(CATEGORY, *aFrame.mCategory);
    }
  } else {
    const JS::ForEachProfiledFrameOp::FrameHandle& jitFrame = *aFrame.mJITFrameHandle;

    writer.StringElement(LOCATION, jitFrame.label());

    JS::ProfilingFrameIterator::FrameKind frameKind = jitFrame.frameKind();
    MOZ_ASSERT(frameKind == JS::ProfilingFrameIterator::Frame_Ion ||
               frameKind == JS::ProfilingFrameIterator::Frame_Baseline);
    writer.StringElement(IMPLEMENTATION,
                         frameKind == JS::ProfilingFrameIterator::Frame_Ion
                         ? "ion"
                         : "baseline");

    if (jitFrame.hasTrackedOptimizations()) {
      writer.FillUpTo(OPTIMIZATIONS);
      mFrameTableWriter.StartObjectElement();
      {
        mFrameTableWriter.StartArrayProperty("types");
        {
          StreamOptimizationTypeInfoOp typeInfoOp(mFrameTableWriter, mUniqueStrings);
          jitFrame.forEachOptimizationTypeInfo(typeInfoOp);
        }
        mFrameTableWriter.EndArray();

        JS::Rooted<JSScript*> script(mContext);
        jsbytecode* pc;
        mFrameTableWriter.StartObjectProperty("attempts");
        {
          {
            JSONSchemaWriter schema(mFrameTableWriter);
            schema.WriteField("strategy");
            schema.WriteField("outcome");
          }

          mFrameTableWriter.StartArrayProperty("data");
          {
            StreamOptimizationAttemptsOp attemptOp(mFrameTableWriter, mUniqueStrings);
            jitFrame.forEachOptimizationAttempt(attemptOp, script.address(), &pc);
          }
          mFrameTableWriter.EndArray();
        }
        mFrameTableWriter.EndObject();

        if (JSAtom* name = js::GetPropertyNameFromPC(script, pc)) {
          char buf[512];
          JS_PutEscapedFlatString(buf, mozilla::ArrayLength(buf), js::AtomToFlatString(name), 0);
          mUniqueStrings.WriteProperty(mFrameTableWriter, "propertyName", buf);
        }

        unsigned line, column;
        line = JS_PCToLineNumber(script, pc, &column);
        mFrameTableWriter.IntProperty("line", line);
        mFrameTableWriter.IntProperty("column", column);
      }
      mFrameTableWriter.EndObject();
    }
  }
}

struct ProfileSample
{
  uint32_t mStack;
  Maybe<double> mTime;
  Maybe<double> mResponsiveness;
  Maybe<double> mRSS;
  Maybe<double> mUSS;
};

static void WriteSample(SpliceableJSONWriter& aWriter, ProfileSample& aSample)
{
  enum Schema : uint32_t {
    STACK = 0,
    TIME = 1,
    RESPONSIVENESS = 2,
    RSS = 3,
    USS = 4
  };

  AutoArraySchemaWriter writer(aWriter);

  writer.IntElement(STACK, aSample.mStack);

  if (aSample.mTime.isSome()) {
    writer.DoubleElement(TIME, *aSample.mTime);
  }

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

void ProfileBuffer::StreamSamplesToJSON(SpliceableJSONWriter& aWriter, int aThreadId,
                                        double aSinceTime, JSContext* aContext,
                                        UniqueStacks& aUniqueStacks)
{
  Maybe<ProfileSample> sample;
  int readPos = mReadPos;
  int currentThreadID = -1;
  Maybe<double> currentTime;
  UniquePtr<char[]> tagBuff = MakeUnique<char[]>(DYNAMIC_MAX_STRING);

  while (readPos != mWritePos) {
    ProfileBufferEntry entry = mEntries[readPos];
    if (entry.isThreadId()) {
      currentThreadID = entry.mTagInt;
      currentTime.reset();
      int readAheadPos = (readPos + 1) % mEntrySize;
      if (readAheadPos != mWritePos) {
        ProfileBufferEntry readAheadEntry = mEntries[readAheadPos];
        if (readAheadEntry.isTime()) {
          currentTime = Some(readAheadEntry.mTagDouble);
        }
      }
    }
    if (currentThreadID == aThreadId && (currentTime.isNothing() || *currentTime >= aSinceTime)) {
      switch (entry.kind()) {
      case ProfileBufferEntry::Kind::Responsiveness:
        if (sample.isSome()) {
          sample->mResponsiveness = Some(entry.mTagDouble);
        }
        break;
      case ProfileBufferEntry::Kind::ResidentMemory:
        if (sample.isSome()) {
          sample->mRSS = Some(entry.mTagDouble);
        }
        break;
      case ProfileBufferEntry::Kind::UnsharedMemory:
        if (sample.isSome()) {
          sample->mUSS = Some(entry.mTagDouble);
         }
        break;
      case ProfileBufferEntry::Kind::Sample:
        {
          // end the previous sample if there was one
          if (sample.isSome()) {
            WriteSample(aWriter, *sample);
            sample.reset();
          }
          // begin the next sample
          sample.emplace();
          sample->mTime = currentTime;

          // Seek forward through the entire sample, looking for frames
          // this is an easier approach to reason about than adding more
          // control variables and cases to the loop that goes through the buffer once

          UniqueStacks::Stack stack =
            aUniqueStacks.BeginStack(UniqueStacks::OnStackFrameKey("(root)"));

          int framePos = (readPos + 1) % mEntrySize;
          ProfileBufferEntry frame = mEntries[framePos];
          while (framePos != mWritePos && !frame.isSample() && !frame.isThreadId()) {
            int incBy = 1;
            frame = mEntries[framePos];

            // Read ahead to the next tag, if it's an EmbeddedString
            // tag process it now
            const char* tagStringData = frame.mTagData;
            int readAheadPos = (framePos + 1) % mEntrySize;
            // Make sure the string is always null terminated if it fills up
            // DYNAMIC_MAX_STRING-2
            tagBuff[DYNAMIC_MAX_STRING-1] = '\0';

            if (readAheadPos != mWritePos && mEntries[readAheadPos].isEmbeddedString()) {
              tagStringData = processDynamicTag(framePos, &incBy, tagBuff.get());
            }

            // Write one frame. It can have either
            // 1. only location - a NativeLeafAddr containing a memory address
            // 2. location and line number - a CodeLocation followed by
            //    EmbeddedStrings, an optional LineNumber and an
            //    optional Category
            // 3. a JitReturnAddress containing a native code address
            if (frame.isNativeLeafAddr()) {
              // Bug 753041
              // We need a double cast here to tell GCC that we don't want to sign
              // extend 32-bit addresses starting with 0xFXXXXXX.
              unsigned long long pc = (unsigned long long)(uintptr_t)frame.mTagPtr;
              snprintf(tagBuff.get(), DYNAMIC_MAX_STRING, "%#llx", pc);
              stack.AppendFrame(UniqueStacks::OnStackFrameKey(tagBuff.get()));
            } else if (frame.isCodeLocation()) {
              UniqueStacks::OnStackFrameKey frameKey(tagStringData);
              readAheadPos = (framePos + incBy) % mEntrySize;
              if (readAheadPos != mWritePos &&
                  mEntries[readAheadPos].isLineNumber()) {
                frameKey.mLine = Some((unsigned) mEntries[readAheadPos].mTagInt);
                incBy++;
              }
              readAheadPos = (framePos + incBy) % mEntrySize;
              if (readAheadPos != mWritePos &&
                  mEntries[readAheadPos].isCategory()) {
                frameKey.mCategory = Some((unsigned) mEntries[readAheadPos].mTagInt);
                incBy++;
              }
              stack.AppendFrame(frameKey);
            } else if (frame.isJitReturnAddr()) {
              // A JIT frame may expand to multiple frames due to inlining.
              void* pc = frame.mTagPtr;
              unsigned depth = aUniqueStacks.LookupJITFrameDepth(pc);
              if (depth == 0) {
                StreamJSFramesOp framesOp(pc, stack);
                MOZ_RELEASE_ASSERT(aContext);
                JS::ForEachProfiledFrame(aContext, pc, framesOp);
                aUniqueStacks.AddJITFrameDepth(pc, framesOp.depth());
              } else {
                for (unsigned i = 0; i < depth; i++) {
                  UniqueStacks::OnStackFrameKey inlineFrameKey(pc, i);
                  stack.AppendFrame(inlineFrameKey);
                }
              }
            }
            framePos = (framePos + incBy) % mEntrySize;
          }

          sample->mStack = stack.GetOrAddIndex();
          break;
        }
      default:
        break;
      } /* switch (entry.kind()) */
    }
    readPos = (readPos + 1) % mEntrySize;
  }
  if (sample.isSome()) {
    WriteSample(aWriter, *sample);
  }
}

void
ProfileBuffer::StreamMarkersToJSON(SpliceableJSONWriter& aWriter,
                                   int aThreadId,
                                   const TimeStamp& aStartTime,
                                   double aSinceTime,
                                   UniqueStacks& aUniqueStacks)
{
  int readPos = mReadPos;
  int currentThreadID = -1;
  while (readPos != mWritePos) {
    ProfileBufferEntry entry = mEntries[readPos];
    if (entry.isThreadId()) {
      currentThreadID = entry.mTagInt;
    } else if (currentThreadID == aThreadId && entry.isMarker()) {
      const ProfilerMarker* marker = entry.getMarker();
      if (marker->GetTime() >= aSinceTime) {
        entry.getMarker()->StreamJSON(aWriter, aStartTime, aUniqueStacks);
      }
    }
    readPos = (readPos + 1) % mEntrySize;
  }
}

int
ProfileBuffer::FindLastSampleOfThread(int aThreadId, const LastSample& aLS)
{
  // |aLS| has a valid generation number if either it matches the buffer's
  // generation, or is one behind the buffer's generation, since the buffer's
  // generation is incremented on wraparound.  There's no ambiguity relative to
  // ProfileBuffer::reset, since that increments mGeneration by two.
  if (aLS.mGeneration == mGeneration ||
      (mGeneration > 0 && aLS.mGeneration == mGeneration - 1)) {
    int ix = aLS.mPos;

    if (ix == -1) {
      // There's no record of |aLS|'s thread ever having recorded a sample in
      // the buffer.
      return -1;
    }

    // It might be that the sample has since been overwritten, so check that it
    // is still valid.
    MOZ_RELEASE_ASSERT(0 <= ix && ix < mEntrySize);
    ProfileBufferEntry& entry = mEntries[ix];
    bool isStillValid = entry.isThreadId() && entry.mTagInt == aThreadId;
    return isStillValid ? ix : -1;
  }

  // |aLS| denotes a sample which is older than either two wraparounds or one
  // call to ProfileBuffer::reset.  In either case it is no longer valid.
  MOZ_ASSERT(aLS.mGeneration <= mGeneration - 2);
  return -1;
}

bool
ProfileBuffer::DuplicateLastSample(int aThreadId, const TimeStamp& aStartTime,
                                   LastSample& aLS)
{
  int lastSampleStartPos = FindLastSampleOfThread(aThreadId, aLS);
  if (lastSampleStartPos == -1) {
    return false;
  }

  MOZ_ASSERT(mEntries[lastSampleStartPos].isThreadId() &&
             mEntries[lastSampleStartPos].mTagInt == aThreadId);

  addTagThreadId(aThreadId, &aLS);

  // Go through the whole entry and duplicate it, until we find the next one.
  for (int readPos = (lastSampleStartPos + 1) % mEntrySize;
       readPos != mWritePos;
       readPos = (readPos + 1) % mEntrySize) {
    switch (mEntries[readPos].kind()) {
      case ProfileBufferEntry::Kind::ThreadId:
        // We're done.
        return true;
      case ProfileBufferEntry::Kind::Time:
        // Copy with new time
        addTag(ProfileBufferEntry::Time((TimeStamp::Now() -
                                         aStartTime).ToMilliseconds()));
        break;
      case ProfileBufferEntry::Kind::Marker:
        // Don't copy markers
        break;
      default:
        // Copy anything else we don't know about.
        addTag(mEntries[readPos]);
        break;
    }
  }
  return true;
}

// END ProfileBuffer
////////////////////////////////////////////////////////////////////////

