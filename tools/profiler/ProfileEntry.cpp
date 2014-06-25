/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ostream>
#include <sstream>
#include "platform.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "jsapi.h"

// JSON
#include "JSStreamWriter.h"

// Self
#include "ProfileEntry.h"

#if _MSC_VER
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

void ProfileEntry::log()
{
  // There is no compiler enforced mapping between tag chars
  // and union variant fields, so the following was derived
  // by looking through all the use points of TableTicker.cpp.
  //   mTagMarker (ProfilerMarker*) m
  //   mTagData   (const char*)  c,s
  //   mTagPtr    (void*)        d,l,L,B (immediate backtrace), S(start-of-stack)
  //   mTagInt    (int)          n,f,y
  //   mTagChar   (char)         h
  //   mTagFloat  (double)       r,t,p,R (resident memory), U (unshared memory)
  switch (mTagName) {
    case 'm':
      LOGF("%c \"%s\"", mTagName, mTagMarker->GetMarkerName()); break;
    case 'c': case 's':
      LOGF("%c \"%s\"", mTagName, mTagData); break;
    case 'd': case 'l': case 'L': case 'B': case 'S':
      LOGF("%c %p", mTagName, mTagPtr); break;
    case 'n': case 'f': case 'y':
      LOGF("%c %d", mTagName, mTagInt); break;
    case 'h':
      LOGF("%c \'%c\'", mTagName, mTagChar); break;
    case 'r': case 't': case 'p': case 'R': case 'U':
      LOGF("%c %f", mTagName, mTagFloat); break;
    default:
      LOGF("'%c' unknown_tag", mTagName); break;
  }
}

std::ostream& operator<<(std::ostream& stream, const ProfileEntry& entry)
{
  if (entry.mTagName == 'r' || entry.mTagName == 't') {
    stream << entry.mTagName << "-" << std::fixed << entry.mTagFloat << "\n";
  } else if (entry.mTagName == 'l' || entry.mTagName == 'L') {
    // Bug 739800 - Force l-tag addresses to have a "0x" prefix on all platforms
    // Additionally, stringstream seemed to be ignoring formatter flags.
    char tagBuff[1024];
    unsigned long long pc = (unsigned long long)(uintptr_t)entry.mTagPtr;
    snprintf(tagBuff, 1024, "%c-%#llx\n", entry.mTagName, pc);
    stream << tagBuff;
  } else if (entry.mTagName == 'd') {
    // TODO implement 'd' tag for text profile
  } else {
    stream << entry.mTagName << "-" << entry.mTagData << "\n";
  }
  return stream;
}

// END ProfileEntry
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
// BEGIN ThreadProfile

#define DYNAMIC_MAX_STRING 512

ThreadProfile::ThreadProfile(const char* aName, int aEntrySize,
                             PseudoStack *aStack, Thread::tid_t aThreadId,
                             PlatformData* aPlatform,
                             bool aIsMainThread, void *aStackTop)
  : mWritePos(0)
  , mLastFlushPos(0)
  , mReadPos(0)
  , mEntrySize(aEntrySize)
  , mPseudoStack(aStack)
  , mMutex("ThreadProfile::mMutex")
  , mName(strdup(aName))
  , mThreadId(aThreadId)
  , mIsMainThread(aIsMainThread)
  , mPlatformData(aPlatform)
  , mGeneration(0)
  , mPendingGenerationFlush(0)
  , mStackTop(aStackTop)
#ifdef XP_LINUX
  , mRssMemory(0)
  , mUssMemory(0)
#endif
{
  mEntries = new ProfileEntry[mEntrySize];
}

ThreadProfile::~ThreadProfile()
{
  free(mName);
  delete[] mEntries;
}

