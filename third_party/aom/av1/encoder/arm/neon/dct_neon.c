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

#include <arm_neon.h>

#include "./av1_rtcd.h"
#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"

#include "av1/common/blockd.h"
#include "aom_dsp/txfm_common.h"

void av1_fdct8x8_quant_neon(const int16_t *input, int stride,
                            int16_t *coeff_ptr, intptr_t n_coeffs,
                            int skip_block, const int16_t *zbin_ptr,
                            const int16_t *round_ptr, const int16_t *quant_ptr,
                            const int16_t *quant_shift_ptr, int16_t *qcoeff_ptr,
                            int16_t *dqcoeff_ptr, const int16_t *dequant_ptr,
                            uint16_t *eob_ptr, const int16_t *scan_ptr,
                            const int16_t *iscan_ptr) {
  int16_t temp_buffer[64];
  (void)coeff_ptr;

  aom_fdct8x8_neon(input, temp_buffer, stride);
  av1_quantize_fp_neon(temp_buffer, n_coeffs, skip_block, zbin_ptr, round_ptr,
                       quant_ptr, quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr,
                       dequant_ptr, eob_ptr, scan_ptr, iscan_ptr);
}
