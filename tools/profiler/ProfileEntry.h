/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_PROFILE_ENTRY_H
#define MOZ_PROFILE_ENTRY_H

#include <ostream>
#include "GeckoProfilerImpl.h"
#include "JSAObjectBuilder.h"
#include "platform.h"
#include "mozilla/Mutex.h"

class ThreadProfile;

class ProfileEntry
{
public:
  ProfileEntry();

  // aTagData must not need release (i.e. be a string from the text segment)
  ProfileEntry(char aTagName, const char *aTagData);
  ProfileEntry(char aTagName, void *aTagPtr);
  ProfileEntry(char aTagName, double aTagFloat);
  ProfileEntry(char aTagName, uintptr_t aTagOffset);
  ProfileEntry(char aTagName, Address aTagAddress);
  ProfileEntry(char aTagName, int aTagLine);
  ProfileEntry(char aTagName, char aTagChar);
  friend std::ostream& operator<<(std::ostream& stream, const ProfileEntry& entry);
  bool is_ent_hint(char hintChar);
  bool is_ent_hint();
  bool is_ent(char tagName);
  void* get_tagPtr();
  void log();

  char getTagName() const { return mTagName; }

private:
  friend class ThreadProfile;
  union {
    const char* mTagData;
    char        mTagChars[sizeof(void*)];
    void*       mTagPtr;
    double      mTagFloat;
    Address     mTagAddress;
    uintptr_t   mTagOffset;
    int         mTagLine;
    char        mTagChar;
  };
  char mTagName;
};

typedef void (*IterateTagsCallback)(const ProfileEntry& entry, const char* tagStringData);

class ThreadProfile
{
public:
  ThreadProfile(const char* aName, int aEntrySize, PseudoStack *aStack,
                int aThreadId, PlatformData* aPlatformData,
                bool aIsMainThread);
  ~ThreadProfile();
  void addTag(ProfileEntry aTag);
  void flush();
  void erase();
  char* processDynamicTag(int readPos, int* tagsConsumed, char* tagBuff);
  void IterateTags(IterateTagsCallback aCallback);
  friend std::ostream& operator<<(std::ostream& stream,
                                  const ThreadProfile& profile);
  void ToStreamAsJSON(std::ostream& stream);
  JSCustomObject *ToJSObject(JSContext *aCx);
  PseudoStack* GetPseudoStack();
  mozilla::Mutex* GetMutex();
  void BuildJSObject(JSAObjectBuilder& b, JSCustomObject* profile);

  bool IsMainThread() const { return mIsMainThread; }
  const char* Name() const { return mName; }
  int ThreadId() const { return mThreadId; }

  PlatformData* GetPlatformData() { return mPlatformData; }
private:
  // Circular buffer 'Keep One Slot Open' implementation
  // for simplicity
  ProfileEntry* mEntries;
  int            mWritePos; // points to the next entry we will write to
  int            mLastFlushPos; // points to the next entry since the last flush()
  int            mReadPos;  // points to the next entry we will read to
  int            mEntrySize;
  PseudoStack*   mPseudoStack;
  mozilla::Mutex mMutex;
  char*          mName;
  int            mThreadId;
  bool           mIsMainThread;
  PlatformData*  mPlatformData;  // Platform specific data.
};

std::ostream& operator<<(std::ostream& stream, const ThreadProfile& profile);

#endif /* ndef MOZ_PROFILE_ENTRY_H */
