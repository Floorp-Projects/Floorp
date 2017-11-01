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

#include <math.h>

#include "./aom_dsp_rtcd.h"
#include "./av1_rtcd.h"
#include "aom_dsp/inv_txfm.h"
#include "aom_ports/mem.h"
#include "av1/common/av1_inv_txfm1d_cfg.h"
#include "av1/common/blockd.h"
#include "av1/common/enums.h"
#include "av1/common/idct.h"
#if CONFIG_DAALA_DCT4 || CONFIG_DAALA_DCT8 || CONFIG_DAALA_DCT16 || \
    CONFIG_DAALA_DCT32 || CONFIG_DAALA_DCT64
#include "av1/common/daala_tx.h"
#endif

int av1_get_tx_scale(const TX_SIZE tx_size) {
  const int pels = tx_size_2d[tx_size];
  return (pels > 256) + (pels > 1024) + (pels > 4096);
}

// NOTE: The implementation of all inverses need to be aware of the fact
// that input and output could be the same buffer.

#if CONFIG_EXT_TX
static void iidtx4_c(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 4; ++i) {
    output[i] = (tran_low_t)dct_const_round_shift(input[i] * Sqrt2);
  }
}

static void iidtx8_c(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 8; ++i) {
    output[i] = input[i] * 2;
  }
}

static void iidtx16_c(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 16; ++i) {
    output[i] = (tran_low_t)dct_const_round_shift(input[i] * 2 * Sqrt2);
  }
}

static void iidtx32_c(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 32; ++i) {
    output[i] = input[i] * 4;
  }
}

#if CONFIG_TX64X64 && !CONFIG_DAALA_DCT64
static void iidtx64_c(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 64; ++i) {
    output[i] = (tran_low_t)dct_const_round_shift(input[i] * 4 * Sqrt2);
  }
}
#endif  // CONFIG_TX64X64
#endif  // CONFIG_EXT_TX

// For use in lieu of ADST
static void ihalfright32_c(const tran_low_t *input, tran_low_t *output) {
  int i;
  tran_low_t inputhalf[16];
  // Multiply input by sqrt(2)
  for (i = 0; i < 16; ++i) {
    inputhalf[i] = (tran_low_t)dct_const_round_shift(input[i] * Sqrt2);
  }
  for (i = 0; i < 16; ++i) {
    output[i] = input[16 + i] * 4;
  }
  aom_idct16_c(inputhalf, output + 16);
  // Note overall scaling factor is 4 times orthogonal
}

#if CONFIG_TX64X64 && !CONFIG_DAALA_DCT64
static void idct64_col_c(const tran_low_t *input, tran_low_t *output) {
  int32_t in[64], out[64];
  int i;
  for (i = 0; i < 64; ++i) in[i] = (int32_t)input[i];
  av1_idct64_new(in, out, inv_cos_bit_col_dct_64, inv_stage_range_col_dct_64);
  for (i = 0; i < 64; ++i) output[i] = (tran_low_t)out[i];
}

static void idct64_row_c(const tran_low_t *input, tran_low_t *output) {
  int32_t in[64], out[64];
  int i;
  for (i = 0; i < 64; ++i) in[i] = (int32_t)input[i];
  av1_idct64_new(in, out, inv_cos_bit_row_dct_64, inv_stage_range_row_dct_64);
  for (i = 0; i < 64; ++i) output[i] = (tran_low_t)out[i];
}

// For use in lieu of ADST
static void ihalfright64_c(const tran_low_t *input, tran_low_t *output) {
  int i;
  tran_low_t inputhalf[32];
  // Multiply input by sqrt(2)
  for (i = 0; i < 32; ++i) {
    inputhalf[i] = (tran_low_t)dct_const_round_shift(input[i] * Sqrt2);
  }
  for (i = 0; i < 32; ++i) {
    output[i] = (tran_low_t)dct_const_round_shift(input[32 + i] * 4 * Sqrt2);
  }
  aom_idct32_c(inputhalf, output + 32);
  // Note overall scaling factor is 4 * sqrt(2)  times orthogonal
}
#endif  // CONFIG_TX64X64

// Inverse identity transform and add.
#if CONFIG_EXT_TX
static void inv_idtx_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                           int bsx, int bsy, TX_TYPE tx_type) {
  int r, c;
  const int pels = bsx * bsy;
  const int shift = 3 - ((pels > 256) + (pels > 1024));
  if (tx_type == IDTX) {
    for (r = 0; r < bsy; ++r) {
      for (c = 0; c < bsx; ++c)
        dest[c] = clip_pixel_add(dest[c], input[c] >> shift);
      dest += stride;
      input += bsx;
    }
  }
}
#endif  // CONFIG_EXT_TX

#define FLIPUD_PTR(dest, stride, size)       \
  do {                                       \
    (dest) = (dest) + ((size)-1) * (stride); \
    (stride) = -(stride);                    \
  } while (0)

#if CONFIG_EXT_TX
static void maybe_flip_strides(uint8_t **dst, int *dstride, tran_low_t **src,
                               int *sstride, TX_TYPE tx_type, int sizey,
                               int sizex) {
  // Note that the transpose of src will be added to dst. In order to LR
  // flip the addends (in dst coordinates), we UD flip the src. To UD flip
  // the addends, we UD flip the dst.
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
    case IDTX:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST: break;
    case FLIPADST_DCT:
    case FLIPADST_ADST:
    case V_FLIPADST:
      // flip UD
      FLIPUD_PTR(*dst, *dstride, sizey);
      break;
    case DCT_FLIPADST:
    case ADST_FLIPADST:
    case H_FLIPADST:
      // flip LR
      FLIPUD_PTR(*src, *sstride, sizex);
      break;
    case FLIPADST_FLIPADST:
      // flip UD
      FLIPUD_PTR(*dst, *dstride, sizey);
      // flip LR
      FLIPUD_PTR(*src, *sstride, sizex);
      break;
    default: assert(0); break;
  }
}
#endif  // CONFIG_EXT_TX

#if CONFIG_HIGHBITDEPTH
#if CONFIG_EXT_TX && CONFIG_TX64X64
static void highbd_inv_idtx_add_c(const tran_low_t *input, uint8_t *dest8,
                                  int stride, int bsx, int bsy, TX_TYPE tx_type,
                                  int bd) {
  int r, c;
  const int pels = bsx * bsy;
  const int shift = 3 - ((pels > 256) + (pels > 1024));
  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  if (tx_type == IDTX) {
    for (r = 0; r < bsy; ++r) {
      for (c = 0; c < bsx; ++c)
        dest[c] = highbd_clip_pixel_add(dest[c], input[c] >> shift, bd);
      dest += stride;
      input += bsx;
    }
  }
}
#endif  // CONFIG_EXT_TX && CONFIG_TX64X64
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_LGT || CONFIG_LGT_FROM_PRED
void ilgt4(const tran_low_t *input, tran_low_t *output,
           const tran_high_t *lgtmtx) {
  if (!lgtmtx) assert(0);
#if CONFIG_LGT_FROM_PRED
  // For DCT/ADST, use butterfly implementations
  if (lgtmtx[0] == DCT4) {
    aom_idct4_c(input, output);
    return;
  } else if (lgtmtx[0] == ADST4) {
    aom_iadst4_c(input, output);
    return;
  }
#endif  // CONFIG_LGT_FROM_PRED

  // evaluate s[j] = sum of all lgtmtx[j]*input[i] over i=1,...,4
  tran_high_t s[4] = { 0 };
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) s[j] += lgtmtx[i * 4 + j] * input[i];

  for (int i = 0; i < 4; ++i) output[i] = WRAPLOW(dct_const_round_shift(s[i]));
}

void ilgt8(const tran_low_t *input, tran_low_t *output,
           const tran_high_t *lgtmtx) {
  if (!lgtmtx) assert(0);
#if CONFIG_LGT_FROM_PRED
  // For DCT/ADST, use butterfly implementations
  if (lgtmtx[0] == DCT8) {
    aom_idct8_c(input, output);
    return;
  } else if (lgtmtx[0] == ADST8) {
    aom_iadst8_c(input, output);
    return;
  }
#endif  // CONFIG_LGT_FROM_PRED

  // evaluate s[j] = sum of all lgtmtx[j]*input[i] over i=1,...,8
  tran_high_t s[8] = { 0 };
  for (int i = 0; i < 8; ++i)
    for (int j = 0; j < 8; ++j) s[j] += lgtmtx[i * 8 + j] * input[i];

  for (int i = 0; i < 8; ++i) output[i] = WRAPLOW(dct_const_round_shift(s[i]));
}
#endif  // CONFIG_LGT || CONFIG_LGT_FROM_PRED

#if CONFIG_LGT
// get_lgt4 and get_lgt8 return 1 and pick a lgt matrix if LGT is chosen to
// apply. Otherwise they return 0
int get_lgt4(const TxfmParam *txfm_param, int is_col,
             const tran_high_t **lgtmtx) {
  if (is_col && (vtx_tab[txfm_param->tx_type] == ADST_1D ||
                 vtx_tab[txfm_param->tx_type] == FLIPADST_1D)) {
    lgtmtx[0] = txfm_param->is_inter ? &lgt4_170[0][0] : &lgt4_140[0][0];
    return 1;
  } else if (!is_col && (htx_tab[txfm_param->tx_type] == ADST_1D ||
                         htx_tab[txfm_param->tx_type] == FLIPADST_1D)) {
    lgtmtx[0] = txfm_param->is_inter ? &lgt4_170[0][0] : &lgt4_140[0][0];
    return 1;
  }
  lgtmtx[0] = NULL;
  return 0;
}

int get_lgt8(const TxfmParam *txfm_param, int is_col,
             const tran_high_t **lgtmtx) {
  if (is_col && (vtx_tab[txfm_param->tx_type] == ADST_1D ||
                 vtx_tab[txfm_param->tx_type] == FLIPADST_1D)) {
    lgtmtx[0] = txfm_param->is_inter ? &lgt8_170[0][0] : &lgt8_150[0][0];
    return 1;
  } else if (!is_col && (htx_tab[txfm_param->tx_type] == ADST_1D ||
                         htx_tab[txfm_param->tx_type] == FLIPADST_1D)) {
    lgtmtx[0] = txfm_param->is_inter ? &lgt8_170[0][0] : &lgt8_150[0][0];
    return 1;
  }
  lgtmtx[0] = NULL;
  return 0;
}
#endif  // CONFIG_LGT

#if CONFIG_LGT_FROM_PRED
void ilgt16up(const tran_low_t *input, tran_low_t *output,
              const tran_high_t *lgtmtx) {
  if (lgtmtx[0] == DCT16) {
    aom_idct16_c(input, output);
    return;
  } else if (lgtmtx[0] == ADST16) {
    aom_iadst16_c(input, output);
    return;
  } else if (lgtmtx[0] == DCT32) {
    aom_idct32_c(input, output);
    return;
  } else if (lgtmtx[0] == ADST32) {
    ihalfright32_c(input, output);
    return;
  } else {
    assert(0);
  }
}

void get_discontinuity_1d(uint8_t *arr, int n, int *idx_max_diff) {
  *idx_max_diff = -1;

  int temp = 0, max_diff = 0, min_diff = INT_MAX;
  for (int i = 1; i < n; ++i) {
    temp = abs(arr[i] - arr[i - 1]);
    if (temp > max_diff) {
      max_diff = temp;
      *idx_max_diff = i;
    }
    if (temp < min_diff) min_diff = temp;
  }
}

void get_discontinuity_2d(uint8_t *dst, int stride, int n, int is_col,
                          int *idx_max_diff, int ntx) {
  *idx_max_diff = -1;

  int diff = 0, temp = 0, max_diff = 0, min_diff = INT_MAX;
  for (int i = 1; i < n; ++i) {
    temp = 0;
    for (int j = 0; j < ntx; ++j) {
      if (is_col)  // vertical diff
        diff = dst[i * stride + j] - dst[(i - 1) * stride + j];
      else  // horizontal diff
        diff = dst[j * stride + i] - dst[j * stride + i - 1];
      temp += diff * diff;
    }
    // temp/w is the i-th avg square diff
    if (temp > max_diff) {
      max_diff = temp;
      *idx_max_diff = i;
    }
    if (temp < min_diff) min_diff = temp;
  }
}

int idx_selfloop_wrt_mode(PREDICTION_MODE mode, int is_col) {
  // 0: no self-loop
  // 1: small self-loop
  // 2: medium self-loop
  // 3: large self-loop
  switch (mode) {
    case DC_PRED:
    case SMOOTH_PRED:
      // predition is good for both directions: large SLs for row and col
      return 3;
    case TM_PRED: return 0;
#if CONFIG_SMOOTH_HV
    case SMOOTH_H_PRED:
#endif
    case H_PRED:
      // prediction is good for H direction: large SL for row only
      return is_col ? 0 : 3;
#if CONFIG_SMOOTH_HV
    case SMOOTH_V_PRED:
#endif
    case V_PRED:
      // prediction is good for V direction: large SL for col only
      return is_col ? 3 : 0;
#if LGT_SL_INTRA
    // directional mode: choose SL based on the direction
    case D45_PRED: return is_col ? 2 : 0;
    case D63_PRED: return is_col ? 3 : 0;
    case D117_PRED: return is_col ? 3 : 1;
    case D135_PRED: return 2;
    case D153_PRED: return is_col ? 1 : 3;
    case D207_PRED: return is_col ? 0 : 3;
#else
    case D45_PRED:
    case D63_PRED:
    case D117_PRED: return is_col ? 3 : 0;
    case D135_PRED:
    case D153_PRED:
    case D207_PRED: return is_col ? 0 : 3;
#endif
    // inter: no SL
    default: return 0;
  }
}

