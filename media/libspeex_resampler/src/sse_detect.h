/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SSE_DETECT
#define SSE_DETECT

#ifdef __cplusplus
extern "C" {
#endif

  int moz_has_sse2();
  int moz_has_sse();

#ifdef __cplusplus
}
#endif

#endif // SSE_DETECT
