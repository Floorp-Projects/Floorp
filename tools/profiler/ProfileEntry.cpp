/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ostream>
#include "platform.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/HashFunctions.h"

// JS
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/TrackedOptimizationInfo.h"

// JSON
#include "ProfileJSONWriter.h"

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

ProfileEntry::ProfileEntry(char aTagName, float aTagFloat)
  : mTagFloat(aTagFloat)
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


////////////////////////////////////////////////////////////////////////
// BEGIN ProfileBuffer

ProfileBuffer::ProfileBuffer(int aEntrySize)
  : mEntries(MakeUnique<ProfileEntry[]>(aEntrySize))
  , mWritePos(0)
  , mReadPos(0)
  , mEntrySize(aEntrySize)
  , mGeneration(0)
{
}

ProfileBuffer::~ProfileBuffer()
{
  while (mStoredMarkers.peek()) {
    delete mStoredMarkers.popHead();
  }
}

// Called from signal, call only reentrant functions
void ProfileBuffer::addTag(const ProfileEntry& aTag)
{
  mEntries[mWritePos++] = aTag;
  if (mWritePos == mEntrySize) {
    // Wrapping around may result in things referenced in the buffer (e.g.,
    // JIT code addresses and markers) being incorrectly collected.
    MOZ_ASSERT(mGeneration != UINT32_MAX);
    mGeneration++;
    mWritePos = 0;
  }
  if (mWritePos == mReadPos) {
    // Keep one slot open.
    mEntries[mReadPos] = ProfileEntry();
    mReadPos = (mReadPos + 1) % mEntrySize;
  }
}

void ProfileBuffer::addStoredMarker(ProfilerMarker *aStoredMarker) {
  aStoredMarker->SetGeneration(mGeneration);
  mStoredMarkers.insert(aStoredMarker);
}

void ProfileBuffer::deleteExpiredStoredMarkers() {
  // Delete markers of samples that have been overwritten due to circular
  // buffer wraparound.
  uint32_t generation = mGeneration;
  while (mStoredMarkers.peek() &&
         mStoredMarkers.peek()->HasExpired(generation)) {
    delete mStoredMarkers.popHead();
  }
}

void ProfileBuffer::reset() {
  mGeneration += 2;
  mReadPos = mWritePos = 0;
}

#define DYNAMIC_MAX_STRING 512

char* ProfileBuffer::processDynamicTag(int readPos,
                                       int* tagsConsumed, char* tagBuff)
{
  int readAheadPos = (readPos + 1) % mEntrySize;
  int tagBuffPos = 0;

  // Read the string stored in mTagData until the null character is seen
  bool seenNullByte = false;
  while (readAheadPos != mWritePos && !seenNullByte) {
    (*tagsConsumed)++;
    ProfileEntry readAheadEntry = mEntries[readAheadPos];
    for (size_t pos = 0; pos < sizeof(void*); pos++) {
      tagBuff[tagBuffPos] = readAheadEntry.mTagChars[pos];
      if (tagBuff[tagBuffPos] == '\0' || tagBuffPos == DYNAMIC_MAX_STRING-2) {
        seenNullByte = true;
        break;
      }
      tagBuffPos++;
    }
    if (!seenNullByte)
      readAheadPos = (readAheadPos + 1) % mEntrySize;
  }
  return tagBuff;
}

void ProfileBuffer::IterateTagsForThread(IterateTagsCallback aCallback, int aThreadId)
{
  MOZ_ASSERT(aCallback);

  int readPos = mReadPos;
  int currentThreadID = -1;

  while (readPos != mWritePos) {
    const ProfileEntry& entry = mEntries[readPos];

    if (entry.mTagName == 'T') {
      currentThreadID = entry.mTagInt;
      readPos = (readPos + 1) % mEntrySize;
      continue;
    }

    // Number of tags consumed
    int incBy = 1;

    // Read ahead to the next tag, if it's a 'd' tag process it now
    const char* tagStringData = entry.mTagData;
    int readAheadPos = (readPos + 1) % mEntrySize;
    char tagBuff[DYNAMIC_MAX_STRING];
    // Make sure the string is always null terminated if it fills up DYNAMIC_MAX_STRING-2
    tagBuff[DYNAMIC_MAX_STRING-1] = '\0';

    if (readAheadPos != mWritePos && mEntries[readAheadPos].mTagName == 'd') {
      tagStringData = processDynamicTag(readPos, &incBy, tagBuff);
    }

    if (currentThreadID == aThreadId) {
      aCallback(entry, tagStringData);
    }

    readPos = (readPos + incBy) % mEntrySize;
  }
}

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

