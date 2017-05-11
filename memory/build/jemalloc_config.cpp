/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <mozilla/Assertions.h>

/* Provide an abort function for use in jemalloc code */
extern "C" void moz_abort() {
  MOZ_CRASH();
}
