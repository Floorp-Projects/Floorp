/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ostream>
#include "GeckoProfilerImpl.h"
#include "platform.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

// JSON
#include "JSObjectBuilder.h"
#include "JSCustomObjectBuilder.h"

// Self
#include "ProfileEntry.h"

#if _MSC_VER
 #define snprintf _snprintf
#endif

////////////////////////////////////////////////////////////////////////
// BEGIN ProfileEntry

ProfileEntry::ProfileEntry()
  : mTagData(NULL)
  , mTagName(0)
{ }

// aTagData must not need release (i.e. be a string from the text segment)
ProfileEntry::ProfileEntry(char aTagName, const char *aTagData)
  : mTagData(aTagData)
  , mTagName(aTagName)
{ }

ProfileEntry::ProfileEntry(char aTagName, void *aTagPtr)
  : mTagPtr(aTagPtr)
  , mTagName(aTagName)
{ }

ProfileEntry::ProfileEntry(char aTagName, double aTagFloat)
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

ProfileEntry::ProfileEntry(char aTagName, int aTagLine)
  : mTagLine(aTagLine)
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
  //   mTagData   (const char*)  m,c,s
  //   mTagPtr    (void*)        d,l,L, S(start-of-stack)
  //   mTagLine   (int)          n,f
  //   mTagChar   (char)         h
  //   mTagFloat  (double)       r,t
  switch (mTagName) {
    case 'm': case 'c': case 's':
      LOGF("%c \"%s\"", mTagName, mTagData); break;
    case 'd': case 'l': case 'L': case 'S':
      LOGF("%c %p", mTagName, mTagPtr); break;
    case 'n': case 'f':
      LOGF("%c %d", mTagName, mTagLine); break;
    case 'h':
      LOGF("%c \'%c\'", mTagName, mTagChar); break;
    case 'r': case 't':
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
                             PseudoStack *aStack, int aThreadId,
                             PlatformData* aPlatform,
                             bool aIsMainThread)
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
  mWritePos = (mWritePos + 1) % mEntrySize;
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
  JSCustomObjectBuilder b;
  JSCustomObject *profile = b.CreateObject();
  BuildJSObject(b, profile);
  b.Serialize(profile, stream);
  b.DeleteObject(profile);
}

JSCustomObject* ThreadProfile::ToJSObject(JSContext *aCx)
{
  JSObjectBuilder b(aCx);
  JSCustomObject *profile = b.CreateObject();
  BuildJSObject(b, profile);

  return profile;
}

void ThreadProfile::BuildJSObject(JSAObjectBuilder& b, JSCustomObject* profile) {

  // Thread meta data
  if (XRE_GetProcessType() == GeckoProcessType_Plugin) {
    // TODO Add the proper plugin name
    b.DefineProperty(profile, "name", "Plugin");
  } else {
    b.DefineProperty(profile, "name", mName);
  }

  b.DefineProperty(profile, "tid", mThreadId);

  JSCustomArray *samples = b.CreateArray();
  b.DefineProperty(profile, "samples", samples);

  JSCustomObject *sample = nullptr;
  JSCustomArray *frames = nullptr;
  JSCustomArray *marker = nullptr;

  int readPos = mReadPos;
  while (readPos != mLastFlushPos) {
    // Number of tag consumed
    int incBy = 1;
    ProfileEntry entry = mEntries[readPos];

    // Read ahead to the next tag, if it's a 'd' tag process it now
    const char* tagStringData = entry.mTagData;
    int readAheadPos = (readPos + 1) % mEntrySize;
    char tagBuff[DYNAMIC_MAX_STRING];
    // Make sure the string is always null terminated if it fills up
    // DYNAMIC_MAX_STRING-2
    tagBuff[DYNAMIC_MAX_STRING-1] = '\0';

    if (readAheadPos != mLastFlushPos && mEntries[readAheadPos].mTagName == 'd') {
      tagStringData = processDynamicTag(readPos, &incBy, tagBuff);
    }

    switch (entry.mTagName) {
      case 's':
        sample = b.CreateObject();
        b.DefineProperty(sample, "name", tagStringData);
        frames = b.CreateArray();
        b.DefineProperty(sample, "frames", frames);
        b.ArrayPush(samples, sample);
        // Created lazily
        marker = nullptr;
        break;
      case 'm':
        {
          if (sample) {
            if (!marker) {
              marker = b.CreateArray();
              b.DefineProperty(sample, "marker", marker);
            }
            b.ArrayPush(marker, tagStringData);
          }
        }
        break;
      case 'r':
        {
          if (sample) {
            b.DefineProperty(sample, "responsiveness", entry.mTagFloat);
          }
        }
        break;
      case 'f':
        {
          if (sample) {
            b.DefineProperty(sample, "frameNumber", entry.mTagLine);
          }
        }
        break;
      case 't':
        {
          if (sample) {
            b.DefineProperty(sample, "time", entry.mTagFloat);
          }
        }
        break;
      case 'c':
      case 'l':
        {
          if (sample) {
            JSCustomObject *frame = b.CreateObject();
            if (entry.mTagName == 'l') {
              // Bug 753041
              // We need a double cast here to tell GCC that we don't want to sign
              // extend 32-bit addresses starting with 0xFXXXXXX.
              unsigned long long pc = (unsigned long long)(uintptr_t)entry.mTagPtr;
              snprintf(tagBuff, DYNAMIC_MAX_STRING, "%#llx", pc);
              b.DefineProperty(frame, "location", tagBuff);
            } else {
              b.DefineProperty(frame, "location", tagStringData);
              readAheadPos = (readPos + incBy) % mEntrySize;
              if (readAheadPos != mLastFlushPos &&
                  mEntries[readAheadPos].mTagName == 'n') {
                b.DefineProperty(frame, "line",
                                 mEntries[readAheadPos].mTagLine);
                incBy++;
              }
            }
            b.ArrayPush(frames, frame);
          }
        }
    }
    readPos = (readPos + incBy) % mEntrySize;
  }
}

PseudoStack* ThreadProfile::GetPseudoStack()
{
  return mPseudoStack;
}

mozilla::Mutex* ThreadProfile::GetMutex()
{
  return &mMutex;
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
