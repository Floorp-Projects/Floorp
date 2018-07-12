/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>

#include "./aom_dsp_rtcd.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem.h"

void aom_convolve8_neon(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                        ptrdiff_t dst_stride, const int16_t *filter_x,
                        int x_step_q4, const int16_t *filter_y, int y_step_q4,
                        int w, int h) {
  /* Given our constraints: w <= 64, h <= 64, taps == 8 we can reduce the
   * maximum buffer size to 64 * 64 + 7 (+ 1 to make it divisible by 4).
   */
  DECLARE_ALIGNED(8, uint8_t, temp[64 * 72]);

  // Account for the vertical phase needing 3 lines prior and 4 lines post
  int intermediate_height = h + 7;

  assert(y_step_q4 == 16);
  assert(x_step_q4 == 16);

  /* Filter starting 3 lines back. The neon implementation will ignore the
   * given height and filter a multiple of 4 lines. Since this goes in to
   * the temp buffer which has lots of extra room and is subsequently discarded
   * this is safe if somewhat less than ideal.
   */
  aom_convolve8_horiz_neon(src - src_stride * 3, src_stride, temp, 64, filter_x,
                           x_step_q4, filter_y, y_step_q4, w,
                           intermediate_height);

  /* Step into the temp buffer 3 lines to get the actual frame data */
  aom_convolve8_vert_neon(temp + 64 * 3, 64, dst, dst_stride, filter_x,
                          x_step_q4, filter_y, y_step_q4, w, h);
}

void aom_convolve8_avg_neon(const uint8_t *src, ptrdiff_t src_stride,
                            uint8_t *dst, ptrdiff_t dst_stride,
                            const int16_t *filter_x, int x_step_q4,
                            const int16_t *filter_y, int y_step_q4, int w,
                            int h) {
  DECLARE_ALIGNED(8, uint8_t, temp[64 * 72]);
  int intermediate_height = h + 7;

  assert(y_step_q4 == 16);
  assert(x_step_q4 == 16);

  /* This implementation has the same issues as above. In addition, we only want
   * to average the values after both passes.
   */
  aom_convolve8_horiz_neon(src - src_stride * 3, src_stride, temp, 64, filter_x,
                           x_step_q4, filter_y, y_step_q4, w,
                           intermediate_height);
  aom_convolve8_avg_vert_neon(temp + 64 * 3, 64, dst, dst_stride, filter_x,
                              x_step_q4, filter_y, y_step_q4, w, h);
}
