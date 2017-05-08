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
#include "av1/common/av1_inv_txfm2d_cfg.h"
#include "av1/common/blockd.h"
#include "av1/common/enums.h"
#include "av1/common/idct.h"

int av1_get_tx_scale(const TX_SIZE tx_size) {
  if (txsize_sqr_up_map[tx_size] == TX_32X32) return 1;
#if CONFIG_TX64X64
  else if (txsize_sqr_up_map[tx_size] == TX_64X64)
    return 2;
#endif  // CONFIG_TX64X64
  else
    return 0;
}

// NOTE: The implementation of all inverses need to be aware of the fact
// that input and output could be the same buffer.

#if CONFIG_EXT_TX
static void iidtx4_c(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 4; ++i)
    output[i] = (tran_low_t)dct_const_round_shift(input[i] * Sqrt2);
}

static void iidtx8_c(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 8; ++i) output[i] = input[i] * 2;
}

static void iidtx16_c(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 16; ++i)
    output[i] = (tran_low_t)dct_const_round_shift(input[i] * 2 * Sqrt2);
}

static void iidtx32_c(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 32; ++i) output[i] = input[i] * 4;
}

#if CONFIG_TX64X64
static void iidtx64_c(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 64; ++i)
    output[i] = (tran_low_t)dct_const_round_shift(input[i] * 4 * Sqrt2);
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

#if CONFIG_TX64X64
static void idct64_col_c(const tran_low_t *input, tran_low_t *output) {
  int32_t in[64], out[64];
  int i;
  for (i = 0; i < 64; ++i) in[i] = (int32_t)input[i];
  av1_idct64_new(in, out, inv_cos_bit_col_dct_dct_64,
                 inv_stage_range_col_dct_dct_64);
  for (i = 0; i < 64; ++i) output[i] = (tran_low_t)out[i];
}