void ThreadProfile::addTag(ProfileEntry aTag)
{
  // Called from signal, call only reentrant functions
  mEntries[mWritePos] = aTag;
  mWritePos = mWritePos + 1;
  if (mWritePos >= mEntrySize) {
    mPendingGenerationFlush++;
    mWritePos = mWritePos % mEntrySize;
  }
  if (mWritePos == mReadPos) {
    // Keep one slot open
    mEntries[mReadPos] = ProfileEntry();
    mReadPos = (mReadPos + 1) % mEntrySize;
  }
  // we also need to move the flush pos to ensure we
  // do not pass it
  if (mWritePos == mLastFlushPos) {
    mLastFlushPos = (mLastFlushPos + 1) % mEntrySize;
  }
}

// flush the new entries
void ThreadProfile::flush()
{
  mLastFlushPos = mWritePos;
  mGeneration += mPendingGenerationFlush;
  mPendingGenerationFlush = 0;
}

// discards all of the entries since the last flush()
// NOTE: that if mWritePos happens to wrap around past
// mLastFlushPos we actually only discard mWritePos - mLastFlushPos entries
//
// r = mReadPos
// w = mWritePos
// f = mLastFlushPos
//
//     r          f    w
// |-----------------------------|
// |   abcdefghijklmnopq         | -> 'abcdefghijklmnopq'
// |-----------------------------|
//
//
// mWritePos and mReadPos have passed mLastFlushPos
//                      f
//                    w r
// |-----------------------------|
// |ABCDEFGHIJKLMNOPQRSqrstuvwxyz|
// |-----------------------------|
//                       w
//                       r
// |-----------------------------|
// |ABCDEFGHIJKLMNOPQRSqrstuvwxyz| -> ''
// |-----------------------------|
//
//
// mWritePos will end up the same as mReadPos
//                r
//              w f
// |-----------------------------|
// |ABCDEFGHIJKLMklmnopqrstuvwxyz|
// |-----------------------------|
//                r
//                w
// |-----------------------------|
// |ABCDEFGHIJKLMklmnopqrstuvwxyz| -> ''
// |-----------------------------|
//
//
// mWritePos has moved past mReadPos
//      w r       f
// |-----------------------------|
// |ABCDEFdefghijklmnopqrstuvwxyz|
// |-----------------------------|
//        r       w
// |-----------------------------|
// |ABCDEFdefghijklmnopqrstuvwxyz| -> 'defghijkl'
// |-----------------------------|

void ThreadProfile::erase()
{
  mWritePos = mLastFlushPos;
  mPendingGenerationFlush = 0;
}

