/*
 *  mpprime.c
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
 *	Netscape Communications Corporation
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
 * may use your version of this file under either the MPL or the GPL.
 */

#include "mpprime.h"
#include <stdlib.h>

#define SMALL_TABLE 0 /* determines size of hard-wired prime table */

#define RANDOM() rand()

#include "primes.c"  /* pull in the prime digit table */

/* 
   Test if any of a given vector of digits divides a.  If not, MP_NO
   is returned; otherwise, MP_YES is returned and 'which' is set to
   the index of the integer in the vector which divided a.
 */
mp_err    s_mpp_divp(mp_int *a, const mp_digit *vec, int size, int *which);

extern mp_err s_mp_pad(mp_int *mp, mp_size min); /* left pad with zeroes  */

/* {{{ mpp_divis(a, b) */

/*
  mpp_divis(a, b)

  Returns MP_YES if a is divisible by b, or MP_NO if it is not.
 */

mp_err  mpp_divis(mp_int *a, mp_int *b)
{
  mp_err  res;
  mp_int  rem;

  if((res = mp_init(&rem)) != MP_OKAY)
    return res;

  if((res = mp_mod(a, b, &rem)) != MP_OKAY)
    goto CLEANUP;

  if(mp_cmp_z(&rem) == 0)
    res = MP_YES;
  else
    res = MP_NO;

CLEANUP:
  mp_clear(&rem);
  return res;

} /* end mpp_divis() */

/* }}} */

/* {{{ mpp_divis_d(a, d) */

/*
  mpp_divis_d(a, d)

  Return MP_YES if a is divisible by d, or MP_NO if it is not.
 */

mp_err  mpp_divis_d(mp_int *a, mp_digit d)
{
  mp_err     res;
  mp_digit   rem;

  ARGCHK(a != NULL, MP_BADARG);

  if(d == 0)
    return MP_NO;

  if((res = mp_mod_d(a, d, &rem)) != MP_OKAY)
    return res;

  if(rem == 0)
    return MP_YES;
  else
    return MP_NO;

} /* end mpp_divis_d() */

/* }}} */

/* {{{ mpp_random(a) */

/*
  mpp_random(a)

  Assigns a random value to a.  This value is generated using the
  standard C library's rand() function, so it should not be used for
  cryptographic purposes, but it should be fine for primality testing,
  since all we really care about there is good statistical properties.

  As many digits as a currently has are filled with random digits.
 */

mp_err  mpp_random(mp_int *a)

{
  mp_digit  next = 0;
  int       ix, jx;

  ARGCHK(a != NULL, MP_BADARG);

  for(ix = 0; ix < USED(a); ix++) {
    for(jx = 0; jx < sizeof(mp_digit); jx++) {
      next = (next << CHAR_BIT) | (RANDOM() & UCHAR_MAX);
    }
    DIGIT(a, ix) = next;
  }

  return MP_OKAY;

} /* end mpp_random() */

/* }}} */

/* {{{ mpp_random_size(a, prec) */

mp_err  mpp_random_size(mp_int *a, mp_size prec)
{
  mp_err   res;

  ARGCHK(a != NULL && prec > 0, MP_BADARG);
  
  if((res = s_mp_pad(a, prec)) != MP_OKAY)
    return res;

  return mpp_random(a);

} /* end mpp_random_size() */

/* }}} */

/* {{{ mpp_divis_vector(a, vec, size, which) */

/*
  mpp_divis_vector(a, vec, size, which)

  Determines if a is divisible by any of the 'size' digits in vec.
  Returns MP_YES and sets 'which' to the index of the offending digit,
  if it is; returns MP_NO if it is not.
 */

mp_err  mpp_divis_vector(mp_int *a, const mp_digit *vec, int size, int *which)
{
  ARGCHK(a != NULL && vec != NULL && size > 0, MP_BADARG);
  
  return s_mpp_divp(a, vec, size, which);

} /* end mpp_divis_vector() */

/* }}} */

/* {{{ mpp_divis_primes(a, np) */

/*
  mpp_divis_primes(a, np)

  Test whether a is divisible by any of the first 'np' primes.  If it
  is, returns MP_YES and sets *np to the value of the digit that did
  it.  If not, returns MP_NO.
 */
mp_err  mpp_divis_primes(mp_int *a, mp_digit *np)
{
  int     size, which;
  mp_err  res;

  ARGCHK(a != NULL && np != NULL, MP_BADARG);

  size = (int)*np;
  if(size > prime_tab_size)
    size = prime_tab_size;

  res = mpp_divis_vector(a, prime_tab, size, &which);
  if(res == MP_YES) 
    *np = prime_tab[which];

  return res;

} /* end mpp_divis_primes() */

/* }}} */

/* {{{ mpp_fermat(a, w) */

/*
  Using w as a witness, try pseudo-primality testing based on Fermat's
  little theorem.  If a is prime, and (w, a) = 1, then w^a == w (mod
  a).  So, we compute z = w^a (mod a) and compare z to w; if they are
  equal, the test passes and we return MP_YES.  Otherwise, we return
  MP_NO.
 */
