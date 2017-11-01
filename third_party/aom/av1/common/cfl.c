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

#include "av1/common/cfl.h"
#include "av1/common/common_data.h"
#include "av1/common/onyxc_int.h"

void cfl_init(CFL_CTX *cfl, AV1_COMMON *cm) {
  if (!((cm->subsampling_x == 0 && cm->subsampling_y == 0) ||
        (cm->subsampling_x == 1 && cm->subsampling_y == 1))) {
    aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                       "Only 4:4:4 and 4:2:0 are currently supported by CfL");
  }
  memset(&cfl->pred_buf_q3, 0, sizeof(cfl->pred_buf_q3));
  cfl->subsampling_x = cm->subsampling_x;
  cfl->subsampling_y = cm->subsampling_y;
  cfl->are_parameters_computed = 0;
  cfl->store_y = 0;
#if CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
  cfl_clear_sub8x8_val(cfl);
#endif  // CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
}

// Due to frame boundary issues, it is possible that the total area covered by
// chroma exceeds that of luma. When this happens, we fill the missing pixels by
// repeating the last columns and/or rows.
static INLINE void cfl_pad(CFL_CTX *cfl, int width, int height) {
  const int diff_width = width - cfl->buf_width;
  const int diff_height = height - cfl->buf_height;

  if (diff_width > 0) {
    const int min_height = height - diff_height;
    int16_t *pred_buf_q3 = cfl->pred_buf_q3 + (width - diff_width);
    for (int j = 0; j < min_height; j++) {
      const int last_pixel = pred_buf_q3[-1];
      for (int i = 0; i < diff_width; i++) {
        pred_buf_q3[i] = last_pixel;
      }
      pred_buf_q3 += MAX_SB_SIZE;
    }
    cfl->buf_width = width;
  }
  if (diff_height > 0) {
    int16_t *pred_buf_q3 =
        cfl->pred_buf_q3 + ((height - diff_height) * MAX_SB_SIZE);
    for (int j = 0; j < diff_height; j++) {
      const int16_t *last_row_q3 = pred_buf_q3 - MAX_SB_SIZE;
      for (int i = 0; i < width; i++) {
        pred_buf_q3[i] = last_row_q3[i];
      }
      pred_buf_q3 += MAX_SB_SIZE;
    }
    cfl->buf_height = height;
  }
}

static void sum_above_row_lbd(const uint8_t *above_u, const uint8_t *above_v,
                              int width, int *out_sum_u, int *out_sum_v) {
  int sum_u = 0;
  int sum_v = 0;
  for (int i = 0; i < width; i++) {
    sum_u += above_u[i];
    sum_v += above_v[i];
  }
  *out_sum_u += sum_u;
  *out_sum_v += sum_v;
}
#if CONFIG_HIGHBITDEPTH
static void sum_above_row_hbd(const uint16_t *above_u, const uint16_t *above_v,
                              int width, int *out_sum_u, int *out_sum_v) {
  int sum_u = 0;
  int sum_v = 0;
  for (int i = 0; i < width; i++) {
    sum_u += above_u[i];
    sum_v += above_v[i];
  }
  *out_sum_u += sum_u;
  *out_sum_v += sum_v;
}
#endif  // CONFIG_HIGHBITDEPTH

static void sum_above_row(const MACROBLOCKD *xd, int width, int *out_sum_u,
                          int *out_sum_v) {
  const struct macroblockd_plane *const pd_u = &xd->plane[AOM_PLANE_U];
  const struct macroblockd_plane *const pd_v = &xd->plane[AOM_PLANE_V];
#if CONFIG_HIGHBITDEPTH
  if (get_bitdepth_data_path_index(xd)) {
    const uint16_t *above_u_16 =
        CONVERT_TO_SHORTPTR(pd_u->dst.buf) - pd_u->dst.stride;
    const uint16_t *above_v_16 =
        CONVERT_TO_SHORTPTR(pd_v->dst.buf) - pd_v->dst.stride;
    sum_above_row_hbd(above_u_16, above_v_16, width, out_sum_u, out_sum_v);
    return;
  }
#endif  // CONFIG_HIGHBITDEPTH
  const uint8_t *above_u = pd_u->dst.buf - pd_u->dst.stride;
  const uint8_t *above_v = pd_v->dst.buf - pd_v->dst.stride;
  sum_above_row_lbd(above_u, above_v, width, out_sum_u, out_sum_v);
}