static void idct64_row_c(const tran_low_t *input, tran_low_t *output) {
  int32_t in[64], out[64];
  int i;
  for (i = 0; i < 64; ++i) in[i] = (int32_t)input[i];
  av1_idct64_new(in, out, inv_cos_bit_row_dct_dct_64,
                 inv_stage_range_row_dct_dct_64);
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

#if CONFIG_HIGHBITDEPTH
static void highbd_idct4(const tran_low_t *input, tran_low_t *output,
                         const int8_t *cos_bit, const int8_t *stage_range,
                         int bd) {
  (void)bd;
  av1_idct4_new(input, output, cos_bit, stage_range);
}

static void highbd_idct8(const tran_low_t *input, tran_low_t *output,
                         const int8_t *cos_bit, const int8_t *stage_range,
                         int bd) {
  (void)bd;
  av1_idct8_new(input, output, cos_bit, stage_range);
}

static void highbd_idct16(const tran_low_t *input, tran_low_t *output,
                          const int8_t *cos_bit, const int8_t *stage_range,
                          int bd) {
  (void)bd;
  av1_idct16_new(input, output, cos_bit, stage_range);
}

static void highbd_idct32(const tran_low_t *input, tran_low_t *output,
                          const int8_t *cos_bit, const int8_t *stage_range,
                          int bd) {
  (void)bd;
  av1_idct32_new(input, output, cos_bit, stage_range);
}

static void highbd_iadst4(const tran_low_t *input, tran_low_t *output,
                          const int8_t *cos_bit, const int8_t *stage_range,
                          int bd) {
  (void)bd;
  av1_iadst4_new(input, output, cos_bit, stage_range);
}

static void highbd_iadst8(const tran_low_t *input, tran_low_t *output,
                          const int8_t *cos_bit, const int8_t *stage_range,
                          int bd) {
  (void)bd;
  av1_iadst8_new(input, output, cos_bit, stage_range);
}

static void highbd_iadst16(const tran_low_t *input, tran_low_t *output,
                           const int8_t *cos_bit, const int8_t *stage_range,
                           int bd) {
  (void)bd;
  av1_iadst16_new(input, output, cos_bit, stage_range);
}

#if CONFIG_EXT_TX
static void highbd_iidtx4_c(const tran_low_t *input, tran_low_t *output,
                            const int8_t *cos_bit, const int8_t *stage_range,
                            int bd) {
  int i;
  (void)cos_bit;
  (void)stage_range;
  for (i = 0; i < 4; ++i)
    output[i] = HIGHBD_WRAPLOW(dct_const_round_shift(input[i] * Sqrt2), bd);
}

static void highbd_iidtx8_c(const tran_low_t *input, tran_low_t *output,
                            const int8_t *cos_bit, const int8_t *stage_range,
                            int bd) {
  int i;
  (void)bd;
  (void)cos_bit;
  (void)stage_range;
  for (i = 0; i < 8; ++i) output[i] = input[i] * 2;
}

static void highbd_iidtx16_c(const tran_low_t *input, tran_low_t *output,
                             const int8_t *cos_bit, const int8_t *stage_range,
                             int bd) {
  int i;
  (void)cos_bit;
  (void)stage_range;
  for (i = 0; i < 16; ++i)
    output[i] = HIGHBD_WRAPLOW(dct_const_round_shift(input[i] * 2 * Sqrt2), bd);
}

static void highbd_iidtx32_c(const tran_low_t *input, tran_low_t *output,
                             const int8_t *cos_bit, const int8_t *stage_range,
                             int bd) {
  int i;
  (void)bd;
  (void)cos_bit;
  (void)stage_range;
  for (i = 0; i < 32; ++i) output[i] = input[i] * 4;
}
#endif  // CONFIG_EXT_TX

static void highbd_ihalfright32_c(const tran_low_t *input, tran_low_t *output,
                                  const int8_t *cos_bit,
                                  const int8_t *stage_range, int bd) {
  int i;
  tran_low_t inputhalf[16];
  // Multiply input by sqrt(2)
  for (i = 0; i < 16; ++i) {
    inputhalf[i] = HIGHBD_WRAPLOW(dct_const_round_shift(input[i] * Sqrt2), bd);
  }
  for (i = 0; i < 16; ++i) {
    output[i] = input[16 + i] * 4;
  }
  highbd_idct16(inputhalf, output + 16, cos_bit, stage_range, bd);
  // Note overall scaling factor is 4 times orthogonal
}

#if CONFIG_EXT_TX
#if CONFIG_TX64X64
static void highbd_iidtx64_c(const tran_low_t *input, tran_low_t *output,
                             const int8_t *cos_bit, const int8_t *stage_range,
                             int bd) {
  (void)cos_bit;
  (void)stage_range;
  int i;
  for (i = 0; i < 64; ++i)
    output[i] = HIGHBD_WRAPLOW(dct_const_round_shift(input[i] * 4 * Sqrt2), bd);
}
#endif  // CONFIG_TX64X64
#endif  // CONFIG_EXT_TX

#if CONFIG_TX64X64
// For use in lieu of ADST
static void highbd_ihalfright64_c(const tran_low_t *input, tran_low_t *output,
                                  const int8_t *cos_bit,
                                  const int8_t *stage_range, int bd) {
  int i;
  tran_low_t inputhalf[32];
  // Multiply input by sqrt(2)
  for (i = 0; i < 32; ++i) {
    inputhalf[i] = HIGHBD_WRAPLOW(dct_const_round_shift(input[i] * Sqrt2), bd);
  }
  for (i = 0; i < 32; ++i) {
    output[i] =
        HIGHBD_WRAPLOW(dct_const_round_shift(input[32 + i] * 4 * Sqrt2), bd);
  }
  highbd_idct32(inputhalf, output + 32, cos_bit, stage_range, bd);
  // Note overall scaling factor is 4 * sqrt(2)  times orthogonal
}

static void highbd_idct64_col_c(const tran_low_t *input, tran_low_t *output,
                                const int8_t *cos_bit,
                                const int8_t *stage_range, int bd) {
  int32_t in[64], out[64];
  int i;
  (void)cos_bit;
  (void)stage_range;
  (void)bd;
  for (i = 0; i < 64; ++i) in[i] = (int32_t)input[i];
  av1_idct64_new(in, out, inv_cos_bit_col_dct_dct_64,
                 inv_stage_range_col_dct_dct_64);
  for (i = 0; i < 64; ++i) output[i] = (tran_low_t)out[i];
}

static void highbd_idct64_row_c(const tran_low_t *input, tran_low_t *output,
                                const int8_t *cos_bit,
                                const int8_t *stage_range, int bd) {
  int32_t in[64], out[64];
  int i;
  (void)cos_bit;
  (void)stage_range;
  (void)bd;
  for (i = 0; i < 64; ++i) in[i] = (int32_t)input[i];
  av1_idct64_new(in, out, inv_cos_bit_row_dct_dct_64,
                 inv_stage_range_row_dct_dct_64);
  for (i = 0; i < 64; ++i) output[i] = (tran_low_t)out[i];
}
#endif  // CONFIG_TX64X64
#endif  // CONFIG_HIGHBITDEPTH

// Inverse identity transform and add.
#if CONFIG_EXT_TX
static void inv_idtx_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                           int bs, int tx_type) {
  int r, c;
  const int shift = bs < 32 ? 3 : (bs < 64 ? 2 : 1);
  if (tx_type == IDTX) {
    for (r = 0; r < bs; ++r) {
      for (c = 0; c < bs; ++c)
        dest[c] = clip_pixel_add(dest[c], input[c] >> shift);
      dest += stride;
      input += bs;
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
                               int *sstride, int tx_type, int sizey,
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
#if CONFIG_EXT_TX
static void highbd_inv_idtx_add_c(const tran_low_t *input, uint8_t *dest8,
                                  int stride, int bs, int tx_type, int bd) {
  int r, c;
  const int shift = bs < 32 ? 3 : 2;
  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  if (tx_type == IDTX) {
    for (r = 0; r < bs; ++r) {
      for (c = 0; c < bs; ++c)
        dest[c] = highbd_clip_pixel_add(dest[c], input[c] >> shift, bd);
      dest += stride;
      input += bs;
    }
  }
}

static void maybe_flip_strides16(uint16_t **dst, int *dstride, tran_low_t **src,
                                 int *sstride, int tx_type, int sizey,
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
#endif  // CONFIG_HIGHBITDEPTH

void av1_iht4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                         int tx_type) {
  static const transform_2d IHT_4[] = {
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
#endif                               // CONFIG_EXT_TX
  };

  int i, j;
  tran_low_t tmp;
  tran_low_t out[4][4];
  tran_low_t *outp = &out[0][0];
  int outstride = 4;

  // inverse transform row vectors
  for (i = 0; i < 4; ++i) {
    IHT_4[tx_type].rows(input, out[i]);
    input += 4;
  }

  // transpose
  for (i = 1; i < 4; i++) {
    for (j = 0; j < i; j++) {
      tmp = out[i][j];
      out[i][j] = out[j][i];
      out[j][i] = tmp;
    }
  }

  // inverse transform column vectors
  for (i = 0; i < 4; ++i) {
    IHT_4[tx_type].cols(out[i], out[i]);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, 4, 4);
#endif

  // Sum with the destination
  for (i = 0; i < 4; ++i) {
    for (j = 0; j < 4; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 4));
    }
  }
}

void av1_iht4x8_32_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                         int tx_type) {
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
  tran_low_t out[4][8], outtmp[4];
  tran_low_t *outp = &out[0][0];
  int outstride = n2;

  // inverse transform row vectors and transpose
  for (i = 0; i < n2; ++i) {
    IHT_4x8[tx_type].rows(input, outtmp);
    for (j = 0; j < n; ++j)
      out[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n;
  }

  // inverse transform column vectors
  for (i = 0; i < n; ++i) {
    IHT_4x8[tx_type].cols(out[i], out[i]);
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
                         int tx_type) {
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
  tran_low_t out[8][4], outtmp[8];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  // inverse transform row vectors and transpose
  for (i = 0; i < n; ++i) {
    IHT_8x4[tx_type].rows(input, outtmp);
    for (j = 0; j < n2; ++j)
      out[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n2;
  }

  // inverse transform column vectors
  for (i = 0; i < n2; ++i) {
    IHT_8x4[tx_type].cols(out[i], out[i]);
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
                          int tx_type) {
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
  tran_low_t out[4][16], outtmp[4];
  tran_low_t *outp = &out[0][0];
  int outstride = n4;

  // inverse transform row vectors and transpose
  for (i = 0; i < n4; ++i) {
    IHT_4x16[tx_type].rows(input, outtmp);
    for (j = 0; j < n; ++j) out[j][i] = outtmp[j];
    input += n;
  }

  // inverse transform column vectors
  for (i = 0; i < n; ++i) IHT_4x16[tx_type].cols(out[i], out[i]);

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
                          int tx_type) {
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
  tran_low_t out[16][4], outtmp[16];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  // inverse transform row vectors and transpose
  for (i = 0; i < n; ++i) {
    IHT_16x4[tx_type].rows(input, outtmp);
    for (j = 0; j < n4; ++j) out[j][i] = outtmp[j];
    input += n4;
  }

  // inverse transform column vectors
  for (i = 0; i < n4; ++i) IHT_16x4[tx_type].cols(out[i], out[i]);

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
                           int tx_type) {
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
  tran_low_t out[8][16], outtmp[8];
  tran_low_t *outp = &out[0][0];
  int outstride = n2;

  // inverse transform row vectors and transpose
  for (i = 0; i < n2; ++i) {
    IHT_8x16[tx_type].rows(input, outtmp);
    for (j = 0; j < n; ++j)
      out[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n;
  }

  // inverse transform column vectors
  for (i = 0; i < n; ++i) {
    IHT_8x16[tx_type].cols(out[i], out[i]);
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
                           int tx_type) {
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
  tran_low_t out[16][8], outtmp[16];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  // inverse transform row vectors and transpose
  for (i = 0; i < n; ++i) {
    IHT_16x8[tx_type].rows(input, outtmp);
    for (j = 0; j < n2; ++j)
      out[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n2;
  }

  // inverse transform column vectors
  for (i = 0; i < n2; ++i) {
    IHT_16x8[tx_type].cols(out[i], out[i]);
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
                           int tx_type) {
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
  tran_low_t out[8][32], outtmp[8];
  tran_low_t *outp = &out[0][0];
  int outstride = n4;

  // inverse transform row vectors and transpose
  for (i = 0; i < n4; ++i) {
    IHT_8x32[tx_type].rows(input, outtmp);
    for (j = 0; j < n; ++j) out[j][i] = outtmp[j];
    input += n;
  }

  // inverse transform column vectors
  for (i = 0; i < n; ++i) IHT_8x32[tx_type].cols(out[i], out[i]);

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
                           int tx_type) {
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
  tran_low_t out[32][8], outtmp[32];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  // inverse transform row vectors and transpose
  for (i = 0; i < n; ++i) {
    IHT_32x8[tx_type].rows(input, outtmp);
    for (j = 0; j < n4; ++j) out[j][i] = outtmp[j];
    input += n4;
  }

  // inverse transform column vectors
  for (i = 0; i < n4; ++i) IHT_32x8[tx_type].cols(out[i], out[i]);

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
                            int tx_type) {
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
  tran_low_t out[16][32], outtmp[16];
  tran_low_t *outp = &out[0][0];
  int outstride = n2;

  // inverse transform row vectors and transpose
  for (i = 0; i < n2; ++i) {
    IHT_16x32[tx_type].rows(input, outtmp);
    for (j = 0; j < n; ++j)
      out[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n;
  }

  // inverse transform column vectors
  for (i = 0; i < n; ++i) {
    IHT_16x32[tx_type].cols(out[i], out[i]);
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

void av1_iht32x16_512_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                            int tx_type) {
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
  tran_low_t out[32][16], outtmp[32];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  // inverse transform row vectors and transpose
  for (i = 0; i < n; ++i) {
    IHT_32x16[tx_type].rows(input, outtmp);
    for (j = 0; j < n2; ++j)
      out[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n2;
  }

  // inverse transform column vectors
  for (i = 0; i < n2; ++i) {
    IHT_32x16[tx_type].cols(out[i], out[i]);
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

void av1_iht8x8_64_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                         int tx_type) {
  static const transform_2d IHT_8[] = {
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
#endif                               // CONFIG_EXT_TX
  };

  int i, j;
  tran_low_t tmp;
  tran_low_t out[8][8];
  tran_low_t *outp = &out[0][0];
  int outstride = 8;

  // inverse transform row vectors
  for (i = 0; i < 8; ++i) {
    IHT_8[tx_type].rows(input, out[i]);
    input += 8;
  }

  // transpose
  for (i = 1; i < 8; i++) {
    for (j = 0; j < i; j++) {
      tmp = out[i][j];
      out[i][j] = out[j][i];
      out[j][i] = tmp;
    }
  }

  // inverse transform column vectors
  for (i = 0; i < 8; ++i) {
    IHT_8[tx_type].cols(out[i], out[i]);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, 8, 8);
#endif

  // Sum with the destination
  for (i = 0; i < 8; ++i) {
    for (j = 0; j < 8; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
    }
  }
}

void av1_iht16x16_256_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                            int tx_type) {
  static const transform_2d IHT_16[] = {
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
#endif                                 // CONFIG_EXT_TX
  };

  int i, j;
  tran_low_t tmp;
  tran_low_t out[16][16];
  tran_low_t *outp = &out[0][0];
  int outstride = 16;

  // inverse transform row vectors
  for (i = 0; i < 16; ++i) {
    IHT_16[tx_type].rows(input, out[i]);
    input += 16;
  }

  // transpose
  for (i = 1; i < 16; i++) {
    for (j = 0; j < i; j++) {
      tmp = out[i][j];
      out[i][j] = out[j][i];
      out[j][i] = tmp;
    }
  }

  // inverse transform column vectors
  for (i = 0; i < 16; ++i) {
    IHT_16[tx_type].cols(out[i], out[i]);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, 16, 16);
#endif

  // Sum with the destination
  for (i = 0; i < 16; ++i) {
    for (j = 0; j < 16; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}

#if CONFIG_EXT_TX
void av1_iht32x32_1024_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                             int tx_type) {
  static const transform_2d IHT_32[] = {
    { aom_idct32_c, aom_idct32_c },      // DCT_DCT
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
  };

  int i, j;
  tran_low_t tmp;
  tran_low_t out[32][32];
  tran_low_t *outp = &out[0][0];
  int outstride = 32;

  // inverse transform row vectors
  for (i = 0; i < 32; ++i) {
    IHT_32[tx_type].rows(input, out[i]);
    input += 32;
  }

  // transpose
  for (i = 1; i < 32; i++) {
    for (j = 0; j < i; j++) {
      tmp = out[i][j];
      out[i][j] = out[j][i];
      out[j][i] = tmp;
    }
  }

  // inverse transform column vectors
  for (i = 0; i < 32; ++i) {
    IHT_32[tx_type].cols(out[i], out[i]);
  }

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, 32, 32);

  // Sum with the destination
  for (i = 0; i < 32; ++i) {
    for (j = 0; j < 32; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}
#endif  // CONFIG_EXT_TX

#if CONFIG_TX64X64
void av1_iht64x64_4096_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                             int tx_type) {
  static const transform_2d IHT_64[] = {
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
#endif                                   // CONFIG_EXT_TX
  };

  int i, j;
  tran_low_t tmp;
  tran_low_t out[64][64];
  tran_low_t *outp = &out[0][0];
  int outstride = 64;

  // inverse transform row vectors
  for (i = 0; i < 64; ++i) {
    IHT_64[tx_type].rows(input, out[i]);
    for (j = 0; j < 64; ++j) out[i][j] = ROUND_POWER_OF_TWO(out[i][j], 1);
    input += 64;
  }

  // transpose
  for (i = 1; i < 64; i++) {
    for (j = 0; j < i; j++) {
      tmp = out[i][j];
      out[i][j] = out[j][i];
      out[j][i] = tmp;
    }
  }

  // inverse transform column vectors
  for (i = 0; i < 64; ++i) {
    IHT_64[tx_type].cols(out[i], out[i]);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, 64, 64);
#endif  // CONFIG_EXT_TX

  // Sum with the destination
  for (i = 0; i < 64; ++i) {
    for (j = 0; j < 64; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
    }
  }
}
#endif  // CONFIG_TX64X64

// idct
void av1_idct4x4_add(const tran_low_t *input, uint8_t *dest, int stride,
                     int eob) {
  if (eob > 1)
    aom_idct4x4_16_add(input, dest, stride);
  else
    aom_idct4x4_1_add(input, dest, stride);
}

void av1_iwht4x4_add(const tran_low_t *input, uint8_t *dest, int stride,
                     int eob) {
  if (eob > 1)
    aom_iwht4x4_16_add(input, dest, stride);
  else
    aom_iwht4x4_1_add(input, dest, stride);
}

static void idct8x8_add(const tran_low_t *input, uint8_t *dest, int stride,
                        int eob) {
  // If dc is 1, then input[0] is the reconstructed value, do not need
  // dequantization. Also, when dc is 1, dc is counted in eobs, namely eobs >=1.

  // The calculation can be simplified if there are not many non-zero dct
  // coefficients. Use eobs to decide what to do.
  // TODO(yunqingwang): "eobs = 1" case is also handled in av1_short_idct8x8_c.
  // Combine that with code here.
  if (eob == 1)
    // DC only DCT coefficient
    aom_idct8x8_1_add(input, dest, stride);
#if !CONFIG_ADAPT_SCAN
  else if (eob <= 12)
    aom_idct8x8_12_add(input, dest, stride);
#endif
  else
    aom_idct8x8_64_add(input, dest, stride);
}

static void idct16x16_add(const tran_low_t *input, uint8_t *dest, int stride,
                          int eob) {
  /* The calculation can be simplified if there are not many non-zero dct
   * coefficients. Use eobs to separate different cases. */
  if (eob == 1) /* DC only DCT coefficient. */
    aom_idct16x16_1_add(input, dest, stride);
#if !CONFIG_ADAPT_SCAN
  else if (eob <= 10)
    aom_idct16x16_10_add(input, dest, stride);
#endif
  else
    aom_idct16x16_256_add(input, dest, stride);
}

static void idct32x32_add(const tran_low_t *input, uint8_t *dest, int stride,
                          int eob) {
  if (eob == 1) aom_idct32x32_1_add(input, dest, stride);
#if !CONFIG_ADAPT_SCAN
  else if (eob <= 34)
    // non-zero coeff only in upper-left 8x8
    aom_idct32x32_34_add(input, dest, stride);
#endif
  else
    aom_idct32x32_1024_add(input, dest, stride);
}

#if CONFIG_TX64X64
static void idct64x64_add(const tran_low_t *input, uint8_t *dest, int stride,
                          int eob) {
  (void)eob;
  av1_iht64x64_4096_add(input, dest, stride, DCT_DCT);
}
#endif  // CONFIG_TX64X64

#if CONFIG_CB4X4
static void inv_txfm_add_2x2(const tran_low_t *input, uint8_t *dest, int stride,
                             int eob, TX_TYPE tx_type, int lossless) {
  tran_high_t a1 = input[0] >> UNIT_QUANT_SHIFT;
  tran_high_t b1 = input[1] >> UNIT_QUANT_SHIFT;
  tran_high_t c1 = input[2] >> UNIT_QUANT_SHIFT;
  tran_high_t d1 = input[3] >> UNIT_QUANT_SHIFT;

  tran_high_t a2 = a1 + c1;
  tran_high_t b2 = b1 + d1;
  tran_high_t c2 = a1 - c1;
  tran_high_t d2 = b1 - d1;

  (void)tx_type;
  (void)lossless;
  (void)eob;

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

void av1_inv_txfm_add_4x4(const tran_low_t *input, uint8_t *dest, int stride,
                          int eob, TX_TYPE tx_type, int lossless) {
  if (lossless) {
    assert(tx_type == DCT_DCT);
    av1_iwht4x4_add(input, dest, stride, eob);
    return;
  }

  switch (tx_type) {
    case DCT_DCT: av1_idct4x4_add(input, dest, stride, eob); break;
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST: av1_iht4x4_16_add(input, dest, stride, tx_type); break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST: av1_iht4x4_16_add(input, dest, stride, tx_type); break;
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
      // Use C version since DST only exists in C code
      av1_iht4x4_16_add_c(input, dest, stride, tx_type);
      break;
    case IDTX: inv_idtx_add_c(input, dest, stride, 4, tx_type); break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}

void av1_inv_txfm_add_4x8(const tran_low_t *input, uint8_t *dest, int stride,
                          int eob, TX_TYPE tx_type) {
  (void)eob;
  av1_iht4x8_32_add(input, dest, stride, tx_type);
}

void av1_inv_txfm_add_8x4(const tran_low_t *input, uint8_t *dest, int stride,
                          int eob, TX_TYPE tx_type) {
  (void)eob;
  av1_iht8x4_32_add(input, dest, stride, tx_type);
}

// These will be used by the masked-tx experiment in the future.
#if CONFIG_MASKED_TX && 0
static void inv_txfm_add_4x16(const tran_low_t *input, uint8_t *dest,
                              int stride, int eob, TX_TYPE tx_type) {
  (void)eob;
  av1_iht4x16_64_add(input, dest, stride, tx_type);
}

static void inv_txfm_add_16x4(const tran_low_t *input, uint8_t *dest,
                              int stride, int eob, TX_TYPE tx_type) {
  (void)eob;
  av1_iht16x4_64_add(input, dest, stride, tx_type);
}

static void inv_txfm_add_8x32(const tran_low_t *input, uint8_t *dest,
                              int stride, int eob, TX_TYPE tx_type) {
  (void)eob;
  av1_iht8x32_256_add(input, dest, stride, tx_type);
}

static void inv_txfm_add_32x8(const tran_low_t *input, uint8_t *dest,
                              int stride, int eob, TX_TYPE tx_type) {
  (void)eob;
  av1_iht32x8_256_add(input, dest, stride, tx_type);
}
#endif  // CONFIG_MASKED_TX

static void inv_txfm_add_8x16(const tran_low_t *input, uint8_t *dest,
                              int stride, int eob, TX_TYPE tx_type) {
  (void)eob;
  av1_iht8x16_128_add(input, dest, stride, tx_type);
}

static void inv_txfm_add_16x8(const tran_low_t *input, uint8_t *dest,
                              int stride, int eob, TX_TYPE tx_type) {
  (void)eob;
  av1_iht16x8_128_add(input, dest, stride, tx_type);
}

static void inv_txfm_add_16x32(const tran_low_t *input, uint8_t *dest,
                               int stride, int eob, TX_TYPE tx_type) {
  (void)eob;
  av1_iht16x32_512_add(input, dest, stride, tx_type);
}

static void inv_txfm_add_32x16(const tran_low_t *input, uint8_t *dest,
                               int stride, int eob, TX_TYPE tx_type) {
  (void)eob;
  av1_iht32x16_512_add(input, dest, stride, tx_type);
}

static void inv_txfm_add_8x8(const tran_low_t *input, uint8_t *dest, int stride,
                             int eob, TX_TYPE tx_type) {
  switch (tx_type) {
    case DCT_DCT: idct8x8_add(input, dest, stride, eob); break;
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST: av1_iht8x8_64_add(input, dest, stride, tx_type); break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST: av1_iht8x8_64_add(input, dest, stride, tx_type); break;
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
      // Use C version since DST only exists in C code
      av1_iht8x8_64_add_c(input, dest, stride, tx_type);
      break;
    case IDTX: inv_idtx_add_c(input, dest, stride, 8, tx_type); break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}

static void inv_txfm_add_16x16(const tran_low_t *input, uint8_t *dest,
                               int stride, int eob, TX_TYPE tx_type) {
  switch (tx_type) {
    case DCT_DCT: idct16x16_add(input, dest, stride, eob); break;
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST: av1_iht16x16_256_add(input, dest, stride, tx_type); break;
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
    case H_FLIPADST: av1_iht16x16_256_add(input, dest, stride, tx_type); break;
    case IDTX: inv_idtx_add_c(input, dest, stride, 16, tx_type); break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}

static void inv_txfm_add_32x32(const tran_low_t *input, uint8_t *dest,
                               int stride, int eob, TX_TYPE tx_type) {
  switch (tx_type) {
    case DCT_DCT: idct32x32_add(input, dest, stride, eob); break;
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
      av1_iht32x32_1024_add_c(input, dest, stride, tx_type);
      break;
    case IDTX: inv_idtx_add_c(input, dest, stride, 32, tx_type); break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}

#if CONFIG_TX64X64
static void inv_txfm_add_64x64(const tran_low_t *input, uint8_t *dest,
                               int stride, int eob, TX_TYPE tx_type) {
  switch (tx_type) {
    case DCT_DCT: idct64x64_add(input, dest, stride, eob); break;
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
      av1_iht64x64_4096_add_c(input, dest, stride, tx_type);
      break;
    case IDTX: inv_idtx_add_c(input, dest, stride, 64, tx_type); break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}
#endif  // CONFIG_TX64X64

#if CONFIG_HIGHBITDEPTH

const TXFM_2D_CFG *inv_txfm_cfg_ls[TX_TYPES][TX_SIZES];

typedef struct {
  const int8_t *cos_bit;
  const int8_t *stage_range;
} tx_1d_cfg;

typedef struct {
  tx_1d_cfg row;
  tx_1d_cfg col;
} tx_2d_cfg;

tx_2d_cfg inv_tx_cfg(int tx_type, int tx_size_row, int tx_size_col) {
  const TXFM_2D_CFG *cfg_row = inv_txfm_cfg_ls[tx_type][tx_size_row];
  const int8_t *stage_range_row = cfg_row->stage_range_row;
  const int8_t *cos_bit_row = cfg_row->cos_bit_row;

  const TXFM_2D_CFG *cfg_col = inv_txfm_cfg_ls[tx_type][tx_size_col];
  const int8_t *stage_range_col = cfg_col->stage_range_col;
  const int8_t *cos_bit_col = cfg_col->cos_bit_col;

  tx_2d_cfg cfg = {
    { cos_bit_row, stage_range_row }, { cos_bit_col, stage_range_col },
  };
  return cfg;
}

void av1_highbd_iht4x4_16_add_c(const tran_low_t *input, uint8_t *dest8,
                                int stride, int tx_type, int bd) {
  static const highbd_transform_2d HIGH_IHT_4[] = {
    { highbd_idct4, highbd_idct4 },    // DCT_DCT
    { highbd_iadst4, highbd_idct4 },   // ADST_DCT
    { highbd_idct4, highbd_iadst4 },   // DCT_ADST
    { highbd_iadst4, highbd_iadst4 },  // ADST_ADST
#if CONFIG_EXT_TX
    { highbd_iadst4, highbd_idct4 },       // FLIPADST_DCT
    { highbd_idct4, highbd_iadst4 },       // DCT_FLIPADST
    { highbd_iadst4, highbd_iadst4 },      // FLIPADST_FLIPADST
    { highbd_iadst4, highbd_iadst4 },      // ADST_FLIPADST
    { highbd_iadst4, highbd_iadst4 },      // FLIPADST_ADST
    { highbd_iidtx4_c, highbd_iidtx4_c },  // IDTX
    { highbd_idct4, highbd_iidtx4_c },     // V_DCT
    { highbd_iidtx4_c, highbd_idct4 },     // H_DCT
    { highbd_iadst4, highbd_iidtx4_c },    // V_ADST
    { highbd_iidtx4_c, highbd_iadst4 },    // H_ADST
    { highbd_iadst4, highbd_iidtx4_c },    // V_FLIPADST
    { highbd_iidtx4_c, highbd_iadst4 },    // H_FLIPADST
#endif                                     // CONFIG_EXT_TX
  };

  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  int i, j;
  tran_low_t tmp;
  tran_low_t out[4][4];
  tran_low_t *outp = &out[0][0];
  int outstride = 4;

  tx_2d_cfg cfg = inv_tx_cfg(tx_type, TX_4X4, TX_4X4);

  // inverse transform row vectors
  for (i = 0; i < 4; ++i) {
    HIGH_IHT_4[tx_type].rows(input, out[i], cfg.row.cos_bit,
                             cfg.row.stage_range, bd);
    input += 4;
  }

  // transpose
  for (i = 1; i < 4; i++) {
    for (j = 0; j < i; j++) {
      tmp = out[i][j];
      out[i][j] = out[j][i];
      out[j][i] = tmp;
    }
  }

  // inverse transform column vectors
  for (i = 0; i < 4; ++i) {
    HIGH_IHT_4[tx_type].cols(out[i], out[i], cfg.col.cos_bit,
                             cfg.col.stage_range, bd);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides16(&dest, &stride, &outp, &outstride, tx_type, 4, 4);
#endif

  // Sum with the destination
  for (i = 0; i < 4; ++i) {
    for (j = 0; j < 4; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] =
          highbd_clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 4), bd);
    }
  }
}

void av1_highbd_iht4x8_32_add_c(const tran_low_t *input, uint8_t *dest8,
                                int stride, int tx_type, int bd) {
  static const highbd_transform_2d HIGH_IHT_4x8[] = {
    { highbd_idct8, highbd_idct4 },    // DCT_DCT
    { highbd_iadst8, highbd_idct4 },   // ADST_DCT
    { highbd_idct8, highbd_iadst4 },   // DCT_ADST
    { highbd_iadst8, highbd_iadst4 },  // ADST_ADST
#if CONFIG_EXT_TX
    { highbd_iadst8, highbd_idct4 },       // FLIPADST_DCT
    { highbd_idct8, highbd_iadst4 },       // DCT_FLIPADST
    { highbd_iadst8, highbd_iadst4 },      // FLIPADST_FLIPADST
    { highbd_iadst8, highbd_iadst4 },      // ADST_FLIPADST
    { highbd_iadst8, highbd_iadst4 },      // FLIPADST_ADST
    { highbd_iidtx8_c, highbd_iidtx4_c },  // IDTX
    { highbd_idct8, highbd_iidtx4_c },     // V_DCT
    { highbd_iidtx8_c, highbd_idct4 },     // H_DCT
    { highbd_iadst8, highbd_iidtx4_c },    // V_ADST
    { highbd_iidtx8_c, highbd_iadst4 },    // H_ADST
    { highbd_iadst8, highbd_iidtx4_c },    // V_FLIPADST
    { highbd_iidtx8_c, highbd_iadst4 },    // H_FLIPADST
#endif                                     // CONFIG_EXT_TX
  };
  const int n = 4;
  const int n2 = 8;

  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  int i, j;
  tran_low_t out[4][8], outtmp[4];
  tran_low_t *outp = &out[0][0];
  int outstride = n2;

  tx_2d_cfg cfg = inv_tx_cfg(tx_type, TX_4X4, TX_8X8);

  // inverse transform row vectors, and transpose
  for (i = 0; i < n2; ++i) {
    HIGH_IHT_4x8[tx_type].rows(input, outtmp, cfg.row.cos_bit,
                               cfg.row.stage_range, bd);
    for (j = 0; j < n; ++j) {
      out[j][i] = HIGHBD_WRAPLOW(dct_const_round_shift(outtmp[j] * Sqrt2), bd);
    }
    input += n;
  }

  // inverse transform column vectors
  for (i = 0; i < n; ++i) {
    HIGH_IHT_4x8[tx_type].cols(out[i], out[i], cfg.col.cos_bit,
                               cfg.col.stage_range, bd);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides16(&dest, &stride, &outp, &outstride, tx_type, n2, n);
#endif  // CONFIG_EXT_TX

  // Sum with the destination
  for (i = 0; i < n2; ++i) {
    for (j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] =
          highbd_clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5), bd);
    }
  }
}

void av1_highbd_iht8x4_32_add_c(const tran_low_t *input, uint8_t *dest8,
                                int stride, int tx_type, int bd) {
  static const highbd_transform_2d HIGH_IHT_8x4[] = {
    { highbd_idct4, highbd_idct8 },    // DCT_DCT
    { highbd_iadst4, highbd_idct8 },   // ADST_DCT
    { highbd_idct4, highbd_iadst8 },   // DCT_ADST
    { highbd_iadst4, highbd_iadst8 },  // ADST_ADST
#if CONFIG_EXT_TX
    { highbd_iadst4, highbd_idct8 },       // FLIPADST_DCT
    { highbd_idct4, highbd_iadst8 },       // DCT_FLIPADST
    { highbd_iadst4, highbd_iadst8 },      // FLIPADST_FLIPADST
    { highbd_iadst4, highbd_iadst8 },      // ADST_FLIPADST
    { highbd_iadst4, highbd_iadst8 },      // FLIPADST_ADST
    { highbd_iidtx4_c, highbd_iidtx8_c },  // IDTX
    { highbd_idct4, highbd_iidtx8_c },     // V_DCT
    { highbd_iidtx4_c, highbd_idct8 },     // H_DCT
    { highbd_iadst4, highbd_iidtx8_c },    // V_ADST
    { highbd_iidtx4_c, highbd_iadst8 },    // H_ADST
    { highbd_iadst4, highbd_iidtx8_c },    // V_FLIPADST
    { highbd_iidtx4_c, highbd_iadst8 },    // H_FLIPADST
#endif                                     // CONFIG_EXT_TX
  };
  const int n = 4;
  const int n2 = 8;

  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  int i, j;
  tran_low_t out[8][4], outtmp[8];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  tx_2d_cfg cfg = inv_tx_cfg(tx_type, TX_8X8, TX_4X4);

  // inverse transform row vectors, and transpose
  for (i = 0; i < n; ++i) {
    HIGH_IHT_8x4[tx_type].rows(input, outtmp, cfg.row.cos_bit,
                               cfg.row.stage_range, bd);
    for (j = 0; j < n2; ++j) {
      out[j][i] = HIGHBD_WRAPLOW(dct_const_round_shift(outtmp[j] * Sqrt2), bd);
    }
    input += n2;
  }

  // inverse transform column vectors
  for (i = 0; i < n2; ++i) {
    HIGH_IHT_8x4[tx_type].cols(out[i], out[i], cfg.col.cos_bit,
                               cfg.col.stage_range, bd);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides16(&dest, &stride, &outp, &outstride, tx_type, n, n2);
#endif  // CONFIG_EXT_TX

  // Sum with the destination
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n2; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] =
          highbd_clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5), bd);
    }
  }
}

void av1_highbd_iht4x16_64_add_c(const tran_low_t *input, uint8_t *dest8,
                                 int stride, int tx_type, int bd) {
  static const highbd_transform_2d HIGH_IHT_4x16[] = {
    { highbd_idct16, highbd_idct4 },    // DCT_DCT
    { highbd_iadst16, highbd_idct4 },   // ADST_DCT
    { highbd_idct16, highbd_iadst4 },   // DCT_ADST
    { highbd_iadst16, highbd_iadst4 },  // ADST_ADST
#if CONFIG_EXT_TX
    { highbd_iadst16, highbd_idct4 },       // FLIPADST_DCT
    { highbd_idct16, highbd_iadst4 },       // DCT_FLIPADST
    { highbd_iadst16, highbd_iadst4 },      // FLIPADST_FLIPADST
    { highbd_iadst16, highbd_iadst4 },      // ADST_FLIPADST
    { highbd_iadst16, highbd_iadst4 },      // FLIPADST_ADST
    { highbd_iidtx16_c, highbd_iidtx4_c },  // IDTX
    { highbd_idct16, highbd_iidtx4_c },     // V_DCT
    { highbd_iidtx16_c, highbd_idct4 },     // H_DCT
    { highbd_iadst16, highbd_iidtx4_c },    // V_ADST
    { highbd_iidtx16_c, highbd_iadst4 },    // H_ADST
    { highbd_iadst16, highbd_iidtx4_c },    // V_FLIPADST
    { highbd_iidtx16_c, highbd_iadst4 },    // H_FLIPADST
#endif                                      // CONFIG_EXT_TX
  };
  const int n = 4;
  const int n4 = 16;

  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  int i, j;
  tran_low_t out[4][16], outtmp[4];
  tran_low_t *outp = &out[0][0];
  int outstride = n4;

  tx_2d_cfg cfg = inv_tx_cfg(tx_type, TX_4X4, TX_16X16);

  // inverse transform row vectors, and transpose
  for (i = 0; i < n4; ++i) {
    HIGH_IHT_4x16[tx_type].rows(input, outtmp, cfg.row.cos_bit,
                                cfg.row.stage_range, bd);
    for (j = 0; j < n; ++j) out[j][i] = outtmp[j];
    input += n;
  }

  // inverse transform column vectors
  for (i = 0; i < n; ++i)
    HIGH_IHT_4x16[tx_type].cols(out[i], out[i], cfg.col.cos_bit,
                                cfg.col.stage_range, bd);

#if CONFIG_EXT_TX
  maybe_flip_strides16(&dest, &stride, &outp, &outstride, tx_type, n4, n);
#endif  // CONFIG_EXT_TX

  // Sum with the destination
  for (i = 0; i < n4; ++i) {
    for (j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] =
          highbd_clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5), bd);
    }
  }
}

void av1_highbd_iht16x4_64_add_c(const tran_low_t *input, uint8_t *dest8,
                                 int stride, int tx_type, int bd) {
  static const highbd_transform_2d HIGH_IHT_16x4[] = {
    { highbd_idct4, highbd_idct16 },    // DCT_DCT
    { highbd_iadst4, highbd_idct16 },   // ADST_DCT
    { highbd_idct4, highbd_iadst16 },   // DCT_ADST
    { highbd_iadst4, highbd_iadst16 },  // ADST_ADST
#if CONFIG_EXT_TX
    { highbd_iadst4, highbd_idct16 },       // FLIPADST_DCT
    { highbd_idct4, highbd_iadst16 },       // DCT_FLIPADST
    { highbd_iadst4, highbd_iadst16 },      // FLIPADST_FLIPADST
    { highbd_iadst4, highbd_iadst16 },      // ADST_FLIPADST
    { highbd_iadst4, highbd_iadst16 },      // FLIPADST_ADST
    { highbd_iidtx4_c, highbd_iidtx16_c },  // IDTX
    { highbd_idct4, highbd_iidtx16_c },     // V_DCT
    { highbd_iidtx4_c, highbd_idct16 },     // H_DCT
    { highbd_iadst4, highbd_iidtx16_c },    // V_ADST
    { highbd_iidtx4_c, highbd_iadst16 },    // H_ADST
    { highbd_iadst4, highbd_iidtx16_c },    // V_FLIPADST
    { highbd_iidtx4_c, highbd_iadst16 },    // H_FLIPADST
#endif                                      // CONFIG_EXT_TX
  };
  const int n = 4;
  const int n4 = 16;

  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  int i, j;
  tran_low_t out[16][4], outtmp[16];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  tx_2d_cfg cfg = inv_tx_cfg(tx_type, TX_16X16, TX_4X4);

  // inverse transform row vectors, and transpose
  for (i = 0; i < n; ++i) {
    HIGH_IHT_16x4[tx_type].rows(input, outtmp, cfg.row.cos_bit,
                                cfg.row.stage_range, bd);
    for (j = 0; j < n4; ++j) out[j][i] = outtmp[j];
    input += n4;
  }

  // inverse transform column vectors
  for (i = 0; i < n4; ++i) {
    HIGH_IHT_16x4[tx_type].cols(out[i], out[i], cfg.col.cos_bit,
                                cfg.col.stage_range, bd);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides16(&dest, &stride, &outp, &outstride, tx_type, n, n4);
#endif  // CONFIG_EXT_TX

  // Sum with the destination
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n4; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] =
          highbd_clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5), bd);
    }
  }
}

void av1_highbd_iht8x16_128_add_c(const tran_low_t *input, uint8_t *dest8,
                                  int stride, int tx_type, int bd) {
  static const highbd_transform_2d HIGH_IHT_8x16[] = {
    { highbd_idct16, highbd_idct8 },    // DCT_DCT
    { highbd_iadst16, highbd_idct8 },   // ADST_DCT
    { highbd_idct16, highbd_iadst8 },   // DCT_ADST
    { highbd_iadst16, highbd_iadst8 },  // ADST_ADST
#if CONFIG_EXT_TX
    { highbd_iadst16, highbd_idct8 },       // FLIPADST_DCT
    { highbd_idct16, highbd_iadst8 },       // DCT_FLIPADST
    { highbd_iadst16, highbd_iadst8 },      // FLIPADST_FLIPADST
    { highbd_iadst16, highbd_iadst8 },      // ADST_FLIPADST
    { highbd_iadst16, highbd_iadst8 },      // FLIPADST_ADST
    { highbd_iidtx16_c, highbd_iidtx8_c },  // IDTX
    { highbd_idct16, highbd_iidtx8_c },     // V_DCT
    { highbd_iidtx16_c, highbd_idct8 },     // H_DCT
    { highbd_iadst16, highbd_iidtx8_c },    // V_ADST
    { highbd_iidtx16_c, highbd_iadst8 },    // H_ADST
    { highbd_iadst16, highbd_iidtx8_c },    // V_FLIPADST
    { highbd_iidtx16_c, highbd_iadst8 },    // H_FLIPADST
#endif                                      // CONFIG_EXT_TX
  };
  const int n = 8;
  const int n2 = 16;

  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  int i, j;
  tran_low_t out[8][16], outtmp[8];
  tran_low_t *outp = &out[0][0];
  int outstride = n2;

  tx_2d_cfg cfg = inv_tx_cfg(tx_type, TX_8X8, TX_16X16);

  // inverse transform row vectors, and transpose
  for (i = 0; i < n2; ++i) {
    HIGH_IHT_8x16[tx_type].rows(input, outtmp, cfg.row.cos_bit,
                                cfg.row.stage_range, bd);
    for (j = 0; j < n; ++j)
      out[j][i] = HIGHBD_WRAPLOW(dct_const_round_shift(outtmp[j] * Sqrt2), bd);
    input += n;
  }

  // inverse transform column vectors
  for (i = 0; i < n; ++i) {
    HIGH_IHT_8x16[tx_type].cols(out[i], out[i], cfg.col.cos_bit,
                                cfg.col.stage_range, bd);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides16(&dest, &stride, &outp, &outstride, tx_type, n2, n);
#endif  // CONFIG_EXT_TX

  // Sum with the destination
  for (i = 0; i < n2; ++i) {
    for (j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] =
          highbd_clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6), bd);
    }
  }
}

void av1_highbd_iht16x8_128_add_c(const tran_low_t *input, uint8_t *dest8,
                                  int stride, int tx_type, int bd) {
  static const highbd_transform_2d HIGH_IHT_16x8[] = {
    { highbd_idct8, highbd_idct16 },    // DCT_DCT
    { highbd_iadst8, highbd_idct16 },   // ADST_DCT
    { highbd_idct8, highbd_iadst16 },   // DCT_ADST
    { highbd_iadst8, highbd_iadst16 },  // ADST_ADST
#if CONFIG_EXT_TX
    { highbd_iadst8, highbd_idct16 },       // FLIPADST_DCT
    { highbd_idct8, highbd_iadst16 },       // DCT_FLIPADST
    { highbd_iadst8, highbd_iadst16 },      // FLIPADST_FLIPADST
    { highbd_iadst8, highbd_iadst16 },      // ADST_FLIPADST
    { highbd_iadst8, highbd_iadst16 },      // FLIPADST_ADST
    { highbd_iidtx8_c, highbd_iidtx16_c },  // IDTX
    { highbd_idct8, highbd_iidtx16_c },     // V_DCT
    { highbd_iidtx8_c, highbd_idct16 },     // H_DCT
    { highbd_iadst8, highbd_iidtx16_c },    // V_ADST
    { highbd_iidtx8_c, highbd_iadst16 },    // H_ADST
    { highbd_iadst8, highbd_iidtx16_c },    // V_FLIPADST
    { highbd_iidtx8_c, highbd_iadst16 },    // H_FLIPADST
#endif                                      // CONFIG_EXT_TX
  };
  const int n = 8;
  const int n2 = 16;

  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  int i, j;
  tran_low_t out[16][8], outtmp[16];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  tx_2d_cfg cfg = inv_tx_cfg(tx_type, TX_16X16, TX_8X8);

  // inverse transform row vectors, and transpose
  for (i = 0; i < n; ++i) {
    HIGH_IHT_16x8[tx_type].rows(input, outtmp, cfg.row.cos_bit,
                                cfg.row.stage_range, bd);
    for (j = 0; j < n2; ++j)
      out[j][i] = HIGHBD_WRAPLOW(dct_const_round_shift(outtmp[j] * Sqrt2), bd);
    input += n2;
  }

  // inverse transform column vectors
  for (i = 0; i < n2; ++i) {
    HIGH_IHT_16x8[tx_type].cols(out[i], out[i], cfg.col.cos_bit,
                                cfg.col.stage_range, bd);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides16(&dest, &stride, &outp, &outstride, tx_type, n, n2);
#endif  // CONFIG_EXT_TX

  // Sum with the destination
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n2; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] =
          highbd_clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6), bd);
    }
  }
}

