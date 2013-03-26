/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_PROFILE_ENTRY_H
#define MOZ_PROFILE_ENTRY_H

#include "GeckoProfilerImpl.h"
#include "mozilla/Mutex.h"

class ThreadProfile2;

class ProfileEntry2
{
public:
  ProfileEntry2();

  // aTagData must not need release (i.e. be a string from the text segment)
  ProfileEntry2(char aTagName, const char *aTagData);
  ProfileEntry2(char aTagName, void *aTagPtr);
  ProfileEntry2(char aTagName, double aTagFloat);
  ProfileEntry2(char aTagName, uintptr_t aTagOffset);
  ProfileEntry2(char aTagName, Address aTagAddress);
  ProfileEntry2(char aTagName, int aTagLine);
  ProfileEntry2(char aTagName, char aTagChar);
  friend std::ostream& operator<<(std::ostream& stream, const ProfileEntry2& entry);
  bool is_ent_hint(char hintChar);
  bool is_ent_hint();
  bool is_ent(char tagName);
  void* get_tagPtr();
  void log();

private:
  friend class ThreadProfile2;
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


class ThreadProfile2
{
public:
  ThreadProfile2(int aEntrySize, PseudoStack *aStack);
  ~ThreadProfile2();
  void addTag(ProfileEntry2 aTag);
  void flush();
  void erase();
  char* processDynamicTag(int readPos, int* tagsConsumed, char* tagBuff);
  friend std::ostream& operator<<(std::ostream& stream,
                                  const ThreadProfile2& profile);
  JSCustomObject *ToJSObject(JSContext *aCx);
  PseudoStack* GetPseudoStack();
  mozilla::Mutex* GetMutex();
private:
  // Circular buffer 'Keep One Slot Open' implementation
  // for simplicity
  ProfileEntry2* mEntries;
  int            mWritePos; // points to the next entry we will write to
  int            mLastFlushPos; // points to the next entry since the last flush()
  int            mReadPos;  // points to the next entry we will read to
  int            mEntrySize;
  PseudoStack*   mPseudoStack;
  mozilla::Mutex mMutex;
};

#endif /* ndef MOZ_PROFILE_ENTRY_H */