void get_lgt4_from_pred(const TxfmParam *txfm_param, int is_col,
                        const tran_high_t **lgtmtx, int ntx) {
  PREDICTION_MODE mode = txfm_param->mode;
  int stride = txfm_param->stride;
  uint8_t *dst = txfm_param->dst;
  int bp = -1;
  uint8_t arr[4];

  // Each lgt4mtx_arr[k][i] corresponds to a line graph with a self-loop on
  // the first node, and possibly a weak edge within the line graph. i is
  // the index of the weak edge (between the i-th and (i+1)-th pixels, i=0
  // means no weak edge). k corresponds to the first self-loop's weight
  const tran_high_t *lgt4mtx_arr[4][4] = {
    { &lgt4_000[0][0], &lgt4_000w1[0][0], &lgt4_000w2[0][0],
      &lgt4_000w3[0][0] },
    { &lgt4_060[0][0], &lgt4_060_000w1[0][0], &lgt4_060_000w2[0][0],
      &lgt4_060_000w3[0][0] },
    { &lgt4_100[0][0], &lgt4_100_000w1[0][0], &lgt4_100_000w2[0][0],
      &lgt4_100_000w3[0][0] },
    { &lgt4_150[0][0], &lgt4_150_000w1[0][0], &lgt4_150_000w2[0][0],
      &lgt4_150_000w3[0][0] },
  };

  // initialize to DCT or some LGTs, and then change later if necessary
  int idx_sl = idx_selfloop_wrt_mode(mode, is_col);
  lgtmtx[0] = lgt4mtx_arr[idx_sl][0];

  // find the break point and replace the line graph by the one with a
  // break point
  if (mode == DC_PRED || mode == SMOOTH_PRED) {
    // Do not use break point, since 1) is_left_available and is_top_available
    // in DC_PRED are not known by txfm_param for now, so accessing
    // both boundaries anyway may cause a mismatch 2) DC prediciton
    // typically yields very smooth residues so having the break point
    // does not usually improve the RD result.
    return;
  } else if (mode == TM_PRED) {
    // TM_PRED: use both 1D top boundary and 1D left boundary
    if (is_col)
      for (int i = 0; i < 4; ++i) arr[i] = dst[i * stride];
    else
      for (int i = 0; i < 4; ++i) arr[i] = dst[i];
    get_discontinuity_1d(&arr[0], 4, &bp);
  } else if (mode == V_PRED) {
    // V_PRED: use 1D top boundary only
    if (is_col) return;
    for (int i = 0; i < 4; ++i) arr[i] = dst[i];
    get_discontinuity_1d(&arr[0], 4, &bp);
  } else if (mode == H_PRED) {
    // H_PRED: use 1D left boundary only
    if (!is_col) return;
    for (int i = 0; i < 4; ++i) arr[i] = dst[i * stride];
    get_discontinuity_1d(&arr[0], 4, &bp);
#if CONFIG_SMOOTH_HV
  } else if (mode == SMOOTH_V_PRED) {
    if (is_col) return;
    for (int i = 0; i < 4; ++i) arr[i] = dst[-stride + i];
    get_discontinuity_1d(&arr[0], 4, &bp);
  } else if (mode == SMOOTH_H_PRED) {
    if (!is_col) return;
    for (int i = 0; i < 4; ++i) arr[i] = dst[i * stride - 1];
    get_discontinuity_1d(&arr[0], 4, &bp);
#endif
  } else if (mode == D45_PRED || mode == D63_PRED || mode == D117_PRED) {
    // directional modes closer to vertical (maybe include D135 later)
    if (!is_col) get_discontinuity_2d(dst, stride, 4, 0, &bp, ntx);
  } else if (mode == D135_PRED || mode == D153_PRED || mode == D207_PRED) {
    // directional modes closer to horizontal
    if (is_col) get_discontinuity_2d(dst, stride, 4, 1, &bp, ntx);
  } else if (mode > TM_PRED) {
    // inter
    get_discontinuity_2d(dst, stride, 4, is_col, &bp, ntx);
  }

#if LGT_SL_INTRA
  if (bp != -1) lgtmtx[0] = lgt4mtx_arr[idx_sl][bp];
#else
  if (bp != -1) lgtmtx[0] = lgt4mtx_arr[0][bp];
#endif
}

void get_lgt8_from_pred(const TxfmParam *txfm_param, int is_col,
                        const tran_high_t **lgtmtx, int ntx) {
  PREDICTION_MODE mode = txfm_param->mode;
  int stride = txfm_param->stride;
  uint8_t *dst = txfm_param->dst;
  int bp = -1;
  uint8_t arr[8];

  const tran_high_t *lgt8mtx_arr[4][8] = {
    { &lgt8_000[0][0], &lgt8_000w1[0][0], &lgt8_000w2[0][0], &lgt8_000w3[0][0],
      &lgt8_000w4[0][0], &lgt8_000w5[0][0], &lgt8_000w6[0][0],
      &lgt8_000w7[0][0] },
    { &lgt8_060[0][0], &lgt8_060_000w1[0][0], &lgt8_060_000w2[0][0],
      &lgt8_060_000w3[0][0], &lgt8_060_000w4[0][0], &lgt8_060_000w5[0][0],
      &lgt8_060_000w6[0][0], &lgt8_060_000w7[0][0] },
    { &lgt8_100[0][0], &lgt8_100_000w1[0][0], &lgt8_100_000w2[0][0],
      &lgt8_100_000w3[0][0], &lgt8_100_000w4[0][0], &lgt8_100_000w5[0][0],
      &lgt8_100_000w6[0][0], &lgt8_100_000w7[0][0] },
    { &lgt8_150[0][0], &lgt8_150_000w1[0][0], &lgt8_150_000w2[0][0],
      &lgt8_150_000w3[0][0], &lgt8_150_000w4[0][0], &lgt8_150_000w5[0][0],
      &lgt8_150_000w6[0][0], &lgt8_150_000w7[0][0] },
  };

  int idx_sl = idx_selfloop_wrt_mode(mode, is_col);
  lgtmtx[0] = lgt8mtx_arr[idx_sl][0];

  if (mode == DC_PRED || mode == SMOOTH_PRED) {
    return;
  } else if (mode == TM_PRED) {
    if (is_col)
      for (int i = 0; i < 8; ++i) arr[i] = dst[i * stride];
    else
      for (int i = 0; i < 8; ++i) arr[i] = dst[i];
    get_discontinuity_1d(&arr[0], 8, &bp);
  } else if (mode == V_PRED) {
    if (is_col) return;
    for (int i = 0; i < 8; ++i) arr[i] = dst[i];
    get_discontinuity_1d(&arr[0], 8, &bp);
  } else if (mode == H_PRED) {
    if (!is_col) return;
    for (int i = 0; i < 8; ++i) arr[i] = dst[i * stride];
    get_discontinuity_1d(&arr[0], 8, &bp);
#if CONFIG_SMOOTH_HV
  } else if (mode == SMOOTH_V_PRED) {
    if (is_col) return;
    for (int i = 0; i < 8; ++i) arr[i] = dst[-stride + i];
    get_discontinuity_1d(&arr[0], 8, &bp);
  } else if (mode == SMOOTH_H_PRED) {
    if (!is_col) return;
    for (int i = 0; i < 8; ++i) arr[i] = dst[i * stride - 1];
    get_discontinuity_1d(&arr[0], 8, &bp);
#endif
  } else if (mode == D45_PRED || mode == D63_PRED || mode == D117_PRED) {
    if (!is_col) get_discontinuity_2d(dst, stride, 8, 0, &bp, ntx);
  } else if (mode == D135_PRED || mode == D153_PRED || mode == D207_PRED) {
    if (is_col) get_discontinuity_2d(dst, stride, 8, 1, &bp, ntx);
  } else if (mode > TM_PRED) {
    get_discontinuity_2d(dst, stride, 8, is_col, &bp, ntx);
  }

#if LGT_SL_INTRA
  if (bp != -1) lgtmtx[0] = lgt8mtx_arr[idx_sl][bp];
#else
  if (bp != -1) lgtmtx[0] = lgt8mtx_arr[0][bp];
#endif
}

// Since LGTs with length >8 are not implemented now, the following function
// will just call DCT or ADST
void get_lgt16up_from_pred(const TxfmParam *txfm_param, int is_col,
                           const tran_high_t **lgtmtx, int ntx) {
  int tx_length = is_col ? tx_size_high[txfm_param->tx_size]
                         : tx_size_wide[txfm_param->tx_size];
  assert(tx_length == 16 || tx_length == 32);
  PREDICTION_MODE mode = txfm_param->mode;

  (void)ntx;
  const tran_high_t *dctmtx =
      tx_length == 16 ? &lgt16_000[0][0] : &lgt32_000[0][0];
  const tran_high_t *adstmtx =
      tx_length == 16 ? &lgt16_200[0][0] : &lgt32_200[0][0];

  switch (mode) {
    case DC_PRED:
    case TM_PRED:
    case SMOOTH_PRED:
      // prediction from both top and left -> ADST
      lgtmtx[0] = adstmtx;
      break;
    case V_PRED:
    case D45_PRED:
    case D63_PRED:
    case D117_PRED:
#if CONFIG_SMOOTH_HV
    case SMOOTH_V_PRED:
#endif
      // prediction from the top more than from the left -> ADST
      lgtmtx[0] = is_col ? adstmtx : dctmtx;
      break;
    case H_PRED:
    case D135_PRED:
    case D153_PRED:
    case D207_PRED:
#if CONFIG_SMOOTH_HV
    case SMOOTH_H_PRED:
#endif
      // prediction from the left more than from the top -> DCT
      lgtmtx[0] = is_col ? dctmtx : adstmtx;
      break;
    default: lgtmtx[0] = dctmtx; break;
  }
}

typedef void (*IlgtFunc)(const tran_low_t *input, tran_low_t *output,
                         const tran_high_t *lgtmtx);

static IlgtFunc ilgt_func[4] = { ilgt4, ilgt8, ilgt16up, ilgt16up };

typedef void (*GetLgtFunc)(const TxfmParam *txfm_param, int is_col,
                           const tran_high_t **lgtmtx, int ntx);

static GetLgtFunc get_lgt_func[4] = { get_lgt4_from_pred, get_lgt8_from_pred,
                                      get_lgt16up_from_pred,
                                      get_lgt16up_from_pred };

// this inline function corresponds to the up scaling before the transpose
// operation in the av1_iht* functions
static INLINE tran_low_t inv_upscale_wrt_txsize(const tran_high_t val,
                                                const TX_SIZE tx_size) {
  switch (tx_size) {
    case TX_4X4:
    case TX_8X8:
    case TX_4X16:
    case TX_16X4:
    case TX_8X32:
    case TX_32X8: return (tran_low_t)val;
    case TX_4X8:
    case TX_8X4:
    case TX_8X16:
    case TX_16X8: return (tran_low_t)dct_const_round_shift(val * Sqrt2);
    default: assert(0); break;
  }
  return 0;
}

// This inline function corresponds to the bit shift before summing with the
// destination in the av1_iht* functions
static INLINE tran_low_t inv_downscale_wrt_txsize(const tran_low_t val,
                                                  const TX_SIZE tx_size) {
  switch (tx_size) {
    case TX_4X4: return ROUND_POWER_OF_TWO(val, 4);
    case TX_4X8:
    case TX_8X4:
    case TX_8X8:
    case TX_4X16:
    case TX_16X4: return ROUND_POWER_OF_TWO(val, 5);
    case TX_8X16:
    case TX_16X8:
    case TX_8X32:
    case TX_32X8: return ROUND_POWER_OF_TWO(val, 6);
    default: assert(0); break;
  }
  return 0;
}

void ilgt2d_from_pred_add(const tran_low_t *input, uint8_t *dest, int stride,
                          const TxfmParam *txfm_param) {
  const TX_SIZE tx_size = txfm_param->tx_size;
  const int w = tx_size_wide[tx_size];
  const int h = tx_size_high[tx_size];
  const int wlog2 = tx_size_wide_log2[tx_size];
  const int hlog2 = tx_size_high_log2[tx_size];
  assert(w <= 8 || h <= 8);

  int i, j;
  // largest 1D size allowed for LGT: 32
  // largest 2D size allowed for LGT: 8x32=256
  tran_low_t tmp[256], out[256], temp1d[32];
  const tran_high_t *lgtmtx_col[1];
  const tran_high_t *lgtmtx_row[1];
  get_lgt_func[hlog2 - 2](txfm_param, 1, lgtmtx_col, w);
  get_lgt_func[wlog2 - 2](txfm_param, 0, lgtmtx_row, h);

// for inverse transform, to be consistent with av1_iht functions, we always
// apply row transforms first and column transforms second, but both
// row-first and column-first versions are implemented here for future
// tests (use different lgtmtx_col[i], and choose row or column tx first
// depending on transforms).
#if 1
  // inverse column transforms
  for (i = 0; i < w; ++i) {
    // transpose
    for (j = 0; j < h; ++j) tmp[i * h + j] = input[j * w + i];
    ilgt_func[hlog2 - 2](&tmp[i * h], temp1d, lgtmtx_col[0]);
    // upscale, and store in place
    for (j = 0; j < h; ++j)
      tmp[i * h + j] = inv_upscale_wrt_txsize(temp1d[j], tx_size);
  }
  // inverse row transforms
  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; ++j) temp1d[j] = tmp[j * h + i];
    ilgt_func[wlog2 - 2](temp1d, &out[i * w], lgtmtx_row[0]);
  }
  // downscale + sum with the destination
  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; ++j) {
      int d = i * stride + j;
      int s = i * w + j;
      dest[d] =
          clip_pixel_add(dest[d], inv_downscale_wrt_txsize(out[s], tx_size));
    }
  }
