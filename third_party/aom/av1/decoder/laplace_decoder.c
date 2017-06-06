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

#include "aom_dsp/bitreader.h"
#include "av1/common/pvq.h"
#include "pvq_decoder.h"

#define aom_decode_pvq_split(r, adapt, sum, ctx, ACCT_STR_NAME) \
  aom_decode_pvq_split_(r, adapt, sum, ctx ACCT_STR_ARG(ACCT_STR_NAME))

static int aom_decode_pvq_split_(aom_reader *r, od_pvq_codeword_ctx *adapt,
 int sum, int ctx ACCT_STR_PARAM) {
  int shift;
  int count;
  int msbs;
  int fctx;
  count = 0;
  if (sum == 0) return 0;
  shift = OD_MAXI(0, OD_ILOG(sum) - 3);
  fctx = 7*ctx + (sum >> shift) - 1;
  msbs = aom_read_symbol_pvq(r, adapt->pvq_split_cdf[fctx], (sum >> shift) + 1,
      ACCT_STR_NAME);
  if (shift) count = aom_read_literal(r, shift, ACCT_STR_NAME);
  count += msbs << shift;
  if (count > sum) {
    count = sum;
#if !CONFIG_ANS
    r->ec.error = 1;
#else
# error "CONFIG_PVQ currently requires !CONFIG_ANS."
#endif
  }
  return count;
}

void aom_decode_band_pvq_splits(aom_reader *r, od_pvq_codeword_ctx *adapt,
 od_coeff *y, int n, int k, int level) {
  int mid;
  int count_right;
  if (n == 1) {
    y[0] = k;
  }
  else if (k == 0) {
    OD_CLEAR(y, n);
  }
  else if (k == 1 && n <= 16) {
    int cdf_id;
    int pos;
    cdf_id = od_pvq_k1_ctx(n, level == 0);
    OD_CLEAR(y, n);
    pos = aom_read_symbol_pvq(r, adapt->pvq_k1_cdf[cdf_id], n, "pvq:k1");
    y[pos] = 1;
  }
  else {
    mid = n >> 1;
    count_right = aom_decode_pvq_split(r, adapt, k, od_pvq_size_ctx(n),
     "pvq:split");
    aom_decode_band_pvq_splits(r, adapt, y, mid, k - count_right, level + 1);
    aom_decode_band_pvq_splits(r, adapt, y + mid, n - mid, count_right,
     level + 1);
  }
}

/** Decodes the tail of a Laplace-distributed variable, i.e. it doesn't
 * do anything special for the zero case.
 *
 * @param [dec] range decoder
 * @param [decay] decay factor of the distribution, i.e. pdf ~= decay^x
 *
 * @retval decoded variable x
 */
int aom_laplace_decode_special_(aom_reader *r, unsigned decay ACCT_STR_PARAM) {
  int pos;
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
  cdf = EXP_CDF_TABLE[(decay + 1) >> 1];
  OD_LOG((OD_LOG_PVQ, OD_LOG_DEBUG, "decay = %d\n", decay));
  xs = 0;
  do {
    sym = OD_MINI(xs, 15);
    {
      int i;
      OD_LOG((OD_LOG_PVQ, OD_LOG_DEBUG, "%d %d %d", xs, shift, sym));
      for (i = 0; i < 16; i++) {
        OD_LOG_PARTIAL((OD_LOG_PVQ, OD_LOG_DEBUG, "%d ", cdf[i]));
      }
      OD_LOG_PARTIAL((OD_LOG_PVQ, OD_LOG_DEBUG, "\n"));
    }
    sym = aom_read_cdf(r, cdf, 16, ACCT_STR_NAME);
    xs += sym;
  } while (sym >= 15);
  if (shift) pos = (xs << shift) + aom_read_literal(r, shift, ACCT_STR_NAME);
  else pos = xs;
  return pos;
}
