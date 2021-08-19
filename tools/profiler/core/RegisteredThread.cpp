/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RegisteredThread.h"

#include "mozilla/ProfilerMarkers.h"
#include "js/AllocationRecording.h"
#include "js/ProfilingStack.h"
#include "js/TraceLoggerAPI.h"

// This is a simplified version of profiler_add_marker that can be easily passed
// into the JS engine.
static void profiler_add_js_marker(const char* aMarkerName,
                                   const char* aMarkerText) {
  PROFILER_MARKER_TEXT(
      mozilla::ProfilerString8View::WrapNullTerminatedString(aMarkerName), JS,
      {}, mozilla::ProfilerString8View::WrapNullTerminatedString(aMarkerText));
}

static void profiler_add_js_allocation_marker(JS::RecordAllocationInfo&& info) {
  if (!profiler_can_accept_markers()) {
    return;
  }

  struct JsAllocationMarker {
    static constexpr mozilla::Span<const char> MarkerTypeName() {
      return mozilla::MakeStringSpan("JS allocation");
    }
    static void StreamJSONMarkerData(
        mozilla::baseprofiler::SpliceableJSONWriter& aWriter,
        const mozilla::ProfilerString16View& aTypeName,
        const mozilla::ProfilerString8View& aClassName,
        const mozilla::ProfilerString16View& aDescriptiveTypeName,
        const mozilla::ProfilerString8View& aCoarseType, uint64_t aSize,
        bool aInNursery) {
      if (aClassName.Length() != 0) {
        aWriter.StringProperty("className", aClassName);
      }
      if (aTypeName.Length() != 0) {
        aWriter.StringProperty(
            "typeName",
            NS_ConvertUTF16toUTF8(aTypeName.Data(), aTypeName.Length()));
      }
      if (aDescriptiveTypeName.Length() != 0) {
        aWriter.StringProperty(
            "descriptiveTypeName",
            NS_ConvertUTF16toUTF8(aDescriptiveTypeName.Data(),
                                  aDescriptiveTypeName.Length()));
      }
      aWriter.StringProperty("coarseType", aCoarseType);
      aWriter.IntProperty("size", aSize);
      aWriter.BoolProperty("inNursery", aInNursery);
    }
    static mozilla::MarkerSchema MarkerTypeDisplay() {
      return mozilla::MarkerSchema::SpecialFrontendLocation{};
    }
  };

  profiler_add_marker(
      "JS allocation", geckoprofiler::category::JS,
      mozilla::MarkerStack::Capture(), JsAllocationMarker{},
      mozilla::ProfilerString16View::WrapNullTerminatedString(info.typeName),
      mozilla::ProfilerString8View::WrapNullTerminatedString(info.className),
      mozilla::ProfilerString16View::WrapNullTerminatedString(
          info.descriptiveTypeName),
      mozilla::ProfilerString8View::WrapNullTerminatedString(info.coarseType),
      info.size, info.inNursery);
}

RacyRegisteredThread::RacyRegisteredThread(ProfilerThreadId aThreadId)
    : mProfilingStackOwner(
          mozilla::MakeNotNull<RefPtr<mozilla::ProfilingStackOwner>>()),
      mThreadId(aThreadId),
      mSleep(AWAKE),
      mIsBeingProfiled(false) {
  MOZ_COUNT_CTOR(RacyRegisteredThread);
}

RegisteredThread::RegisteredThread(ThreadInfo* aInfo, nsIThread* aThread,
                                   void* aStackTop)
    : mRacyRegisteredThread(aInfo->ThreadId()),
      mPlatformData(AllocPlatformData(aInfo->ThreadId())),
      mStackTop(aStackTop),
      mThreadInfo(aInfo),
      mThread(aThread),
      mContext(nullptr),
      mJSSampling(INACTIVE),
      mJSFlags(0) {
  MOZ_COUNT_CTOR(RegisteredThread);

  // NOTE: aThread can be null for the first thread, before the ThreadManager
  // is initialized.

  // We don't have to guess on mac
#if defined(GP_OS_darwin)
  pthread_t self = pthread_self();
  mStackTop = pthread_get_stackaddr_np(self);
#endif
}

RegisteredThread::~RegisteredThread() { MOZ_COUNT_DTOR(RegisteredThread); }

size_t RegisteredThread::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - mPlatformData
  //
  // The following members are not measured:
  // - mThreadInfo: because it is non-owning

  return n;
}

void RegisteredThread::GetRunningEventDelay(const mozilla::TimeStamp& aNow,
                                            mozilla::TimeDuration& aDelay,
                                            mozilla::TimeDuration& aRunning) {
  if (mThread) {  // can be null right at the start of a process
    mozilla::TimeStamp start;
    mThread->GetRunningEventDelay(&aDelay, &start);
    if (!start.IsNull()) {
      // Note: the timestamp used here will be from when we started to
      // suspend and sample the thread; which is also the timestamp
      // associated with the sample.
      aRunning = aNow - start;
      return;
    }
  }
  aDelay = mozilla::TimeDuration();
  aRunning = mozilla::TimeDuration();
}

void RegisteredThread::SetJSContext(JSContext* aContext) {
  // This function runs on-thread.

  MOZ_ASSERT(aContext && !mContext);

  mContext = aContext;

  // We give the JS engine a non-owning reference to the ProfilingStack. It's
  // important that the JS engine doesn't touch this once the thread dies.
  js::SetContextProfilingStack(aContext,
                               &RacyRegisteredThread().ProfilingStack());
}

void RegisteredThread::PollJSSampling() {
  // This function runs on-thread.

  // We can't start/stop profiling until we have the thread's JSContext.
  if (mContext) {
    // It is possible for mJSSampling to go through the following sequences.
    //
    // - INACTIVE, ACTIVE_REQUESTED, INACTIVE_REQUESTED, INACTIVE
    //
    // - ACTIVE, INACTIVE_REQUESTED, ACTIVE_REQUESTED, ACTIVE
    //
    // Therefore, the if and else branches here aren't always interleaved.
    // This is ok because the JS engine can handle that.
    //
    if (mJSSampling == ACTIVE_REQUESTED) {
      mJSSampling = ACTIVE;
      js::EnableContextProfilingStack(mContext, true);
      if (JSTracerEnabled()) {
        JS::StartTraceLogger(mContext);
      }
      if (JSAllocationsEnabled()) {
        // TODO - This probability should not be hardcoded. See Bug 1547284.
        JS::EnableRecordingAllocations(mContext,
                                       profiler_add_js_allocation_marker, 0.01);
      }
      js::RegisterContextProfilingEventMarker(mContext, profiler_add_js_marker);

    } else if (mJSSampling == INACTIVE_REQUESTED) {
      mJSSampling = INACTIVE;
      js::EnableContextProfilingStack(mContext, false);
      if (JSTracerEnabled()) {
        JS::StopTraceLogger(mContext);
      }
      if (JSAllocationsEnabled()) {
        JS::DisableRecordingAllocations(mContext);
      }
    }
  }
}
