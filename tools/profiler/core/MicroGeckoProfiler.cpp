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

void uprofiler_simple_event_marker(const char* name, char phase, int num_args,
                                   const char** arg_names,
                                   const unsigned char* arg_types,
                                   const unsigned long long* arg_values) {
  MOZ_CRASH("Not implemented");
}
