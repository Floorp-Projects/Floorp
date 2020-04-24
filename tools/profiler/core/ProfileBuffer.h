/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_PROFILE_BUFFER_H
#define MOZ_PROFILE_BUFFER_H

#include "GeckoProfiler.h"
#include "ProfileBufferEntry.h"

#include "mozilla/Maybe.h"
#include "mozilla/PowerOfTwo.h"
#include "mozilla/ProfileBufferChunkManagerSingle.h"
#include "mozilla/ProfileChunkedBuffer.h"

// Class storing most profiling data in a ProfileChunkedBuffer.
//
// This class is used as a queue of entries which, after construction, never
// allocates. This makes it safe to use in the profiler's "critical section".
class ProfileBuffer final {
 public:
  // ProfileBuffer constructor
  // @param aBuffer The in-session ProfileChunkedBuffer to use as buffer
  // manager.
  explicit ProfileBuffer(mozilla::ProfileChunkedBuffer& aBuffer);

  ~ProfileBuffer();

  bool IsThreadSafe() const { return mEntries.IsThreadSafe(); }

  // Add |aEntry| to the buffer, ignoring what kind of entry it is.
  uint64_t AddEntry(const ProfileBufferEntry& aEntry);

  // Add to the buffer a sample start (ThreadId) entry for aThreadId.
  // Returns the position of the entry.
  uint64_t AddThreadIdEntry(int aThreadId);

  void CollectCodeLocation(
      const char* aLabel, const char* aStr, uint32_t aFrameFlags,
      uint64_t aInnerWindowID, const mozilla::Maybe<uint32_t>& aLineNumber,
      const mozilla::Maybe<uint32_t>& aColumnNumber,
      const mozilla::Maybe<JS::ProfilingCategoryPair>& aCategoryPair);

  // Maximum size of a frameKey string that we'll handle.
  static const size_t kMaxFrameKeyLength = 512;

