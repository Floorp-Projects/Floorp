/*
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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.	Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.	If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/* This file implements moduluar exponentiation using Montgomery's
 * method for modular reduction.  This file implements the method
 * described as "Improvement 1" in the paper "A Cryptogrpahic Library for
 * the Motorola DSP56000" by Stephen R. Dusse' and Burton S. Kaliski Jr.
 * published in "Advances in Cryptology: Proceedings of EUROCRYPT '90"
 * "Lecture Notes in Computer Science" volume 473, 1991, pg 230-244,
 * published by Springer Verlag.
 */

#include <string.h>
#include "mpi-priv.h"
#include "mplogic.h"
#include "mpprime.h"

#define MP_CHECKOK(x) if (MP_OKAY != (rv = (x))) goto loser
#define STATIC
/* #define DEBUG 1  */

typedef struct {
  mp_int       N;	/* modulus N */
  mp_digit     n0prime; /* n0' = - (n0 ** -1) mod MP_RADIX */
  mp_size      b;	/* R == 2 ** b,  also b = # significant bits in N */
} mp_mont_modulus;

/* computes T = REDC(T), 2^b == R */
STATIC
mp_err s_mp_redc(mp_int *T, mp_mont_modulus *mmm)
{
  mp_int m;
  mp_err rv;
  mp_digit n0prime = mmm->n0prime;
  int i;
  int n = MP_USED(&mmm->N);

  mp_init(&m);

  MP_CHECKOK( s_mp_grow(T, 2 * n + 2) );
  for (i = 0; i < n; ++i ) {
    mp_digit m_i = MP_DIGIT(T, i) * n0prime;
    /* T += N * m_i * (MP_RADIX ** i); */
    MP_CHECKOK( s_mp_mul_d_add_offset(&mmm->N, m_i, T, i) );
  }

  /* T /= R */
#ifdef DEBUG
  MP_CHECKOK( mp_div_2d(T, mmm->b, T, &m));
  /* here, remainder m should be equal to zero */
  if (mp_cmp_z(&m) != 0) {
    rv = MP_UNDEF;
    goto loser;
  }
#else
  s_mp_div_2d(T, mmm->b); 
#endif

  if ((rv = s_mp_cmp(T, &mmm->N)) >= 0) {
    /* T = T - N */
    MP_CHECKOK( s_mp_sub(T, &mmm->N) );
#ifdef DEBUG
    if ((rv = mp_cmp(T, &mmm->N)) >= 0) {
      rv = MP_UNDEF;
    }
#endif
  }
  rv = MP_OKAY;
loser:
  mp_clear(&m);
  return rv;
}

mp_err mp_to_mont(mp_int *x, mp_mont_modulus *mmm, mp_int *xMont)
{
  mp_err rv;

  /* xMont = x * R mod N   where  N is modulus */
  MP_CHECKOK( mpl_lsh(x, xMont, mmm->b) );  		/* xMont = x << b */
  MP_CHECKOK( mp_mod(xMont, &mmm->N, xMont) );		/*         mod N */
loser:
  return rv;
}

/* compute n0', given n0, n0' = -(n0 ** -1) mod MP_RADIX
**		where n0 = least significant mp_digit of N, the modulus.
*/ 
STATIC
mp_err mp_n_to_n0prime(mp_mont_modulus *mmm)
{
  mp_digit n0    	= MP_DIGIT(&mmm->N, 0);
  mp_digit n0inv     	= 1;
  mp_size  i;

#define TWO_TO_I(i) ((mp_digit)1 << (i))
#define MOD_2_TO_I(x, i) ((x) & (mp_digit)((twoToIMinus1 << 1) - 1))

  mp_digit twoToIMinus1 = 1;
  mp_digit xy_mod_2_to_i;
  for (i = 2; i <= MP_DIGIT_BIT; ++i) {
    twoToIMinus1 <<= 1;
    xy_mod_2_to_i = MOD_2_TO_I(n0 * n0inv, i);
    if (xy_mod_2_to_i >= twoToIMinus1) {
      n0inv += twoToIMinus1;
    }
  }
  mmm->n0prime = 0 - n0inv;
  return MP_OKAY;
}

mp_err mp_exptmod(mp_int *inBase, mp_int *exponent, mp_int *modulus, 
		  mp_int *result)
{
  mp_int *base;
  mp_size bits_in_exponent;
  mp_size i;
  mp_err rv;
  mp_int square, accum, goodBase;
  mp_mont_modulus mmm;

  /* function for computing n0prime only works if n0 is odd */
  if (!mp_isodd(modulus))
    return s_mp_exptmod(inBase, exponent, modulus, result);

  mp_init(&goodBase);
  if (mp_cmp(inBase, modulus) < 0) {
    base = inBase;
  } else {
    base = &goodBase;
    MP_CHECKOK( mp_mod(inBase, modulus, &goodBase) );
  }

  mp_init_size(&square, 2 * MP_USED(modulus) + 2);
  mp_init_size(&accum,  2 * MP_USED(modulus) + 2);

  mmm.N = *modulus;			/* a copy of the mp_int struct */
  i = mpl_significant_bits(modulus);
  i += MP_DIGIT_BIT - 1;
  mmm.b = i - i % MP_DIGIT_BIT;
  MP_CHECKOK( mp_n_to_n0prime(&mmm) );

  MP_CHECKOK( mp_to_mont(base, &mmm, &square) );

  if (mp_isodd(exponent)) {
    mp_copy(&square, &accum);
  } else {
    mp_set(&accum, 1);
    MP_CHECKOK( mp_to_mont(&accum, &mmm, &accum) );
  }
  bits_in_exponent = mpl_significant_bits(exponent);
  for (i = 1; i < bits_in_exponent; ++i) {
    MP_CHECKOK( s_mp_sqr(&square));		/* square = square ** 2 */
    MP_CHECKOK( s_mp_redc(&square, &mmm));
    rv = mpl_get_bit(exponent, i);  if (rv < 0) goto loser;
    if (rv) {
      MP_CHECKOK( s_mp_mul(&accum, &square));	/* accum *= square */
      MP_CHECKOK( s_mp_redc(&accum, &mmm));
    }
  }
  rv = s_mp_redc(&accum, &mmm);
  mp_exch(&accum, result);
loser:
  mp_clear(&square);
  mp_clear(&accum);
  mp_clear(&goodBase);
  /* Don't mp_clear mmm.N because it is merely a copy of modulus.
  ** Just zap it.
  */
  memset(&mmm, 0, sizeof mmm);
  return rv;
}
