/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ThreadLocal.h"

// This variable is used to ensure creating new URI doesn't put us in an
// infinite loop
MOZ_THREAD_LOCAL(uint32_t) gTlsURLRecursionCount;

void InitThreadLocalVariables() {
  if (!gTlsURLRecursionCount.init()) {
    MOZ_CRASH("Could not init gTlsURLRecursionCount");
  }
  gTlsURLRecursionCount.set(0);
}
