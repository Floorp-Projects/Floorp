/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ostream>
#include "platform.h"
#include "mozilla/HashFunctions.h"

#ifndef SPS_STANDALONE
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

// JS
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/TrackedOptimizationInfo.h"
#endif

// Self
#include "ProfileEntry.h"

#if defined(_MSC_VER) && _MSC_VER < 1900
 #define snprintf _snprintf
#endif

using mozilla::MakeUnique;
using mozilla::UniquePtr;
using mozilla::Maybe;
using mozilla::Some;
using mozilla::Nothing;
using mozilla::JSONWriter;


////////////////////////////////////////////////////////////////////////
// BEGIN ProfileEntry

ProfileEntry::ProfileEntry()
  : mTagData(nullptr)
  , mTagName(0)
{ }

// aTagData must not need release (i.e. be a string from the text segment)
ProfileEntry::ProfileEntry(char aTagName, const char *aTagData)
  : mTagData(aTagData)
  , mTagName(aTagName)
{ }

ProfileEntry::ProfileEntry(char aTagName, ProfilerMarker *aTagMarker)
  : mTagMarker(aTagMarker)
  , mTagName(aTagName)
{ }

ProfileEntry::ProfileEntry(char aTagName, void *aTagPtr)
  : mTagPtr(aTagPtr)
  , mTagName(aTagName)
{ }

ProfileEntry::ProfileEntry(char aTagName, double aTagDouble)
  : mTagDouble(aTagDouble)
  , mTagName(aTagName)
{ }

ProfileEntry::ProfileEntry(char aTagName, uintptr_t aTagOffset)
  : mTagOffset(aTagOffset)
  , mTagName(aTagName)
{ }

ProfileEntry::ProfileEntry(char aTagName, Address aTagAddress)
  : mTagAddress(aTagAddress)
  , mTagName(aTagName)
{ }

ProfileEntry::ProfileEntry(char aTagName, int aTagInt)
  : mTagInt(aTagInt)
  , mTagName(aTagName)
{ }

ProfileEntry::ProfileEntry(char aTagName, char aTagChar)
  : mTagChar(aTagChar)
  , mTagName(aTagName)
{ }

bool ProfileEntry::is_ent_hint(char hintChar) {
  return mTagName == 'h' && mTagChar == hintChar;
}

bool ProfileEntry::is_ent_hint() {
  return mTagName == 'h';
}

bool ProfileEntry::is_ent(char tagChar) {
  return mTagName == tagChar;
}

void* ProfileEntry::get_tagPtr() {
  // No consistency checking.  Oh well.
  return mTagPtr;
}

// END ProfileEntry
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

