/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/* On Android NDK, rand is inlined function, but postproc needs rand symbol */
#if defined(__ANDROID__)
#define rand __rand
#include <stdlib.h>
#undef rand

extern int rand(void)
{
  return __rand();
}
#else
/* ISO C forbids an empty translation unit. */
int vp8_unused;
#endif
