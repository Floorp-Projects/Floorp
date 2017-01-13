/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_FILTER_H_
#define VP9_COMMON_VP9_FILTER_H_

#include "./vpx_config.h"
#include "vpx/vpx_integer.h"
#include "vpx_ports/mem.h"


#ifdef __cplusplus
extern "C" {
#endif

#define FILTER_BITS 7

#define SUBPEL_BITS 4
#define SUBPEL_MASK ((1 << SUBPEL_BITS) - 1)
#define SUBPEL_SHIFTS (1 << SUBPEL_BITS)
#define SUBPEL_TAPS 8

typedef enum {
  EIGHTTAP = 0,
  EIGHTTAP_SMOOTH = 1,
  EIGHTTAP_SHARP = 2,
  SWITCHABLE_FILTERS = 3, /* Number of switchable filters */
  BILINEAR = 3,
  // The codec can operate in four possible inter prediction filter mode:
  // 8-tap, 8-tap-smooth, 8-tap-sharp, and switching between the three.
  SWITCHABLE_FILTER_CONTEXTS = SWITCHABLE_FILTERS + 1,
  SWITCHABLE = 4  /* should be the last one */
} INTERP_FILTER;

typedef int16_t InterpKernel[SUBPEL_TAPS];

const InterpKernel *vp9_get_interp_kernel(INTERP_FILTER filter);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_COMMON_VP9_FILTER_H_