#else
  // inverse row transforms
  for (i = 0; i < h; ++i) {
    ilgt_func[wlog2 - 2](input, temp1d, lgtmtx_row[0]);
    // upscale and transpose (tmp[j*h+i] <--> tmp[j][i])
    for (j = 0; j < w; ++j)
      tmp[j * h + i] = inv_upscale_wrt_txsize(temp1d[j], tx_size);
    input += w;
  }
  // inverse column transforms
  for (i = 0; i < w; ++i)
    ilgt_func[hlog2 - 2](&tmp[i * h], &out[i * h], lgtmtx_col[0]);
  // here, out[] is the transpose of 2D block of transform coefficients

  // downscale + transform + sum with dest
  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; ++j) {
      int d = i * stride + j;
      int s = j * h + i;
      dest[d] =
          clip_pixel_add(dest[d], inv_downscale_wrt_txsize(out[s], tx_size));
    }
  }
#endif
}
#endif  // CONFIG_LGT_FROM_PRED

void av1_iht4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                         const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
#if !CONFIG_DAALA_DCT4
  if (tx_type == DCT_DCT) {
    aom_idct4x4_16_add(input, dest, stride);
    return;
  }
#endif
  static const transform_2d IHT_4[] = {
#if CONFIG_DAALA_DCT4
    { daala_idct4, daala_idct4 },  // DCT_DCT  = 0
    { daala_idst4, daala_idct4 },  // ADST_DCT = 1
    { daala_idct4, daala_idst4 },  // DCT_ADST = 2
    { daala_idst4, daala_idst4 },  // ADST_ADST = 3
#if CONFIG_EXT_TX
    { daala_idst4, daala_idct4 },  // FLIPADST_DCT
    { daala_idct4, daala_idst4 },  // DCT_FLIPADST
    { daala_idst4, daala_idst4 },  // FLIPADST_FLIPADST
    { daala_idst4, daala_idst4 },  // ADST_FLIPADST
    { daala_idst4, daala_idst4 },  // FLIPADST_ADST
    { daala_idtx4, daala_idtx4 },  // IDTX
    { daala_idct4, daala_idtx4 },  // V_DCT
    { daala_idtx4, daala_idct4 },  // H_DCT
    { daala_idst4, daala_idtx4 },  // V_ADST
    { daala_idtx4, daala_idst4 },  // H_ADST
    { daala_idst4, daala_idtx4 },  // V_FLIPADST
    { daala_idtx4, daala_idst4 },  // H_FLIPADST
#endif
#else
    { aom_idct4_c, aom_idct4_c },    // DCT_DCT  = 0
    { aom_iadst4_c, aom_idct4_c },   // ADST_DCT = 1
    { aom_idct4_c, aom_iadst4_c },   // DCT_ADST = 2
    { aom_iadst4_c, aom_iadst4_c },  // ADST_ADST = 3
#if CONFIG_EXT_TX
    { aom_iadst4_c, aom_idct4_c },   // FLIPADST_DCT
    { aom_idct4_c, aom_iadst4_c },   // DCT_FLIPADST
    { aom_iadst4_c, aom_iadst4_c },  // FLIPADST_FLIPADST
    { aom_iadst4_c, aom_iadst4_c },  // ADST_FLIPADST
    { aom_iadst4_c, aom_iadst4_c },  // FLIPADST_ADST
    { iidtx4_c, iidtx4_c },          // IDTX
    { aom_idct4_c, iidtx4_c },       // V_DCT
    { iidtx4_c, aom_idct4_c },       // H_DCT
    { aom_iadst4_c, iidtx4_c },      // V_ADST
    { iidtx4_c, aom_iadst4_c },      // H_ADST
    { aom_iadst4_c, iidtx4_c },      // V_FLIPADST
    { iidtx4_c, aom_iadst4_c },      // H_FLIPADST
#endif
#endif
  };

  int i, j;
  tran_low_t tmp[4][4];
  tran_low_t out[4][4];
  tran_low_t *outp = &out[0][0];
  int outstride = 4;

#if CONFIG_DCT_ONLY
  assert(tx_type == DCT_DCT);
#endif

#if CONFIG_LGT
  const tran_high_t *lgtmtx_col[1];
  const tran_high_t *lgtmtx_row[1];
  int use_lgt_col = get_lgt4(txfm_param, 1, lgtmtx_col);
  int use_lgt_row = get_lgt4(txfm_param, 0, lgtmtx_row);
#endif

  // inverse transform row vectors
  for (i = 0; i < 4; ++i) {
#if CONFIG_DAALA_DCT4
    tran_low_t temp_in[4];
    for (j = 0; j < 4; j++) temp_in[j] = input[j] * 2;
    IHT_4[tx_type].rows(temp_in, out[i]);
#else
#if CONFIG_LGT
    if (use_lgt_row)
      ilgt4(input, out[i], lgtmtx_row[0]);
    else
#endif
      IHT_4[tx_type].rows(input, out[i]);
#endif
    input += 4;
  }

  // transpose
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      tmp[j][i] = out[i][j];
    }
  }

  // inverse transform column vectors
  for (i = 0; i < 4; ++i) {
#if CONFIG_LGT
    if (use_lgt_col)
      ilgt4(tmp[i], out[i], lgtmtx_col[0]);
    else
#endif
      IHT_4[tx_type].cols(tmp[i], out[i]);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, 4, 4);
#endif

  // Sum with the destination
  for (i = 0; i < 4; ++i) {
    for (j = 0; j < 4; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
#if CONFIG_DAALA_DCT4
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 4));
#else
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 4));
#endif
    }
  }
}

void av1_iht4x8_32_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                         const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
#if CONFIG_DCT_ONLY
  assert(tx_type == DCT_DCT);
#endif
  static const transform_2d IHT_4x8[] = {
    { aom_idct8_c, aom_idct4_c },    // DCT_DCT
    { aom_iadst8_c, aom_idct4_c },   // ADST_DCT
    { aom_idct8_c, aom_iadst4_c },   // DCT_ADST
    { aom_iadst8_c, aom_iadst4_c },  // ADST_ADST
#if CONFIG_EXT_TX
    { aom_iadst8_c, aom_idct4_c },   // FLIPADST_DCT
    { aom_idct8_c, aom_iadst4_c },   // DCT_FLIPADST
    { aom_iadst8_c, aom_iadst4_c },  // FLIPADST_FLIPADST
    { aom_iadst8_c, aom_iadst4_c },  // ADST_FLIPADST
    { aom_iadst8_c, aom_iadst4_c },  // FLIPADST_ADST
    { iidtx8_c, iidtx4_c },          // IDTX
    { aom_idct8_c, iidtx4_c },       // V_DCT
    { iidtx8_c, aom_idct4_c },       // H_DCT
    { aom_iadst8_c, iidtx4_c },      // V_ADST
    { iidtx8_c, aom_iadst4_c },      // H_ADST
    { aom_iadst8_c, iidtx4_c },      // V_FLIPADST
    { iidtx8_c, aom_iadst4_c },      // H_FLIPADST
#endif
  };

  const int n = 4;
  const int n2 = 8;
  int i, j;
  tran_low_t out[4][8], tmp[4][8], outtmp[4];
  tran_low_t *outp = &out[0][0];
  int outstride = n2;

#if CONFIG_LGT
  const tran_high_t *lgtmtx_col[1];
  const tran_high_t *lgtmtx_row[1];
  int use_lgt_col = get_lgt8(txfm_param, 1, lgtmtx_col);
  int use_lgt_row = get_lgt4(txfm_param, 0, lgtmtx_row);
#endif

  // inverse transform row vectors and transpose
  for (i = 0; i < n2; ++i) {
#if CONFIG_LGT
    if (use_lgt_row)
      ilgt4(input, outtmp, lgtmtx_row[0]);
    else
#endif
      IHT_4x8[tx_type].rows(input, outtmp);
    for (j = 0; j < n; ++j)
      tmp[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n;
  }

  // inverse transform column vectors
  for (i = 0; i < n; ++i) {
#if CONFIG_LGT
    if (use_lgt_col)
      ilgt8(tmp[i], out[i], lgtmtx_col[0]);
    else
#endif
      IHT_4x8[tx_type].cols(tmp[i], out[i]);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n2, n);
#endif

  // Sum with the destination
  for (i = 0; i < n2; ++i) {
    for (j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
    }
  }
}

void av1_iht8x4_32_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                         const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
#if CONFIG_DCT_ONLY
  assert(tx_type == DCT_DCT);
#endif
  static const transform_2d IHT_8x4[] = {
    { aom_idct4_c, aom_idct8_c },    // DCT_DCT
    { aom_iadst4_c, aom_idct8_c },   // ADST_DCT
    { aom_idct4_c, aom_iadst8_c },   // DCT_ADST
    { aom_iadst4_c, aom_iadst8_c },  // ADST_ADST
#if CONFIG_EXT_TX
    { aom_iadst4_c, aom_idct8_c },   // FLIPADST_DCT
    { aom_idct4_c, aom_iadst8_c },   // DCT_FLIPADST
    { aom_iadst4_c, aom_iadst8_c },  // FLIPADST_FLIPADST
    { aom_iadst4_c, aom_iadst8_c },  // ADST_FLIPADST
    { aom_iadst4_c, aom_iadst8_c },  // FLIPADST_ADST
    { iidtx4_c, iidtx8_c },          // IDTX
    { aom_idct4_c, iidtx8_c },       // V_DCT
    { iidtx4_c, aom_idct8_c },       // H_DCT
    { aom_iadst4_c, iidtx8_c },      // V_ADST
    { iidtx4_c, aom_iadst8_c },      // H_ADST
    { aom_iadst4_c, iidtx8_c },      // V_FLIPADST
    { iidtx4_c, aom_iadst8_c },      // H_FLIPADST
#endif
  };

  const int n = 4;
  const int n2 = 8;

  int i, j;
  tran_low_t out[8][4], tmp[8][4], outtmp[8];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

#if CONFIG_LGT
  const tran_high_t *lgtmtx_col[1];
  const tran_high_t *lgtmtx_row[1];
  int use_lgt_col = get_lgt4(txfm_param, 1, lgtmtx_col);
  int use_lgt_row = get_lgt8(txfm_param, 0, lgtmtx_row);
#endif

  // inverse transform row vectors and transpose
  for (i = 0; i < n; ++i) {
#if CONFIG_LGT
    if (use_lgt_row)
      ilgt8(input, outtmp, lgtmtx_row[0]);
    else
#endif
      IHT_8x4[tx_type].rows(input, outtmp);
    for (j = 0; j < n2; ++j)
      tmp[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n2;
  }

  // inverse transform column vectors
  for (i = 0; i < n2; ++i) {
#if CONFIG_LGT
    if (use_lgt_col)
      ilgt4(tmp[i], out[i], lgtmtx_col[0]);
    else
#endif
      IHT_8x4[tx_type].cols(tmp[i], out[i]);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n, n2);
#endif

  // Sum with the destination
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n2; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
    }
  }
}

void av1_iht4x16_64_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                          const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
#if CONFIG_DCT_ONLY
  assert(tx_type == DCT_DCT);
#endif
  static const transform_2d IHT_4x16[] = {
    { aom_idct16_c, aom_idct4_c },    // DCT_DCT
    { aom_iadst16_c, aom_idct4_c },   // ADST_DCT
    { aom_idct16_c, aom_iadst4_c },   // DCT_ADST
    { aom_iadst16_c, aom_iadst4_c },  // ADST_ADST
#if CONFIG_EXT_TX
    { aom_iadst16_c, aom_idct4_c },   // FLIPADST_DCT
    { aom_idct16_c, aom_iadst4_c },   // DCT_FLIPADST
    { aom_iadst16_c, aom_iadst4_c },  // FLIPADST_FLIPADST
    { aom_iadst16_c, aom_iadst4_c },  // ADST_FLIPADST
    { aom_iadst16_c, aom_iadst4_c },  // FLIPADST_ADST
    { iidtx16_c, iidtx4_c },          // IDTX
    { aom_idct16_c, iidtx4_c },       // V_DCT
    { iidtx16_c, aom_idct4_c },       // H_DCT
    { aom_iadst16_c, iidtx4_c },      // V_ADST
    { iidtx16_c, aom_iadst4_c },      // H_ADST
    { aom_iadst16_c, iidtx4_c },      // V_FLIPADST
    { iidtx16_c, aom_iadst4_c },      // H_FLIPADST
#endif
  };

  const int n = 4;
  const int n4 = 16;
  int i, j;
  tran_low_t out[4][16], tmp[4][16], outtmp[4];
  tran_low_t *outp = &out[0][0];
  int outstride = n4;

#if CONFIG_LGT
  const tran_high_t *lgtmtx_row[1];
  int use_lgt_row = get_lgt4(txfm_param, 0, lgtmtx_row);
#endif

  // inverse transform row vectors and transpose
  for (i = 0; i < n4; ++i) {
#if CONFIG_LGT
    if (use_lgt_row)
      ilgt4(input, outtmp, lgtmtx_row[0]);
    else
#endif
      IHT_4x16[tx_type].rows(input, outtmp);
    for (j = 0; j < n; ++j) tmp[j][i] = outtmp[j];
    input += n;
  }

  // inverse transform column vectors
  for (i = 0; i < n; ++i) {
    IHT_4x16[tx_type].cols(tmp[i], out[i]);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n4, n);
#endif

  // Sum with the destination
  for (i = 0; i < n4; ++i) {
    for (j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
    }
  }
}

void av1_iht16x4_64_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                          const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
#if CONFIG_DCT_ONLY
  assert(tx_type == DCT_DCT);
