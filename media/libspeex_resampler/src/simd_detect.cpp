/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "simd_detect.h"

#include "mozilla/SSE.h"
#include "mozilla/arm.h"

#ifdef _USE_SSE2
int moz_speex_have_double_simd() {
  return mozilla::supports_sse2() ? 1 : 0;
}
#endif

#ifdef _USE_SSE
int moz_speex_have_single_simd() {
  return mozilla::supports_sse() ? 1 : 0;
}
#endif

#ifdef _USE_NEON
int moz_speex_have_single_simd() {
  return mozilla::supports_neon() ? 1 : 0;
}
#endif