static void sum_left_col_lbd(const uint8_t *left_u, int u_stride,
                             const uint8_t *left_v, int v_stride, int height,
                             int *out_sum_u, int *out_sum_v) {
  int sum_u = 0;
  int sum_v = 0;
  for (int i = 0; i < height; i++) {
    sum_u += left_u[i * u_stride];
    sum_v += left_v[i * v_stride];
  }
  *out_sum_u += sum_u;
  *out_sum_v += sum_v;
}
#if CONFIG_HIGHBITDEPTH
static void sum_left_col_hbd(const uint16_t *left_u, int u_stride,
                             const uint16_t *left_v, int v_stride, int height,
                             int *out_sum_u, int *out_sum_v) {
  int sum_u = 0;
  int sum_v = 0;
  for (int i = 0; i < height; i++) {
    sum_u += left_u[i * u_stride];
    sum_v += left_v[i * v_stride];
  }
  *out_sum_u += sum_u;
  *out_sum_v += sum_v;
}
#endif  // CONFIG_HIGHBITDEPTH
static void sum_left_col(const MACROBLOCKD *xd, int height, int *out_sum_u,
                         int *out_sum_v) {
  const struct macroblockd_plane *const pd_u = &xd->plane[AOM_PLANE_U];
  const struct macroblockd_plane *const pd_v = &xd->plane[AOM_PLANE_V];

#if CONFIG_HIGHBITDEPTH
  if (get_bitdepth_data_path_index(xd)) {
    const uint16_t *left_u_16 = CONVERT_TO_SHORTPTR(pd_u->dst.buf) - 1;
    const uint16_t *left_v_16 = CONVERT_TO_SHORTPTR(pd_v->dst.buf) - 1;
    sum_left_col_hbd(left_u_16, pd_u->dst.stride, left_v_16, pd_v->dst.stride,
                     height, out_sum_u, out_sum_v);
    return;
  }
#endif  // CONFIG_HIGHBITDEPTH
  const uint8_t *left_u = pd_u->dst.buf - 1;
  const uint8_t *left_v = pd_v->dst.buf - 1;
  sum_left_col_lbd(left_u, pd_u->dst.stride, left_v, pd_v->dst.stride, height,
                   out_sum_u, out_sum_v);
}

