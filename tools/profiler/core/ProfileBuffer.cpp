/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfileBuffer.h"

#include "ProfilerMarker.h"

#include "BaseProfiler.h"
#include "jsfriendapi.h"
#include "mozilla/MathAlgorithms.h"
#include "nsJSPrincipals.h"
#include "nsScriptSecurityManager.h"

using namespace mozilla;

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
    const Maybe<JS::ProfilingCategoryPair>& aCategoryPair) {
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
  AUTO_PROFILER_STATS(gecko_ProfileBuffer_DeleteExpiredStoredMarkers);

  // Delete markers of samples that have been overwritten due to circular
  // buffer wraparound.
  while (mStoredMarkers.peek() &&
         mStoredMarkers.peek()->HasExpired(mRangeStart)) {
    delete mStoredMarkers.popHead();
  }
}

size_t ProfileBuffer::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
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
  double time = aSamplingTime.ToMilliseconds() * 1000.0;
  if (mFirstSamplingTimeNs == 0.0) {
    mFirstSamplingTimeNs = time;
  } else {
    // Note that we'll have 1 fewer interval than other numbers (because
    // we need both ends of an interval to know its duration). The final
    // difference should be insignificant over the expected many thousands
    // of iterations.
    mIntervalsNs.Count(time - mLastSamplingTimeNs);
  }
  mLastSamplingTimeNs = time;
  double locking = aLocking.ToMilliseconds() * 1000.0;
  double cleaning = aCleaning.ToMilliseconds() * 1000.0;
  double counters = aCounters.ToMilliseconds() * 1000.0;
  double threads = aThreads.ToMilliseconds() * 1000.0;

  mOverheadsNs.Count(locking + cleaning + counters + threads);
  mLockingsNs.Count(locking);
  mCleaningsNs.Count(cleaning);
  mCountersNs.Count(counters);
  mThreadsNs.Count(threads);

  AddEntry(ProfileBufferEntry::ProfilerOverheadTime(time));
  AddEntry(ProfileBufferEntry::ProfilerOverheadDuration(locking));
  AddEntry(ProfileBufferEntry::ProfilerOverheadDuration(cleaning));
  AddEntry(ProfileBufferEntry::ProfilerOverheadDuration(counters));
  AddEntry(ProfileBufferEntry::ProfilerOverheadDuration(threads));
}

ProfilerBufferInfo ProfileBuffer::GetProfilerBufferInfo() const {
  return {mRangeStart,  mRangeEnd,    mEntryIndexMask.MaskValue() + 1,
          mIntervalsNs, mOverheadsNs, mLockingsNs,
          mCleaningsNs, mCountersNs,  mThreadsNs};
}

/* ProfileBufferCollector */

static bool IsChromeJSScript(JSScript* aScript) {
  // WARNING: this function runs within the profiler's "critical section".
  auto realm = js::GetScriptRealm(aScript);
  return js::IsSystemRealm(realm);
}

void ProfileBufferCollector::CollectNativeLeafAddr(void* aAddr) {
  mBuf.AddEntry(ProfileBufferEntry::NativeLeafAddr(aAddr));
}

void ProfileBufferCollector::CollectJitReturnAddr(void* aAddr) {
  mBuf.AddEntry(ProfileBufferEntry::JitReturnAddr(aAddr));
}

void ProfileBufferCollector::CollectWasmFrame(const char* aLabel) {
  mBuf.CollectCodeLocation("", aLabel, 0, Nothing(), Nothing(), Nothing());
}

void ProfileBufferCollector::CollectProfilingStackFrame(
    const js::ProfilingStackFrame& aFrame) {
  // WARNING: this function runs within the profiler's "critical section".

  MOZ_ASSERT(aFrame.isLabelFrame() ||
             (aFrame.isJsFrame() && !aFrame.isOSRFrame()));

  const char* label = aFrame.label();
  const char* dynamicString = aFrame.dynamicString();
  bool isChromeJSEntry = false;
  Maybe<uint32_t> line;
  Maybe<uint32_t> column;

  if (aFrame.isJsFrame()) {
    // There are two kinds of JS frames that get pushed onto the ProfilingStack.
    //
    // - label = "", dynamic string = <something>
    // - label = "js::RunScript", dynamic string = nullptr
    //
    // The line number is only interesting in the first case.

    if (label[0] == '\0') {
      MOZ_ASSERT(dynamicString);

      // We call aFrame.script() repeatedly -- rather than storing the result in
      // a local variable in order -- to avoid rooting hazards.
      if (aFrame.script()) {
        isChromeJSEntry = IsChromeJSScript(aFrame.script());
        if (aFrame.pc()) {
          unsigned col = 0;
          line = Some(JS_PCToLineNumber(aFrame.script(), aFrame.pc(), &col));
          column = Some(col);
        }
      }

    } else {
      MOZ_ASSERT(strcmp(label, "js::RunScript") == 0 && !dynamicString);
    }
  } else {
    MOZ_ASSERT(aFrame.isLabelFrame());
  }

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