void av1_highbd_iht8x32_256_add_c(const tran_low_t *input, uint8_t *dest8,
                                  int stride, int tx_type, int bd) {
  static const highbd_transform_2d HIGH_IHT_8x32[] = {
    { highbd_idct32, highbd_idct8 },           // DCT_DCT
    { highbd_ihalfright32_c, highbd_idct8 },   // ADST_DCT
    { highbd_idct32, highbd_iadst8 },          // DCT_ADST
    { highbd_ihalfright32_c, highbd_iadst8 },  // ADST_ADST
#if CONFIG_EXT_TX
    { highbd_ihalfright32_c, highbd_idct8 },     // FLIPADST_DCT
    { highbd_idct32, highbd_iadst8 },            // DCT_FLIPADST
    { highbd_ihalfright32_c, highbd_iadst8 },    // FLIPADST_FLIPADST
    { highbd_ihalfright32_c, highbd_iadst8 },    // ADST_FLIPADST
    { highbd_ihalfright32_c, highbd_iadst8 },    // FLIPADST_ADST
    { highbd_iidtx32_c, highbd_iidtx8_c },       // IDTX
    { highbd_idct32, highbd_iidtx8_c },          // V_DCT
    { highbd_iidtx32_c, highbd_idct8 },          // H_DCT
    { highbd_ihalfright32_c, highbd_iidtx8_c },  // V_ADST
    { highbd_iidtx32_c, highbd_iadst8 },         // H_ADST
    { highbd_ihalfright32_c, highbd_iidtx8_c },  // V_FLIPADST
    { highbd_iidtx32_c, highbd_iadst8 },         // H_FLIPADST
#endif                                           // CONFIG_EXT_TX
  };
  const int n = 8;
  const int n4 = 32;

  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  int i, j;
  tran_low_t out[8][32], outtmp[8];
  tran_low_t *outp = &out[0][0];
  int outstride = n4;

  tx_2d_cfg cfg = inv_tx_cfg(tx_type, TX_8X8, TX_32X32);

  // inverse transform row vectors, and transpose
  for (i = 0; i < n4; ++i) {
    HIGH_IHT_8x32[tx_type].rows(input, outtmp, cfg.row.cos_bit,
                                cfg.row.stage_range, bd);
    for (j = 0; j < n; ++j) out[j][i] = outtmp[j];
    input += n;
  }

  // inverse transform column vectors
  for (i = 0; i < n; ++i)
    HIGH_IHT_8x32[tx_type].cols(out[i], out[i], cfg.col.cos_bit,
                                cfg.col.stage_range, bd);

#if CONFIG_EXT_TX
  maybe_flip_strides16(&dest, &stride, &outp, &outstride, tx_type, n4, n);
#endif  // CONFIG_EXT_TX

  // Sum with the destination
  for (i = 0; i < n4; ++i) {
    for (j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] =
          highbd_clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6), bd);
    }
  }
}

