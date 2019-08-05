/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_PROFILE_BUFFER_H
#define MOZ_PROFILE_BUFFER_H

#include "ProfileBufferEntry.h"
#include "ProfilerMarker.h"

#include "mozilla/Maybe.h"
#include "mozilla/PowerOfTwo.h"

namespace mozilla {
namespace baseprofiler {

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
class ProfileBuffer final {
 public:
  // ProfileBuffer constructor
  // @param aCapacity The capacity of the buffer.
  explicit ProfileBuffer(PowerOfTwo32 aCapacity);

  ~ProfileBuffer();

  // Add |aEntry| to the buffer, ignoring what kind of entry it is.
  void AddEntry(const ProfileBufferEntry& aEntry);

  // Add to the buffer a sample start (ThreadId) entry for aThreadId.
  // Returns the position of the entry.
  uint64_t AddThreadIdEntry(int aThreadId);

  void CollectCodeLocation(const char* aLabel, const char* aStr,
                           uint32_t aFrameFlags,
                           const Maybe<uint32_t>& aLineNumber,
                           const Maybe<uint32_t>& aColumnNumber,
                           const Maybe<ProfilingCategoryPair>& aCategoryPair);

  // Maximum size of a frameKey string that we'll handle.
  static const size_t kMaxFrameKeyLength = 512;

  // Stream JSON for samples in the buffer to aWriter, using the supplied
  // UniqueStacks object.
  // Only streams samples for the given thread ID and which were taken at or
  // after aSinceTime.
  void StreamSamplesToJSON(SpliceableJSONWriter& aWriter, int aThreadId,
                           double aSinceTime,
                           UniqueStacks& aUniqueStacks) const;

  void StreamMarkersToJSON(SpliceableJSONWriter& aWriter, int aThreadId,
                           const TimeStamp& aProcessStartTime,
                           double aSinceTime,
                           UniqueStacks& aUniqueStacks) const;
  void StreamPausedRangesToJSON(SpliceableJSONWriter& aWriter,
                                double aSinceTime) const;
  void StreamProfilerOverheadToJSON(SpliceableJSONWriter& aWriter,
                                    const TimeStamp& aProcessStartTime,
                                    double aSinceTime) const;
  void StreamCountersToJSON(SpliceableJSONWriter& aWriter,
                            const TimeStamp& aProcessStartTime,
                            double aSinceTime) const;

  // Find (via |aLastSample|) the most recent sample for the thread denoted by
  // |aThreadId| and clone it, patching in the current time as appropriate.
  // Mutate |aLastSample| to point to the newly inserted sample.
  // Returns whether duplication was successful.
  bool DuplicateLastSample(int aThreadId, const TimeStamp& aProcessStartTime,
                           Maybe<uint64_t>& aLastSample);

  void DiscardSamplesBeforeTime(double aTime);

  void AddStoredMarker(ProfilerMarker* aStoredMarker);

  // The following method is not signal safe!
  void DeleteExpiredStoredMarkers();

  // Access an entry in the buffer.
  ProfileBufferEntry& GetEntry(uint64_t aPosition) const {
    return mEntries[aPosition & mEntryIndexMask];
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  void CollectOverheadStats(TimeDuration aSamplingTime, TimeDuration aLocking,
                            TimeDuration aCleaning, TimeDuration aCounters,
                            TimeDuration aThreads);

  ProfilerBufferInfo GetProfilerBufferInfo() const;

 private:
  // The storage that backs our buffer. Holds capacity entries.
  // All accesses to entries in mEntries need to go through GetEntry(), which
  // translates the given buffer position from the near-infinite uint64_t space
  // into the entry storage space.
  UniquePtr<ProfileBufferEntry[]> mEntries;

  // A mask such that pos & mEntryIndexMask == pos % capacity.
  PowerOfTwoMask32 mEntryIndexMask;

 public:
  // mRangeStart and mRangeEnd are uint64_t values that strictly advance and
  // never wrap around. mRangeEnd is always greater than or equal to
  // mRangeStart, but never gets more than capacity steps ahead of
  // mRangeStart, because we can only store a fixed number of entries in the
  // buffer. Once the entire buffer is in use, adding a new entry will evict an
  // entry from the front of the buffer (and increase mRangeStart).
  // In other words, the following conditions hold true at all times:
  //  (1) mRangeStart <= mRangeEnd
  //  (2) mRangeEnd - mRangeStart <= capacity
  //
  // If there are no live entries, then mRangeStart == mRangeEnd.
  // Otherwise, mRangeStart is the first live entry and mRangeEnd is one past
  // the last live entry, and also the position at which the next entry will be
  // added.
  // (mRangeEnd - mRangeStart) always gives the number of live entries.
  uint64_t mRangeStart;
  uint64_t mRangeEnd;

  // Markers that marker entries in the buffer might refer to.
  ProfilerMarkerLinkedList mStoredMarkers;

 private:
  // Time from launch (ns) when first sampling was recorded.
  double mFirstSamplingTimeNs = 0.0;
  // Time from launch (ns) when last sampling was recorded.
  double mLastSamplingTimeNs = 0.0;
  // Sampling stats: Interval (ns) between successive samplings.
  ProfilerStats mIntervalsNs;
  // Sampling stats: Total duration (ns) of each sampling. (Split detail below.)
  ProfilerStats mOverheadsNs;
  // Sampling stats: Time (ns) to acquire the lock before sampling.
  ProfilerStats mLockingsNs;
  // Sampling stats: Time (ns) to discard expired data.
  ProfilerStats mCleaningsNs;
  // Sampling stats: Time (ns) to collect counter data.
  ProfilerStats mCountersNs;
  // Sampling stats: Time (ns) to sample thread stacks.
  ProfilerStats mThreadsNs;
};

/**
 * Helper type used to implement ProfilerStackCollector. This type is used as
 * the collector for MergeStacks by ProfileBuffer. It holds a reference to the
 * buffer, as well as additional feature flags which are needed to control the
 * data collection strategy
 */
class ProfileBufferCollector final : public ProfilerStackCollector {
 public:
  ProfileBufferCollector(ProfileBuffer& aBuf, uint32_t aFeatures,
                         uint64_t aSamplePos)
      : mBuf(aBuf), mSamplePositionInBuffer(aSamplePos), mFeatures(aFeatures) {}

  Maybe<uint64_t> SamplePositionInBuffer() override {
    return Some(mSamplePositionInBuffer);
  }

  Maybe<uint64_t> BufferRangeStart() override { return Some(mBuf.mRangeStart); }

  virtual void CollectNativeLeafAddr(void* aAddr) override;
  virtual void CollectProfilingStackFrame(
      const ProfilingStackFrame& aFrame) override;

 private:
  ProfileBuffer& mBuf;
  uint64_t mSamplePositionInBuffer;
  uint32_t mFeatures;
};

}  // namespace baseprofiler
}  // namespace mozilla

#endif
