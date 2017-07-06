/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfileBuffer.h"

#include "ProfilerMarker.h"

ProfileBuffer::ProfileBuffer(int aEntrySize)
  : mEntries(mozilla::MakeUnique<ProfileBufferEntry[]>(aEntrySize))
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
void ProfileBuffer::addEntry(const ProfileBufferEntry& aEntry)
{
  mEntries[mWritePos++] = aEntry;
  if (mWritePos == mEntrySize) {
    // Wrapping around may result in things referenced in the buffer (e.g.,
    // JIT code addresses and markers) being incorrectly collected.
    MOZ_ASSERT(mGeneration != UINT32_MAX);
    mGeneration++;
    mWritePos = 0;
  }
  if (mWritePos == mReadPos) {
    // Keep one slot open.
    mEntries[mReadPos] = ProfileBufferEntry();
    mReadPos = (mReadPos + 1) % mEntrySize;
  }
}

void ProfileBuffer::addThreadIdEntry(int aThreadId, LastSample* aLS)
{
  if (aLS) {
    // This is the start of a sample, so make a note of its location in |aLS|.
    aLS->mGeneration = mGeneration;
    aLS->mPos = mWritePos;
  }
  addEntry(ProfileBufferEntry::ThreadId(aThreadId));
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

size_t
ProfileBuffer::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += aMallocSizeOf(mEntries.get());

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - memory pointed to by the elements within mEntries
  // - mStoredMarkers

  return n;
}

#define DYNAMIC_MAX_STRING 8192

char*
ProfileBuffer::processEmbeddedString(int aReadAheadPos, int* aEntriesConsumed,
                                     char* aStrBuf)
{
  int strBufPos = 0;

  // Read the string stored in mChars until the null character is seen.
  bool seenNullByte = false;
  while (aReadAheadPos != mWritePos && !seenNullByte) {
    (*aEntriesConsumed)++;
    ProfileBufferEntry readAheadEntry = mEntries[aReadAheadPos];
    for (size_t pos = 0; pos < ProfileBufferEntry::kNumChars; pos++) {
      aStrBuf[strBufPos] = readAheadEntry.u.mChars[pos];
      if (aStrBuf[strBufPos] == '\0' || strBufPos == DYNAMIC_MAX_STRING-2) {
        seenNullByte = true;
        break;
      }
      strBufPos++;
    }
    if (!seenNullByte) {
      aReadAheadPos = (aReadAheadPos + 1) % mEntrySize;
    }
  }
  return aStrBuf;
}