// CfL computes its own block-level DC_PRED. This is required to compute both
// alpha_cb and alpha_cr before the prediction are computed.
static void cfl_dc_pred(MACROBLOCKD *xd, BLOCK_SIZE plane_bsize) {
  CFL_CTX *const cfl = xd->cfl;

  // Compute DC_PRED until block boundary. We can't assume the neighbor will use
  // the same transform size.
  const int width = max_block_wide(xd, plane_bsize, AOM_PLANE_U)
                    << tx_size_wide_log2[0];
  const int height = max_block_high(xd, plane_bsize, AOM_PLANE_U)
                     << tx_size_high_log2[0];
  // Number of pixel on the top and left borders.
  const int num_pel = width + height;

  int sum_u = 0;
  int sum_v = 0;

// Match behavior of build_intra_predictors_high (reconintra.c) at superblock
// boundaries:
// base-1 base-1 base-1 .. base-1 base-1 base-1 base-1 base-1 base-1
// base+1   A      B  ..     Y      Z
// base+1   C      D  ..     W      X
// base+1   E      F  ..     U      V
// base+1   G      H  ..     S      T      T      T      T      T
// ..

#if CONFIG_CHROMA_SUB8X8
  if (xd->chroma_up_available && xd->mb_to_right_edge >= 0) {
#else
  if (xd->up_available && xd->mb_to_right_edge >= 0) {
#endif
    sum_above_row(xd, width, &sum_u, &sum_v);
  } else {
    const int base = 128 << (xd->bd - 8);
    sum_u = width * (base - 1);
    sum_v = width * (base - 1);
  }

#if CONFIG_CHROMA_SUB8X8
  if (xd->chroma_left_available && xd->mb_to_bottom_edge >= 0) {
#else
  if (xd->left_available && xd->mb_to_bottom_edge >= 0) {
#endif
    sum_left_col(xd, height, &sum_u, &sum_v);
  } else {
    const int base = 128 << (xd->bd - 8);
    sum_u += height * (base + 1);
    sum_v += height * (base + 1);
  }

  // TODO(ltrudeau) Because of max_block_wide and max_block_high, num_pel will
  // not be a power of two. So these divisions will have to use a lookup table.
  cfl->dc_pred[CFL_PRED_U] = (sum_u + (num_pel >> 1)) / num_pel;
  cfl->dc_pred[CFL_PRED_V] = (sum_v + (num_pel >> 1)) / num_pel;
}

static void cfl_subtract_averages(CFL_CTX *cfl, TX_SIZE tx_size) {
  const int width = cfl->uv_width;
  const int height = cfl->uv_height;
  const int tx_height = tx_size_high[tx_size];
  const int tx_width = tx_size_wide[tx_size];
  const int block_row_stride = MAX_SB_SIZE << tx_size_high_log2[tx_size];
  const int num_pel_log2 =
      (tx_size_high_log2[tx_size] + tx_size_wide_log2[tx_size]);

  int16_t *pred_buf_q3 = cfl->pred_buf_q3;

  cfl_pad(cfl, width, height);

  for (int b_j = 0; b_j < height; b_j += tx_height) {
    for (int b_i = 0; b_i < width; b_i += tx_width) {
      int sum_q3 = 0;
      int16_t *tx_pred_buf_q3 = pred_buf_q3;
      for (int t_j = 0; t_j < tx_height; t_j++) {
        for (int t_i = b_i; t_i < b_i + tx_width; t_i++) {
          sum_q3 += tx_pred_buf_q3[t_i];
        }
        tx_pred_buf_q3 += MAX_SB_SIZE;
      }
      int avg_q3 = (sum_q3 + (1 << (num_pel_log2 - 1))) >> num_pel_log2;
      // Loss is never more than 1/2 (in Q3)
      assert(fabs((double)avg_q3 - (sum_q3 / ((double)(1 << num_pel_log2)))) <=
             0.5);

      tx_pred_buf_q3 = pred_buf_q3;
      for (int t_j = 0; t_j < tx_height; t_j++) {
        for (int t_i = b_i; t_i < b_i + tx_width; t_i++) {
          tx_pred_buf_q3[t_i] -= avg_q3;
        }

        tx_pred_buf_q3 += MAX_SB_SIZE;
      }
    }
    pred_buf_q3 += block_row_stride;
  }
}

static INLINE int cfl_idx_to_alpha(int alpha_idx, int joint_sign,
                                   CFL_PRED_TYPE pred_type) {
  const int alpha_sign = (pred_type == CFL_PRED_U) ? CFL_SIGN_U(joint_sign)
                                                   : CFL_SIGN_V(joint_sign);
  if (alpha_sign == CFL_SIGN_ZERO) return 0;
  const int abs_alpha_q3 =
      (pred_type == CFL_PRED_U) ? CFL_IDX_U(alpha_idx) : CFL_IDX_V(alpha_idx);
  return (alpha_sign == CFL_SIGN_POS) ? abs_alpha_q3 + 1 : -abs_alpha_q3 - 1;
}

static void cfl_build_prediction_lbd(const int16_t *pred_buf_q3, uint8_t *dst,
                                     int dst_stride, int width, int height,
                                     int alpha_q3, int dc_pred) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      dst[i] =
          clip_pixel(get_scaled_luma_q0(alpha_q3, pred_buf_q3[i]) + dc_pred);
    }
    dst += dst_stride;
    pred_buf_q3 += MAX_SB_SIZE;
  }
}

#if CONFIG_HIGHBITDEPTH
static void cfl_build_prediction_hbd(const int16_t *pred_buf_q3, uint16_t *dst,
                                     int dst_stride, int width, int height,
                                     int alpha_q3, int dc_pred, int bit_depth) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      dst[i] = clip_pixel_highbd(
          get_scaled_luma_q0(alpha_q3, pred_buf_q3[i]) + dc_pred, bit_depth);
    }
    dst += dst_stride;
    pred_buf_q3 += MAX_SB_SIZE;
  }
}
#endif  // CONFIG_HIGHBITDEPTH

