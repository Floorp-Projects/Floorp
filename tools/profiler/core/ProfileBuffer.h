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

// A fixed-capacity circular buffer.
// This class is used as a queue of entries which, after construction, never
// allocates. This makes it safe to use in the profiler's "critical section".
// Entries are appended at the end. Once the queue capacity has been reached,
// adding a new entry will evict an old entry from the start of the queue.
// Positions in the queue are represented as 64-bit unsigned integers which
// only increase and never wrap around.
// mRangeStart and mRangeEnd describe the range in that uint64_t space which is
// covered by the queue contents.
// Internally, the buffer uses a fixed-size storage and applies a modulo
// operation when accessing entries in that storage buffer. "Evicting" an entry
// really just means that an existing entry in the storage buffer gets
// overwritten and that mRangeStart gets incremented.
class ProfileBuffer final
{
public:
  // ProfileBuffer constructor
  // @param aEntrySize The minimum capacity of the buffer. The actual buffer
  //                   capacity will be rounded up to the next power of two.
  explicit ProfileBuffer(uint32_t aEntrySize);

  ~ProfileBuffer();

  // Add |aEntry| to the buffer, ignoring what kind of entry it is.
  void AddEntry(const ProfileBufferEntry& aEntry);

  // Add to the buffer a sample start (ThreadId) entry for aThreadId.
  // Returns the position of the entry.
  uint64_t AddThreadIdEntry(int aThreadId);

  void CollectCodeLocation(
    const char* aLabel, const char* aStr, int aLineNumber,
    const mozilla::Maybe<js::ProfilingStackFrame::Category>& aCategory);

  // Maximum size of a frameKey string that we'll handle.
  static const size_t kMaxFrameKeyLength = 512;

  // Add JIT frame information to aJITFrameInfo for any JitReturnAddr entries
  // that are currently in the buffer at or after aRangeStart, in samples
  // for the given thread.
  void AddJITInfoForRange(uint64_t aRangeStart,
                          int aThreadId, JSContext* aContext,
                          JITFrameInfo& aJITFrameInfo) const;

  // Stream JSON for samples in the buffer to aWriter, using the supplied
  // UniqueStacks object.
  // Only streams samples for the given thread ID and which were taken at or
  // after aSinceTime.
  // aUniqueStacks needs to contain information about any JIT frames that we
  // might encounter in the buffer, before this method is called. In other
  // words, you need to have called AddJITInfoForRange for every range that
  // might contain JIT frame information before calling this method.
  void StreamSamplesToJSON(SpliceableJSONWriter& aWriter, int aThreadId,
                           double aSinceTime,
                           UniqueStacks& aUniqueStacks) const;

  void StreamMarkersToJSON(SpliceableJSONWriter& aWriter, int aThreadId,
                           const mozilla::TimeStamp& aProcessStartTime,
                           double aSinceTime,
                           UniqueStacks& aUniqueStacks) const;
  void StreamPausedRangesToJSON(SpliceableJSONWriter& aWriter,
                                double aSinceTime) const;

  // Find (via |aLastSample|) the most recent sample for the thread denoted by
  // |aThreadId| and clone it, patching in the current time as appropriate.
  // Mutate |aLastSample| to point to the newly inserted sample.
  // Returns whether duplication was successful.
  bool DuplicateLastSample(int aThreadId,
                           const mozilla::TimeStamp& aProcessStartTime,
                           mozilla::Maybe<uint64_t>& aLastSample);

  void AddStoredMarker(ProfilerMarker* aStoredMarker);

  // The following method is not signal safe!
  void DeleteExpiredStoredMarkers();

  // Access an entry in the buffer.
  ProfileBufferEntry& GetEntry(uint64_t aPosition) const
  {
    return mEntries[aPosition & mEntryIndexMask];
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  // The storage that backs our buffer. Holds mEntrySize entries.
  // All accesses to entries in mEntries need to go through GetEntry(), which
  // translates the given buffer position from the near-infinite uint64_t space
  // into the entry storage space.
  mozilla::UniquePtr<ProfileBufferEntry[]> mEntries;

  // A mask such that pos & mEntryIndexMask == pos % mEntrySize.
  uint32_t mEntryIndexMask;

public:
  // mRangeStart and mRangeEnd are uint64_t values that strictly advance and
  // never wrap around. mRangeEnd is always greater than or equal to
  // mRangeStart, but never gets more than mEntrySize steps ahead of
  // mRangeStart, because we can only store a fixed number of entries in the
  // buffer. Once the entire buffer is in use, adding a new entry will evict an
  // entry from the front of the buffer (and increase mRangeStart).
  // In other words, the following conditions hold true at all times:
  //  (1) mRangeStart <= mRangeEnd
  //  (2) mRangeEnd - mRangeStart <= mEntrySize
  //
  // If there are no live entries, then mRangeStart == mRangeEnd.
  // Otherwise, mRangeStart is the first live entry and mRangeEnd is one past
  // the last live entry, and also the position at which the next entry will be
  // added.
  // (mRangeEnd - mRangeStart) always gives the number of live entries.
  uint64_t mRangeStart;
  uint64_t mRangeEnd;

  // The number of entries in our buffer. Always a power of two.
  uint32_t mEntrySize;

  // Markers that marker entries in the buffer might refer to.
  ProfilerMarkerLinkedList mStoredMarkers;
};

/**
 * Helper type used to implement ProfilerStackCollector. This type is used as
 * the collector for MergeStacks by ProfileBuffer. It holds a reference to the
 * buffer, as well as additional feature flags which are needed to control the
 * data collection strategy
 */
class ProfileBufferCollector final : public ProfilerStackCollector
{
public:
  ProfileBufferCollector(ProfileBuffer& aBuf, uint32_t aFeatures,
                         uint64_t aSamplePos)
    : mBuf(aBuf)
    , mSamplePositionInBuffer(aSamplePos)
    , mFeatures(aFeatures)
  {}

  mozilla::Maybe<uint64_t> SamplePositionInBuffer() override
  {
    return mozilla::Some(mSamplePositionInBuffer);
  }

  mozilla::Maybe<uint64_t> BufferRangeStart() override
  {
    return mozilla::Some(mBuf.mRangeStart);
  }

  virtual void CollectNativeLeafAddr(void* aAddr) override;
  virtual void CollectJitReturnAddr(void* aAddr) override;
  virtual void CollectWasmFrame(const char* aLabel) override;
  virtual void CollectProfilingStackFrame(const js::ProfilingStackFrame& aFrame) override;

private:
  ProfileBuffer& mBuf;
  uint64_t mSamplePositionInBuffer;
  uint32_t mFeatures;
};

#endif
