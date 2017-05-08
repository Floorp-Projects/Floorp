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
#include "av1/common/generic_code.h"
#include "av1/common/odintrin.h"
#include "pvq_encoder.h"

/** Encodes a value from 0 to N-1 (with N up to 16) based on a cdf and adapts
 * the cdf accordingly.
 *
 * @param [in,out] w     multi-symbol entropy encoder
 * @param [in]     val   variable being encoded
 * @param [in,out] cdf   CDF of the variable (Q15)
 * @param [in]     n     number of values possible
 * @param [in,out] count number of symbols encoded with that cdf so far
 * @param [in]     rate  adaptation rate shift (smaller is faster)
 */
void aom_encode_cdf_adapt_q15(aom_writer *w, int val, uint16_t *cdf, int n,
 int *count, int rate) {
  int i;
  if (*count == 0) {
    /* On the first call, we normalize the cdf to (32768 - n). This should
       eventually be moved to the state init, but for now it makes it much
       easier to experiment and convert symbols to the Q15 adaptation.*/
    int ft;
    ft = cdf[n - 1];
    for (i = 0; i < n; i++) {
      cdf[i] = AOM_ICDF(cdf[i]*32768/ft);
    }
  }
  aom_write_cdf(w, val, cdf, n);
  aom_cdf_adapt_q15(val, cdf, n, count, rate);
}

/** Encodes a random variable using a "generic" model, assuming that the
 * distribution is one-sided (zero and up), has a single mode, and decays
 * exponentially past the model.
 *
 * @param [in,out] w     multi-symbol entropy encoder
 * @param [in,out] model generic probability model
 * @param [in]     x     variable being encoded
 * @param [in,out] ExQ16 expectation of x (adapted)
 * @param [in]     integration integration period of ExQ16 (leaky average over
 * 1<<integration samples)
 */
void generic_encode(aom_writer *w, generic_encoder *model, int x,
 int *ex_q16, int integration) {
  int lg_q1;
  int shift;
  int id;
  uint16_t *cdf;
  int xs;
  lg_q1 = log_ex(*ex_q16);
  OD_LOG((OD_LOG_ENTROPY_CODER, OD_LOG_DEBUG,
   "%d %d", *ex_q16, lg_q1));
  /* If expectation is too large, shift x to ensure that
     all we have past xs=15 is the exponentially decaying tail
     of the distribution */
  shift = OD_MAXI(0, (lg_q1 - 5) >> 1);
  /* Choose the cdf to use: we have two per "octave" of ExQ16 */
  id = OD_MINI(GENERIC_TABLES - 1, lg_q1);
  cdf = model->cdf[id];
  xs = (x + (1 << shift >> 1)) >> shift;
  aom_write_symbol_pvq(w, OD_MINI(15, xs), cdf, 16);
  if (xs >= 15) {
    int e;
    unsigned decay;
    /* Estimate decay based on the assumption that the distribution is close
       to Laplacian for large values. We should probably have an adaptive
       estimate instead. Note: The 2* is a kludge that's not fully understood
       yet. */
    OD_ASSERT(*ex_q16 < INT_MAX >> 1);
    e = ((2**ex_q16 >> 8) + (1 << shift >> 1)) >> shift;
    decay = OD_MAXI(2, OD_MINI(254, 256*e/(e + 256)));
    /* Encode the tail of the distribution assuming exponential decay. */
    aom_laplace_encode_special(w, xs - 15, decay);
  }
  if (shift != 0) {
    int special;
    /* Because of the rounding, there's only half the number of possibilities
       for xs=0. */
    special = xs == 0;
    if (shift - special > 0) {
      aom_write_literal(w, x - (xs << shift) + (!special << (shift - 1)),
       shift - special);
    }
  }
  generic_model_update(ex_q16, x, integration);
  OD_LOG((OD_LOG_ENTROPY_CODER, OD_LOG_DEBUG,
   "enc: %d %d %d %d %d %x", *ex_q16, x, shift, id, xs, enc->rng));
}

/** Estimates the cost of encoding a value with generic_encode().
 *
 * @param [in,out] model generic probability model
 * @param [in]     x     variable being encoded
 * @param [in,out] ExQ16 expectation of x (adapted)
 * @return number of bits (approximation)
 */
double generic_encode_cost(generic_encoder *model, int x, int *ex_q16) {
  int lg_q1;
  int shift;
  int id;
  uint16_t *cdf;
  int xs;
  int extra;
  lg_q1 = log_ex(*ex_q16);
  /* If expectation is too large, shift x to ensure that
       all we have past xs=15 is the exponentially decaying tail
       of the distribution */
  shift = OD_MAXI(0, (lg_q1 - 5) >> 1);
  /* Choose the cdf to use: we have two per "octave" of ExQ16 */
  id = OD_MINI(GENERIC_TABLES - 1, lg_q1);
  cdf = model->cdf[id];
  xs = (x + (1 << shift >> 1)) >> shift;
  extra = 0;
  if (shift) extra = shift - (xs == 0);
  xs = OD_MINI(15, xs);
  /* Shortcut: assume it's going to cost 2 bits for the Laplace coder. */
  if (xs == 15) extra += 2;
  return
      extra - OD_LOG2((double)(cdf[xs] - (xs == 0 ? 0 : cdf[xs - 1]))/cdf[15]);
}

/*Estimates the cost of encoding a value with a given CDF.*/
double od_encode_cdf_cost(int val, uint16_t *cdf, int n) {
  int total_prob;
  int prev_prob;
  double val_prob;
  OD_ASSERT(n > 0);
  total_prob = cdf[n - 1];
  if (val == 0) {
    prev_prob = 0;
  }
  else {
    prev_prob = cdf[val - 1];
  }
  val_prob = (cdf[val] - prev_prob) / (double)total_prob;
  return -OD_LOG2(val_prob);
}
