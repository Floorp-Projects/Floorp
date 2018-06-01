/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoProfiler.h"
#include "ProfilerIOInterposeObserver.h"
#include "ProfilerMarkerPayload.h"

using namespace mozilla;

void ProfilerIOInterposeObserver::Observe(Observation& aObservation)
{
  if (!IsMainThread()) {
    return;
  }

  UniqueProfilerBacktrace stack = profiler_get_backtrace();

  nsString filename;
  aObservation.Filename(filename);
  profiler_add_marker(
    aObservation.ObservedOperationString(),
    MakeUnique<IOMarkerPayload>(aObservation.Reference(),
                                NS_ConvertUTF16toUTF8(filename).get(),
                                aObservation.Start(), aObservation.End(),
                                std::move(stack)));
}
