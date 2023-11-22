/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfileBuffer.h"

#include "BaseProfiler.h"
#include "js/ColumnNumber.h"  // JS::LimitedColumnNumberOneOrigin
#include "js/GCAPI.h"
#include "jsfriendapi.h"
#include "mozilla/MathAlgorithms.h"
#include "nsJSPrincipals.h"
#include "nsScriptSecurityManager.h"

using namespace mozilla;

ProfileBuffer::ProfileBuffer(ProfileChunkedBuffer& aBuffer)
    : mEntries(aBuffer) {
  // Assume the given buffer is in-session.
  MOZ_ASSERT(mEntries.IsInSession());
}

/* static */
ProfileBufferBlockIndex ProfileBuffer::AddEntry(
    ProfileChunkedBuffer& aProfileChunkedBuffer,
    const ProfileBufferEntry& aEntry) {
  switch (aEntry.GetKind()) {
#define SWITCH_KIND(KIND, TYPE, SIZE)                          \
  case ProfileBufferEntry::Kind::KIND: {                       \
    return aProfileChunkedBuffer.PutFrom(&aEntry, 1 + (SIZE)); \
  }

    FOR_EACH_PROFILE_BUFFER_ENTRY_KIND(SWITCH_KIND)

#undef SWITCH_KIND
    default:
      MOZ_ASSERT(false, "Unhandled ProfilerBuffer entry KIND");
      return ProfileBufferBlockIndex{};
  }
}

// Called from signal, call only reentrant functions
uint64_t ProfileBuffer::AddEntry(const ProfileBufferEntry& aEntry) {
  return AddEntry(mEntries, aEntry).ConvertToProfileBufferIndex();
}

/* static */
ProfileBufferBlockIndex ProfileBuffer::AddThreadIdEntry(
    ProfileChunkedBuffer& aProfileChunkedBuffer, ProfilerThreadId aThreadId) {
  return AddEntry(aProfileChunkedBuffer,
                  ProfileBufferEntry::ThreadId(aThreadId));
}

uint64_t ProfileBuffer::AddThreadIdEntry(ProfilerThreadId aThreadId) {
  return AddThreadIdEntry(mEntries, aThreadId).ConvertToProfileBufferIndex();
}