void av1_highbd_iht32x8_256_add_c(const tran_low_t *input, uint8_t *dest8,
                                  int stride, int tx_type, int bd) {
  static const highbd_transform_2d HIGH_IHT_32x8[] = {
    { highbd_idct8, highbd_idct32 },           // DCT_DCT
    { highbd_iadst8, highbd_idct32 },          // ADST_DCT
    { highbd_idct8, highbd_ihalfright32_c },   // DCT_ADST
    { highbd_iadst8, highbd_ihalfright32_c },  // ADST_ADST
#if CONFIG_EXT_TX
    { highbd_iadst8, highbd_idct32 },            // FLIPADST_DCT
    { highbd_idct8, highbd_ihalfright32_c },     // DCT_FLIPADST
    { highbd_iadst8, highbd_ihalfright32_c },    // FLIPADST_FLIPADST
    { highbd_iadst8, highbd_ihalfright32_c },    // ADST_FLIPADST
    { highbd_iadst8, highbd_ihalfright32_c },    // FLIPADST_ADST
    { highbd_iidtx8_c, highbd_iidtx32_c },       // IDTX
    { highbd_idct8, highbd_iidtx32_c },          // V_DCT
    { highbd_iidtx8_c, highbd_idct32 },          // H_DCT
    { highbd_iadst8, highbd_iidtx32_c },         // V_ADST
    { highbd_iidtx8_c, highbd_ihalfright32_c },  // H_ADST
    { highbd_iadst8, highbd_iidtx32_c },         // V_FLIPADST
    { highbd_iidtx8_c, highbd_ihalfright32_c },  // H_FLIPADST
#endif                                           // CONFIG_EXT_TX
  };
  const int n = 8;
  const int n4 = 32;

  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  int i, j;
  tran_low_t out[32][8], outtmp[32];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  tx_2d_cfg cfg = inv_tx_cfg(tx_type, TX_32X32, TX_8X8);

  // inverse transform row vectors, and transpose
  for (i = 0; i < n; ++i) {
    HIGH_IHT_32x8[tx_type].rows(input, outtmp, cfg.row.cos_bit,
                                cfg.row.stage_range, bd);
    for (j = 0; j < n4; ++j) out[j][i] = outtmp[j];
    input += n4;
  }

  // inverse transform column vectors
  for (i = 0; i < n4; ++i)
    HIGH_IHT_32x8[tx_type].cols(out[i], out[i], cfg.col.cos_bit,
                                cfg.col.stage_range, bd);

#if CONFIG_EXT_TX
  maybe_flip_strides16(&dest, &stride, &outp, &outstride, tx_type, n, n4);
#endif  // CONFIG_EXT_TX

  // Sum with the destination
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n4; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] =
          highbd_clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6), bd);
    }
  }
}

