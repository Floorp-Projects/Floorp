/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProfilerThreadRegistrationData.h"

#include "mozilla/FOGIPC.h"
#include "mozilla/glean/GleanMetrics.h"
#include "mozilla/ProfilerMarkers.h"
#include "js/AllocationRecording.h"
#include "js/ProfilingStack.h"

#if defined(XP_WIN)
#  include <windows.h>
#elif defined(XP_DARWIN)
#  include <pthread.h>
#endif

#ifdef NIGHTLY_BUILD
namespace geckoprofiler::markers {

using namespace mozilla;

struct ThreadCpuUseMarker {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("ThreadCpuUse");
  }
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   ProfilerThreadId aThreadId,
                                   int64_t aCpuTimeMs, int64_t aWakeUps,
                                   const ProfilerString8View& aThreadName) {
    aWriter.IntProperty("threadId", static_cast<int64_t>(aThreadId.ToNumber()));
    aWriter.IntProperty("time", aCpuTimeMs);
    aWriter.IntProperty("wakeups", aWakeUps);
    aWriter.StringProperty("label", aThreadName);
  }
  static MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable};
    schema.AddKeyLabelFormat("time", "CPU Time", MS::Format::Milliseconds);
    schema.AddKeyLabelFormat("wakeups", "Wake ups", MS::Format::Integer);
    schema.SetTooltipLabel("{marker.name} - {marker.data.label}");
    schema.SetTableLabel(
        "{marker.name} - {marker.data.label}: {marker.data.time} of CPU time, "
        "{marker.data.wakeups} wake ups");
    return schema;
  }
};

}  // namespace geckoprofiler::markers
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
  if (!profiler_thread_is_being_profiled_for_markers()) {
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
        aWriter.StringProperty("typeName", NS_ConvertUTF16toUTF8(aTypeName));
      }
      if (aDescriptiveTypeName.Length() != 0) {
        aWriter.StringProperty("descriptiveTypeName",
                               NS_ConvertUTF16toUTF8(aDescriptiveTypeName));
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

void ThreadRegistrationLockedRWFromAnyThread::SetProfilingFeaturesAndData(
    ThreadProfilingFeatures aProfilingFeatures,
    ProfiledThreadData* aProfiledThreadData, const PSAutoLock&) {
  MOZ_ASSERT(mProfilingFeatures == ThreadProfilingFeatures::NotProfiled);
  mProfilingFeatures = aProfilingFeatures;

  MOZ_ASSERT(!mProfiledThreadData);
  MOZ_ASSERT(aProfiledThreadData);
  mProfiledThreadData = aProfiledThreadData;

  if (mJSContext) {
    // The thread is now being profiled, and we already have a JSContext,
    // allocate a JsFramesBuffer to allow profiler-unlocked on-thread sampling.
    MOZ_ASSERT(!mJsFrameBuffer);
    mJsFrameBuffer = new JsFrame[MAX_JS_FRAMES];
  }

  // Check invariants.
  MOZ_ASSERT((mProfilingFeatures != ThreadProfilingFeatures::NotProfiled) ==
             !!mProfiledThreadData);
  MOZ_ASSERT((mJSContext &&
              (mProfilingFeatures != ThreadProfilingFeatures::NotProfiled)) ==
             !!mJsFrameBuffer);
}

void ThreadRegistrationLockedRWFromAnyThread::ClearProfilingFeaturesAndData(
    const PSAutoLock&) {
  mProfilingFeatures = ThreadProfilingFeatures::NotProfiled;
  mProfiledThreadData = nullptr;

  if (mJsFrameBuffer) {
    delete[] mJsFrameBuffer;
    mJsFrameBuffer = nullptr;
  }

  // Check invariants.
  MOZ_ASSERT((mProfilingFeatures != ThreadProfilingFeatures::NotProfiled) ==
             !!mProfiledThreadData);
  MOZ_ASSERT((mJSContext &&
              (mProfilingFeatures != ThreadProfilingFeatures::NotProfiled)) ==
             !!mJsFrameBuffer);
}

void ThreadRegistrationLockedRWOnThread::SetJSContext(JSContext* aJSContext) {
  MOZ_ASSERT(aJSContext && !mJSContext);

  mJSContext = aJSContext;

  if (mProfiledThreadData) {
    MOZ_ASSERT((mProfilingFeatures != ThreadProfilingFeatures::NotProfiled) ==
               !!mProfiledThreadData);
    // We now have a JSContext, and the thread is already being profiled,
    // allocate a JsFramesBuffer to allow profiler-unlocked on-thread sampling.
    MOZ_ASSERT(!mJsFrameBuffer);
    mJsFrameBuffer = new JsFrame[MAX_JS_FRAMES];
  }

  // We give the JS engine a non-owning reference to the ProfilingStack. It's
  // important that the JS engine doesn't touch this once the thread dies.
  js::SetContextProfilingStack(aJSContext, &ProfilingStackRef());

  // Check invariants.
  MOZ_ASSERT((mJSContext &&
              (mProfilingFeatures != ThreadProfilingFeatures::NotProfiled)) ==
             !!mJsFrameBuffer);
}

void ThreadRegistrationLockedRWOnThread::ClearJSContext() {
  mJSContext = nullptr;

  if (mJsFrameBuffer) {
    delete[] mJsFrameBuffer;
    mJsFrameBuffer = nullptr;
  }

  // Check invariants.
  MOZ_ASSERT((mJSContext &&
              (mProfilingFeatures != ThreadProfilingFeatures::NotProfiled)) ==
             !!mJsFrameBuffer);
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

      if (JSAllocationsEnabled()) {
        JS::DisableRecordingAllocations(mJSContext);
      }
    }
  }
}

