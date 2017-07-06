/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_PROFILE_BUFFER_H
#define MOZ_PROFILE_BUFFER_H

#include "platform.h"
#include "ProfileBufferEntry.h"
#include "ProfilerMarker.h"
#include "ProfileJSONWriter.h"
#include "mozilla/RefPtr.h"
#include "mozilla/RefCounted.h"

class ProfileBuffer final
{
public:
  explicit ProfileBuffer(int aEntrySize);

  ~ProfileBuffer();

  // LastSample is used to record the buffer location of the most recent
  // sample for each thread. It is used for periodic samples stored in the
  // global ProfileBuffer, but *not* for synchronous samples.
  struct LastSample {
    LastSample()
      : mGeneration(0)
      , mPos(-1)
    {}

    // The profiler-buffer generation number at which the sample was created.
    uint32_t mGeneration;
    // And its position in the buffer, or -1 meaning "invalid".
    int mPos;
  };

  // Add |aEntry| to the buffer, ignoring what kind of entry it is.
  void addEntry(const ProfileBufferEntry& aEntry);

  // Add to the buffer a sample start (ThreadId) entry for aThreadId. Also,
  // record the resulting generation and index in |aLS| if it's non-null.
  void addThreadIdEntry(int aThreadId, LastSample* aLS = nullptr);

  void StreamSamplesToJSON(SpliceableJSONWriter& aWriter, int aThreadId,
                           double aSinceTime, JSContext* cx,
                           UniqueStacks& aUniqueStacks);
  void StreamMarkersToJSON(SpliceableJSONWriter& aWriter, int aThreadId,
                           const mozilla::TimeStamp& aProcessStartTime,
                           double aSinceTime,
                           UniqueStacks& aUniqueStacks);

  // Find (via |aLS|) the most recent sample for the thread denoted by
  // |aThreadId| and clone it, patching in |aProcessStartTime| as appropriate.
  bool DuplicateLastSample(int aThreadId,
                           const mozilla::TimeStamp& aProcessStartTime,
                           LastSample& aLS);

  void addStoredMarker(ProfilerMarker* aStoredMarker);

  // The following two methods are not signal safe! They delete markers.
  void deleteExpiredStoredMarkers();
  void reset();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

protected:
  char* processEmbeddedString(int aReadaheadPos, int* aEntriesConsumed,
                              char* aStrBuf);
  int FindLastSampleOfThread(int aThreadId, const LastSample& aLS);

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