static void cfl_build_prediction(const int16_t *pred_buf_q3, uint8_t *dst,
                                 int dst_stride, int width, int height,
                                 int alpha_q3, int dc_pred, int use_hbd,
                                 int bit_depth) {
#if CONFIG_HIGHBITDEPTH
  if (use_hbd) {
    uint16_t *dst_16 = CONVERT_TO_SHORTPTR(dst);
    cfl_build_prediction_hbd(pred_buf_q3, dst_16, dst_stride, width, height,
                             alpha_q3, dc_pred, bit_depth);
    return;
  }
#endif  // CONFIG_HIGHBITDEPTH
  (void)use_hbd;
  (void)bit_depth;
  cfl_build_prediction_lbd(pred_buf_q3, dst, dst_stride, width, height,
                           alpha_q3, dc_pred);
}

void cfl_predict_block(MACROBLOCKD *const xd, uint8_t *dst, int dst_stride,
                       int row, int col, TX_SIZE tx_size, int plane) {
  CFL_CTX *const cfl = xd->cfl;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;

  // CfL parameters must be computed before prediction can be done.
  assert(cfl->are_parameters_computed == 1);

  const int16_t *pred_buf_q3 =
      cfl->pred_buf_q3 + ((row * MAX_SB_SIZE + col) << tx_size_wide_log2[0]);
  const int alpha_q3 =
      cfl_idx_to_alpha(mbmi->cfl_alpha_idx, mbmi->cfl_alpha_signs, plane - 1);

  cfl_build_prediction(pred_buf_q3, dst, dst_stride, tx_size_wide[tx_size],
                       tx_size_high[tx_size], alpha_q3, cfl->dc_pred[plane - 1],
                       get_bitdepth_data_path_index(xd), xd->bd);
}

static void cfl_luma_subsampling_420_lbd(const uint8_t *input, int input_stride,
                                         int16_t *output_q3, int width,
                                         int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      int top = i << 1;
      int bot = top + input_stride;
      output_q3[i] = (input[top] + input[top + 1] + input[bot] + input[bot + 1])
                     << 1;
    }
    input += input_stride << 1;
    output_q3 += MAX_SB_SIZE;
  }
}

static void cfl_luma_subsampling_444_lbd(const uint8_t *input, int input_stride,
                                         int16_t *output_q3, int width,
                                         int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      output_q3[i] = input[i] << 3;
    }
    input += input_stride;
    output_q3 += MAX_SB_SIZE;
  }
}

#if CONFIG_HIGHBITDEPTH
static void cfl_luma_subsampling_420_hbd(const uint16_t *input,
                                         int input_stride, int16_t *output_q3,
                                         int width, int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      int top = i << 1;
      int bot = top + input_stride;
      output_q3[i] = (input[top] + input[top + 1] + input[bot] + input[bot + 1])
                     << 1;
    }
    input += input_stride << 1;
    output_q3 += MAX_SB_SIZE;
  }
}

static void cfl_luma_subsampling_444_hbd(const uint16_t *input,
                                         int input_stride, int16_t *output_q3,
                                         int width, int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      output_q3[i] = input[i] << 3;
    }
    input += input_stride;
    output_q3 += MAX_SB_SIZE;
  }
}
#endif  // CONFIG_HIGHBITDEPTH

static void cfl_luma_subsampling_420(const uint8_t *input, int input_stride,
                                     int16_t *output_q3, int width, int height,
                                     int use_hbd) {
#if CONFIG_HIGHBITDEPTH
  if (use_hbd) {
    const uint16_t *input_16 = CONVERT_TO_SHORTPTR(input);
    cfl_luma_subsampling_420_hbd(input_16, input_stride, output_q3, width,
                                 height);
    return;
  }
#endif  // CONFIG_HIGHBITDEPTH
  (void)use_hbd;
  cfl_luma_subsampling_420_lbd(input, input_stride, output_q3, width, height);
}

