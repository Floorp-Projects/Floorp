/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfileBuffer.h"

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

#define DYNAMIC_MAX_STRING 8192

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


