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
                    const mp_int* roots, const mp_int* mod, bool invert)
{
  SECStatus rv = SECSuccess;
  mp_int tmp[nPoints];
  mp_int ySub[nPoints];
  mp_int rootsSub[nPoints];
  for (int i = 0; i < nPoints; i++) {
    MP_DIGITS(&tmp[i]) = NULL;
    MP_DIGITS(&ySub[i]) = NULL;
    MP_DIGITS(&rootsSub[i]) = NULL;
  }

  mp_int n_inverse;
  MP_DIGITS(&n_inverse) = NULL;

  for (int i = 0; i < nPoints; i++) {
    MP_CHECKC(mp_init(&tmp[i]));
    MP_CHECKC(mp_init(&ySub[i]));
    MP_CHECKC(mp_init(&rootsSub[i]));
  }

  MP_CHECK(fft_recurse(out, mod, nPoints, roots, ys, tmp, ySub, rootsSub));

  if (invert) {
    MP_CHECKC(mp_init(&n_inverse));

    mp_set(&n_inverse, nPoints);
    MP_CHECKC(mp_invmod(&n_inverse, mod, &n_inverse));
    for (int i = 0; i < nPoints; i++) {
      MP_CHECKC(mp_mulmod(&out[i], &n_inverse, mod, &out[i]));
    }
  }

cleanup:
  mp_clear(&n_inverse);
  for (int i = 0; i < nPoints; i++) {
    mp_clear(&tmp[i]);
    mp_clear(&ySub[i]);
    mp_clear(&rootsSub[i]);
  }

  return rv;
}

/*
 * The PrioConfig object has a list of N-th roots of unity for large N.
 * This routine returns the n-th roots of unity for n < N, where n is
 * a power of two. If the `invert` flag is set, it returns the inverses
 * of the n-th roots of unity.
 */
SECStatus
poly_fft_get_roots(mp_int* roots_out, int n_points, const_PrioConfig cfg,
                   bool invert)
{
  if (n_points > cfg->n_roots)
    return SECFailure;
  const mp_int* roots_in = invert ? cfg->rootsInv->data : cfg->roots->data;
  const int step_size = cfg->n_roots / n_points;

  for (int i = 0; i < n_points; i++) {
    roots_out[i] = roots_in[i * step_size];
  }

  return SECSuccess;
}

SECStatus
poly_fft(MPArray points_out, const_MPArray points_in, const_PrioConfig cfg,
         bool invert)
{
  SECStatus rv = SECSuccess;
  const int n_points = points_in->len;
  if (points_out->len != points_in->len)
    return SECFailure;
  if (n_points > cfg->n_roots)
    return SECFailure;
  if (cfg->n_roots % n_points != 0)
    return SECFailure;

  mp_int scaled_roots[n_points];
  P_CHECK(poly_fft_get_roots(scaled_roots, n_points, cfg, invert));

  MP_CHECK(fft_interpolate_raw(points_out->data, points_in->data, n_points,
                               scaled_roots, &cfg->modulus, invert));

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
  mp_copy(&coeffs->data[n - 1], value);
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
  mp_int roots[N];

  P_CHECKA(coeffs = MPArray_new(N));
  P_CHECKC(poly_fft_get_roots(roots, N, cfg, false));

  // Interpolate polynomial through roots of unity
  P_CHECKC(poly_fft(coeffs, poly_points, cfg, true))
  P_CHECKC(poly_eval(value, coeffs, eval_at, cfg));

cleanup:
  MPArray_clear(coeffs);
  return rv;
}