void ProfileBuffer::CollectCodeLocation(
    const char* aLabel, const char* aStr, uint32_t aFrameFlags,
    uint64_t aInnerWindowID, const Maybe<uint32_t>& aLineNumber,
    const Maybe<uint32_t>& aColumnNumber,
    const Maybe<JS::ProfilingCategoryPair>& aCategoryPair) {
  AddEntry(ProfileBufferEntry::Label(aLabel));
  AddEntry(ProfileBufferEntry::FrameFlags(uint64_t(aFrameFlags)));

  if (aStr) {
    // Store the string using one or more DynamicStringFragment entries.
    size_t strLen = strlen(aStr) + 1;  // +1 for the null terminator
    // If larger than the prescribed limit, we will cut the string and end it
    // with an ellipsis.
    const bool tooBig = strLen > kMaxFrameKeyLength;
    if (tooBig) {
      strLen = kMaxFrameKeyLength;
    }
    char chars[ProfileBufferEntry::kNumChars];
    for (size_t j = 0;; j += ProfileBufferEntry::kNumChars) {
      // Store up to kNumChars characters in the entry.
      size_t len = ProfileBufferEntry::kNumChars;
      const bool last = j + len >= strLen;
      if (last) {
        // Only the last entry may be smaller than kNumChars.
        len = strLen - j;
        if (tooBig) {
          // That last entry is part of a too-big string, replace the end
          // characters with an ellipsis "...".
          len = std::max(len, size_t(4));
          chars[len - 4] = '.';
          chars[len - 3] = '.';
          chars[len - 2] = '.';
          chars[len - 1] = '\0';
          // Make sure the memcpy will not overwrite our ellipsis!
          len -= 4;
        }
      }
      memcpy(chars, &aStr[j], len);
      AddEntry(ProfileBufferEntry::DynamicStringFragment(chars));
      if (last) {
        break;
      }
    }
  }

  if (aInnerWindowID) {
    AddEntry(ProfileBufferEntry::InnerWindowID(aInnerWindowID));
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

size_t ProfileBuffer::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - memory pointed to by the elements within mEntries
  return mEntries.SizeOfExcludingThis(aMallocSizeOf);
}

size_t ProfileBuffer::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

void ProfileBuffer::CollectOverheadStats(double aSamplingTimeMs,
                                         TimeDuration aLocking,
                                         TimeDuration aCleaning,
                                         TimeDuration aCounters,
                                         TimeDuration aThreads) {
  double timeUs = aSamplingTimeMs * 1000.0;
  if (mFirstSamplingTimeUs == 0.0) {
    mFirstSamplingTimeUs = timeUs;
  } else {
    // Note that we'll have 1 fewer interval than other numbers (because
    // we need both ends of an interval to know its duration). The final
    // difference should be insignificant over the expected many thousands
    // of iterations.
    mIntervalsUs.Count(timeUs - mLastSamplingTimeUs);
  }
  mLastSamplingTimeUs = timeUs;
  double locking = aLocking.ToMilliseconds() * 1000.0;
  double cleaning = aCleaning.ToMilliseconds() * 1000.0;
  double counters = aCounters.ToMilliseconds() * 1000.0;
  double threads = aThreads.ToMilliseconds() * 1000.0;

  mOverheadsUs.Count(locking + cleaning + counters + threads);
  mLockingsUs.Count(locking);
  mCleaningsUs.Count(cleaning);
  mCountersUs.Count(counters);
  mThreadsUs.Count(threads);

  static const bool sRecordSamplingOverhead = []() {
    const char* recordOverheads = getenv("MOZ_PROFILER_RECORD_OVERHEADS");
    return recordOverheads && recordOverheads[0] != '\0';
  }();
  if (sRecordSamplingOverhead) {
    AddEntry(ProfileBufferEntry::ProfilerOverheadTime(aSamplingTimeMs));
    AddEntry(ProfileBufferEntry::ProfilerOverheadDuration(locking));
    AddEntry(ProfileBufferEntry::ProfilerOverheadDuration(cleaning));
    AddEntry(ProfileBufferEntry::ProfilerOverheadDuration(counters));
    AddEntry(ProfileBufferEntry::ProfilerOverheadDuration(threads));
  }
}

ProfilerBufferInfo ProfileBuffer::GetProfilerBufferInfo() const {
  return {BufferRangeStart(),
          BufferRangeEnd(),
          static_cast<uint32_t>(*mEntries.BufferLength() /
                                8),  // 8 bytes per entry.
          mIntervalsUs,
          mOverheadsUs,
          mLockingsUs,
          mCleaningsUs,
          mCountersUs,
          mThreadsUs};
}

/* ProfileBufferCollector */

void ProfileBufferCollector::CollectNativeLeafAddr(void* aAddr) {
  mBuf.AddEntry(ProfileBufferEntry::NativeLeafAddr(aAddr));
}

void ProfileBufferCollector::CollectJitReturnAddr(void* aAddr) {
  mBuf.AddEntry(ProfileBufferEntry::JitReturnAddr(aAddr));
}

void ProfileBufferCollector::CollectWasmFrame(const char* aLabel) {
  mBuf.CollectCodeLocation("", aLabel, 0, 0, Nothing(), Nothing(),
                           Some(JS::ProfilingCategoryPair::JS_Wasm));
}

void ProfileBufferCollector::CollectProfilingStackFrame(
    const js::ProfilingStackFrame& aFrame) {
  // WARNING: this function runs within the profiler's "critical section".

  MOZ_ASSERT(aFrame.isLabelFrame() ||
             (aFrame.isJsFrame() && !aFrame.isOSRFrame()));

  const char* label = aFrame.label();
  const char* dynamicString = aFrame.dynamicString();
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
        if (aFrame.pc()) {
          JS::LimitedColumnNumberOneOrigin col;
          line = Some(JS_PCToLineNumber(aFrame.script(), aFrame.pc(), &col));
          column = Some(col.zeroOriginValue());
        }
      }

    } else {
      MOZ_ASSERT(strcmp(label, "js::RunScript") == 0 && !dynamicString);
    }
  } else {
    MOZ_ASSERT(aFrame.isLabelFrame());
  }

  mBuf.CollectCodeLocation(label, dynamicString, aFrame.flags(),
                           aFrame.realmID(), line, column,
                           Some(aFrame.categoryPair()));
}
