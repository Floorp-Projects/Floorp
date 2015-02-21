/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ostream>
#include <sstream>
#include "platform.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

// JS
#include "jsapi.h"
#include "js/TrackedOptimizationInfo.h"

// JSON
#include "JSStreamWriter.h"

// Self
#include "ProfileEntry.h"

#if defined(_MSC_VER) && _MSC_VER < 1900
 #define snprintf _snprintf
#endif

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

// Called from signal, call only reentrant functions
void ProfileBuffer::addTag(const ProfileEntry& aTag)
{
  mEntries[mWritePos++] = aTag;
  if (mWritePos == mEntrySize) {
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
  int generation = mGeneration;
  while (mStoredMarkers.peek() &&
         mStoredMarkers.peek()->HasExpired(generation)) {
    delete mStoredMarkers.popHead();
  }
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

class StreamOptimizationTypeInfoOp : public JS::ForEachTrackedOptimizationTypeInfoOp
{
  JSStreamWriter& mWriter;
  bool mStartedTypeList;

public:
  explicit StreamOptimizationTypeInfoOp(JSStreamWriter& b)
    : mWriter(b)
    , mStartedTypeList(false)
  { }

  void readType(const char *keyedBy, const char *name,
                const char *location, unsigned lineno) MOZ_OVERRIDE {
    if (!mStartedTypeList) {
      mStartedTypeList = true;
      mWriter.BeginObject();
        mWriter.Name("types");
        mWriter.BeginArray();
    }

    mWriter.BeginObject();
      mWriter.NameValue("keyedBy", keyedBy);
      mWriter.NameValue("name", name);
      if (location) {
        mWriter.NameValue("location", location);
        mWriter.NameValue("line", lineno);
      }
    mWriter.EndObject();
  }

  void operator()(JS::TrackedTypeSite site, const char *mirType) MOZ_OVERRIDE {
    if (mStartedTypeList) {
      mWriter.EndArray();
      mStartedTypeList = false;
    } else {
      mWriter.BeginObject();
    }

      mWriter.NameValue("site", JS::TrackedTypeSiteString(site));
      mWriter.NameValue("mirType", mirType);
    mWriter.EndObject();
  }
};

class StreamOptimizationAttemptsOp : public JS::ForEachTrackedOptimizationAttemptOp
{
  JSStreamWriter& mWriter;

public:
  explicit StreamOptimizationAttemptsOp(JSStreamWriter& b)
    : mWriter(b)
  { }

  void operator()(JS::TrackedStrategy strategy, JS::TrackedOutcome outcome) MOZ_OVERRIDE {
    mWriter.BeginObject();
    {
      // Stringify the reasons for now; could stream enum values in the future
      // to save space.
      mWriter.NameValue("strategy", JS::TrackedStrategyString(strategy));
      mWriter.NameValue("outcome", JS::TrackedOutcomeString(outcome));
    }
    mWriter.EndObject();
  }
};

void ProfileBuffer::StreamSamplesToJSObject(JSStreamWriter& b, int aThreadId, JSRuntime* rt)
{
  b.BeginArray();

    bool sample = false;
    int readPos = mReadPos;
    int currentThreadID = -1;
    while (readPos != mWritePos) {
      ProfileEntry entry = mEntries[readPos];
      if (entry.mTagName == 'T') {
        currentThreadID = entry.mTagInt;
      }
      if (currentThreadID == aThreadId) {
        switch (entry.mTagName) {
          case 'r':
            {
              if (sample) {
                b.NameValue("responsiveness", entry.mTagFloat);
              }
            }
            break;
          case 'p':
            {
              if (sample) {
                b.NameValue("power", entry.mTagFloat);
              }
            }
            break;
          case 'R':
            {
              if (sample) {
                b.NameValue("rss", entry.mTagFloat);
              }
            }
            break;
          case 'U':
            {
              if (sample) {
                b.NameValue("uss", entry.mTagFloat);
              }
            }
            break;
          case 'f':
            {
              if (sample) {
                b.NameValue("frameNumber", entry.mTagInt);
              }
            }
            break;
          case 't':
            {
              if (sample) {
                b.NameValue("time", entry.mTagFloat);
              }
            }
            break;
          case 's':
            {
              // end the previous sample if there was one
              if (sample) {
                b.EndObject();
              }
              // begin the next sample
              b.BeginObject();

              sample = true;

              // Seek forward through the entire sample, looking for frames
              // this is an easier approach to reason about than adding more
              // control variables and cases to the loop that goes through the buffer once
              b.Name("frames");
              b.BeginArray();

                b.BeginObject();
                  b.NameValue("location", "(root)");
                b.EndObject();

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
                  if (frame.mTagName == 'l') {
                    b.BeginObject();
                      // Bug 753041
                      // We need a double cast here to tell GCC that we don't want to sign
                      // extend 32-bit addresses starting with 0xFXXXXXX.
                      unsigned long long pc = (unsigned long long)(uintptr_t)frame.mTagPtr;
                      snprintf(tagBuff, DYNAMIC_MAX_STRING, "%#llx", pc);
                      b.NameValue("location", tagBuff);
                    b.EndObject();
                  } else if (frame.mTagName == 'c') {
                    b.BeginObject();
                      b.NameValue("location", tagStringData);
                      readAheadPos = (framePos + incBy) % mEntrySize;
                      if (readAheadPos != mWritePos &&
                          mEntries[readAheadPos].mTagName == 'n') {
                        b.NameValue("line", mEntries[readAheadPos].mTagInt);
                        incBy++;
                      }
                      readAheadPos = (framePos + incBy) % mEntrySize;
                      if (readAheadPos != mWritePos &&
                          mEntries[readAheadPos].mTagName == 'y') {
                        b.NameValue("category", mEntries[readAheadPos].mTagInt);
                        incBy++;
                      }
                      readAheadPos = (framePos + incBy) % mEntrySize;
                      if (readAheadPos != mWritePos &&
                          mEntries[readAheadPos].mTagName == 'J') {
                        void* pc = mEntries[readAheadPos].mTagPtr;

                        // TODOshu: cannot stream tracked optimization info if
                        // the JS engine has already shut down when streaming.
                        if (rt) {
                          b.Name("opts");
                          b.BeginArray();
                            StreamOptimizationTypeInfoOp typeInfoOp(b);
                            JS::ForEachTrackedOptimizationTypeInfo(rt, pc, typeInfoOp);
                            StreamOptimizationAttemptsOp attemptOp(b);
                            JS::ForEachTrackedOptimizationAttempt(rt, pc, attemptOp);
                          b.EndArray();
                        }
                      }
                    b.EndObject();
                  }
                  framePos = (framePos + incBy) % mEntrySize;
                }
              b.EndArray();
            }
            break;
        }
      }
      readPos = (readPos + 1) % mEntrySize;
    }
    if (sample) {
      b.EndObject();
    }
  b.EndArray();
}

void ProfileBuffer::StreamMarkersToJSObject(JSStreamWriter& b, int aThreadId)
{
  b.BeginArray();
    int readPos = mReadPos;
    int currentThreadID = -1;
    while (readPos != mWritePos) {
      ProfileEntry entry = mEntries[readPos];
      if (entry.mTagName == 'T') {
        currentThreadID = entry.mTagInt;
      } else if (currentThreadID == aThreadId && entry.mTagName == 'm') {
        entry.getMarker()->StreamJSObject(b);
      }
      readPos = (readPos + 1) % mEntrySize;
    }
  b.EndArray();
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

void ThreadProfile::ToStreamAsJSON(std::ostream& stream)
{
  JSStreamWriter b(stream);
  StreamJSObject(b);
}

void ThreadProfile::StreamJSObject(JSStreamWriter& b)
{
  b.BeginObject();
    // Thread meta data
    if (XRE_GetProcessType() == GeckoProcessType_Plugin) {
      // TODO Add the proper plugin name
      b.NameValue("name", "Plugin");
    } else if (XRE_GetProcessType() == GeckoProcessType_Content) {
      // This isn't going to really help once we have multiple content
      // processes, but it'll do for now.
      b.NameValue("name", "Content");
    } else {
      b.NameValue("name", Name());
    }
    b.NameValue("tid", static_cast<int>(mThreadId));

    b.Name("samples");
    mBuffer->StreamSamplesToJSObject(b, mThreadId, mPseudoStack->mRuntime);

    b.Name("markers");
    mBuffer->StreamMarkersToJSObject(b, mThreadId);
  b.EndObject();
}

JSObject* ThreadProfile::ToJSObject(JSContext *aCx)
{
  JS::RootedValue val(aCx);
  std::stringstream ss;
  {
    // Define a scope to prevent a moving GC during ~JSStreamWriter from
    // trashing the return value.
    JSStreamWriter b(ss);
    StreamJSObject(b);
    NS_ConvertUTF8toUTF16 js_string(nsDependentCString(ss.str().c_str()));
    JS_ParseJSON(aCx, static_cast<const char16_t*>(js_string.get()),
                 js_string.Length(), &val);
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
