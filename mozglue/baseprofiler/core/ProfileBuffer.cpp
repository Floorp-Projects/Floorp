/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseProfiler.h"

#ifdef MOZ_BASE_PROFILER

#  include "ProfileBuffer.h"

#  include "ProfilerMarker.h"

#  include "mozilla/MathAlgorithms.h"

namespace mozilla {
namespace baseprofiler {

ProfileBuffer::ProfileBuffer(PowerOfTwo32 aCapacity)
    : mEntries(MakeUnique<ProfileBufferEntry[]>(aCapacity.Value())),
      mEntryIndexMask(aCapacity.Mask()),
      mRangeStart(0),
      mRangeEnd(0) {}

ProfileBuffer::~ProfileBuffer() {
  while (mStoredMarkers.peek()) {
    delete mStoredMarkers.popHead();
  }
}

// Called from signal, call only reentrant functions
void ProfileBuffer::AddEntry(const ProfileBufferEntry& aEntry) {
  GetEntry(mRangeEnd++) = aEntry;

  // The distance between mRangeStart and mRangeEnd must never exceed
  // capacity, so advance mRangeStart if necessary.
  if (mRangeEnd - mRangeStart > mEntryIndexMask.MaskValue() + 1) {
    mRangeStart++;
  }
}

uint64_t ProfileBuffer::AddThreadIdEntry(int aThreadId) {
  uint64_t pos = mRangeEnd;
  AddEntry(ProfileBufferEntry::ThreadId(aThreadId));
  return pos;
}

void ProfileBuffer::AddStoredMarker(ProfilerMarker* aStoredMarker) {
  aStoredMarker->SetPositionInBuffer(mRangeEnd);
  mStoredMarkers.insert(aStoredMarker);
}

void ProfileBuffer::CollectCodeLocation(
    const char* aLabel, const char* aStr, uint32_t aFrameFlags,
    const Maybe<uint32_t>& aLineNumber, const Maybe<uint32_t>& aColumnNumber,
    const Maybe<ProfilingCategoryPair>& aCategoryPair) {
  AddEntry(ProfileBufferEntry::Label(aLabel));
  AddEntry(ProfileBufferEntry::FrameFlags(uint64_t(aFrameFlags)));

  if (aStr) {
    // Store the string using one or more DynamicStringFragment entries.
    size_t strLen = strlen(aStr) + 1;  // +1 for the null terminator
    for (size_t j = 0; j < strLen;) {
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

  if (aLineNumber) {
    AddEntry(ProfileBufferEntry::LineNumber(*aLineNumber));
  }

  if (aColumnNumber) {
    AddEntry(ProfileBufferEntry::ColumnNumber(*aColumnNumber));
  }

  if (aCategoryPair.isSome()) {
    AddEntry(ProfileBufferEntry::CategoryPair(int(*aCategoryPair)));
  }
}

void ProfileBuffer::DeleteExpiredStoredMarkers() {
  AUTO_PROFILER_STATS(base_ProfileBuffer_DeleteExpiredStoredMarkers);

  // Delete markers of samples that have been overwritten due to circular
  // buffer wraparound.
  while (mStoredMarkers.peek() &&
         mStoredMarkers.peek()->HasExpired(mRangeStart)) {
    delete mStoredMarkers.popHead();
  }
}

size_t ProfileBuffer::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);
  n += aMallocSizeOf(mEntries.get());

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - memory pointed to by the elements within mEntries
  // - mStoredMarkers

  return n;
}

void ProfileBuffer::CollectOverheadStats(TimeDuration aSamplingTime,
                                         TimeDuration aLocking,
                                         TimeDuration aCleaning,
                                         TimeDuration aCounters,
                                         TimeDuration aThreads) {
  double timeNs = aSamplingTime.ToMilliseconds() * 1000.0;
  if (mFirstSamplingTimeNs == 0.0) {
    mFirstSamplingTimeNs = timeNs;
  } else {
    // Note that we'll have 1 fewer interval than other numbers (because
    // we need both ends of an interval to know its duration). The final
    // difference should be insignificant over the expected many thousands
    // of iterations.
    mIntervalsNs.Count(timeNs - mLastSamplingTimeNs);
  }
  mLastSamplingTimeNs = timeNs;
  // Time to take the lock before sampling.
  double lockingNs = aLocking.ToMilliseconds() * 1000.0;
  // Time to discard expired markers.
  double cleaningNs = aCleaning.ToMilliseconds() * 1000.0;
  // Time to gather all counters.
  double countersNs = aCounters.ToMilliseconds() * 1000.0;
  // Time to sample all threads.
  double threadsNs = aThreads.ToMilliseconds() * 1000.0;

  // Add to our gathered stats.
  mOverheadsNs.Count(lockingNs + cleaningNs + countersNs + threadsNs);
  mLockingsNs.Count(lockingNs);
  mCleaningsNs.Count(cleaningNs);
  mCountersNs.Count(countersNs);
  mThreadsNs.Count(threadsNs);

  // Record details in buffer.
  AddEntry(ProfileBufferEntry::ProfilerOverheadTime(timeNs));
  AddEntry(ProfileBufferEntry::ProfilerOverheadDuration(lockingNs));
  AddEntry(ProfileBufferEntry::ProfilerOverheadDuration(cleaningNs));
  AddEntry(ProfileBufferEntry::ProfilerOverheadDuration(countersNs));
  AddEntry(ProfileBufferEntry::ProfilerOverheadDuration(threadsNs));
}

ProfilerBufferInfo ProfileBuffer::GetProfilerBufferInfo() const {
  return {mRangeStart,  mRangeEnd,    mEntryIndexMask.MaskValue() + 1,
          mIntervalsNs, mOverheadsNs, mLockingsNs,
          mCleaningsNs, mCountersNs,  mThreadsNs};
}

/* ProfileBufferCollector */

void ProfileBufferCollector::CollectNativeLeafAddr(void* aAddr) {
  mBuf.AddEntry(ProfileBufferEntry::NativeLeafAddr(aAddr));
}

void ProfileBufferCollector::CollectProfilingStackFrame(
    const ProfilingStackFrame& aFrame) {
  // WARNING: this function runs within the profiler's "critical section".

  MOZ_ASSERT(aFrame.isLabelFrame() ||
             (aFrame.isJsFrame() && !aFrame.isOSRFrame()));

  const char* label = aFrame.label();
  const char* dynamicString = aFrame.dynamicString();
  bool isChromeJSEntry = false;
  Maybe<uint32_t> line;
  Maybe<uint32_t> column;

  MOZ_ASSERT(aFrame.isLabelFrame());

  if (dynamicString) {
    // Adjust the dynamic string as necessary.
    if (ProfilerFeature::HasPrivacy(mFeatures) && !isChromeJSEntry) {
      dynamicString = "(private)";
    } else if (strlen(dynamicString) >= ProfileBuffer::kMaxFrameKeyLength) {
      dynamicString = "(too long)";
    }
  }

  mBuf.CollectCodeLocation(label, dynamicString, aFrame.flags(), line, column,
                           Some(aFrame.categoryPair()));
}

}  // namespace baseprofiler
}  // namespace mozilla

#endif  // MOZ_BASE_PROFILER