void av1_highbd_iht16x32_512_add_c(const tran_low_t *input, uint8_t *dest8,
                                   int stride, int tx_type, int bd) {
  static const highbd_transform_2d HIGH_IHT_16x32[] = {
    { highbd_idct32, highbd_idct16 },           // DCT_DCT
    { highbd_ihalfright32_c, highbd_idct16 },   // ADST_DCT
    { highbd_idct32, highbd_iadst16 },          // DCT_ADST
    { highbd_ihalfright32_c, highbd_iadst16 },  // ADST_ADST
#if CONFIG_EXT_TX
    { highbd_ihalfright32_c, highbd_idct16 },     // FLIPADST_DCT
    { highbd_idct32, highbd_iadst16 },            // DCT_FLIPADST
    { highbd_ihalfright32_c, highbd_iadst16 },    // FLIPADST_FLIPADST
    { highbd_ihalfright32_c, highbd_iadst16 },    // ADST_FLIPADST
    { highbd_ihalfright32_c, highbd_iadst16 },    // FLIPADST_ADST
    { highbd_iidtx32_c, highbd_iidtx16_c },       // IDTX
    { highbd_idct32, highbd_iidtx16_c },          // V_DCT
    { highbd_iidtx32_c, highbd_idct16 },          // H_DCT
    { highbd_ihalfright32_c, highbd_iidtx16_c },  // V_ADST
    { highbd_iidtx32_c, highbd_iadst16 },         // H_ADST
    { highbd_ihalfright32_c, highbd_iidtx16_c },  // V_FLIPADST
    { highbd_iidtx32_c, highbd_iadst16 },         // H_FLIPADST
#endif                                            // CONFIG_EXT_TX
  };
  const int n = 16;
  const int n2 = 32;

  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  int i, j;
  tran_low_t out[16][32], outtmp[16];
  tran_low_t *outp = &out[0][0];
  int outstride = n2;

  tx_2d_cfg cfg = inv_tx_cfg(tx_type, TX_16X16, TX_32X32);

  // inverse transform row vectors, and transpose
  for (i = 0; i < n2; ++i) {
    HIGH_IHT_16x32[tx_type].rows(input, outtmp, cfg.row.cos_bit,
                                 cfg.row.stage_range, bd);
    for (j = 0; j < n; ++j)
      out[j][i] = HIGHBD_WRAPLOW(dct_const_round_shift(outtmp[j] * Sqrt2), bd);
    input += n;
  }

  // inverse transform column vectors
  for (i = 0; i < n; ++i) {
    HIGH_IHT_16x32[tx_type].cols(out[i], out[i], cfg.col.cos_bit,
                                 cfg.col.stage_range, bd);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides16(&dest, &stride, &outp, &outstride, tx_type, n2, n);
#endif  // CONFIG_EXT_TX

  // Sum with the destination
  for (i = 0; i < n2; ++i) {
    for (j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] =
          highbd_clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6), bd);
    }
  }
}

