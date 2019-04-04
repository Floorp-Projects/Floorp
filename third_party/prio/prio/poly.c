/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <mprio.h>

#include "config.h"
#include "poly.h"
#include "util.h"

/*
 * A nice exposition of the recursive FFT/DFT algorithm we implement
 * is in the book:
 *
 *   "Modern Computer Algebra"
 *    by Von zur Gathen and Gerhard.
 *    Cambridge University Press, 2013.
 *
 * They present this algorithm as Algorithm 8.14.
 */

static SECStatus
fft_recurse(mp_int* out, const mp_int* mod, int n, const mp_int* roots,
            const mp_int* ys, mp_int* tmp, mp_int* ySub, mp_int* rootsSub)
{
  if (n == 1) {
    MP_CHECK(mp_copy(&ys[0], &out[0]));
    return SECSuccess;
  }

  // Recurse on the first half
  for (int i = 0; i < n / 2; i++) {
    MP_CHECK(mp_addmod(&ys[i], &ys[i + (n / 2)], mod, &ySub[i]));
    MP_CHECK(mp_copy(&roots[2 * i], &rootsSub[i]));
  }

  MP_CHECK(fft_recurse(tmp, mod, n / 2, rootsSub, ySub, &tmp[n / 2],
                       &ySub[n / 2], &rootsSub[n / 2]));
  for (int i = 0; i < n / 2; i++) {
    MP_CHECK(mp_copy(&tmp[i], &out[2 * i]));
  }

  // Recurse on the second half
  for (int i = 0; i < n / 2; i++) {
    MP_CHECK(mp_submod(&ys[i], &ys[i + (n / 2)], mod, &ySub[i]));
    MP_CHECK(mp_mulmod(&ySub[i], &roots[i], mod, &ySub[i]));
  }

  MP_CHECK(fft_recurse(tmp, mod, n / 2, rootsSub, ySub, &tmp[n / 2],
                       &ySub[n / 2], &rootsSub[n / 2]));
  for (int i = 0; i < n / 2; i++) {
    MP_CHECK(mp_copy(&tmp[i], &out[2 * i + 1]));
  }

  return SECSuccess;
}

static SECStatus
fft_interpolate_raw(mp_int* out, const mp_int* ys, int nPoints,
                    const_MPArray roots, const mp_int* mod, bool invert)
{
  SECStatus rv = SECSuccess;
  MPArray tmp = NULL;
  MPArray ySub = NULL;
  MPArray rootsSub = NULL;

  P_CHECKA(tmp = MPArray_new(nPoints));
  P_CHECKA(ySub = MPArray_new(nPoints));
  P_CHECKA(rootsSub = MPArray_new(nPoints));

  mp_int n_inverse;
  MP_DIGITS(&n_inverse) = NULL;

  MP_CHECKC(fft_recurse(out, mod, nPoints, roots->data, ys, tmp->data,
                        ySub->data, rootsSub->data));

  if (invert) {
    MP_CHECKC(mp_init(&n_inverse));

    mp_set(&n_inverse, nPoints);
    MP_CHECKC(mp_invmod(&n_inverse, mod, &n_inverse));
    for (int i = 0; i < nPoints; i++) {
      MP_CHECKC(mp_mulmod(&out[i], &n_inverse, mod, &out[i]));
    }
  }

cleanup:
  MPArray_clear(tmp);
  MPArray_clear(ySub);
  MPArray_clear(rootsSub);
  mp_clear(&n_inverse);

  return rv;
}

/*
 * The PrioConfig object has a list of N-th roots of unity for large N.
 * This routine returns the n-th roots of unity for n < N, where n is
 * a power of two. If the `invert` flag is set, it returns the inverses
 * of the n-th roots of unity.
 */
SECStatus
poly_fft_get_roots(MPArray roots_out, int n_points, const_PrioConfig cfg,
                   bool invert)
{
  if (n_points < 1) {
    return SECFailure;
  }

  if (n_points != roots_out->len) {
    return SECFailure;
  }

  if (n_points > cfg->n_roots) {
    return SECFailure;
  }

  mp_set(&roots_out->data[0], 1);
  if (n_points == 1) {
    return SECSuccess;
  }

  const int step_size = cfg->n_roots / n_points;
  mp_int* gen = &roots_out->data[1];

  MP_CHECK(mp_copy(&cfg->generator, gen));

  if (invert) {
    MP_CHECK(mp_invmod(gen, &cfg->modulus, gen));
  }

  // Compute g' = g^step_size
  // Now, g' generates a subgroup of order n_points.
  MP_CHECK(mp_exptmod_d(gen, step_size, &cfg->modulus, gen));

  for (int i = 2; i < n_points; i++) {
    // Compute g^i for all i in {0,..., n-1}
    MP_CHECK(mp_mulmod(gen, &roots_out->data[i - 1], &cfg->modulus,
                       &roots_out->data[i]));
  }

  return SECSuccess;
}

SECStatus
poly_fft(MPArray points_out, const_MPArray points_in, const_PrioConfig cfg,
         bool invert)
{
  SECStatus rv = SECSuccess;
  const int n_points = points_in->len;
  MPArray scaled_roots = NULL;

  if (points_out->len != points_in->len)
    return SECFailure;
  if (n_points > cfg->n_roots)
    return SECFailure;
  if (cfg->n_roots % n_points != 0)
    return SECFailure;

  P_CHECKA(scaled_roots = MPArray_new(n_points));
  P_CHECKC(poly_fft_get_roots(scaled_roots, n_points, cfg, invert));

  P_CHECKC(fft_interpolate_raw(points_out->data, points_in->data, n_points,
                               scaled_roots, &cfg->modulus, invert));

cleanup:
  MPArray_clear(scaled_roots);

  return SECSuccess;
}

SECStatus
poly_eval(mp_int* value, const_MPArray coeffs, const mp_int* eval_at,
          const_PrioConfig cfg)
{
  SECStatus rv = SECSuccess;
  const int n = coeffs->len;

  // Use Horner's method to evaluate the polynomial at the point
  // `eval_at`
  MP_CHECK(mp_copy(&coeffs->data[n - 1], value));
  for (int i = n - 2; i >= 0; i--) {
    MP_CHECK(mp_mulmod(value, eval_at, &cfg->modulus, value));
    MP_CHECK(mp_addmod(value, &coeffs->data[i], &cfg->modulus, value));
  }

  return rv;
}

SECStatus
poly_interp_evaluate(mp_int* value, const_MPArray poly_points,
                     const mp_int* eval_at, const_PrioConfig cfg)
{
  SECStatus rv;
  MPArray coeffs = NULL;
  const int N = poly_points->len;

  P_CHECKA(coeffs = MPArray_new(N));

  // Interpolate polynomial through roots of unity
  P_CHECKC(poly_fft(coeffs, poly_points, cfg, true))
  P_CHECKC(poly_eval(value, coeffs, eval_at, cfg));

cleanup:
  MPArray_clear(coeffs);
  return rv;
}
