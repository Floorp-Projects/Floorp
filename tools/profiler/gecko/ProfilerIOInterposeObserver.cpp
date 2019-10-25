/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerIOInterposeObserver.h"

#include "GeckoProfiler.h"
#include "ProfilerMarkerPayload.h"

using namespace mozilla;

void ProfilerIOInterposeObserver::Observe(Observation& aObservation) {
  // 1. Only the main thread observes I/Os.
  // 2. Don't observe if the main thread is not currently being profiled.
  // 3. Don't observe I/Os originating from the profiler itself (when
  //    internally locked) to avoid deadlocks when calling profiler functions.
  if (!IsMainThread() || !profiler_thread_is_being_profiled() ||
      profiler_is_locked_on_current_thread()) {
    return;
  }

  UniqueProfilerBacktrace stack = profiler_get_backtrace();

  nsString filename;
  aObservation.Filename(filename);
  PROFILER_ADD_MARKER_WITH_PAYLOAD(
      "FileIO", OTHER, FileIOMarkerPayload,
      (aObservation.ObservedOperationString(), aObservation.Reference(),
       NS_ConvertUTF16toUTF8(filename).get(), aObservation.Start(),
       aObservation.End(), std::move(stack)));
}
