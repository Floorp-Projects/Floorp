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

#include "./av1_rtcd.h"
#include "av1/common/enums.h"
#include "av1/common/av1_txfm.h"
#include "av1/common/x86/av1_txfm1d_sse4.h"

static INLINE void int16_array_with_stride_to_int32_array_without_stride(
    const int16_t *input, int stride, int32_t *output, int txfm1d_size) {
  int r, c;
  for (r = 0; r < txfm1d_size; r++) {
    for (c = 0; c < txfm1d_size; c++) {
      output[r * txfm1d_size + c] = (int32_t)input[r * stride + c];
    }
  }
}

typedef void (*TxfmFuncSSE2)(const __m128i *input, __m128i *output,
                             const int8_t *cos_bit, const int8_t *stage_range);

static INLINE TxfmFuncSSE2 fwd_txfm_type_to_func(TXFM_TYPE txfm_type) {
  switch (txfm_type) {
    case TXFM_TYPE_DCT32: return av1_fdct32_new_sse4_1; break;
    case TXFM_TYPE_ADST32: return av1_fadst32_new_sse4_1; break;
    default: assert(0);
  }
  return NULL;
}

static INLINE void fwd_txfm2d_sse4_1(const int16_t *input, int32_t *output,
                                     const int stride,
                                     const TXFM_2D_FLIP_CFG *cfg,
                                     int32_t *txfm_buf) {
  // TODO(sarahparker) This does not currently support rectangular transforms
  // and will break without splitting txfm_size out into row and col size.
  // Rectangular transforms use c code only, so it should be ok for now.
  // It will be corrected when there are sse implementations for rectangular
  // transforms.
  assert(cfg->row_cfg->txfm_size == cfg->col_cfg->txfm_size);
  const int txfm_size = cfg->row_cfg->txfm_size;
  const int8_t *shift = cfg->row_cfg->shift;
  const int8_t *stage_range_col = cfg->col_cfg->stage_range;
  const int8_t *stage_range_row = cfg->row_cfg->stage_range;
  const int8_t *cos_bit_col = cfg->col_cfg->cos_bit;
  const int8_t *cos_bit_row = cfg->row_cfg->cos_bit;
  const TxfmFuncSSE2 txfm_func_col =
      fwd_txfm_type_to_func(cfg->col_cfg->txfm_type);
  const TxfmFuncSSE2 txfm_func_row =
      fwd_txfm_type_to_func(cfg->row_cfg->txfm_type);

  __m128i *buf_128 = (__m128i *)txfm_buf;
  __m128i *out_128 = (__m128i *)output;
  int num_per_128 = 4;
  int txfm2d_size_128 = txfm_size * txfm_size / num_per_128;

  int16_array_with_stride_to_int32_array_without_stride(input, stride, txfm_buf,
                                                        txfm_size);
  round_shift_array_32_sse4_1(buf_128, out_128, txfm2d_size_128, -shift[0]);
  txfm_func_col(out_128, buf_128, cos_bit_col, stage_range_col);
  round_shift_array_32_sse4_1(buf_128, out_128, txfm2d_size_128, -shift[1]);
  transpose_32(txfm_size, out_128, buf_128);
  txfm_func_row(buf_128, out_128, cos_bit_row, stage_range_row);
  round_shift_array_32_sse4_1(out_128, buf_128, txfm2d_size_128, -shift[2]);
  transpose_32(txfm_size, buf_128, out_128);
}

void av1_fwd_txfm2d_32x32_sse4_1(const int16_t *input, int32_t *output,
                                 int stride, int tx_type, int bd) {
  DECLARE_ALIGNED(16, int32_t, txfm_buf[1024]);
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(tx_type, TX_32X32);
  (void)bd;
  fwd_txfm2d_sse4_1(input, output, stride, &cfg, txfm_buf);
}

void av1_fwd_txfm2d_64x64_sse4_1(const int16_t *input, int32_t *output,
                                 int stride, int tx_type, int bd) {
  DECLARE_ALIGNED(16, int32_t, txfm_buf[4096]);
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_64x64_cfg(tx_type);
  (void)bd;
  fwd_txfm2d_sse4_1(input, output, stride, &cfg, txfm_buf);
}