#endif
  static const transform_2d IHT_16x4[] = {
    { aom_idct4_c, aom_idct16_c },    // DCT_DCT
    { aom_iadst4_c, aom_idct16_c },   // ADST_DCT
    { aom_idct4_c, aom_iadst16_c },   // DCT_ADST
    { aom_iadst4_c, aom_iadst16_c },  // ADST_ADST
#if CONFIG_EXT_TX
    { aom_iadst4_c, aom_idct16_c },   // FLIPADST_DCT
    { aom_idct4_c, aom_iadst16_c },   // DCT_FLIPADST
    { aom_iadst4_c, aom_iadst16_c },  // FLIPADST_FLIPADST
    { aom_iadst4_c, aom_iadst16_c },  // ADST_FLIPADST
    { aom_iadst4_c, aom_iadst16_c },  // FLIPADST_ADST
    { iidtx4_c, iidtx16_c },          // IDTX
    { aom_idct4_c, iidtx16_c },       // V_DCT
    { iidtx4_c, aom_idct16_c },       // H_DCT
    { aom_iadst4_c, iidtx16_c },      // V_ADST
    { iidtx4_c, aom_iadst16_c },      // H_ADST
    { aom_iadst4_c, iidtx16_c },      // V_FLIPADST
    { iidtx4_c, aom_iadst16_c },      // H_FLIPADST
#endif
  };

  const int n = 4;
  const int n4 = 16;

  int i, j;
  tran_low_t out[16][4], tmp[16][4], outtmp[16];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

#if CONFIG_LGT
  const tran_high_t *lgtmtx_col[1];
  int use_lgt_col = get_lgt4(txfm_param, 1, lgtmtx_col);
#endif

  // inverse transform row vectors and transpose
  for (i = 0; i < n; ++i) {
    IHT_16x4[tx_type].rows(input, outtmp);
    for (j = 0; j < n4; ++j) tmp[j][i] = outtmp[j];
    input += n4;
  }

  // inverse transform column vectors
  for (i = 0; i < n4; ++i) {
#if CONFIG_LGT
    if (use_lgt_col)
      ilgt4(tmp[i], out[i], lgtmtx_col[0]);
    else
#endif
      IHT_16x4[tx_type].cols(tmp[i], out[i]);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n, n4);
#endif

  // Sum with the destination
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n4; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
    }
  }
}

void av1_iht8x16_128_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                           const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
#if CONFIG_DCT_ONLY
  assert(tx_type == DCT_DCT);
#endif
  static const transform_2d IHT_8x16[] = {
    { aom_idct16_c, aom_idct8_c },    // DCT_DCT
    { aom_iadst16_c, aom_idct8_c },   // ADST_DCT
    { aom_idct16_c, aom_iadst8_c },   // DCT_ADST
    { aom_iadst16_c, aom_iadst8_c },  // ADST_ADST
#if CONFIG_EXT_TX
    { aom_iadst16_c, aom_idct8_c },   // FLIPADST_DCT
    { aom_idct16_c, aom_iadst8_c },   // DCT_FLIPADST
    { aom_iadst16_c, aom_iadst8_c },  // FLIPADST_FLIPADST
    { aom_iadst16_c, aom_iadst8_c },  // ADST_FLIPADST
    { aom_iadst16_c, aom_iadst8_c },  // FLIPADST_ADST
    { iidtx16_c, iidtx8_c },          // IDTX
    { aom_idct16_c, iidtx8_c },       // V_DCT
    { iidtx16_c, aom_idct8_c },       // H_DCT
    { aom_iadst16_c, iidtx8_c },      // V_ADST
    { iidtx16_c, aom_iadst8_c },      // H_ADST
    { aom_iadst16_c, iidtx8_c },      // V_FLIPADST
    { iidtx16_c, aom_iadst8_c },      // H_FLIPADST
#endif
  };

  const int n = 8;
  const int n2 = 16;
  int i, j;
  tran_low_t out[8][16], tmp[8][16], outtmp[8];
  tran_low_t *outp = &out[0][0];
  int outstride = n2;

#if CONFIG_LGT
  const tran_high_t *lgtmtx_row[1];
  int use_lgt_row = get_lgt8(txfm_param, 0, lgtmtx_row);
#endif

  // inverse transform row vectors and transpose
  for (i = 0; i < n2; ++i) {
#if CONFIG_LGT
    if (use_lgt_row)
      ilgt8(input, outtmp, lgtmtx_row[0]);
    else
#endif
      IHT_8x16[tx_type].rows(input, outtmp);
    for (j = 0; j < n; ++j)
      tmp[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n;
  }

  // inverse transform column vectors
  for (i = 0; i < n; ++i) {
    IHT_8x16[tx_type].cols(tmp[i], out[i]);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n2, n);
#endif

  // Sum with the destination
  for (i = 0; i < n2; ++i) {
    for (j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}

void av1_iht16x8_128_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                           const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
#if CONFIG_DCT_ONLY
  assert(tx_type == DCT_DCT);
#endif
  static const transform_2d IHT_16x8[] = {
    { aom_idct8_c, aom_idct16_c },    // DCT_DCT
    { aom_iadst8_c, aom_idct16_c },   // ADST_DCT
    { aom_idct8_c, aom_iadst16_c },   // DCT_ADST
    { aom_iadst8_c, aom_iadst16_c },  // ADST_ADST
#if CONFIG_EXT_TX
    { aom_iadst8_c, aom_idct16_c },   // FLIPADST_DCT
    { aom_idct8_c, aom_iadst16_c },   // DCT_FLIPADST
    { aom_iadst8_c, aom_iadst16_c },  // FLIPADST_FLIPADST
    { aom_iadst8_c, aom_iadst16_c },  // ADST_FLIPADST
    { aom_iadst8_c, aom_iadst16_c },  // FLIPADST_ADST
    { iidtx8_c, iidtx16_c },          // IDTX
    { aom_idct8_c, iidtx16_c },       // V_DCT
    { iidtx8_c, aom_idct16_c },       // H_DCT
    { aom_iadst8_c, iidtx16_c },      // V_ADST
    { iidtx8_c, aom_iadst16_c },      // H_ADST
    { aom_iadst8_c, iidtx16_c },      // V_FLIPADST
    { iidtx8_c, aom_iadst16_c },      // H_FLIPADST
#endif
  };

  const int n = 8;
  const int n2 = 16;

  int i, j;
  tran_low_t out[16][8], tmp[16][8], outtmp[16];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

#if CONFIG_LGT
  const tran_high_t *lgtmtx_col[1];
  int use_lgt_col = get_lgt8(txfm_param, 1, lgtmtx_col);
#endif

  // inverse transform row vectors and transpose
  for (i = 0; i < n; ++i) {
    IHT_16x8[tx_type].rows(input, outtmp);
    for (j = 0; j < n2; ++j)
      tmp[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n2;
  }

  // inverse transform column vectors
  for (i = 0; i < n2; ++i) {
#if CONFIG_LGT
    if (use_lgt_col)
      ilgt8(tmp[i], out[i], lgtmtx_col[0]);
    else
#endif
      IHT_16x8[tx_type].cols(tmp[i], out[i]);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n, n2);
#endif

  // Sum with the destination
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n2; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}

void av1_iht8x32_256_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                           const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
#if CONFIG_DCT_ONLY
  assert(tx_type == DCT_DCT);
#endif
  static const transform_2d IHT_8x32[] = {
    { aom_idct32_c, aom_idct8_c },     // DCT_DCT
    { ihalfright32_c, aom_idct8_c },   // ADST_DCT
    { aom_idct32_c, aom_iadst8_c },    // DCT_ADST
    { ihalfright32_c, aom_iadst8_c },  // ADST_ADST
#if CONFIG_EXT_TX
    { ihalfright32_c, aom_idct8_c },   // FLIPADST_DCT
    { aom_idct32_c, aom_iadst8_c },    // DCT_FLIPADST
    { ihalfright32_c, aom_iadst8_c },  // FLIPADST_FLIPADST
    { ihalfright32_c, aom_iadst8_c },  // ADST_FLIPADST
    { ihalfright32_c, aom_iadst8_c },  // FLIPADST_ADST
    { iidtx32_c, iidtx8_c },           // IDTX
    { aom_idct32_c, iidtx8_c },        // V_DCT
    { iidtx32_c, aom_idct8_c },        // H_DCT
    { ihalfright32_c, iidtx8_c },      // V_ADST
    { iidtx32_c, aom_iadst8_c },       // H_ADST
    { ihalfright32_c, iidtx8_c },      // V_FLIPADST
    { iidtx32_c, aom_iadst8_c },       // H_FLIPADST
#endif
  };

  const int n = 8;
  const int n4 = 32;
  int i, j;
  tran_low_t out[8][32], tmp[8][32], outtmp[8];
  tran_low_t *outp = &out[0][0];
  int outstride = n4;

#if CONFIG_LGT
  const tran_high_t *lgtmtx_row[1];
  int use_lgt_row = get_lgt8(txfm_param, 0, lgtmtx_row);
#endif

  // inverse transform row vectors and transpose
  for (i = 0; i < n4; ++i) {
#if CONFIG_LGT
    if (use_lgt_row)
      ilgt8(input, outtmp, lgtmtx_row[0]);
    else
#endif
      IHT_8x32[tx_type].rows(input, outtmp);
    for (j = 0; j < n; ++j) tmp[j][i] = outtmp[j];
    input += n;
  }

  // inverse transform column vectors
  for (i = 0; i < n; ++i) {
    IHT_8x32[tx_type].cols(tmp[i], out[i]);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n4, n);
#endif

  // Sum with the destination
  for (i = 0; i < n4; ++i) {
    for (j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}

void av1_iht32x8_256_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                           const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
#if CONFIG_DCT_ONLY
  assert(tx_type == DCT_DCT);
#endif
  static const transform_2d IHT_32x8[] = {
    { aom_idct8_c, aom_idct32_c },     // DCT_DCT
    { aom_iadst8_c, aom_idct32_c },    // ADST_DCT
    { aom_idct8_c, ihalfright32_c },   // DCT_ADST
    { aom_iadst8_c, ihalfright32_c },  // ADST_ADST
#if CONFIG_EXT_TX
    { aom_iadst8_c, aom_idct32_c },    // FLIPADST_DCT
    { aom_idct8_c, ihalfright32_c },   // DCT_FLIPADST
    { aom_iadst8_c, ihalfright32_c },  // FLIPADST_FLIPADST
    { aom_iadst8_c, ihalfright32_c },  // ADST_FLIPADST
    { aom_iadst8_c, ihalfright32_c },  // FLIPADST_ADST
    { iidtx8_c, iidtx32_c },           // IDTX
    { aom_idct8_c, iidtx32_c },        // V_DCT
    { iidtx8_c, aom_idct32_c },        // H_DCT
    { aom_iadst8_c, iidtx32_c },       // V_ADST
    { iidtx8_c, ihalfright32_c },      // H_ADST
    { aom_iadst8_c, iidtx32_c },       // V_FLIPADST
    { iidtx8_c, ihalfright32_c },      // H_FLIPADST
#endif
  };

  const int n = 8;
  const int n4 = 32;

  int i, j;
  tran_low_t out[32][8], tmp[32][8], outtmp[32];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

#if CONFIG_LGT
  const tran_high_t *lgtmtx_col[1];
  int use_lgt_col = get_lgt4(txfm_param, 1, lgtmtx_col);
#endif

  // inverse transform row vectors and transpose
  for (i = 0; i < n; ++i) {
    IHT_32x8[tx_type].rows(input, outtmp);
    for (j = 0; j < n4; ++j) tmp[j][i] = outtmp[j];
    input += n4;
  }

  // inverse transform column vectors
  for (i = 0; i < n4; ++i) {
#if CONFIG_LGT
    if (use_lgt_col)
      ilgt8(tmp[i], out[i], lgtmtx_col[0]);
    else
#endif
      IHT_32x8[tx_type].cols(tmp[i], out[i]);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n, n4);
#endif

  // Sum with the destination
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n4; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}

void av1_iht16x32_512_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                            const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
#if CONFIG_DCT_ONLY
  assert(tx_type == DCT_DCT);
#endif
  static const transform_2d IHT_16x32[] = {
    { aom_idct32_c, aom_idct16_c },     // DCT_DCT
    { ihalfright32_c, aom_idct16_c },   // ADST_DCT
    { aom_idct32_c, aom_iadst16_c },    // DCT_ADST
    { ihalfright32_c, aom_iadst16_c },  // ADST_ADST
#if CONFIG_EXT_TX
    { ihalfright32_c, aom_idct16_c },   // FLIPADST_DCT
    { aom_idct32_c, aom_iadst16_c },    // DCT_FLIPADST
    { ihalfright32_c, aom_iadst16_c },  // FLIPADST_FLIPADST
    { ihalfright32_c, aom_iadst16_c },  // ADST_FLIPADST
    { ihalfright32_c, aom_iadst16_c },  // FLIPADST_ADST
    { iidtx32_c, iidtx16_c },           // IDTX
    { aom_idct32_c, iidtx16_c },        // V_DCT
    { iidtx32_c, aom_idct16_c },        // H_DCT
    { ihalfright32_c, iidtx16_c },      // V_ADST
    { iidtx32_c, aom_iadst16_c },       // H_ADST
    { ihalfright32_c, iidtx16_c },      // V_FLIPADST
    { iidtx32_c, aom_iadst16_c },       // H_FLIPADST
#endif
  };

  const int n = 16;
  const int n2 = 32;
  int i, j;
  tran_low_t out[16][32], tmp[16][32], outtmp[16];
  tran_low_t *outp = &out[0][0];
  int outstride = n2;

  // inverse transform row vectors and transpose
  for (i = 0; i < n2; ++i) {
    IHT_16x32[tx_type].rows(input, outtmp);
    for (j = 0; j < n; ++j)
      tmp[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n;
  }

  // inverse transform column vectors
  for (i = 0; i < n; ++i) IHT_16x32[tx_type].cols(tmp[i], out[i]);

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n2, n);
#endif

  // Sum with the destination
  for (i = 0; i < n2; ++i) {
    for (j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}

void av1_iht32x16_512_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                            const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
#if CONFIG_DCT_ONLY
  assert(tx_type == DCT_DCT);
#endif
  static const transform_2d IHT_32x16[] = {
    { aom_idct16_c, aom_idct32_c },     // DCT_DCT
    { aom_iadst16_c, aom_idct32_c },    // ADST_DCT
    { aom_idct16_c, ihalfright32_c },   // DCT_ADST
    { aom_iadst16_c, ihalfright32_c },  // ADST_ADST
#if CONFIG_EXT_TX
    { aom_iadst16_c, aom_idct32_c },    // FLIPADST_DCT
    { aom_idct16_c, ihalfright32_c },   // DCT_FLIPADST
    { aom_iadst16_c, ihalfright32_c },  // FLIPADST_FLIPADST
    { aom_iadst16_c, ihalfright32_c },  // ADST_FLIPADST
    { aom_iadst16_c, ihalfright32_c },  // FLIPADST_ADST
    { iidtx16_c, iidtx32_c },           // IDTX
    { aom_idct16_c, iidtx32_c },        // V_DCT
    { iidtx16_c, aom_idct32_c },        // H_DCT
    { aom_iadst16_c, iidtx32_c },       // V_ADST
    { iidtx16_c, ihalfright32_c },      // H_ADST
    { aom_iadst16_c, iidtx32_c },       // V_FLIPADST
    { iidtx16_c, ihalfright32_c },      // H_FLIPADST
#endif
  };
  const int n = 16;
  const int n2 = 32;

  int i, j;
  tran_low_t out[32][16], tmp[32][16], outtmp[32];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  // inverse transform row vectors and transpose
  for (i = 0; i < n; ++i) {
    IHT_32x16[tx_type].rows(input, outtmp);
    for (j = 0; j < n2; ++j)
      tmp[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n2;
  }

  // inverse transform column vectors
  for (i = 0; i < n2; ++i) IHT_32x16[tx_type].cols(tmp[i], out[i]);

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n, n2);
#endif

  // Sum with the destination
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n2; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}

void av1_iht8x8_64_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                         const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
#if CONFIG_DCT_ONLY
  assert(tx_type == DCT_DCT);
#endif
  static const transform_2d IHT_8[] = {
#if CONFIG_DAALA_DCT8
    { daala_idct8, daala_idct8 },  // DCT_DCT  = 0
    { daala_idst8, daala_idct8 },  // ADST_DCT = 1
    { daala_idct8, daala_idst8 },  // DCT_ADST = 2
    { daala_idst8, daala_idst8 },  // ADST_ADST = 3
#if CONFIG_EXT_TX
    { daala_idst8, daala_idct8 },  // FLIPADST_DCT
    { daala_idct8, daala_idst8 },  // DCT_FLIPADST
    { daala_idst8, daala_idst8 },  // FLIPADST_FLIPADST
    { daala_idst8, daala_idst8 },  // ADST_FLIPADST
    { daala_idst8, daala_idst8 },  // FLIPADST_ADST
    { daala_idtx8, daala_idtx8 },  // IDTX
    { daala_idct8, daala_idtx8 },  // V_DCT
    { daala_idtx8, daala_idct8 },  // H_DCT
    { daala_idst8, daala_idtx8 },  // V_ADST
    { daala_idtx8, daala_idst8 },  // H_ADST
    { daala_idst8, daala_idtx8 },  // V_FLIPADST
    { daala_idtx8, daala_idst8 },  // H_FLIPADST
#endif
#else
    { aom_idct8_c, aom_idct8_c },    // DCT_DCT  = 0
    { aom_iadst8_c, aom_idct8_c },   // ADST_DCT = 1
    { aom_idct8_c, aom_iadst8_c },   // DCT_ADST = 2
    { aom_iadst8_c, aom_iadst8_c },  // ADST_ADST = 3
#if CONFIG_EXT_TX
    { aom_iadst8_c, aom_idct8_c },   // FLIPADST_DCT
    { aom_idct8_c, aom_iadst8_c },   // DCT_FLIPADST
    { aom_iadst8_c, aom_iadst8_c },  // FLIPADST_FLIPADST
    { aom_iadst8_c, aom_iadst8_c },  // ADST_FLIPADST
    { aom_iadst8_c, aom_iadst8_c },  // FLIPADST_ADST
    { iidtx8_c, iidtx8_c },          // IDTX
    { aom_idct8_c, iidtx8_c },       // V_DCT
    { iidtx8_c, aom_idct8_c },       // H_DCT
    { aom_iadst8_c, iidtx8_c },      // V_ADST
    { iidtx8_c, aom_iadst8_c },      // H_ADST
    { aom_iadst8_c, iidtx8_c },      // V_FLIPADST
    { iidtx8_c, aom_iadst8_c },      // H_FLIPADST
#endif
#endif
  };

  int i, j;
  tran_low_t tmp[8][8];
  tran_low_t out[8][8];
  tran_low_t *outp = &out[0][0];
  int outstride = 8;

#if CONFIG_LGT
  const tran_high_t *lgtmtx_col[1];
  const tran_high_t *lgtmtx_row[1];
  int use_lgt_col = get_lgt8(txfm_param, 1, lgtmtx_col);
  int use_lgt_row = get_lgt8(txfm_param, 0, lgtmtx_row);
#endif

  // inverse transform row vectors
  for (i = 0; i < 8; ++i) {
#if CONFIG_DAALA_DCT8
    tran_low_t temp_in[8];
    for (j = 0; j < 8; j++) temp_in[j] = input[j] * 2;
    IHT_8[tx_type].rows(temp_in, out[i]);
#else
#if CONFIG_LGT
    if (use_lgt_row)
      ilgt8(input, out[i], lgtmtx_row[0]);
    else
#endif
      IHT_8[tx_type].rows(input, out[i]);
#endif
    input += 8;
  }

  // transpose
  for (i = 0; i < 8; i++) {
    for (j = 0; j < 8; j++) {
      tmp[j][i] = out[i][j];
    }
  }

  // inverse transform column vectors
  for (i = 0; i < 8; ++i) {
#if CONFIG_LGT
    if (use_lgt_col)
      ilgt8(tmp[i], out[i], lgtmtx_col[0]);
    else
#endif
      IHT_8[tx_type].cols(tmp[i], out[i]);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, 8, 8);
#endif

  // Sum with the destination
  for (i = 0; i < 8; ++i) {
    for (j = 0; j < 8; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
#if CONFIG_DAALA_DCT8
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 4));
#else
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
#endif
    }
  }
}

