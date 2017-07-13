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
void
ProfileBuffer::AddEntry(const ProfileBufferEntry& aEntry)
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

void
ProfileBuffer::AddThreadIdEntry(int aThreadId, LastSample* aLS)
{
  if (aLS) {
    // This is the start of a sample, so make a note of its location in |aLS|.
    aLS->mGeneration = mGeneration;
    aLS->mPos = mWritePos;
  }
  AddEntry(ProfileBufferEntry::ThreadId(aThreadId));
}

void
ProfileBuffer::AddDynamicStringEntry(const char* aStr)
{
  size_t strLen = strlen(aStr) + 1;   // +1 for the null terminator
  for (size_t j = 0; j < strLen; ) {
    // Store up to kNumChars characters in the entry.
    char chars[ProfileBufferEntry::kNumChars];
    size_t len = ProfileBufferEntry::kNumChars;
    if (j + len >= strLen) {
      len = strLen - j;
    }
    memcpy(chars, &aStr[j], len);
    j += ProfileBufferEntry::kNumChars;

    AddEntry(ProfileBufferEntry::DynamicStringFragment(chars));
  }
}

void
ProfileBuffer::AddStoredMarker(ProfilerMarker *aStoredMarker)
{
  aStoredMarker->SetGeneration(mGeneration);
  mStoredMarkers.insert(aStoredMarker);
}

void
ProfileBuffer::DeleteExpiredStoredMarkers()
{
  // Delete markers of samples that have been overwritten due to circular
  // buffer wraparound.
  uint32_t generation = mGeneration;
  while (mStoredMarkers.peek() &&
         mStoredMarkers.peek()->HasExpired(generation)) {
    delete mStoredMarkers.popHead();
  }
}

void
ProfileBuffer::Reset()
{
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

