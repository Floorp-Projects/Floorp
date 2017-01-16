/*
 *  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits.h>
#include <math.h>

#include "vp9/common/vp9_blockd.h"
#include "vp9/encoder/vp9_encoder.h"
#include "vp9/encoder/vp9_skin_detection.h"

#define MODEL_MODE 1

// Fixed-point skin color model parameters.
static const int skin_mean[5][2] = {
    {7463, 9614}, {6400, 10240}, {7040, 10240}, {8320, 9280}, {6800, 9614}};
static const int skin_inv_cov[4] = {4107, 1663, 1663, 2157};  // q16
static const int skin_threshold[6] = {1570636, 1400000, 800000, 800000, 800000,
    800000};  // q18

// Thresholds on luminance.
static const int y_low = 40;
static const int y_high = 220;

// Evaluates the Mahalanobis distance measure for the input CbCr values.
static int evaluate_skin_color_difference(int cb, int cr, int idx) {
  const int cb_q6 = cb << 6;
  const int cr_q6 = cr << 6;
  const int cb_diff_q12 =
      (cb_q6 - skin_mean[idx][0]) * (cb_q6 - skin_mean[idx][0]);
  const int cbcr_diff_q12 =
      (cb_q6 - skin_mean[idx][0]) * (cr_q6 - skin_mean[idx][1]);
  const int cr_diff_q12 =
      (cr_q6 - skin_mean[idx][1]) * (cr_q6 - skin_mean[idx][1]);
  const int cb_diff_q2 = (cb_diff_q12 + (1 << 9)) >> 10;
  const int cbcr_diff_q2 = (cbcr_diff_q12 + (1 << 9)) >> 10;
  const int cr_diff_q2 = (cr_diff_q12 + (1 << 9)) >> 10;
  const int skin_diff = skin_inv_cov[0] * cb_diff_q2 +
      skin_inv_cov[1] * cbcr_diff_q2 +
      skin_inv_cov[2] * cbcr_diff_q2 +
      skin_inv_cov[3] * cr_diff_q2;
  return skin_diff;
}

int vp9_skin_pixel(const uint8_t y, const uint8_t cb, const uint8_t cr,
                   int motion) {
  if (y < y_low || y > y_high) {
    return 0;
  } else {
    if (MODEL_MODE == 0) {
      return (evaluate_skin_color_difference(cb, cr, 0) < skin_threshold[0]);
    } else {
      int i = 0;
      // Exit on grey.
      if (cb == 128 && cr == 128)
        return 0;
      // Exit on very strong cb.
      if (cb > 150 && cr < 110)
        return 0;
      for (; i < 5; i++) {
        int skin_color_diff = evaluate_skin_color_difference(cb, cr, i);
        if (skin_color_diff < skin_threshold[i + 1]) {
           if (y < 60 && skin_color_diff > 3 * (skin_threshold[i + 1] >> 2))
             return 0;
           else if (motion == 0 &&
                    skin_color_diff > (skin_threshold[i + 1] >> 1))
             return 0;
           else
            return 1;
        }
        // Exit if difference is much large than the threshold.
        if (skin_color_diff > (skin_threshold[i + 1] << 3)) {
          return 0;
        }
      }
      return 0;
    }
  }
}

int vp9_compute_skin_block(const uint8_t *y, const uint8_t *u, const uint8_t *v,
                           int stride, int strideuv, int bsize,
                           int consec_zeromv, int curr_motion_magn) {
  // No skin if block has been zero/small motion for long consecutive time.
  if (consec_zeromv > 60 && curr_motion_magn == 0) {
    return 0;
  } else {
    int motion = 1;
    // Take center pixel in block to determine is_skin.
    const int y_width_shift = (4 << b_width_log2_lookup[bsize]) >> 1;
    const int y_height_shift = (4 << b_height_log2_lookup[bsize]) >> 1;
    const int uv_width_shift = y_width_shift >> 1;
    const int uv_height_shift = y_height_shift >> 1;
    const uint8_t ysource = y[y_height_shift * stride + y_width_shift];
    const uint8_t usource = u[uv_height_shift * strideuv + uv_width_shift];
    const uint8_t vsource = v[uv_height_shift * strideuv + uv_width_shift];
    if (consec_zeromv > 25 && curr_motion_magn == 0)
      motion = 0;
    return vp9_skin_pixel(ysource, usource, vsource, motion);
  }
}


#ifdef OUTPUT_YUV_SKINMAP
// For viewing skin map on input source.
void vp9_compute_skin_map(VP9_COMP *const cpi, FILE *yuv_skinmap_file) {
  int i, j, mi_row, mi_col, num_bl;
  VP9_COMMON *const cm = &cpi->common;
  uint8_t *y;
  const uint8_t *src_y = cpi->Source->y_buffer;
  const uint8_t *src_u = cpi->Source->u_buffer;
  const uint8_t *src_v = cpi->Source->v_buffer;
  const int src_ystride = cpi->Source->y_stride;
  const int src_uvstride = cpi->Source->uv_stride;
  int y_bsize = 16;  // Use 8x8 or 16x16.
  int uv_bsize = y_bsize >> 1;
  int ypos = y_bsize >> 1;
  int uvpos = uv_bsize >> 1;
  int shy = (y_bsize == 8) ? 3 : 4;
  int shuv = shy - 1;
  int fac = y_bsize / 8;
  // Use center pixel or average of center 2x2 pixels.
  int mode_filter = 0;
  YV12_BUFFER_CONFIG skinmap;
  memset(&skinmap, 0, sizeof(YV12_BUFFER_CONFIG));
  if (vpx_alloc_frame_buffer(&skinmap, cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
                               VP9_ENC_BORDER_IN_PIXELS, cm->byte_alignment)) {
      vpx_free_frame_buffer(&skinmap);
      return;
  }
  memset(skinmap.buffer_alloc, 128, skinmap.frame_size);
  y = skinmap.y_buffer;
  // Loop through blocks and set skin map based on center pixel of block.
  // Set y to white for skin block, otherwise set to source with gray scale.
  // Ignore rightmost/bottom boundary blocks.
  for (mi_row = 0; mi_row < cm->mi_rows - 1; mi_row += fac) {
    num_bl = 0;
    for (mi_col = 0; mi_col < cm->mi_cols - 1; mi_col += fac) {
      int is_skin = 0;
      if (mode_filter == 1) {
        // Use 2x2 average at center.
        uint8_t ysource = src_y[ypos * src_ystride + ypos];
        uint8_t usource = src_u[uvpos * src_uvstride + uvpos];
        uint8_t vsource = src_v[uvpos * src_uvstride + uvpos];
        uint8_t ysource2 = src_y[(ypos + 1) * src_ystride + ypos];
        uint8_t usource2 = src_u[(uvpos + 1) * src_uvstride + uvpos];
        uint8_t vsource2 = src_v[(uvpos + 1) * src_uvstride + uvpos];
        uint8_t ysource3 = src_y[ypos * src_ystride + (ypos + 1)];
        uint8_t usource3 = src_u[uvpos * src_uvstride + (uvpos  + 1)];
        uint8_t vsource3 = src_v[uvpos * src_uvstride + (uvpos +  1)];
        uint8_t ysource4 = src_y[(ypos + 1) * src_ystride + (ypos + 1)];
        uint8_t usource4 = src_u[(uvpos + 1) * src_uvstride + (uvpos  + 1)];
        uint8_t vsource4 = src_v[(uvpos + 1) * src_uvstride + (uvpos +  1)];
        ysource = (ysource + ysource2 + ysource3 + ysource4) >> 2;
        usource = (usource + usource2 + usource3 + usource4) >> 2;
        vsource = (vsource + vsource2 + vsource3 + vsource4) >> 2;
        is_skin = vp9_skin_pixel(ysource, usource, vsource, 1);
      } else {
        int block_size = BLOCK_8X8;
        int consec_zeromv = 0;
        int bl_index = mi_row * cm->mi_cols + mi_col;
        int bl_index1 = bl_index + 1;
        int bl_index2 = bl_index + cm->mi_cols;
        int bl_index3 = bl_index2 + 1;
        if (y_bsize == 8)
          consec_zeromv = cpi->consec_zero_mv[bl_index];
        else
          consec_zeromv = VPXMIN(cpi->consec_zero_mv[bl_index],
                                 VPXMIN(cpi->consec_zero_mv[bl_index1],
                                 VPXMIN(cpi->consec_zero_mv[bl_index2],
                                 cpi->consec_zero_mv[bl_index3])));
        if (y_bsize == 16)
          block_size = BLOCK_16X16;
        is_skin  = vp9_compute_skin_block(src_y, src_u, src_v, src_ystride,
                                          src_uvstride, block_size,
                                          consec_zeromv,
                                          0);
      }
      for (i = 0; i < y_bsize; i++) {
        for (j = 0; j < y_bsize; j++) {
          if (is_skin)
            y[i * src_ystride + j] = 255;
          else
            y[i * src_ystride + j] = src_y[i * src_ystride + j];
        }
      }
      num_bl++;
      y += y_bsize;
      src_y += y_bsize;
      src_u += uv_bsize;
      src_v += uv_bsize;
    }
    y += (src_ystride << shy) - (num_bl << shy);
    src_y += (src_ystride << shy) - (num_bl << shy);
    src_u += (src_uvstride << shuv) - (num_bl << shuv);
    src_v += (src_uvstride << shuv) - (num_bl << shuv);
  }
  vp9_write_yuv_frame_420(&skinmap, yuv_skinmap_file);
  vpx_free_frame_buffer(&skinmap);
}
#endif
