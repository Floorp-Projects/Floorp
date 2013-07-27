/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/SSE.h"
#include "sse_detect.h"

int moz_has_sse2() {
  return mozilla::supports_sse2() ? 1 : 0;
}

int moz_has_sse() {
  return mozilla::supports_sse() ? 1 : 0;
}