char* ThreadProfile::processDynamicTag(int readPos,
                                       int* tagsConsumed, char* tagBuff)
{
  int readAheadPos = (readPos + 1) % mEntrySize;
  int tagBuffPos = 0;

  // Read the string stored in mTagData until the null character is seen
  bool seenNullByte = false;
  while (readAheadPos != mLastFlushPos && !seenNullByte) {
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

void ThreadProfile::IterateTags(IterateTagsCallback aCallback)
{
  MOZ_ASSERT(aCallback);

  int readPos = mReadPos;
  while (readPos != mLastFlushPos) {
    // Number of tag consumed
    int incBy = 1;
    const ProfileEntry& entry = mEntries[readPos];

    // Read ahead to the next tag, if it's a 'd' tag process it now
    const char* tagStringData = entry.mTagData;
    int readAheadPos = (readPos + 1) % mEntrySize;
    char tagBuff[DYNAMIC_MAX_STRING];
    // Make sure the string is always null terminated if it fills up DYNAMIC_MAX_STRING-2
    tagBuff[DYNAMIC_MAX_STRING-1] = '\0';

    if (readAheadPos != mLastFlushPos && mEntries[readAheadPos].mTagName == 'd') {
      tagStringData = processDynamicTag(readPos, &incBy, tagBuff);
    }

    aCallback(entry, tagStringData);

    readPos = (readPos + incBy) % mEntrySize;
  }
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
    } else {
      b.NameValue("name", mName);
    }
    b.NameValue("tid", static_cast<int>(mThreadId));

    b.Name("samples");
    b.BeginArray();

      bool sample = false;
      int readPos = mReadPos;
      while (readPos != mLastFlushPos) {
        // Number of tag consumed
        ProfileEntry entry = mEntries[readPos];

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
                while (framePos != mLastFlushPos && frame.mTagName != 's') {
                  int incBy = 1;
                  frame = mEntries[framePos];

                  // Read ahead to the next tag, if it's a 'd' tag process it now
                  const char* tagStringData = frame.mTagData;
                  int readAheadPos = (framePos + 1) % mEntrySize;
                  char tagBuff[DYNAMIC_MAX_STRING];
                  // Make sure the string is always null terminated if it fills up
                  // DYNAMIC_MAX_STRING-2
                  tagBuff[DYNAMIC_MAX_STRING-1] = '\0';

                  if (readAheadPos != mLastFlushPos && mEntries[readAheadPos].mTagName == 'd') {
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
                      if (readAheadPos != mLastFlushPos &&
                          mEntries[readAheadPos].mTagName == 'n') {
                        b.NameValue("line", mEntries[readAheadPos].mTagInt);
                        incBy++;
                      }
                      readAheadPos = (framePos + incBy) % mEntrySize;
                      if (readAheadPos != mLastFlushPos &&
                          mEntries[readAheadPos].mTagName == 'y') {
                        b.NameValue("category", mEntries[readAheadPos].mTagInt);
                        incBy++;
                      }
                    b.EndObject();
                  }
                  framePos = (framePos + incBy) % mEntrySize;
                }
              b.EndArray();
            }
            break;
        }
        readPos = (readPos + 1) % mEntrySize;
      }
      if (sample) {
        b.EndObject();
      }
    b.EndArray();

    b.Name("markers");
    b.BeginArray();
      readPos = mReadPos;
      while (readPos != mLastFlushPos) {
        ProfileEntry entry = mEntries[readPos];
        if (entry.mTagName == 'm') {
           entry.getMarker()->StreamJSObject(b);
        }
        readPos = (readPos + 1) % mEntrySize;
      }
    b.EndArray();
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
    JS_ParseJSON(aCx, static_cast<const jschar*>(js_string.get()), js_string.Length(), &val);
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

void ThreadProfile::DuplicateLastSample() {
  // Scan the whole buffer (even unflushed parts)
  // Adding mEntrySize makes the result of the modulus positive
  // We search backwards from mWritePos-1 to mReadPos
  for (int readPos  = (mWritePos + mEntrySize - 1) % mEntrySize;
           readPos !=  (mReadPos + mEntrySize - 1) % mEntrySize;
           readPos  =   (readPos + mEntrySize - 1) % mEntrySize) {
    if (mEntries[readPos].mTagName == 's') {
      // Found the start of the last entry at position readPos
      int copyEndIdx = mWritePos;
      // Go through the whole entry and duplicate it
      for (;readPos != copyEndIdx; readPos = (readPos + 1) % mEntrySize) {
        switch (mEntries[readPos].mTagName) {
          // Copy with new time
          case 't':
            addTag(ProfileEntry('t', static_cast<float>((mozilla::TimeStamp::Now() - sStartTime).ToMilliseconds())));
            break;
          // Don't copy markers
          case 'm':
            break;
          // Copy anything else we don't know about
          // L, B, S, c, s, d, l, f, h, r, t, p
          default:
            addTag(mEntries[readPos]);
            break;
        }
      }
      break;
    }
  }
}

std::ostream& operator<<(std::ostream& stream, const ThreadProfile& profile)
{
  int readPos = profile.mReadPos;
  while (readPos != profile.mLastFlushPos) {
    stream << profile.mEntries[readPos];
    readPos = (readPos + 1) % profile.mEntrySize;
  }
  return stream;
}

// END ThreadProfile
////////////////////////////////////////////////////////////////////////