void av1_iht16x16_256_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                            const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
#if CONFIG_DCT_ONLY
  assert(tx_type == DCT_DCT);
#endif
  static const transform_2d IHT_16[] = {
#if CONFIG_DAALA_DCT16
    { daala_idct16, daala_idct16 },  // DCT_DCT  = 0
    { daala_idst16, daala_idct16 },  // ADST_DCT = 1
    { daala_idct16, daala_idst16 },  // DCT_ADST = 2
    { daala_idst16, daala_idst16 },  // ADST_ADST = 3
#if CONFIG_EXT_TX
    { daala_idst16, daala_idct16 },  // FLIPADST_DCT
    { daala_idct16, daala_idst16 },  // DCT_FLIPADST
    { daala_idst16, daala_idst16 },  // FLIPADST_FLIPADST
    { daala_idst16, daala_idst16 },  // ADST_FLIPADST
    { daala_idst16, daala_idst16 },  // FLIPADST_ADST
    { daala_idtx16, daala_idtx16 },  // IDTX
    { daala_idct16, daala_idtx16 },  // V_DCT
    { daala_idtx16, daala_idct16 },  // H_DCT
    { daala_idst16, daala_idtx16 },  // V_ADST
    { daala_idtx16, daala_idst16 },  // H_ADST
    { daala_idst16, daala_idtx16 },  // V_FLIPADST
    { daala_idtx16, daala_idst16 },  // H_FLIPADST
#endif
#else
    { aom_idct16_c, aom_idct16_c },    // DCT_DCT  = 0
    { aom_iadst16_c, aom_idct16_c },   // ADST_DCT = 1
    { aom_idct16_c, aom_iadst16_c },   // DCT_ADST = 2
    { aom_iadst16_c, aom_iadst16_c },  // ADST_ADST = 3
#if CONFIG_EXT_TX
    { aom_iadst16_c, aom_idct16_c },   // FLIPADST_DCT
    { aom_idct16_c, aom_iadst16_c },   // DCT_FLIPADST
    { aom_iadst16_c, aom_iadst16_c },  // FLIPADST_FLIPADST
    { aom_iadst16_c, aom_iadst16_c },  // ADST_FLIPADST
    { aom_iadst16_c, aom_iadst16_c },  // FLIPADST_ADST
    { iidtx16_c, iidtx16_c },          // IDTX
    { aom_idct16_c, iidtx16_c },       // V_DCT
    { iidtx16_c, aom_idct16_c },       // H_DCT
    { aom_iadst16_c, iidtx16_c },      // V_ADST
    { iidtx16_c, aom_iadst16_c },      // H_ADST
    { aom_iadst16_c, iidtx16_c },      // V_FLIPADST
    { iidtx16_c, aom_iadst16_c },      // H_FLIPADST
#endif
#endif
  };

  int i, j;
  tran_low_t tmp[16][16];
  tran_low_t out[16][16];
  tran_low_t *outp = &out[0][0];
  int outstride = 16;

  // inverse transform row vectors
  for (i = 0; i < 16; ++i) {
#if CONFIG_DAALA_DCT16
    tran_low_t temp_in[16];
    for (j = 0; j < 16; j++) temp_in[j] = input[j] * 2;
    IHT_16[tx_type].rows(temp_in, out[i]);
#else
    IHT_16[tx_type].rows(input, out[i]);
#endif
    input += 16;
  }

  // transpose
  for (i = 0; i < 16; i++) {
    for (j = 0; j < 16; j++) {
      tmp[j][i] = out[i][j];
    }
  }

  // inverse transform column vectors
  for (i = 0; i < 16; ++i) IHT_16[tx_type].cols(tmp[i], out[i]);

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, 16, 16);
#endif

  // Sum with the destination
  for (i = 0; i < 16; ++i) {
    for (j = 0; j < 16; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
#if CONFIG_DAALA_DCT16
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 4));
#else
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
#endif
    }
  }
}

#if CONFIG_EXT_TX || CONFIG_DAALA_DCT32
void av1_iht32x32_1024_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                             const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_DCT_ONLY
  assert(tx_type == DCT_DCT);
#endif
  static const transform_2d IHT_32[] = {
#if CONFIG_DAALA_DCT32
    { daala_idct32, daala_idct32 },  // DCT_DCT
#if CONFIG_EXT_TX
    { daala_idst32, daala_idct32 },  // ADST_DCT
    { daala_idct32, daala_idst32 },  // DCT_ADST
    { daala_idst32, daala_idst32 },  // ADST_ADST
    { daala_idst32, daala_idct32 },  // FLIPADST_DCT
    { daala_idct32, daala_idst32 },  // DCT_FLIPADST
    { daala_idst32, daala_idst32 },  // FLIPADST_FLIPADST
    { daala_idst32, daala_idst32 },  // ADST_FLIPADST
    { daala_idst32, daala_idst32 },  // FLIPADST_ADST
    { daala_idtx32, daala_idtx32 },  // IDTX
    { daala_idct32, daala_idtx32 },  // V_DCT
    { daala_idtx32, daala_idct32 },  // H_DCT
    { daala_idst32, daala_idtx32 },  // V_ADST
    { daala_idtx32, daala_idst32 },  // H_ADST
    { daala_idst32, daala_idtx32 },  // V_FLIPADST
    { daala_idtx32, daala_idst32 },  // H_FLIPADST
#endif
#else
    { aom_idct32_c, aom_idct32_c },      // DCT_DCT
#if CONFIG_EXT_TX
    { ihalfright32_c, aom_idct32_c },    // ADST_DCT
    { aom_idct32_c, ihalfright32_c },    // DCT_ADST
    { ihalfright32_c, ihalfright32_c },  // ADST_ADST
    { ihalfright32_c, aom_idct32_c },    // FLIPADST_DCT
    { aom_idct32_c, ihalfright32_c },    // DCT_FLIPADST
    { ihalfright32_c, ihalfright32_c },  // FLIPADST_FLIPADST
    { ihalfright32_c, ihalfright32_c },  // ADST_FLIPADST
    { ihalfright32_c, ihalfright32_c },  // FLIPADST_ADST
    { iidtx32_c, iidtx32_c },            // IDTX
    { aom_idct32_c, iidtx32_c },         // V_DCT
    { iidtx32_c, aom_idct32_c },         // H_DCT
    { ihalfright32_c, iidtx32_c },       // V_ADST
    { iidtx32_c, ihalfright32_c },       // H_ADST
    { ihalfright32_c, iidtx32_c },       // V_FLIPADST
    { iidtx32_c, ihalfright32_c },       // H_FLIPADST
#endif
#endif
  };

  int i, j;
  tran_low_t tmp[32][32];
  tran_low_t out[32][32];
  tran_low_t *outp = &out[0][0];
  int outstride = 32;

  // inverse transform row vectors
  for (i = 0; i < 32; ++i) {
#if CONFIG_DAALA_DCT32
    tran_low_t temp_in[32];
    for (j = 0; j < 32; j++) temp_in[j] = input[j] * 2;
    IHT_32[tx_type].rows(temp_in, out[i]);
#else
    IHT_32[tx_type].rows(input, out[i]);
#endif
    input += 32;
  }

  // transpose
  for (i = 0; i < 32; i++) {
    for (j = 0; j < 32; j++) {
#if CONFIG_DAALA_DCT32
      tmp[j][i] = out[i][j] * 4;
#else
      tmp[j][i] = out[i][j];
#endif
    }
  }

  // inverse transform column vectors
  for (i = 0; i < 32; ++i) IHT_32[tx_type].cols(tmp[i], out[i]);

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, 32, 32);

  // Sum with the destination
  for (i = 0; i < 32; ++i) {
    for (j = 0; j < 32; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
#if CONFIG_DAALA_DCT32
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
#else
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
#endif
    }
  }
}
#endif  // CONFIG_EXT_TX || CONFIG_DAALA_DCT32