mp_err  mpp_fermat(mp_int *a, mp_digit w)
{
  mp_int  base, test;
  mp_err  res;
  
  if((res = mp_init(&base)) != MP_OKAY)
    return res;

  mp_set(&base, w);

  if((res = mp_init(&test)) != MP_OKAY)
    goto TEST;

  /* Compute test = base^a (mod a) */
  if((res = mp_exptmod(&base, a, a, &test)) != MP_OKAY)
    goto CLEANUP;

  
  if(mp_cmp(&base, &test) == 0)
    res = MP_YES;
  else
    res = MP_NO;

 CLEANUP:
  mp_clear(&test);
 TEST:
  mp_clear(&base);

  return res;

} /* end mpp_fermat() */

/* }}} */

/* {{{ mpp_pprime(a, nt) */

/*
  mpp_pprime(a, nt)

  Performs nt iteration of the Miller-Rabin probabilistic primality
  test on a.  Returns MP_YES if the tests pass, MP_NO if one fails.
  If MP_NO is returned, the number is definitely composite.  If MP_YES
  is returned, it is probably prime (but that is not guaranteed).
 */

mp_err  mpp_pprime(mp_int *a, int nt)
{
  mp_err   res;
  mp_int   x, amo, m, z;
  int      iter, jx, b;

  ARGCHK(a != NULL, MP_BADARG);

  /* Initialize temporaries... */
  if((res = mp_init_copy(&amo, a)) != MP_OKAY)
    return res;
  if((res = mp_init_size(&x, USED(a))) != MP_OKAY)
    goto X;
  if((res = mp_init(&z)) != MP_OKAY)
    goto Z;

  /* Compute m = a - 1 for what follows...    */
  mp_sub_d(&amo, 1, &amo);
  if((res = mp_init_copy(&m, &amo)) != MP_OKAY)
    goto M;

  /* How many times does 2 divide (a - 1)?    */
  b = 0;
  while((DIGIT(&m, 0) & 1) == 0) {
    mp_div_2(&m, &m);
    ++b;
  }

  /* Do the test nt times... */
  for(iter = 0; iter < nt; iter++) {

    /* Choose a random value for x < a          */
    s_mp_pad(&x, USED(a));
    mpp_random(&x);
    if((res = mp_mod(&x, a, &x)) != MP_OKAY)
      goto CLEANUP;

    /* Compute z = (x ** m) mod a               */
    if((res = mp_exptmod(&x, &m, a, &z)) != MP_OKAY)
      goto CLEANUP;
    
    jx = 0;
    
    if(mp_cmp_d(&z, 1) == 0 || mp_cmp(&z, &amo) == 0) {
      res = MP_YES;
      continue;
    }
    
    for(;;) {
      if(jx > 0 && mp_cmp_d(&z, 1) == 0) {
	res = MP_NO;
	break;
      }
      
      ++jx;
      
      if(jx < b && mp_cmp(&z, &amo) != 0) {
	/* z = z^2 (mod a) */
	if((res = mp_sqrmod(&z, a, &z)) != MP_OKAY)
	  goto CLEANUP;
	
      } else if(mp_cmp(&z, &amo) == 0) {
	res = MP_YES;
	break;
	
      } else if(jx == b && mp_cmp(&z, &amo) != 0) {
	res = MP_NO;
	break;
	
      }
    } /* end testing loop */

    /* If the test passes, we will continue iterating, but a failed
       test means the candidate is definitely NOT prime, so we will
       immediately break out of this loop
     */
    if(res == MP_NO)
      break;

  } /* end iterations loop */
  
CLEANUP:
  mp_clear(&m);
M:
  mp_clear(&z);
Z:
  mp_clear(&x);
X:
  mp_clear(&amo);
  return res;

} /* end mpp_pprime() */

/* }}} */

/*========================================================================*/
/*------------------------------------------------------------------------*/
/* Static functions visible only to the library internally                */

/* {{{ s_mpp_divp(a, vec, size, which) */

/* 
   Test for divisibility by members of a vector of digits.  Returns
   MP_NO if a is not divisible by any of them; returns MP_YES and sets
   'which' to the index of the offender, if it is.  Will stop on the
   first digit against which a is divisible.
 */

mp_err    s_mpp_divp(mp_int *a, const mp_digit *vec, int size, int *which)
{
  mp_err    res;
  mp_digit  rem;

  int     ix;

  for(ix = 0; ix < size; ix++) {
    if((res = mp_mod_d(a, vec[ix], &rem)) != MP_OKAY) 
      return res;

    if(rem == 0) {
      if(which)
	*which = ix;
      return MP_YES;
    }
  }

  return MP_NO;

} /* end s_mpp_divp() */

/* }}} */

/*------------------------------------------------------------------------*/
/* HERE THERE BE DRAGONS                                                  */
