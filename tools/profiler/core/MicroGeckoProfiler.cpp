/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoProfiler.h"

using namespace mozilla;

void uprofiler_register_thread(const char* name, void* stacktop) {
#ifdef MOZ_GECKO_PROFILER
  profiler_register_thread(name, stacktop);
#endif  // MOZ_GECKO_PROFILER
}

void uprofiler_unregister_thread() {
#ifdef MOZ_GECKO_PROFILER
  profiler_unregister_thread();
#endif  // MOZ_GECKO_PROFILER
}

// The category string will be handled later in Bug 1715047
void uprofiler_simple_event_marker(const char* name, const char*, char phase) {
#ifdef MOZ_GECKO_PROFILER
  switch (phase) {
    case 'B':
      profiler_add_marker(ProfilerString8View::WrapNullTerminatedString(name),
                          geckoprofiler::category::MEDIA_RT,
                          {MarkerTiming::IntervalStart()});
      break;
    case 'E':
      profiler_add_marker(ProfilerString8View::WrapNullTerminatedString(name),
                          geckoprofiler::category::MEDIA_RT,
                          {MarkerTiming::IntervalEnd()});
      break;
    case 'I':
      profiler_add_marker(ProfilerString8View::WrapNullTerminatedString(name),
                          geckoprofiler::category::MEDIA_RT,
                          {MarkerTiming::InstantNow()});
      break;
    default:
      if (getenv("MOZ_LOG_UNKNOWN_TRACE_EVENT_PHASES")) {
        fprintf(stderr, "XXX UProfiler: phase not handled: '%c'\n", phase);
      }
      break;
  }
#endif  // MOZ_GECKO_PROFILER
}
