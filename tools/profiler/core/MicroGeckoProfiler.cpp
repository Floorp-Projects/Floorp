/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoProfiler.h"

using namespace mozilla;

void uprofiler_register_thread(const char* name, void* stacktop) {
  profiler_register_thread(name, stacktop);
}

void uprofiler_unregister_thread() { profiler_unregister_thread(); }

