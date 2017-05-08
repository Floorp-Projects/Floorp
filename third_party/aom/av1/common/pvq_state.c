/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/common/pvq_state.h"
#include "av1/common/odintrin.h"

void od_adapt_ctx_reset(od_adapt_ctx *adapt, int is_keyframe) {
  int pli;
  od_adapt_pvq_ctx_reset(&adapt->pvq, is_keyframe);
  OD_CDFS_INIT_Q15(adapt->skip_cdf);
  for (pli = 0; pli < OD_NPLANES_MAX; pli++) {
    int i;
    OD_CDFS_INIT_DYNAMIC(adapt->model_dc[pli].cdf);
    for (i = 0; i < OD_TXSIZES; i++) {
      int j;
      adapt->ex_g[pli][i] = 8;
      for (j = 0; j < 3; j++) {
        adapt->ex_dc[pli][i][j] = pli > 0 ? 8 : 32768;
      }
    }
  }
}

void od_init_skipped_coeffs(int16_t *d, int16_t *pred, int is_keyframe, int bo,
                            int n, int w) {
  int i;
  int j;
  if (is_keyframe) {
    for (i = 0; i < n; i++) {
      for (j = 0; j < n; j++) {
        /* skip DC */
        if (i || j) d[bo + i * w + j] = 0;
      }
    }
  } else {
    for (i = 0; i < n; i++) {
      for (j = 0; j < n; j++) {
        d[bo + i * w + j] = pred[i * n + j];
      }
    }
  }
}