void av1_highbd_iht32x16_512_add_c(const tran_low_t *input, uint8_t *dest8,
                                   int stride, int tx_type, int bd) {
  static const highbd_transform_2d HIGH_IHT_32x16[] = {
    { highbd_idct16, highbd_idct32 },           // DCT_DCT
    { highbd_iadst16, highbd_idct32 },          // ADST_DCT
    { highbd_idct16, highbd_ihalfright32_c },   // DCT_ADST
    { highbd_iadst16, highbd_ihalfright32_c },  // ADST_ADST
#if CONFIG_EXT_TX
    { highbd_iadst16, highbd_idct32 },            // FLIPADST_DCT
    { highbd_idct16, highbd_ihalfright32_c },     // DCT_FLIPADST
    { highbd_iadst16, highbd_ihalfright32_c },    // FLIPADST_FLIPADST
    { highbd_iadst16, highbd_ihalfright32_c },    // ADST_FLIPADST
    { highbd_iadst16, highbd_ihalfright32_c },    // FLIPADST_ADST
    { highbd_iidtx16_c, highbd_iidtx32_c },       // IDTX
    { highbd_idct16, highbd_iidtx32_c },          // V_DCT
    { highbd_iidtx16_c, highbd_idct32 },          // H_DCT
    { highbd_iadst16, highbd_iidtx32_c },         // V_ADST
    { highbd_iidtx16_c, highbd_ihalfright32_c },  // H_ADST
    { highbd_iadst16, highbd_iidtx32_c },         // V_FLIPADST
    { highbd_iidtx16_c, highbd_ihalfright32_c },  // H_FLIPADST
#endif                                            // CONFIG_EXT_TX
  };
  const int n = 16;
  const int n2 = 32;

  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  int i, j;
  tran_low_t out[32][16], outtmp[32];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  tx_2d_cfg cfg = inv_tx_cfg(tx_type, TX_32X32, TX_16X16);

  // inverse transform row vectors, and transpose
  for (i = 0; i < n; ++i) {
    HIGH_IHT_32x16[tx_type].rows(input, outtmp, cfg.row.cos_bit,
                                 cfg.row.stage_range, bd);
    for (j = 0; j < n2; ++j)
      out[j][i] = HIGHBD_WRAPLOW(dct_const_round_shift(outtmp[j] * Sqrt2), bd);
    input += n2;
  }

  // inverse transform column vectors
  for (i = 0; i < n2; ++i) {
    HIGH_IHT_32x16[tx_type].cols(out[i], out[i], cfg.col.cos_bit,
                                 cfg.col.stage_range, bd);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides16(&dest, &stride, &outp, &outstride, tx_type, n, n2);
#endif  // CONFIG_EXT_TX

  // Sum with the destination
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n2; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] =
          highbd_clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6), bd);
    }
  }
}

void av1_highbd_iht8x8_64_add_c(const tran_low_t *input, uint8_t *dest8,
                                int stride, int tx_type, int bd) {
  static const highbd_transform_2d HIGH_IHT_8[] = {
    { highbd_idct8, highbd_idct8 },    // DCT_DCT
    { highbd_iadst8, highbd_idct8 },   // ADST_DCT
    { highbd_idct8, highbd_iadst8 },   // DCT_ADST
    { highbd_iadst8, highbd_iadst8 },  // ADST_ADST
#if CONFIG_EXT_TX
    { highbd_iadst8, highbd_idct8 },       // FLIPADST_DCT
    { highbd_idct8, highbd_iadst8 },       // DCT_FLIPADST
    { highbd_iadst8, highbd_iadst8 },      // FLIPADST_FLIPADST
    { highbd_iadst8, highbd_iadst8 },      // ADST_FLIPADST
    { highbd_iadst8, highbd_iadst8 },      // FLIPADST_ADST
    { highbd_iidtx8_c, highbd_iidtx8_c },  // IDTX
    { highbd_idct8, highbd_iidtx8_c },     // V_DCT
    { highbd_iidtx8_c, highbd_idct8 },     // H_DCT
    { highbd_iadst8, highbd_iidtx8_c },    // V_ADST
    { highbd_iidtx8_c, highbd_iadst8 },    // H_ADST
    { highbd_iadst8, highbd_iidtx8_c },    // V_FLIPADST
    { highbd_iidtx8_c, highbd_iadst8 },    // H_FLIPADST
#endif                                     // CONFIG_EXT_TX
  };

  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  int i, j;
  tran_low_t tmp;
  tran_low_t out[8][8];
  tran_low_t *outp = &out[0][0];
  int outstride = 8;

  tx_2d_cfg cfg = inv_tx_cfg(tx_type, TX_8X8, TX_8X8);

  // inverse transform row vectors
  for (i = 0; i < 8; ++i) {
    HIGH_IHT_8[tx_type].rows(input, out[i], cfg.row.cos_bit,
                             cfg.row.stage_range, bd);
    input += 8;
  }

  // transpose
  for (i = 1; i < 8; i++) {
    for (j = 0; j < i; j++) {
      tmp = out[i][j];
      out[i][j] = out[j][i];
      out[j][i] = tmp;
    }
  }

  // inverse transform column vectors
  for (i = 0; i < 8; ++i) {
    HIGH_IHT_8[tx_type].cols(out[i], out[i], cfg.col.cos_bit,
                             cfg.col.stage_range, bd);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides16(&dest, &stride, &outp, &outstride, tx_type, 8, 8);
#endif

  // Sum with the destination
  for (i = 0; i < 8; ++i) {
    for (j = 0; j < 8; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] =
          highbd_clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5), bd);
    }
  }
}

void av1_highbd_iht16x16_256_add_c(const tran_low_t *input, uint8_t *dest8,
                                   int stride, int tx_type, int bd) {
  static const highbd_transform_2d HIGH_IHT_16[] = {
    { highbd_idct16, highbd_idct16 },    // DCT_DCT
    { highbd_iadst16, highbd_idct16 },   // ADST_DCT
    { highbd_idct16, highbd_iadst16 },   // DCT_ADST
    { highbd_iadst16, highbd_iadst16 },  // ADST_ADST
#if CONFIG_EXT_TX
    { highbd_iadst16, highbd_idct16 },       // FLIPADST_DCT
    { highbd_idct16, highbd_iadst16 },       // DCT_FLIPADST
    { highbd_iadst16, highbd_iadst16 },      // FLIPADST_FLIPADST
    { highbd_iadst16, highbd_iadst16 },      // ADST_FLIPADST
    { highbd_iadst16, highbd_iadst16 },      // FLIPADST_ADST
    { highbd_iidtx16_c, highbd_iidtx16_c },  // IDTX
    { highbd_idct16, highbd_iidtx16_c },     // V_DCT
    { highbd_iidtx16_c, highbd_idct16 },     // H_DCT
    { highbd_iadst16, highbd_iidtx16_c },    // V_ADST
    { highbd_iidtx16_c, highbd_iadst16 },    // H_ADST
    { highbd_iadst16, highbd_iidtx16_c },    // V_FLIPADST
    { highbd_iidtx16_c, highbd_iadst16 },    // H_FLIPADST
#endif                                       // CONFIG_EXT_TX
  };

  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  int i, j;
  tran_low_t tmp;
  tran_low_t out[16][16];
  tran_low_t *outp = &out[0][0];
  int outstride = 16;

  tx_2d_cfg cfg = inv_tx_cfg(tx_type, TX_16X16, TX_16X16);

  // inverse transform row vectors
  for (i = 0; i < 16; ++i) {
    HIGH_IHT_16[tx_type].rows(input, out[i], cfg.row.cos_bit,
                              cfg.row.stage_range, bd);
    input += 16;
  }

  // transpose
  for (i = 1; i < 16; i++) {
    for (j = 0; j < i; j++) {
      tmp = out[i][j];
      out[i][j] = out[j][i];
      out[j][i] = tmp;
    }
  }

  // inverse transform column vectors
  for (i = 0; i < 16; ++i) {
    HIGH_IHT_16[tx_type].cols(out[i], out[i], cfg.col.cos_bit,
                              cfg.col.stage_range, bd);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides16(&dest, &stride, &outp, &outstride, tx_type, 16, 16);
#endif

  // Sum with the destination
  for (i = 0; i < 16; ++i) {
    for (j = 0; j < 16; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] =
          highbd_clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6), bd);
    }
  }
}

