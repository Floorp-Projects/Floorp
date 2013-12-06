/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_SADMXN_H_
#define VP9_COMMON_VP9_SADMXN_H_

#include "./vpx_config.h"
#include "vpx/vpx_integer.h"

static INLINE unsigned int sad_mx_n_c(const uint8_t *src_ptr,
                                      int src_stride,
                                      const uint8_t *ref_ptr,
                                      int ref_stride,
                                      int m,
                                      int n) {
  int r, c;
  unsigned int sad = 0;

  for (r = 0; r < n; r++) {
    for (c = 0; c < m; c++) {
      sad += abs(src_ptr[c] - ref_ptr[c]);
    }

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  }

  return sad;
}

#endif  // VP9_COMMON_VP9_SADMXN_H_
