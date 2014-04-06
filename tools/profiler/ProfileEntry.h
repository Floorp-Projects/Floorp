/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_PROFILE_ENTRY_H
#define MOZ_PROFILE_ENTRY_H

#include <ostream>
#include "GeckoProfiler.h"
#include "platform.h"
#include "ProfilerBacktrace.h"
#include "mozilla/Mutex.h"

class ThreadProfile;

#pragma pack(push, 1)

class ProfileEntry
{
public:
  ProfileEntry();

  // aTagData must not need release (i.e. be a string from the text segment)
  ProfileEntry(char aTagName, const char *aTagData);
  ProfileEntry(char aTagName, void *aTagPtr);
  ProfileEntry(char aTagName, ProfilerMarker *aTagMarker);
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
  const ProfilerMarker* getMarker() {
    MOZ_ASSERT(mTagName == 'm');
    return mTagMarker;
  }

  char getTagName() const { return mTagName; }

private:
  friend class ThreadProfile;
  union {
    const char* mTagData;
    char        mTagChars[sizeof(void*)];
    void*       mTagPtr;
    ProfilerMarker* mTagMarker;
    double      mTagFloat;
    Address     mTagAddress;
    uintptr_t   mTagOffset;
    int         mTagLine;
    char        mTagChar;
  };
  char mTagName;
};

#pragma pack(pop)

typedef void (*IterateTagsCallback)(const ProfileEntry& entry, const char* tagStringData);

class ThreadProfile
{
public:
  ThreadProfile(const char* aName, int aEntrySize, PseudoStack *aStack,
                Thread::tid_t aThreadId, PlatformData* aPlatformData,
                bool aIsMainThread, void *aStackTop);
  virtual ~ThreadProfile();
  void addTag(ProfileEntry aTag);
  void flush();
  void erase();
  char* processDynamicTag(int readPos, int* tagsConsumed, char* tagBuff);
  void IterateTags(IterateTagsCallback aCallback);
  friend std::ostream& operator<<(std::ostream& stream,
                                  const ThreadProfile& profile);
  void ToStreamAsJSON(std::ostream& stream);
  JSObject *ToJSObject(JSContext *aCx);
  PseudoStack* GetPseudoStack();
  mozilla::Mutex* GetMutex();
  template <typename Builder> void BuildJSObject(Builder& b, typename Builder::ObjectHandle profile);
  void BeginUnwind();
  virtual void EndUnwind();
  virtual SyncProfile* AsSyncProfile() { return nullptr; }

  bool IsMainThread() const { return mIsMainThread; }
  const char* Name() const { return mName; }
  Thread::tid_t ThreadId() const { return mThreadId; }

  PlatformData* GetPlatformData() { return mPlatformData; }
  int GetGenerationID() const { return mGeneration; }
  bool HasGenerationExpired(int aGenID) {
    return aGenID + 2 <= mGeneration;
  }
  void* GetStackTop() const { return mStackTop; }
  void DuplicateLastSample();
private:
  // Circular buffer 'Keep One Slot Open' implementation
  // for simplicity
  ProfileEntry*  mEntries;
  int            mWritePos; // points to the next entry we will write to
  int            mLastFlushPos; // points to the next entry since the last flush()
  int            mReadPos;  // points to the next entry we will read to
  int            mEntrySize;
  PseudoStack*   mPseudoStack;
  mozilla::Mutex mMutex;
  char*          mName;
  Thread::tid_t  mThreadId;
  bool           mIsMainThread;
  PlatformData*  mPlatformData;  // Platform specific data.
  int            mGeneration;
  int            mPendingGenerationFlush;
  void* const    mStackTop;
};

std::ostream& operator<<(std::ostream& stream, const ThreadProfile& profile);

#endif /* ndef MOZ_PROFILE_ENTRY_H */
