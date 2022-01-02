/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MP_GF2M_H_
#define _MP_GF2M_H_

#include "mpi.h"

mp_err mp_badd(const mp_int *a, const mp_int *b, mp_int *c);
mp_err mp_bmul(const mp_int *a, const mp_int *b, mp_int *c);

/* For modular arithmetic, the irreducible polynomial f(t) is represented
 * as an array of int[], where f(t) is of the form:
 *     f(t) = t^p[0] + t^p[1] + ... + t^p[k]
 * where m = p[0] > p[1] > ... > p[k] = 0.
 */
mp_err mp_bmod(const mp_int *a, const unsigned int p[], mp_int *r);
mp_err mp_bmulmod(const mp_int *a, const mp_int *b, const unsigned int p[],
                  mp_int *r);
mp_err mp_bsqrmod(const mp_int *a, const unsigned int p[], mp_int *r);
mp_err mp_bdivmod(const mp_int *y, const mp_int *x, const mp_int *pp,
                  const unsigned int p[], mp_int *r);

int mp_bpoly2arr(const mp_int *a, unsigned int p[], int max);
mp_err mp_barr2poly(const unsigned int p[], mp_int *a);

#endif /* _MP_GF2M_H_ */