static void cfl_luma_subsampling_444(const uint8_t *input, int input_stride,
                                     int16_t *output_q3, int width, int height,
                                     int use_hbd) {
#if CONFIG_HIGHBITDEPTH
  if (use_hbd) {
    uint16_t *input_16 = CONVERT_TO_SHORTPTR(input);
    cfl_luma_subsampling_444_hbd(input_16, input_stride, output_q3, width,
                                 height);
    return;
  }
#endif  // CONFIG_HIGHBITDEPTH
  (void)use_hbd;
  cfl_luma_subsampling_444_lbd(input, input_stride, output_q3, width, height);
}

static INLINE void cfl_store(CFL_CTX *cfl, const uint8_t *input,
                             int input_stride, int row, int col, int width,
                             int height, int use_hbd) {
  const int tx_off_log2 = tx_size_wide_log2[0];
  const int sub_x = cfl->subsampling_x;
  const int sub_y = cfl->subsampling_y;
  const int store_row = row << (tx_off_log2 - sub_y);
  const int store_col = col << (tx_off_log2 - sub_x);
  const int store_height = height >> sub_y;
  const int store_width = width >> sub_x;

  // Invalidate current parameters
  cfl->are_parameters_computed = 0;

  // Store the surface of the pixel buffer that was written to, this way we
  // can manage chroma overrun (e.g. when the chroma surfaces goes beyond the
  // frame boundary)
  if (col == 0 && row == 0) {
    cfl->buf_width = store_width;
    cfl->buf_height = store_height;
  } else {
    cfl->buf_width = OD_MAXI(store_col + store_width, cfl->buf_width);
    cfl->buf_height = OD_MAXI(store_row + store_height, cfl->buf_height);
  }

  // Check that we will remain inside the pixel buffer.
  assert(store_row + store_height <= MAX_SB_SIZE);
  assert(store_col + store_width <= MAX_SB_SIZE);

  // Store the input into the CfL pixel buffer
  int16_t *pred_buf_q3 =
      cfl->pred_buf_q3 + (store_row * MAX_SB_SIZE + store_col);

  if (sub_y == 0 && sub_x == 0) {
    cfl_luma_subsampling_444(input, input_stride, pred_buf_q3, store_width,
                             store_height, use_hbd);
  } else if (sub_y == 1 && sub_x == 1) {
    cfl_luma_subsampling_420(input, input_stride, pred_buf_q3, store_width,
                             store_height, use_hbd);
  } else {
    // TODO(ltrudeau) add support for 4:2:2
    assert(0);  // Unsupported chroma subsampling
  }
}

#if CONFIG_CHROMA_SUB8X8
// Adjust the row and column of blocks smaller than 8X8, as chroma-referenced
// and non-chroma-referenced blocks are stored together in the CfL buffer.
static INLINE void sub8x8_adjust_offset(const CFL_CTX *cfl, int *row_out,
                                        int *col_out) {
  // Increment row index for bottom: 8x4, 16x4 or both bottom 4x4s.
  if ((cfl->mi_row & 0x01) && cfl->subsampling_y) {
    assert(*row_out == 0);
    (*row_out)++;
  }

  // Increment col index for right: 4x8, 4x16 or both right 4x4s.
  if ((cfl->mi_col & 0x01) && cfl->subsampling_x) {
    assert(*col_out == 0);
    (*col_out)++;
  }
}
#if CONFIG_DEBUG
static INLINE void sub8x8_set_val(CFL_CTX *cfl, int row, int col, int val_high,
                                  int val_wide) {
  for (int val_r = 0; val_r < val_high; val_r++) {
    assert(row + val_r < CFL_SUB8X8_VAL_MI_SIZE);
    int row_off = (row + val_r) * CFL_SUB8X8_VAL_MI_SIZE;
    for (int val_c = 0; val_c < val_wide; val_c++) {
      assert(col + val_c < CFL_SUB8X8_VAL_MI_SIZE);
      assert(cfl->sub8x8_val[row_off + col + val_c] == 0);
      cfl->sub8x8_val[row_off + col + val_c]++;
    }
  }
}
#endif  // CONFIG_DEBUG
#endif  // CONFIG_CHROMA_SUB8X8