#if CONFIG_EXT_TX
static void highbd_iht32x32_1024_add_c(const tran_low_t *input, uint8_t *dest8,
                                       int stride, int tx_type, int bd) {
  static const highbd_transform_2d HIGH_IHT_32[] = {
    { highbd_idct32, highbd_idct32 },                  // DCT_DCT
    { highbd_ihalfright32_c, highbd_idct32 },          // ADST_DCT
    { highbd_idct32, highbd_ihalfright32_c },          // DCT_ADST
    { highbd_ihalfright32_c, highbd_ihalfright32_c },  // ADST_ADST
    { highbd_ihalfright32_c, highbd_idct32 },          // FLIPADST_DCT
    { highbd_idct32, highbd_ihalfright32_c },          // DCT_FLIPADST
    { highbd_ihalfright32_c, highbd_ihalfright32_c },  // FLIPADST_FLIPADST
    { highbd_ihalfright32_c, highbd_ihalfright32_c },  // ADST_FLIPADST
    { highbd_ihalfright32_c, highbd_ihalfright32_c },  // FLIPADST_ADST
    { highbd_iidtx32_c, highbd_iidtx32_c },            // IDTX
    { highbd_idct32, highbd_iidtx32_c },               // V_DCT
    { highbd_iidtx32_c, highbd_idct32 },               // H_DCT
    { highbd_ihalfright32_c, highbd_iidtx32_c },       // V_ADST
    { highbd_iidtx32_c, highbd_ihalfright32_c },       // H_ADST
    { highbd_ihalfright32_c, highbd_iidtx32_c },       // V_FLIPADST
    { highbd_iidtx32_c, highbd_ihalfright32_c },       // H_FLIPADST
  };

  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  int i, j;
  tran_low_t tmp;
  tran_low_t out[32][32];
  tran_low_t *outp = &out[0][0];
  int outstride = 32;

  tx_2d_cfg cfg = inv_tx_cfg(tx_type, TX_32X32, TX_32X32);

  // inverse transform row vectors
  for (i = 0; i < 32; ++i) {
    HIGH_IHT_32[tx_type].rows(input, out[i], cfg.row.cos_bit,
                              cfg.row.stage_range, bd);
    input += 32;
  }

  // transpose
  for (i = 1; i < 32; i++) {
    for (j = 0; j < i; j++) {
      tmp = out[i][j];
      out[i][j] = out[j][i];
      out[j][i] = tmp;
    }
  }

  // inverse transform column vectors
  for (i = 0; i < 32; ++i) {
    HIGH_IHT_32[tx_type].cols(out[i], out[i], cfg.col.cos_bit,
                              cfg.col.stage_range, bd);
  }

  maybe_flip_strides16(&dest, &stride, &outp, &outstride, tx_type, 32, 32);

  // Sum with the destination
  for (i = 0; i < 32; ++i) {
    for (j = 0; j < 32; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] =
          highbd_clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6), bd);
    }
  }
}
#endif  // CONFIG_EXT_TX

#if CONFIG_TX64X64
static void highbd_iht64x64_4096_add_c(const tran_low_t *input, uint8_t *dest8,
                                       int stride, int tx_type, int bd) {
  static const highbd_transform_2d HIGH_IHT_64[] = {
    { highbd_idct64_col_c, highbd_idct64_row_c },      // DCT_DCT
    { highbd_ihalfright64_c, highbd_idct64_row_c },    // ADST_DCT
    { highbd_idct64_col_c, highbd_ihalfright64_c },    // DCT_ADST
    { highbd_ihalfright64_c, highbd_ihalfright64_c },  // ADST_ADST
#if CONFIG_EXT_TX
    { highbd_ihalfright64_c, highbd_idct64_row_c },    // FLIPADST_DCT
    { highbd_idct64_col_c, highbd_ihalfright64_c },    // DCT_FLIPADST
    { highbd_ihalfright64_c, highbd_ihalfright64_c },  // FLIPADST_FLIPADST
    { highbd_ihalfright64_c, highbd_ihalfright64_c },  // ADST_FLIPADST
    { highbd_ihalfright64_c, highbd_ihalfright64_c },  // FLIPADST_ADST
    { highbd_iidtx64_c, highbd_iidtx64_c },            // IDTX
    { highbd_idct64_col_c, highbd_iidtx64_c },         // V_DCT
    { highbd_iidtx64_c, highbd_idct64_row_c },         // H_DCT
    { highbd_ihalfright64_c, highbd_iidtx64_c },       // V_ADST
    { highbd_iidtx64_c, highbd_ihalfright64_c },       // H_ADST
    { highbd_ihalfright64_c, highbd_iidtx64_c },       // V_FLIPADST
    { highbd_iidtx64_c, highbd_ihalfright64_c },       // H_FLIPADST
#endif                                                 // CONFIG_EXT_TX
  };

  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  int i, j;
  tran_low_t tmp;
  tran_low_t out[64][64];
  tran_low_t *outp = &out[0][0];
  int outstride = 64;

  tx_2d_cfg cfg = inv_tx_cfg(tx_type, TX_64X64, TX_64X64);

  // inverse transform row vectors
  for (i = 0; i < 64; ++i) {
    HIGH_IHT_64[tx_type].rows(input, out[i], cfg.row.cos_bit,
                              cfg.row.stage_range, bd);
    for (j = 0; j < 64; ++j) out[i][j] = ROUND_POWER_OF_TWO(out[i][j], 1);
    input += 64;
  }

  // transpose
  for (i = 1; i < 64; i++) {
    for (j = 0; j < i; j++) {
      tmp = out[i][j];
      out[i][j] = out[j][i];
      out[j][i] = tmp;
    }
  }

  // inverse transform column vectors
  for (i = 0; i < 64; ++i) {
    HIGH_IHT_64[tx_type].cols(out[i], out[i], cfg.col.cos_bit_col,
                              cfg.col.stage_range, bd);
  }

#if CONFIG_EXT_TX
  maybe_flip_strides16(&dest, &stride, &outp, &outstride, tx_type, 64, 64);
#endif  // CONFIG_EXT_TX

  // Sum with the destination
  for (i = 0; i < 64; ++i) {
    for (j = 0; j < 64; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] =
          highbd_clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5), bd);
    }
  }
}
#endif  // CONFIG_TX64X64

// idct
void av1_highbd_idct4x4_add(const tran_low_t *input, uint8_t *dest, int stride,
                            int eob, int bd) {
  if (eob > 1)
    aom_highbd_idct4x4_16_add(input, dest, stride, bd);
  else
    aom_highbd_idct4x4_1_add(input, dest, stride, bd);
}

void av1_highbd_iwht4x4_add(const tran_low_t *input, uint8_t *dest, int stride,
                            int eob, int bd) {
  if (eob > 1)
    aom_highbd_iwht4x4_16_add(input, dest, stride, bd);
  else
    aom_highbd_iwht4x4_1_add(input, dest, stride, bd);
}