uint32_t UniqueJSONStrings::GetOrAddIndex(const char* aStr)
{
  uint32_t index;
  if (mStringToIndexMap.Get(aStr, &index)) {
    return index;
  }
  index = mStringToIndexMap.Count();
  mStringToIndexMap.Put(aStr, index);
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
  mStack.mPrefixHash = Some(prefixHash);
  mStack.mPrefix = Some(prefix);
  mStack.mFrame = mUniqueStacks.GetOrAddFrameIndex(aFrame);
}

uint32_t UniqueStacks::Stack::GetOrAddIndex() const
{
  return mUniqueStacks.GetOrAddStackIndex(mStack);
}

uint32_t UniqueStacks::FrameKey::Hash() const
{
  uint32_t hash = mozilla::HashString(mLocation.c_str(), mLocation.length());
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

UniqueStacks::~UniqueStacks()
{
  mFrameTableWriter.EndBareList();
  mStackTableWriter.EndBareList();
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
  if (mJITFrameDepthMap.Get(aAddr, &depth)) {
    MOZ_ASSERT(depth > 0);
    return depth;
  }
  return 0;
}

void UniqueStacks::AddJITFrameDepth(void* aAddr, unsigned depth)
{
  mJITFrameDepthMap.Put(aAddr, depth);
}

void UniqueStacks::SpliceFrameTableElements(SpliceableJSONWriter& aWriter) const
{
  aWriter.Splice(mFrameTableWriter.WriteFunc());
}

void UniqueStacks::SpliceStackTableElements(SpliceableJSONWriter& aWriter) const
{
  aWriter.Splice(mStackTableWriter.WriteFunc());
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
  if (!aFrame.mJITFrameHandle) {
    mUniqueStrings.WriteElement(mFrameTableWriter, aFrame.mLocation.c_str());
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
  } else {
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
  mFrameTableWriter.EndArray();
}

struct ProfileSample
{
  uint32_t mStack;
  Maybe<float> mTime;
  Maybe<float> mResponsiveness;
  Maybe<float> mRSS;
  Maybe<float> mUSS;
  Maybe<int> mFrameNumber;
  Maybe<float> mPower;
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
                                        float aSinceTime, JSRuntime* aRuntime,
                                        UniqueStacks& aUniqueStacks)
{
  Maybe<ProfileSample> sample;
  int readPos = mReadPos;
  int currentThreadID = -1;
  Maybe<float> currentTime;

  while (readPos != mWritePos) {
    ProfileEntry entry = mEntries[readPos];
    if (entry.mTagName == 'T') {
      currentThreadID = entry.mTagInt;
      currentTime.reset();
      int readAheadPos = (readPos + 1) % mEntrySize;
      if (readAheadPos != mWritePos) {
        ProfileEntry readAheadEntry = mEntries[readAheadPos];
        if (readAheadEntry.mTagName == 't') {
          currentTime = Some(readAheadEntry.mTagFloat);
        }
      }
    }
    if (currentThreadID == aThreadId && (currentTime.isNothing() || *currentTime >= aSinceTime)) {
      switch (entry.mTagName) {
      case 'r':
        if (sample.isSome()) {
          sample->mResponsiveness = Some(entry.mTagFloat);
        }
        break;
      case 'p':
        if (sample.isSome()) {
          sample->mPower = Some(entry.mTagFloat);
        }
        break;
      case 'R':
        if (sample.isSome()) {
          sample->mRSS = Some(entry.mTagFloat);
        }
        break;
      case 'U':
        if (sample.isSome()) {
          sample->mUSS = Some(entry.mTagFloat);
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
            char tagBuff[DYNAMIC_MAX_STRING];
            // Make sure the string is always null terminated if it fills up
            // DYNAMIC_MAX_STRING-2
            tagBuff[DYNAMIC_MAX_STRING-1] = '\0';

            if (readAheadPos != mWritePos && mEntries[readAheadPos].mTagName == 'd') {
              tagStringData = processDynamicTag(framePos, &incBy, tagBuff);
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
              snprintf(tagBuff, DYNAMIC_MAX_STRING, "%#llx", pc);
              stack.AppendFrame(UniqueStacks::OnStackFrameKey(tagBuff));
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
                                        float aSinceTime, UniqueStacks& aUniqueStacks)
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
        addTag(ProfileEntry('t', static_cast<float>((mozilla::TimeStamp::Now() - sStartTime).ToMilliseconds())));
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

ThreadProfile::ThreadProfile(ThreadInfo* aInfo, ProfileBuffer* aBuffer)
  : mThreadInfo(aInfo)
  , mBuffer(aBuffer)
  , mPseudoStack(aInfo->Stack())
  , mMutex("ThreadProfile::mMutex")
  , mThreadId(int(aInfo->ThreadId()))
  , mIsMainThread(aInfo->IsMainThread())
  , mPlatformData(aInfo->GetPlatformData())
  , mStackTop(aInfo->StackTop())
  , mRespInfo(this)
#ifdef XP_LINUX
  , mRssMemory(0)
  , mUssMemory(0)
#endif
{
  MOZ_COUNT_CTOR(ThreadProfile);
  MOZ_ASSERT(aBuffer);

  // I don't know if we can assert this. But we should warn.
  MOZ_ASSERT(aInfo->ThreadId() >= 0, "native thread ID is < 0");
  MOZ_ASSERT(aInfo->ThreadId() <= INT32_MAX, "native thread ID is > INT32_MAX");
}

ThreadProfile::~ThreadProfile()
{
  MOZ_COUNT_DTOR(ThreadProfile);
}

void ThreadProfile::addTag(const ProfileEntry& aTag)
{
  mBuffer->addTag(aTag);
}

void ThreadProfile::addStoredMarker(ProfilerMarker *aStoredMarker) {
  mBuffer->addStoredMarker(aStoredMarker);
}

void ThreadProfile::IterateTags(IterateTagsCallback aCallback)
{
  mBuffer->IterateTagsForThread(aCallback, mThreadId);
}

void ThreadProfile::ToStreamAsJSON(std::ostream& stream, float aSinceTime)
{
  SpliceableJSONWriter b(MakeUnique<OStreamJSONWriteFunc>(stream));
  StreamJSON(b, aSinceTime);
}

void ThreadProfile::StreamJSON(SpliceableJSONWriter& aWriter, float aSinceTime)
{
  // mUniqueStacks may already be emplaced from FlushSamplesAndMarkers.
  if (!mUniqueStacks.isSome()) {
    mUniqueStacks.emplace(mPseudoStack->mRuntime);
  }

  aWriter.Start(SpliceableJSONWriter::SingleLineStyle);
  {
    StreamSamplesAndMarkers(aWriter, aSinceTime, *mUniqueStacks);

    aWriter.StartObjectProperty("stackTable");
    {
      {
        JSONSchemaWriter schema(aWriter);
        schema.WriteField("prefix");
        schema.WriteField("frame");
      }

      aWriter.StartArrayProperty("data");
      {
        mUniqueStacks->SpliceStackTableElements(aWriter);
      }
      aWriter.EndArray();
    }
    aWriter.EndObject();

    aWriter.StartObjectProperty("frameTable");
    {
      {
        JSONSchemaWriter schema(aWriter);
        schema.WriteField("location");
        schema.WriteField("implementation");
        schema.WriteField("optimizations");
        schema.WriteField("line");
        schema.WriteField("category");
      }

      aWriter.StartArrayProperty("data");
      {
        mUniqueStacks->SpliceFrameTableElements(aWriter);
      }
      aWriter.EndArray();
    }
    aWriter.EndObject();

    aWriter.StartArrayProperty("stringTable");
    {
      mUniqueStacks->mUniqueStrings.SpliceStringTableElements(aWriter);
    }
    aWriter.EndArray();
  }
  aWriter.End();

  mUniqueStacks.reset();
}

void ThreadProfile::StreamSamplesAndMarkers(SpliceableJSONWriter& aWriter, float aSinceTime,
                                            UniqueStacks& aUniqueStacks)
{
  // Thread meta data
  if (XRE_GetProcessType() == GeckoProcessType_Plugin) {
    // TODO Add the proper plugin name
    aWriter.StringProperty("name", "Plugin");
  } else if (XRE_GetProcessType() == GeckoProcessType_Content) {
    // This isn't going to really help once we have multiple content
    // processes, but it'll do for now.
    aWriter.StringProperty("name", "Content");
  } else {
    aWriter.StringProperty("name", Name());
  }
  aWriter.IntProperty("tid", static_cast<int>(mThreadId));

  aWriter.StartObjectProperty("samples");
  {
    {
      JSONSchemaWriter schema(aWriter);
      schema.WriteField("stack");
      schema.WriteField("time");
      schema.WriteField("responsiveness");
      schema.WriteField("rss");
      schema.WriteField("uss");
      schema.WriteField("frameNumber");
      schema.WriteField("power");
    }

    aWriter.StartArrayProperty("data");
    {
      if (mSavedStreamedSamples) {
        // We would only have saved streamed samples during shutdown
        // streaming, which cares about dumping the entire buffer, and thus
        // should have passed in 0 for aSinceTime.
        MOZ_ASSERT(aSinceTime == 0);
        aWriter.Splice(mSavedStreamedSamples.get());
        mSavedStreamedSamples.reset();
      }
      mBuffer->StreamSamplesToJSON(aWriter, mThreadId, aSinceTime,
                                   mPseudoStack->mRuntime, aUniqueStacks);
    }
    aWriter.EndArray();
  }
  aWriter.EndObject();

  aWriter.StartObjectProperty("markers");
  {
    {
      JSONSchemaWriter schema(aWriter);
      schema.WriteField("name");
      schema.WriteField("time");
      schema.WriteField("data");
    }

    aWriter.StartArrayProperty("data");
    {
      if (mSavedStreamedMarkers) {
        MOZ_ASSERT(aSinceTime == 0);
        aWriter.Splice(mSavedStreamedMarkers.get());
        mSavedStreamedMarkers.reset();
      }
      mBuffer->StreamMarkersToJSON(aWriter, mThreadId, aSinceTime, aUniqueStacks);
    }
    aWriter.EndArray();
  }
  aWriter.EndObject();
}

void ThreadProfile::FlushSamplesAndMarkers()
{
  // This function is used to serialize the current buffer just before
  // JSRuntime destruction.
  MOZ_ASSERT(mPseudoStack->mRuntime);

  // Unlike StreamJSObject, do not surround the samples in brackets by calling
  // aWriter.{Start,End}BareList. The result string will be a comma-separated
  // list of JSON object literals that will prepended by StreamJSObject into
  // an existing array.
  //
  // Note that the UniqueStacks instance is persisted so that the frame-index
  // mapping is stable across JS shutdown.
  mUniqueStacks.emplace(mPseudoStack->mRuntime);

  {
    SpliceableChunkedJSONWriter b;
    b.StartBareList();
    {
      mBuffer->StreamSamplesToJSON(b, mThreadId, /* aSinceTime = */ 0,
                                   mPseudoStack->mRuntime, *mUniqueStacks);
    }
    b.EndBareList();
    mSavedStreamedSamples = b.WriteFunc()->CopyData();
  }

  {
    SpliceableChunkedJSONWriter b;
    b.StartBareList();
    {
      mBuffer->StreamMarkersToJSON(b, mThreadId, /* aSinceTime = */ 0, *mUniqueStacks);
    }
    b.EndBareList();
    mSavedStreamedMarkers = b.WriteFunc()->CopyData();
  }

  // Reset the buffer. Attempting to symbolicate JS samples after mRuntime has
  // gone away will crash.
  mBuffer->reset();
}

JSObject* ThreadProfile::ToJSObject(JSContext* aCx, float aSinceTime)
{
  JS::RootedValue val(aCx);
  {
    SpliceableChunkedJSONWriter b;
    StreamJSON(b, aSinceTime);
    UniquePtr<char[]> buf = b.WriteFunc()->CopyData();
    NS_ConvertUTF8toUTF16 js_string(nsDependentCString(buf.get()));
    MOZ_ALWAYS_TRUE(JS_ParseJSON(aCx, static_cast<const char16_t*>(js_string.get()),
                                 js_string.Length(), &val));
  }
  return &val.toObject();
}

PseudoStack* ThreadProfile::GetPseudoStack()
{
  return mPseudoStack;
}

void ThreadProfile::BeginUnwind()
{
  mMutex.Lock();
}

void ThreadProfile::EndUnwind()
{
  mMutex.Unlock();
}

mozilla::Mutex* ThreadProfile::GetMutex()
{
  return &mMutex;
}

void ThreadProfile::DuplicateLastSample()
{
  mBuffer->DuplicateLastSample(mThreadId);
}

// END ThreadProfile
////////////////////////////////////////////////////////////////////////