void cfl_store_tx(MACROBLOCKD *const xd, int row, int col, TX_SIZE tx_size,
                  BLOCK_SIZE bsize) {
  CFL_CTX *const cfl = xd->cfl;
  struct macroblockd_plane *const pd = &xd->plane[AOM_PLANE_Y];
  uint8_t *dst =
      &pd->dst.buf[(row * pd->dst.stride + col) << tx_size_wide_log2[0]];
  (void)bsize;
#if CONFIG_CHROMA_SUB8X8

  if (block_size_high[bsize] == 4 || block_size_wide[bsize] == 4) {
    // Only dimensions of size 4 can have an odd offset.
    assert(!((col & 1) && tx_size_wide[tx_size] != 4));
    assert(!((row & 1) && tx_size_high[tx_size] != 4));
    sub8x8_adjust_offset(cfl, &row, &col);
#if CONFIG_DEBUG
    sub8x8_set_val(cfl, row, col, tx_size_high_unit[tx_size],
                   tx_size_wide_unit[tx_size]);
#endif  // CONFIG_DEBUG
  }
#endif
  cfl_store(cfl, dst, pd->dst.stride, row, col, tx_size_wide[tx_size],
            tx_size_high[tx_size], get_bitdepth_data_path_index(xd));
}

void cfl_store_block(MACROBLOCKD *const xd, BLOCK_SIZE bsize, TX_SIZE tx_size) {
  CFL_CTX *const cfl = xd->cfl;
  struct macroblockd_plane *const pd = &xd->plane[AOM_PLANE_Y];
  int row = 0;
  int col = 0;
#if CONFIG_CHROMA_SUB8X8
  bsize = AOMMAX(BLOCK_4X4, bsize);
  if (block_size_high[bsize] == 4 || block_size_wide[bsize] == 4) {
    sub8x8_adjust_offset(cfl, &row, &col);
#if CONFIG_DEBUG
    sub8x8_set_val(cfl, row, col, mi_size_high[bsize], mi_size_wide[bsize]);
#endif  // CONFIG_DEBUG
  }
#endif  // CONFIG_CHROMA_SUB8X8
  const int width = max_intra_block_width(xd, bsize, AOM_PLANE_Y, tx_size);
  const int height = max_intra_block_height(xd, bsize, AOM_PLANE_Y, tx_size);
  cfl_store(cfl, pd->dst.buf, pd->dst.stride, row, col, width, height,
            get_bitdepth_data_path_index(xd));
}

void cfl_compute_parameters(MACROBLOCKD *const xd, TX_SIZE tx_size) {
  CFL_CTX *const cfl = xd->cfl;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;

  // Do not call cfl_compute_parameters multiple time on the same values.
  assert(cfl->are_parameters_computed == 0);

#if CONFIG_CHROMA_SUB8X8
  const BLOCK_SIZE plane_bsize = AOMMAX(
      BLOCK_4X4, get_plane_block_size(mbmi->sb_type, &xd->plane[AOM_PLANE_U]));
#if CONFIG_DEBUG
  if (mbmi->sb_type < BLOCK_8X8) {
    for (int val_r = 0; val_r < mi_size_high[mbmi->sb_type]; val_r++) {
      for (int val_c = 0; val_c < mi_size_wide[mbmi->sb_type]; val_c++) {
        assert(cfl->sub8x8_val[val_r * CFL_SUB8X8_VAL_MI_SIZE + val_c] == 1);
      }
    }
    cfl_clear_sub8x8_val(cfl);
  }
#endif  // CONFIG_DEBUG
#else
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(mbmi->sb_type, &xd->plane[AOM_PLANE_U]);
#endif
  // AOM_PLANE_U is used, but both planes will have the same sizes.
  cfl->uv_width = max_intra_block_width(xd, plane_bsize, AOM_PLANE_U, tx_size);
  cfl->uv_height =
      max_intra_block_height(xd, plane_bsize, AOM_PLANE_U, tx_size);

  assert(cfl->buf_width <= cfl->uv_width);
  assert(cfl->buf_height <= cfl->uv_height);

  cfl_dc_pred(xd, plane_bsize);
  cfl_subtract_averages(cfl, tx_size);
  cfl->are_parameters_computed = 1;
}