  // Add JIT frame information to aJITFrameInfo for any JitReturnAddr entries
  // that are currently in the buffer at or after aRangeStart, in samples
  // for the given thread.
  void AddJITInfoForRange(uint64_t aRangeStart, int aThreadId,
                          JSContext* aContext,
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
  void StreamProfilerOverheadToJSON(SpliceableJSONWriter& aWriter,
                                    const mozilla::TimeStamp& aProcessStartTime,
                                    double aSinceTime) const;
  void StreamCountersToJSON(SpliceableJSONWriter& aWriter,
                            const mozilla::TimeStamp& aProcessStartTime,
                            double aSinceTime) const;

  // Find (via |aLastSample|) the most recent sample for the thread denoted by
  // |aThreadId| and clone it, patching in the current time as appropriate.
  // Mutate |aLastSample| to point to the newly inserted sample.
  // Returns whether duplication was successful.
  bool DuplicateLastSample(int aThreadId,
                           const mozilla::TimeStamp& aProcessStartTime,
                           mozilla::Maybe<uint64_t>& aLastSample);

  void DiscardSamplesBeforeTime(double aTime);

  // Read an entry in the buffer.
  ProfileBufferEntry GetEntry(uint64_t aPosition) const {
    return mEntries.ReadAt(
        mozilla::ProfileBufferBlockIndex::CreateFromProfileBufferIndex(
            aPosition),
        [&](mozilla::Maybe<mozilla::ProfileBufferEntryReader>&& aMER) {
          ProfileBufferEntry entry;
          if (aMER.isSome()) {
            if (aMER->CurrentBlockIndex().ConvertToProfileBufferIndex() ==
                aPosition) {
              // If we're here, it means `aPosition` pointed at a valid block.
              MOZ_RELEASE_ASSERT(aMER->RemainingBytes() <= sizeof(entry));
              aMER->ReadBytes(&entry, aMER->RemainingBytes());
            } else {
              // EntryReader at the wrong position, pretend to have read
              // everything.
              aMER->SetRemainingBytes(0);
            }
          }
          return entry;
        });
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  void CollectOverheadStats(mozilla::TimeDuration aSamplingTime,
                            mozilla::TimeDuration aLocking,
                            mozilla::TimeDuration aCleaning,
                            mozilla::TimeDuration aCounters,
                            mozilla::TimeDuration aThreads);

  ProfilerBufferInfo GetProfilerBufferInfo() const;

 private:
  // Add |aEntry| to the provided ProfileChunkedBuffer.
  // `static` because it may be used to add an entry to a `ProfileChunkedBuffer`
  // that is not attached to a `ProfileBuffer`.
  static mozilla::ProfileBufferBlockIndex AddEntry(
      mozilla::ProfileChunkedBuffer& aProfileChunkedBuffer,
      const ProfileBufferEntry& aEntry);

  // Add a sample start (ThreadId) entry for aThreadId to the provided
  // ProfileChunkedBuffer. Returns the position of the entry.
  // `static` because it may be used to add an entry to a `ProfileChunkedBuffer`
  // that is not attached to a `ProfileBuffer`.
  static mozilla::ProfileBufferBlockIndex AddThreadIdEntry(
      mozilla::ProfileChunkedBuffer& aProfileChunkedBuffer, int aThreadId);

  // The storage in which this ProfileBuffer stores its entries.
  mozilla::ProfileChunkedBuffer& mEntries;

 public:
  // `BufferRangeStart()` and `BufferRangeEnd()` return `uint64_t` values
  // corresponding to the first entry and past the last entry stored in
  // `mEntries`.
  //
  // The returned values are not guaranteed to be stable, because other threads
  // may also be accessing the buffer concurrently. But they will always
  // increase, and can therefore give an indication of how far these values have
  // *at least* reached. In particular:
  // - Entries whose index is strictly less that `BufferRangeStart()` have been
  //   discarded by now, so any related data may also be safely discarded.
  // - It is safe to try and read entries at any index strictly less than
  //   `BufferRangeEnd()` -- but note that these reads may fail by the time you
  //   request them, as old entries get overwritten by new ones.
  uint64_t BufferRangeStart() const { return mEntries.GetState().mRangeStart; }
  uint64_t BufferRangeEnd() const { return mEntries.GetState().mRangeEnd; }

 private:
  // 65536 bytes should be plenty for a single backtrace.
  static constexpr auto WorkerBufferBytes = mozilla::MakePowerOfTwo32<65536>();

  // Single pre-allocated chunk (to avoid spurious mallocs), used when:
  // - Duplicating sleeping stacks.
  // - Adding JIT info.
  // - Streaming stacks to JSON.
  // Mutable because it's accessed from non-multithreaded const methods.
  mutable mozilla::ProfileBufferChunkManagerSingle mWorkerChunkManager{
      mozilla::ProfileBufferChunk::Create(
          mozilla::ProfileBufferChunk::SizeofChunkMetadata() +
          WorkerBufferBytes.Value())};

  double mFirstSamplingTimeNs = 0.0;
  double mLastSamplingTimeNs = 0.0;
  ProfilerStats mIntervalsNs;
  ProfilerStats mOverheadsNs;
  ProfilerStats mLockingsNs;
  ProfilerStats mCleaningsNs;
  ProfilerStats mCountersNs;
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

  mozilla::Maybe<uint64_t> SamplePositionInBuffer() override {
    return mozilla::Some(mSamplePositionInBuffer);
  }

  mozilla::Maybe<uint64_t> BufferRangeStart() override {
    return mozilla::Some(mBuf.BufferRangeStart());
  }

  virtual void CollectNativeLeafAddr(void* aAddr) override;
  virtual void CollectJitReturnAddr(void* aAddr) override;
  virtual void CollectWasmFrame(const char* aLabel) override;
  virtual void CollectProfilingStackFrame(
      const js::ProfilingStackFrame& aFrame) override;

 private:
  ProfileBuffer& mBuf;
  uint64_t mSamplePositionInBuffer;
  uint32_t mFeatures;
};

#endif
