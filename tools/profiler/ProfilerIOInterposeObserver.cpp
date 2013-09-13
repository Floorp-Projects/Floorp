/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoProfiler.h"
#include "ProfilerIOInterposeObserver.h"

using namespace mozilla;

void ProfilerIOInterposeObserver::Observe(Observation& aObservation)
{
  // TODO: The profile might want to take notice of non-main-thread IO, as
  // well as noting what files or references causes the IO.
  if (NS_IsMainThread()) {
    const char* ops[] = {"none", "read", "write", "invalid", "fsync"};
    PROFILER_MARKER(ops[aObservation.ObservedOperation()]);
  }
}
