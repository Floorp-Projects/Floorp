/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProfilerThreadRegistrationData.h"

#include "mozilla/ProfilerMarkers.h"
#include "js/AllocationRecording.h"
#include "js/ProfilingStack.h"
#include "js/TraceLoggerAPI.h"

#if defined(XP_WIN)
#  include <windows.h>
#elif defined(XP_DARWIN)
#  include <pthread.h>
#endif

namespace mozilla::profiler {

ThreadRegistrationData::ThreadRegistrationData(const char* aName,
                                               const void* aStackTop)
    : mInfo(aName),
      mPlatformData(mInfo.ThreadId()),
      mStackTop(
#if defined(XP_WIN)
          // We don't have to guess on Windows.
          reinterpret_cast<const void*>(
              reinterpret_cast<PNT_TIB>(NtCurrentTeb())->StackBase)
#elif defined(XP_DARWIN)
          // We don't have to guess on Mac/Darwin.
          reinterpret_cast<const void*>(
              pthread_get_stackaddr_np(pthread_self()))
#else
          // Otherwise use the given guess.
          aStackTop
#endif
      ) {
}

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

void ThreadRegistrationLockedRWFromAnyThread::
    SetIsBeingProfiledWithProfiledThreadData(
        ProfiledThreadData* aProfiledThreadData, const PSAutoLock&) {
  MOZ_ASSERT(!mIsBeingProfiled);
  mIsBeingProfiled = true;

  MOZ_ASSERT(!mProfiledThreadData);
  MOZ_ASSERT(aProfiledThreadData);
  mProfiledThreadData = aProfiledThreadData;

  // Check invariants.
  MOZ_ASSERT(mIsBeingProfiled == !!mProfiledThreadData);
}

void ThreadRegistrationLockedRWFromAnyThread::
    ClearIsBeingProfiledAndProfiledThreadData(const PSAutoLock&) {
  mIsBeingProfiled = false;
  mProfiledThreadData = nullptr;
  // Check invariants.
  MOZ_ASSERT(mIsBeingProfiled == !!mProfiledThreadData);
}

void ThreadRegistrationLockedRWOnThread::SetJSContext(JSContext* aJSContext) {
  MOZ_ASSERT(aJSContext && !mJSContext);

  mJSContext = aJSContext;

  // We give the JS engine a non-owning reference to the ProfilingStack. It's
  // important that the JS engine doesn't touch this once the thread dies.
  js::SetContextProfilingStack(aJSContext, &ProfilingStackRef());
}

void ThreadRegistrationLockedRWOnThread::ClearJSContext() {
  mJSContext = nullptr;
}

void ThreadRegistrationLockedRWOnThread::PollJSSampling() {
  // We can't start/stop profiling until we have the thread's JSContext.
  if (mJSContext) {
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
      js::EnableContextProfilingStack(mJSContext, true);
      if (JSTracerEnabled()) {
        JS::StartTraceLogger(mJSContext);
      }
      if (JSAllocationsEnabled()) {
        // TODO - This probability should not be hardcoded. See Bug 1547284.
        JS::EnableRecordingAllocations(mJSContext,
                                       profiler_add_js_allocation_marker, 0.01);
      }
      js::RegisterContextProfilingEventMarker(mJSContext,
                                              profiler_add_js_marker);

    } else if (mJSSampling == INACTIVE_REQUESTED) {
      mJSSampling = INACTIVE;
      js::EnableContextProfilingStack(mJSContext, false);
      if (JSTracerEnabled()) {
        JS::StopTraceLogger(mJSContext);
      }
      if (JSAllocationsEnabled()) {
        JS::DisableRecordingAllocations(mJSContext);
      }
    }
  }
}

}  // namespace mozilla::profiler