#if CONFIG_TX64X64
void av1_iht64x64_4096_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                             const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
#if CONFIG_DCT_ONLY
  assert(tx_type == DCT_DCT);
#endif
  static const transform_2d IHT_64[] = {
#if CONFIG_DAALA_DCT64
    { daala_idct64, daala_idct64 },  // DCT_DCT
    { daala_idst64, daala_idct64 },  // ADST_DCT
    { daala_idct64, daala_idst64 },  // DCT_ADST
    { daala_idst64, daala_idst64 },  // ADST_ADST
#if CONFIG_EXT_TX
    { daala_idst64, daala_idct64 },  // FLIPADST_DCT
    { daala_idct64, daala_idst64 },  // DCT_FLIPADST
    { daala_idst64, daala_idst64 },  // FLIPADST_FLIPADST
    { daala_idst64, daala_idst64 },  // ADST_FLIPADST
    { daala_idst64, daala_idst64 },  // FLIPADST_ADST
    { daala_idtx64, daala_idtx64 },  // IDTX
    { daala_idct64, daala_idtx64 },  // V_DCT
    { daala_idtx64, daala_idct64 },  // H_DCT
    { daala_idst64, daala_idtx64 },  // V_ADST
    { daala_idtx64, daala_idst64 },  // H_ADST
    { daala_idst64, daala_idtx64 },  // V_FLIPADST
    { daala_idtx64, daala_idst64 },  // H_FLIPADST
#endif
#else
    { idct64_col_c, idct64_row_c },      // DCT_DCT
    { ihalfright64_c, idct64_row_c },    // ADST_DCT
    { idct64_col_c, ihalfright64_c },    // DCT_ADST
    { ihalfright64_c, ihalfright64_c },  // ADST_ADST
#if CONFIG_EXT_TX
    { ihalfright64_c, idct64_row_c },    // FLIPADST_DCT
    { idct64_col_c, ihalfright64_c },    // DCT_FLIPADST
    { ihalfright64_c, ihalfright64_c },  // FLIPADST_FLIPADST
    { ihalfright64_c, ihalfright64_c },  // ADST_FLIPADST
    { ihalfright64_c, ihalfright64_c },  // FLIPADST_ADST
    { iidtx64_c, iidtx64_c },            // IDTX
    { idct64_col_c, iidtx64_c },         // V_DCT
    { iidtx64_c, idct64_row_c },         // H_DCT
    { ihalfright64_c, iidtx64_c },       // V_ADST
    { iidtx64_c, ihalfright64_c },       // H_ADST
    { ihalfright64_c, iidtx64_c },       // V_FLIPADST
    { iidtx64_c, ihalfright64_c },       // H_FLIPADST
#endif
#endif
  };

  int i, j;
  tran_low_t tmp[64][64];
  tran_low_t out[64][64];
  tran_low_t *outp = &out[0][0];
  int outstride = 64;

  // inverse transform row vectors
  for (i = 0; i < 64; ++i) {
#if CONFIG_DAALA_DCT64
    tran_low_t temp_in[64];
    for (j = 0; j < 64; j++) temp_in[j] = input[j] * 2;
    IHT_64[tx_type].rows(temp_in, out[i]);
// Do not rescale intermediate for Daala
#else
    IHT_64[tx_type].rows(input, out[i]);
    for (j = 0; j < 64; ++j) out[i][j] = ROUND_POWER_OF_TWO(out[i][j], 1);
#endif
    input += 64;
  }

  // transpose
  for (i = 0; i < 64; i++) {
    for (j = 0; j < 64; j++) {
      tmp[j][i] = out[i][j];
    }
  }

  // inverse transform column vectors
  for (i = 0; i < 64; ++i) IHT_64[tx_type].cols(tmp[i], out[i]);

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, 64, 64);
#endif  // CONFIG_EXT_TX

  // Sum with the destination
  for (i = 0; i < 64; ++i) {
    for (j = 0; j < 64; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
#if CONFIG_DAALA_DCT64
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 2));
#else
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
#endif
    }
  }
}

void av1_iht64x32_2048_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                             const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
#if CONFIG_DCT_ONLY
  assert(tx_type == DCT_DCT);
#endif
  static const transform_2d IHT_64x32[] = {
    { aom_idct32_c, idct64_row_c },      // DCT_DCT
    { ihalfright32_c, idct64_row_c },    // ADST_DCT
    { aom_idct32_c, ihalfright64_c },    // DCT_ADST
    { ihalfright32_c, ihalfright64_c },  // ADST_ADST
#if CONFIG_EXT_TX
    { ihalfright32_c, idct64_row_c },    // FLIPADST_DCT
    { aom_idct32_c, ihalfright64_c },    // DCT_FLIPADST
    { ihalfright32_c, ihalfright64_c },  // FLIPADST_FLIPADST
    { ihalfright32_c, ihalfright64_c },  // ADST_FLIPADST
    { ihalfright32_c, ihalfright64_c },  // FLIPADST_ADST
    { iidtx32_c, iidtx64_c },            // IDTX
    { aom_idct32_c, iidtx64_c },         // V_DCT
    { iidtx32_c, idct64_row_c },         // H_DCT
    { ihalfright32_c, iidtx64_c },       // V_ADST
    { iidtx32_c, ihalfright64_c },       // H_ADST
    { ihalfright32_c, iidtx64_c },       // V_FLIPADST
    { iidtx32_c, ihalfright64_c },       // H_FLIPADST
#endif
  };
  const int n = 32;
  const int n2 = 64;

  int i, j;
  tran_low_t out[64][32], tmp[64][32], outtmp[64];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  // inverse transform row vectors and transpose
  for (i = 0; i < n; ++i) {
    IHT_64x32[tx_type].rows(input, outtmp);
    for (j = 0; j < n2; ++j)
      tmp[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * InvSqrt2);
    input += n2;
  }

  // inverse transform column vectors
  for (i = 0; i < n2; ++i) IHT_64x32[tx_type].cols(tmp[i], out[i]);

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n, n2);
#endif

  // Sum with the destination
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n2; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
    }
  }
}

void av1_iht32x64_2048_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                             const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
#if CONFIG_DCT_ONLY
  assert(tx_type == DCT_DCT);
#endif
  static const transform_2d IHT_32x64[] = {
    { idct64_col_c, aom_idct32_c },      // DCT_DCT
    { ihalfright64_c, aom_idct32_c },    // ADST_DCT
    { idct64_col_c, ihalfright32_c },    // DCT_ADST
    { ihalfright64_c, ihalfright32_c },  // ADST_ADST
#if CONFIG_EXT_TX
    { ihalfright64_c, aom_idct32_c },    // FLIPADST_DCT
    { idct64_col_c, ihalfright32_c },    // DCT_FLIPADST
    { ihalfright64_c, ihalfright32_c },  // FLIPADST_FLIPADST
    { ihalfright64_c, ihalfright32_c },  // ADST_FLIPADST
    { ihalfright64_c, ihalfright32_c },  // FLIPADST_ADST
    { iidtx64_c, iidtx32_c },            // IDTX
    { idct64_col_c, iidtx32_c },         // V_DCT
    { iidtx64_c, aom_idct32_c },         // H_DCT
    { ihalfright64_c, iidtx32_c },       // V_ADST
    { iidtx64_c, ihalfright32_c },       // H_ADST
    { ihalfright64_c, iidtx32_c },       // V_FLIPADST
    { iidtx64_c, ihalfright32_c },       // H_FLIPADST
#endif
  };

  const int n = 32;
  const int n2 = 64;
  int i, j;
  tran_low_t out[32][64], tmp[32][64], outtmp[32];
  tran_low_t *outp = &out[0][0];
  int outstride = n2;

  // inverse transform row vectors and transpose
  for (i = 0; i < n2; ++i) {
    IHT_32x64[tx_type].rows(input, outtmp);
    for (j = 0; j < n; ++j)
      tmp[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * InvSqrt2);
    input += n;
  }

  // inverse transform column vectors
  for (i = 0; i < n; ++i) IHT_32x64[tx_type].cols(tmp[i], out[i]);

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n2, n);
#endif

  // Sum with the destination
  for (i = 0; i < n2; ++i) {
    for (j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
    }
  }
}

#endif  // CONFIG_TX64X64

// idct
void av1_idct4x4_add(const tran_low_t *input, uint8_t *dest, int stride,
                     const TxfmParam *txfm_param) {
  const int eob = txfm_param->eob;
  if (eob > 1)
    av1_iht4x4_16_add(input, dest, stride, txfm_param);
  else
    aom_idct4x4_1_add(input, dest, stride);
}

void av1_iwht4x4_add(const tran_low_t *input, uint8_t *dest, int stride,
                     const TxfmParam *txfm_param) {
  const int eob = txfm_param->eob;
  if (eob > 1)
    aom_iwht4x4_16_add(input, dest, stride);
  else
    aom_iwht4x4_1_add(input, dest, stride);
}

#if !CONFIG_DAALA_DCT8
static void idct8x8_add(const tran_low_t *input, uint8_t *dest, int stride,
                        const TxfmParam *txfm_param) {
// If dc is 1, then input[0] is the reconstructed value, do not need
// dequantization. Also, when dc is 1, dc is counted in eobs, namely eobs >=1.

// The calculation can be simplified if there are not many non-zero dct
// coefficients. Use eobs to decide what to do.
// TODO(yunqingwang): "eobs = 1" case is also handled in av1_short_idct8x8_c.
// Combine that with code here.
#if CONFIG_ADAPT_SCAN
  const int16_t half = txfm_param->eob_threshold[0];
#else
  const int16_t half = 12;
#endif

  const int eob = txfm_param->eob;
  if (eob == 1)
    // DC only DCT coefficient
    aom_idct8x8_1_add(input, dest, stride);
  else if (eob <= half)
    aom_idct8x8_12_add(input, dest, stride);
  else
    aom_idct8x8_64_add(input, dest, stride);
}
#endif

#if !CONFIG_DAALA_DCT16
static void idct16x16_add(const tran_low_t *input, uint8_t *dest, int stride,
                          const TxfmParam *txfm_param) {
// The calculation can be simplified if there are not many non-zero dct
// coefficients. Use eobs to separate different cases.
#if CONFIG_ADAPT_SCAN
  const int16_t half = txfm_param->eob_threshold[0];
  const int16_t quarter = txfm_param->eob_threshold[1];
#else
  const int16_t half = 38;
  const int16_t quarter = 10;
#endif

  const int eob = txfm_param->eob;
  if (eob == 1) /* DC only DCT coefficient. */
    aom_idct16x16_1_add(input, dest, stride);
  else if (eob <= quarter)
    aom_idct16x16_10_add(input, dest, stride);
  else if (eob <= half)
    aom_idct16x16_38_add(input, dest, stride);
  else
    aom_idct16x16_256_add(input, dest, stride);
}
#endif

#if CONFIG_MRC_TX
static void imrc32x32_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                            const TxfmParam *txfm_param) {
#if CONFIG_ADAPT_SCAN
  const int16_t half = txfm_param->eob_threshold[0];
  const int16_t quarter = txfm_param->eob_threshold[1];
#else
  const int16_t half = 135;
  const int16_t quarter = 34;
#endif

  const int eob = txfm_param->eob;
  int n_masked_vals = 0;
  uint8_t *mask;
  uint8_t mask_tmp[32 * 32];
  if (eob == 1) {
    aom_idct32x32_1_add_c(input, dest, stride);
  } else {
    if ((txfm_param->is_inter && SIGNAL_MRC_MASK_INTER) ||
        (!txfm_param->is_inter && SIGNAL_MRC_MASK_INTRA)) {
      mask = txfm_param->mask;
    } else {
      n_masked_vals =
          get_mrc_pred_mask(txfm_param->dst, txfm_param->stride, mask_tmp, 32,
                            32, 32, txfm_param->is_inter);
      if (!is_valid_mrc_mask(n_masked_vals, 32, 32))
        assert(0 && "Invalid MRC mask");
      mask = mask_tmp;
    }
    if (eob <= quarter)
      // non-zero coeff only in upper-left 8x8
      aom_imrc32x32_34_add_c(input, dest, stride, mask);
    else if (eob <= half)
      // non-zero coeff only in upper-left 16x16
      aom_imrc32x32_135_add_c(input, dest, stride, mask);
    else
      aom_imrc32x32_1024_add_c(input, dest, stride, mask);
  }
}
#endif  // CONFIG_MRC_TX