#ifdef NIGHTLY_BUILD
void ThreadRegistrationUnlockedConstReaderAndAtomicRW::RecordWakeCount() const {
  baseprofiler::detail::BaseProfilerAutoLock lock(mRecordWakeCountMutex);

  uint64_t newWakeCount = mWakeCount - mAlreadyRecordedWakeCount;
  if (newWakeCount == 0 && mSleep != AWAKE) {
    // If no new wake-up was counted, and the thread is not marked awake,
    // we can be pretty sure there is no CPU activity to record.
    // Threads that are never annotated as asleep/awake (typically rust threads)
    // start as awake.
    return;
  }

  uint64_t cpuTimeNs;
  if (!GetCpuTimeSinceThreadStartInNs(&cpuTimeNs, PlatformDataCRef())) {
    cpuTimeNs = 0;
  }

  constexpr uint64_t NS_PER_MS = 1'000'000;
  uint64_t cpuTimeMs = cpuTimeNs / NS_PER_MS;

  uint64_t newCpuTimeMs = MOZ_LIKELY(cpuTimeMs > mAlreadyRecordedCpuTimeInMs)
                              ? cpuTimeMs - mAlreadyRecordedCpuTimeInMs
                              : 0;

  if (!newWakeCount && !newCpuTimeMs) {
    // Nothing to report, avoid computing the Glean friendly thread name.
    return;
  }

  nsAutoCString threadName(mInfo.Name());
  // Trim the trailing number of threads that are part of a thread pool.
  for (size_t length = threadName.Length(); length > 0; --length) {
    const char c = threadName.CharAt(length - 1);
    if ((c < '0' || c > '9') && c != '#' && c != ' ') {
      if (length != threadName.Length()) {
        threadName.SetLength(length);
      }
      break;
    }
  }

  mozilla::glean::RecordThreadCpuUse(threadName, newCpuTimeMs, newWakeCount);

  // The thread id is provided as part of the payload because this call is
  // inside a ThreadRegistration data function, which could be invoked with
  // the ThreadRegistry locked. We cannot call any function/option that could
  // attempt to lock the ThreadRegistry again, like MarkerThreadId.
  PROFILER_MARKER("Thread CPU use", OTHER, {}, ThreadCpuUseMarker,
                  mInfo.ThreadId(), newCpuTimeMs, newWakeCount, threadName);
  mAlreadyRecordedCpuTimeInMs = cpuTimeMs;
  mAlreadyRecordedWakeCount += newWakeCount;
}
#endif

}  // namespace mozilla::profiler