#ifndef SPS_STANDALONE
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
                const char* location, Maybe<unsigned> lineno) override {
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

class StreamOptimizationAttemptsOp : public JS::ForEachTrackedOptimizationAttemptOp
{
  JSONWriter& mWriter;
  UniqueJSONStrings& mUniqueStrings;

public:
  StreamOptimizationAttemptsOp(JSONWriter& aWriter, UniqueJSONStrings& aUniqueStrings)
    : mWriter(aWriter),
      mUniqueStrings(aUniqueStrings)
  { }

  void operator()(JS::TrackedStrategy strategy, JS::TrackedOutcome outcome) override {
    // Schema:
    //   [strategy, outcome]

    mWriter.StartArrayElement();
    {
      mUniqueStrings.WriteElement(mWriter, JS::TrackedStrategyString(strategy));
      mUniqueStrings.WriteElement(mWriter, JS::TrackedOutcomeString(outcome));
    }
    mWriter.EndArray();
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
#endif

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
#ifdef SPS_STANDALONE
    hash = mozilla::HashString(mLocation.c_str());
#else
    hash = mozilla::HashString(mLocation.get());
#endif
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

UniqueStacks::UniqueStacks(JSRuntime* aRuntime)
 : mRuntime(aRuntime)
 , mFrameCount(0)
{
  mFrameTableWriter.StartBareList();
  mStackTableWriter.StartBareList();
}

#ifdef SPS_STANDALONE
uint32_t UniqueStacks::GetOrAddStackIndex(const StackKey& aStack)
{
  uint32_t index;
  auto it = mStackToIndexMap.find(aStack);

  if (it != mStackToIndexMap.end()) {
    return it->second;
  }

  index = mStackToIndexMap.size();
  mStackToIndexMap[aStack] = index;
  StreamStack(aStack);
  return index;
}
#else
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
#endif

#ifdef SPS_STANDALONE
uint32_t UniqueStacks::GetOrAddFrameIndex(const OnStackFrameKey& aFrame)
{
  uint32_t index;
  auto it = mFrameToIndexMap.find(aFrame);
  if (it != mFrameToIndexMap.end()) {
    MOZ_ASSERT(it->second < mFrameCount);
    return it->second;
  }

  // A manual count is used instead of mFrameToIndexMap.Count() due to
  // forwarding of canonical JIT frames above.
  index = mFrameCount++;
  mFrameToIndexMap[aFrame] = index;
  StreamFrame(aFrame);
  return index;
}
#else
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
#endif

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
  // Schema:
  //   [prefix, frame]

  mStackTableWriter.StartArrayElement();
  {
    if (aStack.mPrefix.isSome()) {
      mStackTableWriter.IntElement(*aStack.mPrefix);
    } else {
      mStackTableWriter.NullElement();
    }
    mStackTableWriter.IntElement(aStack.mFrame);
  }
  mStackTableWriter.EndArray();
}

void UniqueStacks::StreamFrame(const OnStackFrameKey& aFrame)
{
  // Schema:
  //   [location, implementation, optimizations, line, category]

  mFrameTableWriter.StartArrayElement();
#ifndef SPS_STANDALONE
  if (!aFrame.mJITFrameHandle) {
#else
  {
#endif
#ifdef SPS_STANDALONE
    mUniqueStrings.WriteElement(mFrameTableWriter, aFrame.mLocation.c_str());
#else
    mUniqueStrings.WriteElement(mFrameTableWriter, aFrame.mLocation.get());
#endif
    if (aFrame.mLine.isSome()) {
      mFrameTableWriter.NullElement(); // implementation
      mFrameTableWriter.NullElement(); // optimizations
      mFrameTableWriter.IntElement(*aFrame.mLine);
    }
    if (aFrame.mCategory.isSome()) {
      if (aFrame.mLine.isNothing()) {
        mFrameTableWriter.NullElement(); // line
      }
      mFrameTableWriter.IntElement(*aFrame.mCategory);
    }
  }
#ifndef SPS_STANDALONE
  else {
    const JS::ForEachProfiledFrameOp::FrameHandle& jitFrame = *aFrame.mJITFrameHandle;

    mUniqueStrings.WriteElement(mFrameTableWriter, jitFrame.label());

    JS::ProfilingFrameIterator::FrameKind frameKind = jitFrame.frameKind();
    MOZ_ASSERT(frameKind == JS::ProfilingFrameIterator::Frame_Ion ||
               frameKind == JS::ProfilingFrameIterator::Frame_Baseline);
    mUniqueStrings.WriteElement(mFrameTableWriter,
                                frameKind == JS::ProfilingFrameIterator::Frame_Ion
                                ? "ion"
                                : "baseline");

    if (jitFrame.hasTrackedOptimizations()) {
      mFrameTableWriter.StartObjectElement();
      {
        mFrameTableWriter.StartArrayProperty("types");
        {
          StreamOptimizationTypeInfoOp typeInfoOp(mFrameTableWriter, mUniqueStrings);
          jitFrame.forEachOptimizationTypeInfo(typeInfoOp);
        }
        mFrameTableWriter.EndArray();

        JS::Rooted<JSScript*> script(mRuntime);
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
#endif
  mFrameTableWriter.EndArray();
}

struct ProfileSample
{
  uint32_t mStack;
  Maybe<double> mTime;
  Maybe<double> mResponsiveness;
  Maybe<double> mRSS;
  Maybe<double> mUSS;
  Maybe<int> mFrameNumber;
  Maybe<double> mPower;
};

static void WriteSample(SpliceableJSONWriter& aWriter, ProfileSample& aSample)
{
  // Schema:
  //   [stack, time, responsiveness, rss, uss, frameNumber, power]

  aWriter.StartArrayElement();
  {
    // The last non-null index is tracked to save space in the JSON by avoid
    // emitting 'null's at the end of the array, as they're only needed if
    // followed by non-null elements.
    uint32_t index = 0;
    uint32_t lastNonNullIndex = 0;

    aWriter.IntElement(aSample.mStack);
    index++;

    if (aSample.mTime.isSome()) {
      lastNonNullIndex = index;
      aWriter.DoubleElement(*aSample.mTime);
    }
    index++;

    if (aSample.mResponsiveness.isSome()) {
      aWriter.NullElements(index - lastNonNullIndex - 1);
      lastNonNullIndex = index;
      aWriter.DoubleElement(*aSample.mResponsiveness);
    }
    index++;

    if (aSample.mRSS.isSome()) {
      aWriter.NullElements(index - lastNonNullIndex - 1);
      lastNonNullIndex = index;
      aWriter.DoubleElement(*aSample.mRSS);
    }
    index++;

    if (aSample.mUSS.isSome()) {
      aWriter.NullElements(index - lastNonNullIndex - 1);
      lastNonNullIndex = index;
      aWriter.DoubleElement(*aSample.mUSS);
    }
    index++;

    if (aSample.mFrameNumber.isSome()) {
      aWriter.NullElements(index - lastNonNullIndex - 1);
      lastNonNullIndex = index;
      aWriter.IntElement(*aSample.mFrameNumber);
    }
    index++;

    if (aSample.mPower.isSome()) {
      aWriter.NullElements(index - lastNonNullIndex - 1);
      lastNonNullIndex = index;
      aWriter.DoubleElement(*aSample.mPower);
    }
    index++;
  }
  aWriter.EndArray();
}

void ProfileBuffer::StreamSamplesToJSON(SpliceableJSONWriter& aWriter, int aThreadId,
                                        double aSinceTime, JSRuntime* aRuntime,
                                        UniqueStacks& aUniqueStacks)
{
  Maybe<ProfileSample> sample;
  int readPos = mReadPos;
  int currentThreadID = -1;
  Maybe<double> currentTime;
  UniquePtr<char[]> tagBuff = MakeUnique<char[]>(DYNAMIC_MAX_STRING);

  while (readPos != mWritePos) {
    ProfileEntry entry = mEntries[readPos];
    if (entry.mTagName == 'T') {
      currentThreadID = entry.mTagInt;
      currentTime.reset();
      int readAheadPos = (readPos + 1) % mEntrySize;
      if (readAheadPos != mWritePos) {
        ProfileEntry readAheadEntry = mEntries[readAheadPos];
        if (readAheadEntry.mTagName == 't') {
          currentTime = Some(readAheadEntry.mTagDouble);
        }
      }
    }
    if (currentThreadID == aThreadId && (currentTime.isNothing() || *currentTime >= aSinceTime)) {
      switch (entry.mTagName) {
      case 'r':
        if (sample.isSome()) {
          sample->mResponsiveness = Some(entry.mTagDouble);
        }
        break;
      case 'p':
        if (sample.isSome()) {
          sample->mPower = Some(entry.mTagDouble);
        }
        break;
      case 'R':
        if (sample.isSome()) {
          sample->mRSS = Some(entry.mTagDouble);
        }
        break;
      case 'U':
        if (sample.isSome()) {
          sample->mUSS = Some(entry.mTagDouble);
         }
        break;
      case 'f':
        if (sample.isSome()) {
          sample->mFrameNumber = Some(entry.mTagInt);
        }
        break;
      case 's':
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
          ProfileEntry frame = mEntries[framePos];
          while (framePos != mWritePos && frame.mTagName != 's' && frame.mTagName != 'T') {
            int incBy = 1;
            frame = mEntries[framePos];

            // Read ahead to the next tag, if it's a 'd' tag process it now
            const char* tagStringData = frame.mTagData;
            int readAheadPos = (framePos + 1) % mEntrySize;
            // Make sure the string is always null terminated if it fills up
            // DYNAMIC_MAX_STRING-2
            tagBuff[DYNAMIC_MAX_STRING-1] = '\0';

            if (readAheadPos != mWritePos && mEntries[readAheadPos].mTagName == 'd') {
              tagStringData = processDynamicTag(framePos, &incBy, tagBuff.get());
            }

            // Write one frame. It can have either
            // 1. only location - 'l' containing a memory address
            // 2. location and line number - 'c' followed by 'd's,
            // an optional 'n' and an optional 'y'
            // 3. a JIT return address - 'j' containing native code address
            if (frame.mTagName == 'l') {
              // Bug 753041
              // We need a double cast here to tell GCC that we don't want to sign
              // extend 32-bit addresses starting with 0xFXXXXXX.
              unsigned long long pc = (unsigned long long)(uintptr_t)frame.mTagPtr;
              snprintf(tagBuff.get(), DYNAMIC_MAX_STRING, "%#llx", pc);
              stack.AppendFrame(UniqueStacks::OnStackFrameKey(tagBuff.get()));
            } else if (frame.mTagName == 'c') {
              UniqueStacks::OnStackFrameKey frameKey(tagStringData);
              readAheadPos = (framePos + incBy) % mEntrySize;
              if (readAheadPos != mWritePos &&
                  mEntries[readAheadPos].mTagName == 'n') {
                frameKey.mLine = Some((unsigned) mEntries[readAheadPos].mTagInt);
                incBy++;
              }
              readAheadPos = (framePos + incBy) % mEntrySize;
              if (readAheadPos != mWritePos &&
                  mEntries[readAheadPos].mTagName == 'y') {
                frameKey.mCategory = Some((unsigned) mEntries[readAheadPos].mTagInt);
                incBy++;
              }
              stack.AppendFrame(frameKey);
#ifndef SPS_STANDALONE
            } else if (frame.mTagName == 'J') {
              // A JIT frame may expand to multiple frames due to inlining.
              void* pc = frame.mTagPtr;
              unsigned depth = aUniqueStacks.LookupJITFrameDepth(pc);
              if (depth == 0) {
                StreamJSFramesOp framesOp(pc, stack);
                JS::ForEachProfiledFrame(aRuntime, pc, framesOp);
                aUniqueStacks.AddJITFrameDepth(pc, framesOp.depth());
              } else {
                for (unsigned i = 0; i < depth; i++) {
                  UniqueStacks::OnStackFrameKey inlineFrameKey(pc, i);
                  stack.AppendFrame(inlineFrameKey);
                }
              }
#endif
            }
            framePos = (framePos + incBy) % mEntrySize;
          }

          sample->mStack = stack.GetOrAddIndex();
          break;
        }
      }
    }
    readPos = (readPos + 1) % mEntrySize;
  }
  if (sample.isSome()) {
    WriteSample(aWriter, *sample);
  }
}

void ProfileBuffer::StreamMarkersToJSON(SpliceableJSONWriter& aWriter, int aThreadId,
                                        double aSinceTime, UniqueStacks& aUniqueStacks)
{
  int readPos = mReadPos;
  int currentThreadID = -1;
  while (readPos != mWritePos) {
    ProfileEntry entry = mEntries[readPos];
    if (entry.mTagName == 'T') {
      currentThreadID = entry.mTagInt;
    } else if (currentThreadID == aThreadId && entry.mTagName == 'm') {
      const ProfilerMarker* marker = entry.getMarker();
      if (marker->GetTime() >= aSinceTime) {
        entry.getMarker()->StreamJSON(aWriter, aUniqueStacks);
      }
    }
    readPos = (readPos + 1) % mEntrySize;
  }
}

int ProfileBuffer::FindLastSampleOfThread(int aThreadId)
{
  // We search backwards from mWritePos-1 to mReadPos.
  // Adding mEntrySize makes the result of the modulus positive.
  for (int readPos  = (mWritePos + mEntrySize - 1) % mEntrySize;
           readPos !=  (mReadPos + mEntrySize - 1) % mEntrySize;
           readPos  =   (readPos + mEntrySize - 1) % mEntrySize) {
    ProfileEntry entry = mEntries[readPos];
    if (entry.mTagName == 'T' && entry.mTagInt == aThreadId) {
      return readPos;
    }
  }

  return -1;
}

void ProfileBuffer::DuplicateLastSample(int aThreadId)
{
  int lastSampleStartPos = FindLastSampleOfThread(aThreadId);
  if (lastSampleStartPos == -1) {
    return;
  }

  MOZ_ASSERT(mEntries[lastSampleStartPos].mTagName == 'T');

  addTag(mEntries[lastSampleStartPos]);

  // Go through the whole entry and duplicate it, until we find the next one.
  for (int readPos = (lastSampleStartPos + 1) % mEntrySize;
       readPos != mWritePos;
       readPos = (readPos + 1) % mEntrySize) {
    switch (mEntries[readPos].mTagName) {
      case 'T':
        // We're done.
        return;
      case 't':
        // Copy with new time
        addTag(ProfileEntry('t', (mozilla::TimeStamp::Now() - sStartTime).ToMilliseconds()));
        break;
      case 'm':
        // Don't copy markers
        break;
      // Copy anything else we don't know about
      // L, B, S, c, s, d, l, f, h, r, t, p
      default:
        addTag(mEntries[readPos]);
        break;
    }
  }
}

// END ProfileBuffer
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
// BEGIN ThreadProfile

// END ThreadProfile
////////////////////////////////////////////////////////////////////////
