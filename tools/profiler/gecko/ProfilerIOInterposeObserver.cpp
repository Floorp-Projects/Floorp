/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerIOInterposeObserver.h"

#include "GeckoProfiler.h"
#include "ProfilerMarkerPayload.h"

using namespace mozilla;

static auto GetFilename(IOInterposeObserver::Observation& aObservation) {
  AUTO_PROFILER_STATS(IO_filename);
  constexpr size_t scExpectedMaxFilename = 512;
  nsAutoStringN<scExpectedMaxFilename> filename16;
  aObservation.Filename(filename16);
  nsAutoCStringN<scExpectedMaxFilename> filename8;
  if (!filename16.IsEmpty()) {
    CopyUTF16toUTF8(filename16, filename8);
  }
  return filename8;
}

static UniqueProfilerBacktrace GetBacktraceUnless(bool aPrevent) {
  if (aPrevent) {
    return nullptr;
  }
  AUTO_PROFILER_STATS(IO_backtrace);
  return profiler_get_backtrace();
}

void ProfilerIOInterposeObserver::Observe(Observation& aObservation) {
  if (profiler_is_locked_on_current_thread()) {
    // Don't observe I/Os originating from the profiler itself (when internally
    // locked) to avoid deadlocks when calling profiler functions.
    AUTO_PROFILER_STATS(IO_profiler_locked);
    return;
  }

  Maybe<uint32_t> maybeFeatures = profiler_features_if_active_and_unpaused();
  if (maybeFeatures.isNothing()) {
    return;
  }
  uint32_t features = *maybeFeatures;

  if (!profiler_can_accept_markers()) {
    return;
  }

  if (IsMainThread()) {
    // This is the main thread.
    // Capture a marker if any "IO" feature is on.
    // If it's not being profiled, we have nowhere to store FileIO markers.
    if (!profiler_thread_is_being_profiled() ||
        !(features & ProfilerFeature::MainThreadIO)) {
      return;
    }
    AUTO_PROFILER_STATS(IO_MT);
    nsAutoCString type{aObservation.FileType()};
    type.AppendLiteral("IO");
    PROFILER_ADD_MARKER_WITH_PAYLOAD(
        type.get(), OTHER, FileIOMarkerPayload,
        (aObservation.ObservedOperationString(), aObservation.Reference(),
         GetFilename(aObservation).get(), aObservation.Start(),
         aObservation.End(),
         GetBacktraceUnless(features & ProfilerFeature::NoIOStacks)));

  } else if (profiler_thread_is_being_profiled()) {
    // This is a non-main thread that is being profiled.
    if (!(features & ProfilerFeature::FileIO)) {
      return;
    }
    AUTO_PROFILER_STATS(IO_off_MT);
    FileIOMarkerPayload payload{
        aObservation.ObservedOperationString(),
        aObservation.Reference(),
        GetFilename(aObservation).get(),
        aObservation.Start(),
        aObservation.End(),
        GetBacktraceUnless(features & ProfilerFeature::NoIOStacks)};
    nsAutoCString type{aObservation.FileType()};
    type.AppendLiteral("IO");
    // Store the marker in the both:
    // - The current thread.
    profiler_add_marker(type.get(), JS::ProfilingCategoryPair::OTHER, payload);
    // - The main thread (with a distinct marker name and the thread id).
    payload.SetIOThreadId(profiler_current_thread_id());
    type.AppendLiteral(" (non-main thread)");
    profiler_add_marker_for_mainthread(JS::ProfilingCategoryPair::OTHER,
                                       type.get(), payload);

  } else {
    // This is a thread that is not being profiled. We still want to capture
    // file I/Os (to the main thread) if the "FileIOAll" feature is on.
    if (!(features & ProfilerFeature::FileIOAll)) {
      return;
    }
    AUTO_PROFILER_STATS(IO_other);
    nsAutoCString type{aObservation.FileType()};
    if (profiler_is_active_and_thread_is_registered()) {
      type.AppendLiteral("IO (non-profiled thread)");
    } else {
      type.AppendLiteral("IO (unregistered thread)");
    }
    profiler_add_marker_for_mainthread(
        JS::ProfilingCategoryPair::OTHER, type.get(),
        FileIOMarkerPayload(
            aObservation.ObservedOperationString(), aObservation.Reference(),
            GetFilename(aObservation).get(), aObservation.Start(),
            aObservation.End(),
            GetBacktraceUnless(features & ProfilerFeature::NoIOStacks),
            Some(profiler_current_thread_id())));
  }
}
