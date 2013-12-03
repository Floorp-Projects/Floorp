/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vpx_config.h"
#include "vpx_rtcd.h"

#if HAVE_MEDIA

void vp8_short_fdct8x4_armv6(short *input, short *output, int pitch)
{
    vp8_short_fdct4x4_armv6(input,   output,    pitch);
    vp8_short_fdct4x4_armv6(input + 4, output + 16, pitch);
}

#endif /* HAVE_MEDIA */
