/*
 *  mpprime.h
 *
 *  Utilities for finding and working with prime and pseudo-prime
 *  integers
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the MPI Arbitrary Precision Integer Arithmetic
 * library.
 *
 * The Initial Developer of the Original Code is Michael J. Fromberger.
 * Portions created by Michael J. Fromberger are 
 * Copyright (C) 1997, 1998, 1999, 2000 Michael J. Fromberger. 
 * All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable
 * instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

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
