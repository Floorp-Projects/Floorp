/*
 *  mpprime.h
 *
 *  Utilities for finding and working with prime and pseudo-prime
 *  integers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _H_MP_PRIME_
#define _H_MP_PRIME_

#include "mpi.h"

extern const int prime_tab_size;   /* number of primes available */
extern const mp_digit prime_tab[];

/* Tests for divisibility    */
mp_err  mpp_divis(mp_int *a, mp_int *b);
mp_err  mpp_divis_d(mp_int *a, mp_digit d);

/* Random selection          */
mp_err  mpp_random(mp_int *a);
mp_err  mpp_random_size(mp_int *a, mp_size prec);

/* Pseudo-primality testing  */
mp_err  mpp_divis_vector(mp_int *a, const mp_digit *vec, int size, int *which);
mp_err  mpp_divis_primes(mp_int *a, mp_digit *np);
mp_err  mpp_fermat(mp_int *a, mp_digit w);
mp_err mpp_fermat_list(mp_int *a, const mp_digit *primes, mp_size nPrimes);
mp_err  mpp_pprime(mp_int *a, int nt);
mp_err mpp_sieve(mp_int *trial, const mp_digit *primes, mp_size nPrimes, 
		 unsigned char *sieve, mp_size nSieve);
mp_err mpp_make_prime(mp_int *start, mp_size nBits, mp_size strong,
		      unsigned long * nTries);

#endif /* end _H_MP_PRIME_ */
