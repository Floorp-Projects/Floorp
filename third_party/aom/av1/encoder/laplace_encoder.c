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

/* clang-format off */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#include "aom_dsp/bitwriter.h"
#include "av1/common/odintrin.h"
#include "av1/common/pvq.h"
#include "pvq_encoder.h"

static void aom_encode_pvq_split(aom_writer *w, od_pvq_codeword_ctx *adapt,
 int count, int sum, int ctx) {
  int shift;
  int rest;
  int fctx;
  if (sum == 0) return;
  shift = OD_MAXI(0, OD_ILOG(sum) - 3);
  if (shift) {
    rest = count & ((1 << shift) - 1);
    count >>= shift;
    sum >>= shift;
  }
  fctx = 7*ctx + sum - 1;
  aom_write_symbol_pvq(w, count, adapt->pvq_split_cdf[fctx], sum + 1);
  if (shift) aom_write_literal(w, rest, shift);
}

void aom_encode_band_pvq_splits(aom_writer *w, od_pvq_codeword_ctx *adapt,
 const int *y, int n, int k, int level) {
  int mid;
  int i;
  int count_right;
  if (n <= 1 || k == 0) return;
  if (k == 1 && n <= 16) {
    int cdf_id;
    int pos;
    cdf_id = od_pvq_k1_ctx(n, level == 0);
    for (pos = 0; !y[pos]; pos++);
    OD_ASSERT(pos < n);
    aom_write_symbol_pvq(w, pos, adapt->pvq_k1_cdf[cdf_id], n);
  }
  else {
    mid = n >> 1;
    count_right = k;
    for (i = 0; i < mid; i++) count_right -= abs(y[i]);
    aom_encode_pvq_split(w, adapt, count_right, k, od_pvq_size_ctx(n));
    aom_encode_band_pvq_splits(w, adapt, y, mid, k - count_right, level + 1);
    aom_encode_band_pvq_splits(w, adapt, y + mid, n - mid, count_right,
     level + 1);
  }
}

/** Encodes the tail of a Laplace-distributed variable, i.e. it doesn't
 * do anything special for the zero case.
 *
 * @param [in,out] enc     range encoder
 * @param [in]     x       variable to encode (has to be positive)
 * @param [in]     decay   decay factor of the distribution in Q8 format,
 * i.e. pdf ~= decay^x
 */
void aom_laplace_encode_special(aom_writer *w, int x, unsigned decay) {
  int shift;
  int xs;
  int sym;
  const uint16_t *cdf;
  shift = 0;
  /* We don't want a large decay value because that would require too many
     symbols. */
  while (decay > 235) {
    decay = (decay*decay + 128) >> 8;
    shift++;
  }
  decay = OD_MINI(decay, 254);
  decay = OD_MAXI(decay, 2);
  xs = x >> shift;
  cdf = EXP_CDF_TABLE[(decay + 1) >> 1];
  OD_LOG((OD_LOG_PVQ, OD_LOG_DEBUG, "decay = %d", decay));
  do {
    sym = OD_MINI(xs, 15);
    {
      int i;
      OD_LOG((OD_LOG_PVQ, OD_LOG_DEBUG, "%d %d %d %d %d\n", x, xs, shift,
       sym, max));
      for (i = 0; i < 16; i++) {
        OD_LOG_PARTIAL((OD_LOG_PVQ, OD_LOG_DEBUG, "%d ", cdf[i]));
      }
      OD_LOG_PARTIAL((OD_LOG_PVQ, OD_LOG_DEBUG, "\n"));
    }
    aom_write_cdf(w, sym, cdf, 16);
    xs -= 15;
  } while (sym >= 15);
  if (shift) aom_write_literal(w, x & ((1 << shift) - 1), shift);
}