#if CONFIG_CB4X4
static void highbd_inv_txfm_add_2x2(const tran_low_t *input, uint8_t *dest,
                                    int stride, int eob, int bd,
                                    TX_TYPE tx_type, int lossless) {
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

void av1_highbd_inv_txfm_add_4x4(const tran_low_t *input, uint8_t *dest,
                                 int stride, int eob, int bd, TX_TYPE tx_type,
                                 int lossless) {
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
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
#endif  // CONFIG_EXT_TX
      av1_inv_txfm2d_add_4x4(input, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                             bd);
      break;
#if CONFIG_EXT_TX
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
      // Use C version since DST only exists in C code
      av1_highbd_iht4x4_16_add_c(input, dest, stride, tx_type, bd);
      break;
    case IDTX:
      highbd_inv_idtx_add_c(input, dest, stride, 4, tx_type, bd);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}

void av1_highbd_inv_txfm_add_4x8(const tran_low_t *input, uint8_t *dest,
                                 int stride, int eob, int bd, TX_TYPE tx_type) {
  (void)eob;
  av1_highbd_iht4x8_32_add_c(input, dest, stride, tx_type, bd);
}

void av1_highbd_inv_txfm_add_8x4(const tran_low_t *input, uint8_t *dest,
                                 int stride, int eob, int bd, TX_TYPE tx_type) {
  (void)eob;
  av1_highbd_iht8x4_32_add_c(input, dest, stride, tx_type, bd);
}

void av1_highbd_inv_txfm_add_4x16(const tran_low_t *input, uint8_t *dest,
                                  int stride, int eob, int bd,
                                  TX_TYPE tx_type) {
  (void)eob;
  av1_highbd_iht4x16_64_add_c(input, dest, stride, tx_type, bd);
}

void av1_highbd_inv_txfm_add_16x4(const tran_low_t *input, uint8_t *dest,
                                  int stride, int eob, int bd,
                                  TX_TYPE tx_type) {
  (void)eob;
  av1_highbd_iht16x4_64_add_c(input, dest, stride, tx_type, bd);
}

static void highbd_inv_txfm_add_8x16(const tran_low_t *input, uint8_t *dest,
                                     int stride, int eob, int bd,
                                     TX_TYPE tx_type) {
  (void)eob;
  av1_highbd_iht8x16_128_add_c(input, dest, stride, tx_type, bd);
}

static void highbd_inv_txfm_add_16x8(const tran_low_t *input, uint8_t *dest,
                                     int stride, int eob, int bd,
                                     TX_TYPE tx_type) {
  (void)eob;
  av1_highbd_iht16x8_128_add_c(input, dest, stride, tx_type, bd);
}

void av1_highbd_inv_txfm_add_8x32(const tran_low_t *input, uint8_t *dest,
                                  int stride, int eob, int bd,
                                  TX_TYPE tx_type) {
  (void)eob;
  av1_highbd_iht8x32_256_add_c(input, dest, stride, tx_type, bd);
}

void av1_highbd_inv_txfm_add_32x8(const tran_low_t *input, uint8_t *dest,
                                  int stride, int eob, int bd,
                                  TX_TYPE tx_type) {
  (void)eob;
  av1_highbd_iht32x8_256_add_c(input, dest, stride, tx_type, bd);
}

static void highbd_inv_txfm_add_16x32(const tran_low_t *input, uint8_t *dest,
                                      int stride, int eob, int bd,
                                      TX_TYPE tx_type) {
  (void)eob;
  av1_highbd_iht16x32_512_add_c(input, dest, stride, tx_type, bd);
}

static void highbd_inv_txfm_add_32x16(const tran_low_t *input, uint8_t *dest,
                                      int stride, int eob, int bd,
                                      TX_TYPE tx_type) {
  (void)eob;
  av1_highbd_iht32x16_512_add_c(input, dest, stride, tx_type, bd);
}

static void highbd_inv_txfm_add_8x8(const tran_low_t *input, uint8_t *dest,
                                    int stride, int eob, int bd,
                                    TX_TYPE tx_type) {
  (void)eob;
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
#endif  // CONFIG_EXT_TX
      av1_inv_txfm2d_add_8x8(input, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                             bd);
      break;
#if CONFIG_EXT_TX
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
      // Use C version since DST only exists in C code
      av1_highbd_iht8x8_64_add_c(input, dest, stride, tx_type, bd);
      break;
    case IDTX:
      highbd_inv_idtx_add_c(input, dest, stride, 8, tx_type, bd);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}

static void highbd_inv_txfm_add_16x16(const tran_low_t *input, uint8_t *dest,
                                      int stride, int eob, int bd,
                                      TX_TYPE tx_type) {
  (void)eob;
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
#endif  // CONFIG_EXT_TX
      av1_inv_txfm2d_add_16x16(input, CONVERT_TO_SHORTPTR(dest), stride,
                               tx_type, bd);
      break;
#if CONFIG_EXT_TX
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
      // Use C version since DST only exists in C code
      av1_highbd_iht16x16_256_add_c(input, dest, stride, tx_type, bd);
      break;
    case IDTX:
      highbd_inv_idtx_add_c(input, dest, stride, 16, tx_type, bd);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}

static void highbd_inv_txfm_add_32x32(const tran_low_t *input, uint8_t *dest,
                                      int stride, int eob, int bd,
                                      TX_TYPE tx_type) {
  (void)eob;
  switch (tx_type) {
    case DCT_DCT:
      av1_inv_txfm2d_add_32x32(input, CONVERT_TO_SHORTPTR(dest), stride,
                               DCT_DCT, bd);
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
      highbd_iht32x32_1024_add_c(input, dest, stride, tx_type, bd);
      break;
    case IDTX:
      highbd_inv_idtx_add_c(input, dest, stride, 32, tx_type, bd);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}

#if CONFIG_TX64X64
static void highbd_inv_txfm_add_64x64(const tran_low_t *input, uint8_t *dest,
                                      int stride, int eob, int bd,
                                      TX_TYPE tx_type) {
  (void)eob;
  switch (tx_type) {
    case DCT_DCT:
      av1_inv_txfm2d_add_64x64(input, CONVERT_TO_SHORTPTR(dest), stride,
                               DCT_DCT, bd);
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
      highbd_iht64x64_4096_add_c(input, dest, stride, tx_type, bd);
      break;
    case IDTX:
      highbd_inv_idtx_add_c(input, dest, stride, 64, tx_type, bd);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}
#endif  // CONFIG_TX64X64
#endif  // CONFIG_HIGHBITDEPTH

void av1_inv_txfm_add(const tran_low_t *input, uint8_t *dest, int stride,
                      INV_TXFM_PARAM *inv_txfm_param) {
  const TX_TYPE tx_type = inv_txfm_param->tx_type;
  const TX_SIZE tx_size = inv_txfm_param->tx_size;
  const int eob = inv_txfm_param->eob;
  const int lossless = inv_txfm_param->lossless;

  switch (tx_size) {
#if CONFIG_TX64X64
    case TX_64X64: inv_txfm_add_64x64(input, dest, stride, eob, tx_type); break;
#endif  // CONFIG_TX64X64
    case TX_32X32: inv_txfm_add_32x32(input, dest, stride, eob, tx_type); break;
    case TX_16X16: inv_txfm_add_16x16(input, dest, stride, eob, tx_type); break;
    case TX_8X8: inv_txfm_add_8x8(input, dest, stride, eob, tx_type); break;
    case TX_4X8: av1_inv_txfm_add_4x8(input, dest, stride, eob, tx_type); break;
    case TX_8X4: av1_inv_txfm_add_8x4(input, dest, stride, eob, tx_type); break;
    case TX_8X16: inv_txfm_add_8x16(input, dest, stride, eob, tx_type); break;
    case TX_16X8: inv_txfm_add_16x8(input, dest, stride, eob, tx_type); break;
    case TX_16X32: inv_txfm_add_16x32(input, dest, stride, eob, tx_type); break;
    case TX_32X16: inv_txfm_add_32x16(input, dest, stride, eob, tx_type); break;
    case TX_4X4:
      // this is like av1_short_idct4x4 but has a special case around eob<=1
      // which is significant (not just an optimization) for the lossless
      // case.
      av1_inv_txfm_add_4x4(input, dest, stride, eob, tx_type, lossless);
      break;
#if CONFIG_CB4X4
    case TX_2X2:
      inv_txfm_add_2x2(input, dest, stride, eob, tx_type, lossless);
      break;
#endif
    default: assert(0 && "Invalid transform size"); break;
  }
}

static void init_inv_txfm_param(const MACROBLOCKD *xd, TX_SIZE tx_size,
                                TX_TYPE tx_type, int eob, INV_TXFM_PARAM *inv) {
  inv->tx_type = tx_type;
  inv->tx_size = tx_size;
  inv->eob = eob;
  inv->lossless = xd->lossless[xd->mi[0]->mbmi.segment_id];
#if CONFIG_HIGHBITDEPTH
  inv->bd = xd->bd;
#endif
#if CONFIG_ADAPT_SCAN
  inv->eob_threshold = &xd->eob_threshold_md[tx_size][tx_type][0];
#endif
}

void av1_inverse_transform_block(const MACROBLOCKD *xd,
                                 const tran_low_t *dqcoeff, TX_TYPE tx_type,
                                 TX_SIZE tx_size, uint8_t *dst, int stride,
                                 int eob) {
  if (!eob) return;
#if CONFIG_PVQ
  const BLOCK_SIZE tx_bsize = txsize_to_bsize[tx_size];
  const int txb_width = block_size_wide[tx_bsize];
  const int txb_height = block_size_high[tx_bsize];
  int r, c;
#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    uint16_t *dst16 = CONVERT_TO_SHORTPTR(dst);
    for (r = 0; r < txb_height; r++)
      for (c = 0; c < txb_width; c++)
        CONVERT_TO_SHORTPTR(dst)[r * stride + c] = 0;
  } else {
#endif  // CONFIG_HIGHBITDEPTH
    for (r = 0; r < txb_height; r++)
      for (c = 0; c < txb_width; c++) dst[r * stride + c] = 0;
#if CONFIG_HIGHBITDEPTH
  }
#endif  // CONFIG_HIGHBITDEPTH
#endif  // CONFIG_PVQ
  INV_TXFM_PARAM inv_txfm_param;
  init_inv_txfm_param(xd, tx_size, tx_type, eob, &inv_txfm_param);

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    av1_highbd_inv_txfm_add(dqcoeff, dst, stride, &inv_txfm_param);
  } else {
#endif  // CONFIG_HIGHBITDEPTH
    av1_inv_txfm_add(dqcoeff, dst, stride, &inv_txfm_param);
#if CONFIG_HIGHBITDEPTH
  }
#endif  // CONFIG_HIGHBITDEPTH
}

void av1_inverse_transform_block_facade(MACROBLOCKD *xd, int plane, int block,
                                        int blk_row, int blk_col, int eob) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  tran_low_t *dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_SIZE tx_size = get_tx_size(plane, xd);
  const TX_TYPE tx_type = get_tx_type(plane_type, xd, block, tx_size);
  const int dst_stride = pd->dst.stride;
  uint8_t *dst =
      &pd->dst.buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];
  av1_inverse_transform_block(xd, dqcoeff, tx_type, tx_size, dst, dst_stride,
                              eob);
}

#if CONFIG_HIGHBITDEPTH
void av1_highbd_inv_txfm_add(const tran_low_t *input, uint8_t *dest, int stride,
                             INV_TXFM_PARAM *inv_txfm_param) {
  const TX_TYPE tx_type = inv_txfm_param->tx_type;
  const TX_SIZE tx_size = inv_txfm_param->tx_size;
  const int eob = inv_txfm_param->eob;
  const int bd = inv_txfm_param->bd;
  const int lossless = inv_txfm_param->lossless;

  switch (tx_size) {
#if CONFIG_TX64X64
    case TX_64X64:
      highbd_inv_txfm_add_64x64(input, dest, stride, eob, bd, tx_type);
      break;
#endif  // CONFIG_TX64X64
    case TX_32X32:
      highbd_inv_txfm_add_32x32(input, dest, stride, eob, bd, tx_type);
      break;
    case TX_16X16:
      highbd_inv_txfm_add_16x16(input, dest, stride, eob, bd, tx_type);
      break;
    case TX_8X8:
      highbd_inv_txfm_add_8x8(input, dest, stride, eob, bd, tx_type);
      break;
    case TX_4X8:
      av1_highbd_inv_txfm_add_4x8(input, dest, stride, eob, bd, tx_type);
      break;
    case TX_8X4:
      av1_highbd_inv_txfm_add_8x4(input, dest, stride, eob, bd, tx_type);
      break;
    case TX_8X16:
      highbd_inv_txfm_add_8x16(input, dest, stride, eob, bd, tx_type);
      break;
    case TX_16X8:
      highbd_inv_txfm_add_16x8(input, dest, stride, eob, bd, tx_type);
      break;
    case TX_16X32:
      highbd_inv_txfm_add_16x32(input, dest, stride, eob, bd, tx_type);
      break;
    case TX_32X16:
      highbd_inv_txfm_add_32x16(input, dest, stride, eob, bd, tx_type);
      break;
    case TX_4X4:
      // this is like av1_short_idct4x4 but has a special case around eob<=1
      // which is significant (not just an optimization) for the lossless
      // case.
      av1_highbd_inv_txfm_add_4x4(input, dest, stride, eob, bd, tx_type,
                                  lossless);
      break;
#if CONFIG_CB4X4
    case TX_2X2:
      highbd_inv_txfm_add_2x2(input, dest, stride, eob, bd, tx_type, lossless);
      break;
#endif
    default: assert(0 && "Invalid transform size"); break;
  }
}
#endif  // CONFIG_HIGHBITDEPTH
