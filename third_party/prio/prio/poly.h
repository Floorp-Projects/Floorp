/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef _FFT__H
#define _FFT__H

#include <mpi.h>
#include <mprio.h>
#include <stdbool.h>

#include "mparray.h"

/*
 * Compute the FFT or inverse FFT of the array in `points_in`.
 * The length of the input and output arrays must be a multiple
 * of two and must be no longer than the number of precomputed
 * roots in the PrioConfig object passed in.
 */
SECStatus poly_fft(MPArray points_out, const_MPArray points_in,
                   const_PrioConfig cfg, bool invert);

/*
 * Get an array
 *    (r^0, r^1, r^2, ... )
 * where r is an n-th root of unity, for n a power of two
 * less than cfg->n_roots.
 *
 * Do NOT mp_clear() the mp_ints stored in roots_out.
 * These are owned by the PrioConfig object.
 */
SECStatus poly_fft_get_roots(MPArray roots_out, int n_points,
                             const_PrioConfig cfg, bool invert);

/*
 * Evaluate the polynomial specified by the coefficients
 * at the point `eval_at` and return the result as `value`.
 */
SECStatus poly_eval(mp_int* value, const_MPArray coeffs, const mp_int* eval_at,
                    const_PrioConfig cfg);

/*
 * Interpolate the polynomial through the points
 *    (x_1, y_1), ..., (x_N, y_N),
 * where x_i is an N-th root of unity and the y_i values are
 * specified by `poly_points`. Evaluate the resulting polynomial
 * at the point `eval_at`. Return the result as `value`.
 */
SECStatus poly_interp_evaluate(mp_int* value, const_MPArray poly_points,
                               const mp_int* eval_at, const_PrioConfig cfg);

#endif