#if !CONFIG_DAALA_DCT32
static void idct32x32_add(const tran_low_t *input, uint8_t *dest, int stride,
                          const TxfmParam *txfm_param) {
#if CONFIG_ADAPT_SCAN
  const int16_t half = txfm_param->eob_threshold[0];
  const int16_t quarter = txfm_param->eob_threshold[1];
#else
  const int16_t half = 135;
  const int16_t quarter = 34;
#endif

  const int eob = txfm_param->eob;
  if (eob == 1)
    aom_idct32x32_1_add(input, dest, stride);
  else if (eob <= quarter)
    // non-zero coeff only in upper-left 8x8
    aom_idct32x32_34_add(input, dest, stride);
  else if (eob <= half)
    // non-zero coeff only in upper-left 16x16
    aom_idct32x32_135_add(input, dest, stride);
  else
    aom_idct32x32_1024_add(input, dest, stride);
}
#endif

#if CONFIG_TX64X64 && !CONFIG_DAALA_DCT64
static void idct64x64_add(const tran_low_t *input, uint8_t *dest, int stride,
                          const TxfmParam *txfm_param) {
  (void)txfm_param;
  av1_iht64x64_4096_add(input, dest, stride, txfm_param);
}
#endif  // CONFIG_TX64X64 && !CONFIG_DAALA_DCT64

#if CONFIG_CHROMA_2X2
static void inv_txfm_add_2x2(const tran_low_t *input, uint8_t *dest, int stride,
                             const TxfmParam *txfm_param) {
  tran_high_t a1 = input[0] >> UNIT_QUANT_SHIFT;
  tran_high_t b1 = input[1] >> UNIT_QUANT_SHIFT;
  tran_high_t c1 = input[2] >> UNIT_QUANT_SHIFT;
  tran_high_t d1 = input[3] >> UNIT_QUANT_SHIFT;

  tran_high_t a2 = a1 + c1;
  tran_high_t b2 = b1 + d1;
  tran_high_t c2 = a1 - c1;
  tran_high_t d2 = b1 - d1;

  (void)txfm_param;

  a1 = (a2 + b2) >> 2;
  b1 = (a2 - b2) >> 2;
  c1 = (c2 + d2) >> 2;
  d1 = (c2 - d2) >> 2;

  dest[0] = clip_pixel_add(dest[0], WRAPLOW(a1));
  dest[1] = clip_pixel_add(dest[1], WRAPLOW(b1));
  dest[stride] = clip_pixel_add(dest[stride], WRAPLOW(c1));
  dest[stride + 1] = clip_pixel_add(dest[stride + 1], WRAPLOW(d1));
}
#endif

static void inv_txfm_add_4x4(const tran_low_t *input, uint8_t *dest, int stride,
                             const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  if (txfm_param->lossless) {
    assert(tx_type == DCT_DCT);
    av1_iwht4x4_add(input, dest, stride, txfm_param);
    return;
  }

  switch (tx_type) {
#if !CONFIG_DAALA_DCT4
    case DCT_DCT: av1_idct4x4_add(input, dest, stride, txfm_param); break;
#else
    case DCT_DCT:
#endif
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
#if CONFIG_LGT || CONFIG_DAALA_DCT4
      // LGT only exists in C verson
      av1_iht4x4_16_add_c(input, dest, stride, txfm_param);
      break;
#else
      av1_iht4x4_16_add(input, dest, stride, txfm_param);
      break;
#endif
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
#if CONFIG_LGT || CONFIG_DAALA_DCT4
      av1_iht4x4_16_add_c(input, dest, stride, txfm_param);
      break;
#else
      av1_iht4x4_16_add(input, dest, stride, txfm_param);
      break;
#endif
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
      // Use C version since DST only exists in C code
      av1_iht4x4_16_add_c(input, dest, stride, txfm_param);
      break;
    case IDTX: inv_idtx_add_c(input, dest, stride, 4, 4, tx_type); break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}

static void inv_txfm_add_4x8(const tran_low_t *input, uint8_t *dest, int stride,
                             const TxfmParam *txfm_param) {
#if CONFIG_LGT
  av1_iht4x8_32_add_c(input, dest, stride, txfm_param);
#else
  av1_iht4x8_32_add(input, dest, stride, txfm_param);
#endif
}

static void inv_txfm_add_8x4(const tran_low_t *input, uint8_t *dest, int stride,
                             const TxfmParam *txfm_param) {
#if CONFIG_LGT
  av1_iht8x4_32_add_c(input, dest, stride, txfm_param);
#else
  av1_iht8x4_32_add(input, dest, stride, txfm_param);
#endif
}

// These will be used by the masked-tx experiment in the future.
#if CONFIG_RECT_TX_EXT && (CONFIG_EXT_TX || CONFIG_VAR_TX)
static void inv_txfm_add_4x16(const tran_low_t *input, uint8_t *dest,
                              int stride, const TxfmParam *txfm_param) {
#if CONFIG_LGT
  av1_iht4x16_64_add_c(input, dest, stride, txfm_param);
#else
  av1_iht4x16_64_add(input, dest, stride, txfm_param);
#endif
}

static void inv_txfm_add_16x4(const tran_low_t *input, uint8_t *dest,
                              int stride, const TxfmParam *txfm_param) {
#if CONFIG_LGT
  av1_iht16x4_64_add_c(input, dest, stride, txfm_param);
#else
  av1_iht16x4_64_add(input, dest, stride, txfm_param);
#endif
}

static void inv_txfm_add_8x32(const tran_low_t *input, uint8_t *dest,
                              int stride, const TxfmParam *txfm_param) {
#if CONFIG_LGT
  av1_iht8x32_256_add_c(input, dest, stride, txfm_param);
#else
  av1_iht8x32_256_add(input, dest, stride, txfm_param);
#endif
}

static void inv_txfm_add_32x8(const tran_low_t *input, uint8_t *dest,
                              int stride, const TxfmParam *txfm_param) {
#if CONFIG_LGT
  av1_iht32x8_256_add_c(input, dest, stride, txfm_param);
#else
  av1_iht32x8_256_add(input, dest, stride, txfm_param);
#endif
}
#endif

static void inv_txfm_add_8x16(const tran_low_t *input, uint8_t *dest,
                              int stride, const TxfmParam *txfm_param) {
#if CONFIG_LGT
  av1_iht8x16_128_add_c(input, dest, stride, txfm_param);
#else
  av1_iht8x16_128_add(input, dest, stride, txfm_param);
#endif
}

static void inv_txfm_add_16x8(const tran_low_t *input, uint8_t *dest,
                              int stride, const TxfmParam *txfm_param) {
#if CONFIG_LGT
  av1_iht16x8_128_add_c(input, dest, stride, txfm_param);
#else
  av1_iht16x8_128_add(input, dest, stride, txfm_param);
#endif
}

static void inv_txfm_add_16x32(const tran_low_t *input, uint8_t *dest,
                               int stride, const TxfmParam *txfm_param) {
  av1_iht16x32_512_add(input, dest, stride, txfm_param);
}

static void inv_txfm_add_32x16(const tran_low_t *input, uint8_t *dest,
                               int stride, const TxfmParam *txfm_param) {
  av1_iht32x16_512_add(input, dest, stride, txfm_param);
}

#if CONFIG_TX64X64
static void inv_txfm_add_32x64(const tran_low_t *input, uint8_t *dest,
                               int stride, const TxfmParam *txfm_param) {
  av1_iht32x64_2048_add(input, dest, stride, txfm_param);
}

static void inv_txfm_add_64x32(const tran_low_t *input, uint8_t *dest,
                               int stride, const TxfmParam *txfm_param) {
  av1_iht64x32_2048_add(input, dest, stride, txfm_param);
}
#endif  // CONFIG_TX64X64

static void inv_txfm_add_8x8(const tran_low_t *input, uint8_t *dest, int stride,
                             const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  switch (tx_type) {
#if !CONFIG_DAALA_DCT8
    case DCT_DCT: idct8x8_add(input, dest, stride, txfm_param); break;
#else
    case DCT_DCT:
#endif
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
#if CONFIG_LGT || CONFIG_DAALA_DCT8
      av1_iht8x8_64_add_c(input, dest, stride, txfm_param);
      break;
#else
      av1_iht8x8_64_add(input, dest, stride, txfm_param);
      break;
#endif
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
#if CONFIG_LGT || CONFIG_DAALA_DCT8
      av1_iht8x8_64_add_c(input, dest, stride, txfm_param);
      break;
#else
      av1_iht8x8_64_add(input, dest, stride, txfm_param);
      break;
#endif
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
      // Use C version since DST only exists in C code
      av1_iht8x8_64_add_c(input, dest, stride, txfm_param);
      break;
    case IDTX: inv_idtx_add_c(input, dest, stride, 8, 8, tx_type); break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}

static void inv_txfm_add_16x16(const tran_low_t *input, uint8_t *dest,
                               int stride, const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  switch (tx_type) {
#if !CONFIG_DAALA_DCT16
    case DCT_DCT: idct16x16_add(input, dest, stride, txfm_param); break;
#else
    case DCT_DCT:
#endif
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
#if CONFIG_DAALA_DCT16
      av1_iht16x16_256_add_c(input, dest, stride, txfm_param);
#else
      av1_iht16x16_256_add(input, dest, stride, txfm_param);
#endif  // CONFIG_DAALA_DCT16
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
#if CONFIG_DAALA_DCT16
      av1_iht16x16_256_add_c(input, dest, stride, txfm_param);
#else
      av1_iht16x16_256_add(input, dest, stride, txfm_param);
#endif  // CONFIG_DAALA_DCT16
      break;
    case IDTX: inv_idtx_add_c(input, dest, stride, 16, 16, tx_type); break;
#endif  // CONFIG_EXT_TX
#if CONFIG_MRC_TX
    case MRC_DCT: assert(0 && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
    default: assert(0); break;
  }
}

static void inv_txfm_add_32x32(const tran_low_t *input, uint8_t *dest,
                               int stride, const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  switch (tx_type) {
#if !CONFIG_DAALA_DCT32
    case DCT_DCT: idct32x32_add(input, dest, stride, txfm_param); break;
#else
    case DCT_DCT:
      av1_iht32x32_1024_add_c(input, dest, stride, txfm_param);
      break;
#endif
#if CONFIG_EXT_TX
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
      av1_iht32x32_1024_add_c(input, dest, stride, txfm_param);
      break;
    case IDTX: inv_idtx_add_c(input, dest, stride, 32, 32, tx_type); break;
#endif  // CONFIG_EXT_TX
#if CONFIG_MRC_TX
    case MRC_DCT: imrc32x32_add_c(input, dest, stride, txfm_param); break;
#endif  // CONFIG_MRC_TX
    default: assert(0); break;
  }
}

#if CONFIG_TX64X64
static void inv_txfm_add_64x64(const tran_low_t *input, uint8_t *dest,
                               int stride, const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  assert(tx_type == DCT_DCT);
  switch (tx_type) {
#if !CONFIG_DAALA_DCT64
    case DCT_DCT: idct64x64_add(input, dest, stride, txfm_param); break;
#else
    case DCT_DCT:
#endif
#if CONFIG_EXT_TX
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
      av1_iht64x64_4096_add_c(input, dest, stride, txfm_param);
      break;
    case IDTX: inv_idtx_add_c(input, dest, stride, 64, 64, tx_type); break;
#endif  // CONFIG_EXT_TX
#if CONFIG_MRC_TX
    case MRC_DCT: assert(0 && "Invalid tx type for tx size");
#endif  // CONFIG_MRC_TX
    default: assert(0); break;
  }
}
#endif  // CONFIG_TX64X64

void av1_highbd_iwht4x4_add(const tran_low_t *input, uint8_t *dest, int stride,
                            int eob, int bd) {
  if (eob > 1)
    aom_highbd_iwht4x4_16_add(input, dest, stride, bd);
  else
    aom_highbd_iwht4x4_1_add(input, dest, stride, bd);
}

#if CONFIG_CHROMA_2X2
static void highbd_inv_txfm_add_2x2(const tran_low_t *input, uint8_t *dest,
                                    int stride, const TxfmParam *txfm_param) {
  int eob = txfm_param->eob;
  int bd = txfm_param->bd;
  int lossless = txfm_param->lossless;
  const TX_TYPE tx_type = txfm_param->tx_type;
  tran_high_t a1 = input[0] >> UNIT_QUANT_SHIFT;
  tran_high_t b1 = input[1] >> UNIT_QUANT_SHIFT;
  tran_high_t c1 = input[2] >> UNIT_QUANT_SHIFT;
  tran_high_t d1 = input[3] >> UNIT_QUANT_SHIFT;

  tran_high_t a2 = a1 + c1;
  tran_high_t b2 = b1 + d1;
  tran_high_t c2 = a1 - c1;
  tran_high_t d2 = b1 - d1;

  uint16_t *dst = CONVERT_TO_SHORTPTR(dest);

  (void)tx_type;
  (void)lossless;
  (void)eob;

  a1 = (a2 + b2) >> 2;
  b1 = (a2 - b2) >> 2;
  c1 = (c2 + d2) >> 2;
  d1 = (c2 - d2) >> 2;

  dst[0] = highbd_clip_pixel_add(dst[0], a1, bd);
  dst[1] = highbd_clip_pixel_add(dst[1], b1, bd);
  dst[stride] = highbd_clip_pixel_add(dst[stride], c1, bd);
  dst[stride + 1] = highbd_clip_pixel_add(dst[stride + 1], d1, bd);
}
#endif

static const int32_t *cast_to_int32(const tran_low_t *input) {
  assert(sizeof(int32_t) == sizeof(tran_low_t));
  return (const int32_t *)input;
}

void av1_highbd_inv_txfm_add_4x4(const tran_low_t *input, uint8_t *dest,
                                 int stride, const TxfmParam *txfm_param) {
  int eob = txfm_param->eob;
  int bd = txfm_param->bd;
  int lossless = txfm_param->lossless;
  const int32_t *src = cast_to_int32(input);
  const TX_TYPE tx_type = txfm_param->tx_type;
  if (lossless) {
    assert(tx_type == DCT_DCT);
    av1_highbd_iwht4x4_add(input, dest, stride, eob, bd);
    return;
  }
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
      av1_inv_txfm2d_add_4x4(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                             bd);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
      av1_inv_txfm2d_add_4x4(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                             bd);
      break;
    // use the c version for anything including identity for now
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
    case IDTX:
      av1_inv_txfm2d_add_4x4_c(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                               bd);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}

void av1_highbd_inv_txfm_add_4x8(const tran_low_t *input, uint8_t *dest,
                                 int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_4x8_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                           txfm_param->tx_type, txfm_param->bd);
}

void av1_highbd_inv_txfm_add_8x4(const tran_low_t *input, uint8_t *dest,
                                 int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_8x4_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                           txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_8x16(const tran_low_t *input, uint8_t *dest,
                                     int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_8x16_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                            txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_16x8(const tran_low_t *input, uint8_t *dest,
                                     int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_16x8_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                            txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_16x32(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_16x32_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                             txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_32x16(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_32x16_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                             txfm_param->tx_type, txfm_param->bd);
}

#if CONFIG_TX64X64
static void highbd_inv_txfm_add_32x64(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_32x64_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                             txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_64x32(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_64x32_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                             txfm_param->tx_type, txfm_param->bd);
}
#endif  // CONFIG_TX64X64

static void highbd_inv_txfm_add_8x8(const tran_low_t *input, uint8_t *dest,
                                    int stride, const TxfmParam *txfm_param) {
  int bd = txfm_param->bd;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int32_t *src = cast_to_int32(input);
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
      av1_inv_txfm2d_add_8x8(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                             bd);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
      av1_inv_txfm2d_add_8x8(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                             bd);
      break;
    // use the c version for anything including identity for now
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
    case IDTX:
      av1_inv_txfm2d_add_8x8_c(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                               bd);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0);
  }
}

static void highbd_inv_txfm_add_16x16(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  int bd = txfm_param->bd;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int32_t *src = cast_to_int32(input);
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
      av1_inv_txfm2d_add_16x16(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                               bd);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
      av1_inv_txfm2d_add_16x16(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                               bd);
      break;
    // use the c version for anything including identity for now
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
    case IDTX:
      av1_inv_txfm2d_add_16x16_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                                 tx_type, bd);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0);
  }
}

