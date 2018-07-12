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

#include "./aom_dsp_rtcd.h"
#include "./aom_config.h"
#include "aom/aom_integer.h"

void aom_lpf_vertical_4_dual_neon(uint8_t *s, int p, const uint8_t *blimit0,
                                  const uint8_t *limit0, const uint8_t *thresh0,
                                  const uint8_t *blimit1, const uint8_t *limit1,
                                  const uint8_t *thresh1) {
  aom_lpf_vertical_4_neon(s, p, blimit0, limit0, thresh0);
  aom_lpf_vertical_4_neon(s + 8 * p, p, blimit1, limit1, thresh1);
}

#if HAVE_NEON_ASM
void aom_lpf_horizontal_8_dual_neon(
    uint8_t *s, int p /* pitch */, const uint8_t *blimit0,
    const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1,
    const uint8_t *limit1, const uint8_t *thresh1) {
  aom_lpf_horizontal_8_neon(s, p, blimit0, limit0, thresh0);
  aom_lpf_horizontal_8_neon(s + 8, p, blimit1, limit1, thresh1);
}

void aom_lpf_vertical_8_dual_neon(uint8_t *s, int p, const uint8_t *blimit0,
                                  const uint8_t *limit0, const uint8_t *thresh0,
                                  const uint8_t *blimit1, const uint8_t *limit1,
                                  const uint8_t *thresh1) {
  aom_lpf_vertical_8_neon(s, p, blimit0, limit0, thresh0);
  aom_lpf_vertical_8_neon(s + 8 * p, p, blimit1, limit1, thresh1);
}

void aom_lpf_vertical_16_dual_neon(uint8_t *s, int p, const uint8_t *blimit,
                                   const uint8_t *limit,
                                   const uint8_t *thresh) {
  aom_lpf_vertical_16_neon(s, p, blimit, limit, thresh);
  aom_lpf_vertical_16_neon(s + 8 * p, p, blimit, limit, thresh);
}
#endif  // HAVE_NEON_ASM
