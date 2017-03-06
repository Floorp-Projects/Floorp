/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_PROFILE_BUFFER_H
#define MOZ_PROFILE_BUFFER_H

#include "ProfileBufferEntry.h"
#include "platform.h"
#include "ProfileJSONWriter.h"
#include "mozilla/RefPtr.h"
#include "mozilla/RefCounted.h"

class ProfileBuffer final
{
public:
  explicit ProfileBuffer(int aEntrySize);

  ~ProfileBuffer();

  void addTag(const ProfileBufferEntry& aTag);
  void StreamSamplesToJSON(SpliceableJSONWriter& aWriter, int aThreadId, double aSinceTime,
                           JSContext* cx, UniqueStacks& aUniqueStacks);
  void StreamMarkersToJSON(SpliceableJSONWriter& aWriter, int aThreadId,
                           const mozilla::TimeStamp& aStartTime,
                           double aSinceTime,
                           UniqueStacks& aUniqueStacks);
  bool DuplicateLastSample(int aThreadId, const mozilla::TimeStamp& aStartTime);

  void addStoredMarker(ProfilerMarker* aStoredMarker);

  // The following two methods are not signal safe! They delete markers.
  void deleteExpiredStoredMarkers();
  void reset();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

protected:
  char* processDynamicTag(int readPos, int* tagsConsumed, char* tagBuff);
  int FindLastSampleOfThread(int aThreadId);

public:
  // Circular buffer 'Keep One Slot Open' implementation for simplicity
  mozilla::UniquePtr<ProfileBufferEntry[]> mEntries;

  // Points to the next entry we will write to, which is also the one at which
  // we need to stop reading.
  int mWritePos;

  // Points to the entry at which we can start reading.
  int mReadPos;

  // The number of entries in our buffer.
  int mEntrySize;

  // How many times mWritePos has wrapped around.
  uint32_t mGeneration;

  // Markers that marker entries in the buffer might refer to.
  ProfilerMarkerLinkedList mStoredMarkers;
};

#endif