static void highbd_inv_txfm_add_32x32(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  int bd = txfm_param->bd;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int32_t *src = cast_to_int32(input);
  switch (tx_type) {
    case DCT_DCT:
      av1_inv_txfm2d_add_32x32(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                               bd);
      break;

    // The optimised version only supports DCT_DCT, so force use of
    // the C version for all other transform types.
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
    case IDTX:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
#endif  // CONFIG_EXT_TX
      av1_inv_txfm2d_add_32x32_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                                 tx_type, bd);
      break;

    default: assert(0);
  }
}

#if CONFIG_TX64X64
static void highbd_inv_txfm_add_64x64(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  int bd = txfm_param->bd;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int32_t *src = cast_to_int32(input);
  switch (tx_type) {
    case DCT_DCT:
      av1_inv_txfm2d_add_64x64(src, CONVERT_TO_SHORTPTR(dest), stride, DCT_DCT,
                               bd);
      break;
#if CONFIG_EXT_TX
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
      // TODO(sarahparker)
      // I've deleted the 64x64 implementations that existed in lieu
      // of adst, flipadst and identity for simplicity but will bring back
      // in a later change. This shouldn't impact performance since
      // DCT_DCT is the only extended type currently allowed for 64x64,
      // as dictated by get_ext_tx_set_type in blockd.h.
      av1_inv_txfm2d_add_64x64_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                                 DCT_DCT, bd);
      break;
    case IDTX:
      highbd_inv_idtx_add_c(input, dest, stride, 64, 64, tx_type, bd);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}
#endif  // CONFIG_TX64X64

void av1_inv_txfm_add(const tran_low_t *input, uint8_t *dest, int stride,
                      TxfmParam *txfm_param) {
  const TX_SIZE tx_size = txfm_param->tx_size;
#if CONFIG_LGT_FROM_PRED
  if (txfm_param->use_lgt) {
    assert(is_lgt_allowed(txfm_param->mode, tx_size));
    ilgt2d_from_pred_add(input, dest, stride, txfm_param);
    return;
  }
#endif  // CONFIG_LGT_FROM_PRED
  switch (tx_size) {
#if CONFIG_TX64X64
    case TX_64X64: inv_txfm_add_64x64(input, dest, stride, txfm_param); break;
#endif  // CONFIG_TX64X64
    case TX_32X32: inv_txfm_add_32x32(input, dest, stride, txfm_param); break;
    case TX_16X16: inv_txfm_add_16x16(input, dest, stride, txfm_param); break;
    case TX_8X8: inv_txfm_add_8x8(input, dest, stride, txfm_param); break;
    case TX_4X8: inv_txfm_add_4x8(input, dest, stride, txfm_param); break;
    case TX_8X4: inv_txfm_add_8x4(input, dest, stride, txfm_param); break;
    case TX_8X16: inv_txfm_add_8x16(input, dest, stride, txfm_param); break;
    case TX_16X8: inv_txfm_add_16x8(input, dest, stride, txfm_param); break;
    case TX_16X32: inv_txfm_add_16x32(input, dest, stride, txfm_param); break;
    case TX_32X16: inv_txfm_add_32x16(input, dest, stride, txfm_param); break;
#if CONFIG_TX64X64
    case TX_64X32: inv_txfm_add_64x32(input, dest, stride, txfm_param); break;
    case TX_32X64: inv_txfm_add_32x64(input, dest, stride, txfm_param); break;
#endif  // CONFIG_TX64X64
    case TX_4X4:
      // this is like av1_short_idct4x4 but has a special case around eob<=1
      // which is significant (not just an optimization) for the lossless
      // case.
      inv_txfm_add_4x4(input, dest, stride, txfm_param);
      break;
#if CONFIG_CHROMA_2X2
    case TX_2X2: inv_txfm_add_2x2(input, dest, stride, txfm_param); break;
#endif
#if CONFIG_RECT_TX_EXT && (CONFIG_EXT_TX || CONFIG_VAR_TX)
    case TX_32X8: inv_txfm_add_32x8(input, dest, stride, txfm_param); break;
    case TX_8X32: inv_txfm_add_8x32(input, dest, stride, txfm_param); break;
    case TX_16X4: inv_txfm_add_16x4(input, dest, stride, txfm_param); break;
    case TX_4X16: inv_txfm_add_4x16(input, dest, stride, txfm_param); break;
#endif
    default: assert(0 && "Invalid transform size"); break;
  }
}

static void init_txfm_param(const MACROBLOCKD *xd, TX_SIZE tx_size,
                            TX_TYPE tx_type, int eob, TxfmParam *txfm_param) {
  txfm_param->tx_type = tx_type;
  txfm_param->tx_size = tx_size;
  txfm_param->eob = eob;
  txfm_param->lossless = xd->lossless[xd->mi[0]->mbmi.segment_id];
  txfm_param->bd = xd->bd;
#if CONFIG_LGT
  txfm_param->is_inter = is_inter_block(&xd->mi[0]->mbmi);
#endif
#if CONFIG_LGT_FROM_PRED
  txfm_param->use_lgt = xd->mi[0]->mbmi.use_lgt;
#endif
#if CONFIG_ADAPT_SCAN
  txfm_param->eob_threshold =
      (const int16_t *)&xd->eob_threshold_md[tx_size][tx_type][0];
#endif
}

#if !CONFIG_TXMG
typedef void (*InvTxfmFunc)(const tran_low_t *dqcoeff, uint8_t *dst, int stride,
                            TxfmParam *txfm_param);

static InvTxfmFunc inv_txfm_func[2] = { av1_inv_txfm_add,
                                        av1_highbd_inv_txfm_add };
#endif

void av1_inverse_transform_block(const MACROBLOCKD *xd,
                                 const tran_low_t *dqcoeff,
#if CONFIG_LGT_FROM_PRED
                                 PREDICTION_MODE mode,
#endif
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                                 uint8_t *mrc_mask,
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                                 TX_TYPE tx_type, TX_SIZE tx_size, uint8_t *dst,
                                 int stride, int eob) {
  if (!eob) return;
#if CONFIG_PVQ
  const BLOCK_SIZE tx_bsize = txsize_to_bsize[tx_size];
  const int txb_width = block_size_wide[tx_bsize];
  const int txb_height = block_size_high[tx_bsize];
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    for (int r = 0; r < txb_height; r++)
      for (int c = 0; c < txb_width; c++)
        CONVERT_TO_SHORTPTR(dst)[r * stride + c] = 0;
  } else {
    for (int r = 0; r < txb_height; r++)
      for (int c = 0; c < txb_width; c++) dst[r * stride + c] = 0;
  }
#endif  // CONFIG_PVQ
  TxfmParam txfm_param;
  init_txfm_param(xd, tx_size, tx_type, eob, &txfm_param);
#if CONFIG_LGT || CONFIG_MRC_TX
  txfm_param.is_inter = is_inter_block(&xd->mi[0]->mbmi);
#endif  // CONFIG_LGT || CONFIG_MRC_TX
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
  txfm_param.mask = mrc_mask;
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
#if CONFIG_LGT_FROM_PRED || CONFIG_MRC_TX
  txfm_param.dst = dst;
  txfm_param.stride = stride;
#if CONFIG_LGT_FROM_PRED
  txfm_param.mode = mode;
#endif  // CONFIG_LGT_FROM_PRED
#endif  // CONFIG_LGT_FROM_PRED || CONFIG_MRC_TX

  const int is_hbd = get_bitdepth_data_path_index(xd);
#if CONFIG_TXMG
  if (is_hbd) {
    av1_highbd_inv_txfm_add(dqcoeff, dst, stride, &txfm_param);
  } else {
    DECLARE_ALIGNED(16, uint16_t, tmp[MAX_TX_SQUARE]);
    int tmp_stride = MAX_TX_SIZE;
    int w = tx_size_wide[tx_size];
    int h = tx_size_high[tx_size];
    for (int r = 0; r < h; ++r) {
      for (int c = 0; c < w; ++c) {
        tmp[r * tmp_stride + c] = dst[r * stride + c];
      }
    }

    av1_highbd_inv_txfm_add(dqcoeff, CONVERT_TO_BYTEPTR(tmp), tmp_stride,
                            &txfm_param);

    for (int r = 0; r < h; ++r) {
      for (int c = 0; c < w; ++c) {
        dst[r * stride + c] = (uint8_t)tmp[r * tmp_stride + c];
      }
    }
  }
#else  // CONFIG_TXMG
  inv_txfm_func[is_hbd](dqcoeff, dst, stride, &txfm_param);
#endif  // CONFIG_TXMG
}

void av1_inverse_transform_block_facade(MACROBLOCKD *xd, int plane, int block,
                                        int blk_row, int blk_col, int eob) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  tran_low_t *dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
  uint8_t *mrc_mask = BLOCK_OFFSET(xd->mrc_mask, block);
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  const int dst_stride = pd->dst.stride;
  uint8_t *dst =
      &pd->dst.buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];
  av1_inverse_transform_block(xd, dqcoeff,
#if CONFIG_LGT_FROM_PRED
                              xd->mi[0]->mbmi.mode,
#endif  // CONFIG_LGT_FROM_PRED
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                              mrc_mask,
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                              tx_type, tx_size, dst, dst_stride, eob);
}

void av1_highbd_inv_txfm_add(const tran_low_t *input, uint8_t *dest, int stride,
                             TxfmParam *txfm_param) {
  const TX_SIZE tx_size = txfm_param->tx_size;
  switch (tx_size) {
#if CONFIG_TX64X64
    case TX_64X64:
      highbd_inv_txfm_add_64x64(input, dest, stride, txfm_param);
      break;
#endif  // CONFIG_TX64X64
    case TX_32X32:
      highbd_inv_txfm_add_32x32(input, dest, stride, txfm_param);
      break;
    case TX_16X16:
      highbd_inv_txfm_add_16x16(input, dest, stride, txfm_param);
      break;
    case TX_8X8:
      highbd_inv_txfm_add_8x8(input, dest, stride, txfm_param);
      break;
    case TX_4X8:
      av1_highbd_inv_txfm_add_4x8(input, dest, stride, txfm_param);
      break;
    case TX_8X4:
      av1_highbd_inv_txfm_add_8x4(input, dest, stride, txfm_param);
      break;
    case TX_8X16:
      highbd_inv_txfm_add_8x16(input, dest, stride, txfm_param);
      break;
    case TX_16X8:
      highbd_inv_txfm_add_16x8(input, dest, stride, txfm_param);
      break;
    case TX_16X32:
      highbd_inv_txfm_add_16x32(input, dest, stride, txfm_param);
      break;
    case TX_32X16:
      highbd_inv_txfm_add_32x16(input, dest, stride, txfm_param);
      break;
#if CONFIG_TX64X64
    case TX_64X32:
      highbd_inv_txfm_add_64x32(input, dest, stride, txfm_param);
      break;
    case TX_32X64:
      highbd_inv_txfm_add_32x64(input, dest, stride, txfm_param);
      break;
#endif  // CONFIG_TX64X64
    case TX_4X4:
      // this is like av1_short_idct4x4 but has a special case around eob<=1
      // which is significant (not just an optimization) for the lossless
      // case.
      av1_highbd_inv_txfm_add_4x4(input, dest, stride, txfm_param);
      break;
#if CONFIG_CHROMA_2X2
    case TX_2X2:
      highbd_inv_txfm_add_2x2(input, dest, stride, txfm_param);
      break;
#endif
    default: assert(0 && "Invalid transform size"); break;
  }
}
