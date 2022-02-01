/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Common code for the unified fuzzing interface
 */

#include <stdarg.h>
#include <stdlib.h>
#include "FuzzingInterface.h"

namespace mozilla {

#ifdef JS_STANDALONE
static bool fuzzing_verbose = !!getenv("MOZ_FUZZ_LOG");
void fuzzing_log(const char* aFmt, ...) {
  if (fuzzing_verbose) {
    va_list ap;
    va_start(ap, aFmt);
    vfprintf(stderr, aFmt, ap);
    va_end(ap);
  }
}
#else
LazyLogModule gFuzzingLog("nsFuzzing");
#endif

}  // namespace mozilla
