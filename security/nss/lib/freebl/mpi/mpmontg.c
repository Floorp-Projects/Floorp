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
 *  $Id: mpmontg.c,v 1.7 2000/08/22 01:57:34 nelsonb%netscape.com Exp $
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

#define STATIC
/* #define DEBUG 1  */

#define MAX_WINDOW_BITS 6
#define MAX_ODD_INTS    32   /* 2 ** (WINDOW_BITS - 1) */

typedef struct {
  mp_int       N;	/* modulus N */
  mp_digit     n0prime; /* n0' = - (n0 ** -1) mod MP_RADIX */
  mp_size      b;	/* R == 2 ** b,  also b = # significant bits in N */
} mp_mont_modulus;

mp_err   s_mp_mul_mont(const mp_int *a, const mp_int *b, mp_int *c, 
	               mp_mont_modulus *mmm);

/* computes T = REDC(T), 2^b == R */
STATIC
mp_err s_mp_redc(mp_int *T, mp_mont_modulus *mmm)
{
  mp_err res;
  mp_size i;
#ifdef DEBUG
  mp_int m;
  MP_DIGITS(&m) = 0;
#endif

  i = MP_USED(T) + MP_USED(&mmm->N) + 2;
  MP_CHECKOK( s_mp_pad(T, i) );
  for (i = 0; i < MP_USED(&mmm->N); ++i ) {
    mp_digit m_i = MP_DIGIT(T, i) * mmm->n0prime;
    /* T += N * m_i * (MP_RADIX ** i); */
    MP_CHECKOK( s_mp_mul_d_add_offset(&mmm->N, m_i, T, i) );
  }
  s_mp_clamp(T);

  /* T /= R */
#ifdef DEBUG
  MP_CHECKOK( mp_init(&m) );
  MP_CHECKOK( mp_div_2d(T, mmm->b, T, &m));
  /* here, remainder m should be equal to zero */
  if (mp_cmp_z(&m) != 0) {
    res = MP_UNDEF;
    goto CLEANUP;
  }
#else
  s_mp_div_2d(T, mmm->b); 
#endif

  if ((res = s_mp_cmp(T, &mmm->N)) >= 0) {
    /* T = T - N */
    MP_CHECKOK( s_mp_sub(T, &mmm->N) );
#ifdef DEBUG
    if ((res = mp_cmp(T, &mmm->N)) >= 0) {
      res = MP_UNDEF;
      goto CLEANUP;
    }
#endif
  }
  res = MP_OKAY;
CLEANUP:
#ifdef DEBUG
  mp_clear(&m);
#endif
  return res;
}

#if !defined(MP_ASSEMBLY_MUL_MONT) && !defined(MP_MONT_USE_MP_MUL)
mp_err s_mp_mul_mont(const mp_int *a, const mp_int *b, mp_int *c, 
	           mp_mont_modulus *mmm)
{
  mp_digit *pb;
  mp_digit m_i;
  mp_err   res;
  mp_size  ib;
  mp_size  useda, usedb;

  ARGCHK(a != NULL && b != NULL && c != NULL, MP_BADARG);

  if (MP_USED(a) < MP_USED(b)) {
    const mp_int *xch = b;	/* switch a and b, to do fewer outer loops */
    b = a;
    a = xch;
  }

  MP_USED(c) = 1; MP_DIGIT(c, 0) = 0;
  ib = MP_USED(a) + MP_MAX(MP_USED(b), MP_USED(&mmm->N)) + 2;
  if((res = s_mp_pad(c, ib)) != MP_OKAY)
    goto CLEANUP;

  useda = MP_USED(a);
  pb = MP_DIGITS(b);
  s_mpv_mul_d(MP_DIGITS(a), useda, *pb++, MP_DIGITS(c));
  s_mp_setz(MP_DIGITS(c) + useda + 1, ib - (useda + 1));
  m_i = MP_DIGIT(c, 0) * mmm->n0prime;
  s_mp_mul_d_add_offset(&mmm->N, m_i, c, 0);

  /* Outer loop:  Digits of b */
  usedb = MP_USED(b);
  for (ib = 1; ib < usedb; ib++) {
    mp_digit b_i    = *pb++;

    /* Inner product:  Digits of a */
    if (b_i)
      s_mpv_mul_d_add_prop(MP_DIGITS(a), useda, b_i, MP_DIGITS(c) + ib);
    m_i = MP_DIGIT(c, ib) * mmm->n0prime;
    s_mp_mul_d_add_offset(&mmm->N, m_i, c, ib);
  }
  if (usedb < MP_USED(&mmm->N)) {
    for (usedb = MP_USED(&mmm->N); ib < usedb; ++ib ) {
      m_i = MP_DIGIT(c, ib) * mmm->n0prime;
      s_mp_mul_d_add_offset(&mmm->N, m_i, c, ib);
    }
  }
  s_mp_clamp(c);
  s_mp_div_2d(c, mmm->b); 
  if (s_mp_cmp(c, &mmm->N) >= 0) {
    MP_CHECKOK( s_mp_sub(c, &mmm->N) );
  }
  res = MP_OKAY;

CLEANUP:
  return res;
}
#endif

STATIC
mp_err s_mp_to_mont(const mp_int *x, mp_mont_modulus *mmm, mp_int *xMont)
{
  mp_err res;

  /* xMont = x * R mod N   where  N is modulus */
  MP_CHECKOK( mpl_lsh(x, xMont, mmm->b) );  		/* xMont = x << b */
  MP_CHECKOK( mp_div(xMont, &mmm->N, 0, xMont) );	/*         mod N */
CLEANUP:
  return res;
}


mp_err mp_exptmod(const mp_int *inBase, const mp_int *exponent, 
		  const mp_int *modulus, mp_int *result)
{
  const mp_int *base;
  mp_int *pa1, *pa2, *ptmp;
  mp_size bits_in_exponent;
  mp_size i;
  mp_size window_bits, odd_ints;
  mp_err res;
  mp_int square, accum1, accum2, goodBase;
  mp_mont_modulus mmm;

  /* function for computing n0prime only works if n0 is odd */
  if (!mp_isodd(modulus))
    return s_mp_exptmod(inBase, exponent, modulus, result);

  MP_DIGITS(&square) = 0;
  MP_DIGITS(&accum1) = 0;
  MP_DIGITS(&accum2) = 0;
  MP_DIGITS(&goodBase) = 0;

  if (mp_cmp(inBase, modulus) < 0) {
    base = inBase;
  } else {
    MP_CHECKOK( mp_init(&goodBase) );
    base = &goodBase;
    MP_CHECKOK( mp_mod(inBase, modulus, &goodBase) );
  }

  MP_CHECKOK( mp_init_size(&square, 2 * MP_USED(modulus) + 2) );
  MP_CHECKOK( mp_init_size(&accum1, 3 * MP_USED(modulus) + 2) );
  MP_CHECKOK( mp_init_size(&accum2, 3 * MP_USED(modulus) + 2) );

  mmm.N = *modulus;			/* a copy of the mp_int struct */
  i = mpl_significant_bits(modulus);
  i += MP_DIGIT_BIT - 1;
  mmm.b = i - i % MP_DIGIT_BIT;

  /* compute n0', given n0, n0' = -(n0 ** -1) mod MP_RADIX
  **		where n0 = least significant mp_digit of N, the modulus.
  */
  mmm.n0prime = 0 - s_mp_invmod_32b( MP_DIGIT(modulus, 0) );

  MP_CHECKOK( s_mp_to_mont(base, &mmm, &square) );

  bits_in_exponent = mpl_significant_bits(exponent);
  if (bits_in_exponent > 480)
    window_bits = 6;
  else if (bits_in_exponent > 160)
    window_bits = 5;
  else
    window_bits = 4;
  odd_ints = 1 << (window_bits - 1);
  i = bits_in_exponent % window_bits;
  if (i != 0) {
    bits_in_exponent += window_bits - i;
  } 
  {
    /* oddPowers[i] = base ** (2*i + 1); */
    int expOff;
    /* power2 = base ** 2; oddPowers[i] = base ** (2*i + 1); */
    mp_int power2, oddPowers[MAX_ODD_INTS];

    MP_CHECKOK( mp_init_copy(oddPowers, &square) );

    mp_init_size(&power2, MP_USED(modulus) + 2 * MP_USED(&square) + 2);
    MP_CHECKOK( mp_sqr(&square, &power2) );	/* square = square ** 2 */
    MP_CHECKOK( s_mp_redc(&power2, &mmm) );

    for (i = 1; i < odd_ints; ++i) {
      mp_init_size(oddPowers + i, MP_USED(modulus) + 2 * MP_USED(&power2) + 2);
      MP_CHECKOK( mp_mul(oddPowers + (i - 1), &power2, oddPowers + i) );
      MP_CHECKOK( s_mp_redc(oddPowers + i, &mmm) );
    }
    mp_set(&accum1, 1);
    MP_CHECKOK( s_mp_to_mont(&accum1, &mmm, &accum1) );
    pa1 = &accum1;
    pa2 = &accum2;

#define SQR(a,b) \
    MP_CHECKOK( mp_sqr(a, b) );\
    MP_CHECKOK( s_mp_redc(b, &mmm) )

#if defined(MP_MONT_USE_MP_MUL)
#define MUL(x,a,b) \
    MP_CHECKOK( mp_mul(a, oddPowers + (x), b) ); \
    MP_CHECKOK( s_mp_redc(b, &mmm) ) 
#else
#define MUL(x,a,b) \
    MP_CHECKOK( s_mp_mul_mont(a, oddPowers + (x), b, &mmm) )
#endif

#define SWAPPA ptmp = pa1; pa1 = pa2; pa2 = ptmp

    for (expOff = bits_in_exponent - window_bits; expOff >= 0; expOff -= window_bits) {
      mp_size smallExp;
      MP_CHECKOK( mpl_get_bits(exponent, expOff, window_bits) );
      smallExp = (mp_size)res;

      if (window_bits == 4) {
	if (!smallExp) {
	  SQR(pa1,pa2); SQR(pa2,pa1); SQR(pa1,pa2); SQR(pa2,pa1);
	} else if (smallExp & 1) {
	  SQR(pa1,pa2); SQR(pa2,pa1); SQR(pa1,pa2); SQR(pa2,pa1); 
	  MUL(smallExp/2, pa1,pa2); SWAPPA;
	} else if (smallExp & 2) {
	  SQR(pa1,pa2); SQR(pa2,pa1); SQR(pa1,pa2); 
	  MUL(smallExp/4,pa2,pa1); SQR(pa1,pa2); SWAPPA;
	} else if (smallExp & 4) {
	  SQR(pa1,pa2); SQR(pa2,pa1); MUL(smallExp/8,pa1,pa2); 
	  SQR(pa2,pa1); SQR(pa1,pa2); SWAPPA;
	} else if (smallExp & 8) {
	  SQR(pa1,pa2); MUL(smallExp/16,pa2,pa1); SQR(pa1,pa2); 
	  SQR(pa2,pa1); SQR(pa1,pa2); SWAPPA;
	} else {
	  abort();
	}
      } else if (window_bits == 5) {
	if (!smallExp) {
	  SQR(pa1,pa2); SQR(pa2,pa1); SQR(pa1,pa2); SQR(pa2,pa1); 
	  SQR(pa1,pa2); SWAPPA;
	} else if (smallExp & 1) {
	  SQR(pa1,pa2); SQR(pa2,pa1); SQR(pa1,pa2); SQR(pa2,pa1); 
	  SQR(pa1,pa2); MUL(smallExp/2,pa2,pa1);
	} else if (smallExp & 2) {
	  SQR(pa1,pa2); SQR(pa2,pa1); SQR(pa1,pa2); SQR(pa2,pa1); 
	  MUL(smallExp/4,pa1,pa2); SQR(pa2,pa1);
	} else if (smallExp & 4) {
	  SQR(pa1,pa2); SQR(pa2,pa1); SQR(pa1,pa2); 
	  MUL(smallExp/8,pa2,pa1); SQR(pa1,pa2); SQR(pa2,pa1);
	} else if (smallExp & 8) {
	  SQR(pa1,pa2); SQR(pa2,pa1); MUL(smallExp/16,pa1,pa2); 
	  SQR(pa2,pa1); SQR(pa1,pa2); SQR(pa2,pa1);
	} else if (smallExp & 0x10) {
	  SQR(pa1,pa2); MUL(smallExp/32,pa2,pa1); SQR(pa1,pa2); 
	  SQR(pa2,pa1); SQR(pa1,pa2); SQR(pa2,pa1);
	} else {
	    abort();
	}
      } else if (window_bits == 6) {
	if (!smallExp) {
	  SQR(pa1,pa2); SQR(pa2,pa1); SQR(pa1,pa2); SQR(pa2,pa1); 
	  SQR(pa1,pa2); SQR(pa2,pa1);
	} else if (smallExp & 1) {
	  SQR(pa1,pa2); SQR(pa2,pa1); SQR(pa1,pa2); SQR(pa2,pa1); 
	  SQR(pa1,pa2); SQR(pa2,pa1); MUL(smallExp/2,pa1,pa2); SWAPPA;
	} else if (smallExp & 2) {
	  SQR(pa1,pa2); SQR(pa2,pa1); SQR(pa1,pa2); SQR(pa2,pa1); 
	  SQR(pa1,pa2); MUL(smallExp/4,pa2,pa1); SQR(pa1,pa2); SWAPPA;
	} else if (smallExp & 4) {
	  SQR(pa1,pa2); SQR(pa2,pa1); SQR(pa1,pa2); SQR(pa2,pa1); 
	  MUL(smallExp/8,pa1,pa2); SQR(pa2,pa1); SQR(pa1,pa2); SWAPPA;
	} else if (smallExp & 8) {
	  SQR(pa1,pa2); SQR(pa2,pa1); SQR(pa1,pa2); 
	  MUL(smallExp/16,pa2,pa1); SQR(pa1,pa2); SQR(pa2,pa1); 
	  SQR(pa1,pa2); SWAPPA;
	} else if (smallExp & 0x10) {
	  SQR(pa1,pa2); SQR(pa2,pa1); MUL(smallExp/32,pa1,pa2); 
	  SQR(pa2,pa1); SQR(pa1,pa2); SQR(pa2,pa1); SQR(pa1,pa2); SWAPPA;
	} else if (smallExp & 0x20) {
	  SQR(pa1,pa2); MUL(smallExp/64,pa2,pa1); SQR(pa1,pa2); 
	  SQR(pa2,pa1); SQR(pa1,pa2); SQR(pa2,pa1); SQR(pa1,pa2); SWAPPA;
	} else {
	  abort();
	}
      } else {
	abort();
      }
    }

    mp_clear(&power2);
    for (i = 0; i < odd_ints; ++i) {
      mp_clear(oddPowers + i);
    }
  }
  res = s_mp_redc(pa1, &mmm);
  mp_exch(pa1, result);
CLEANUP:
  mp_clear(&square);
  mp_clear(&accum1);
  mp_clear(&accum2);
  mp_clear(&goodBase);
  /* Don't mp_clear mmm.N because it is merely a copy of modulus.
  ** Just zap it.
  */
  memset(&mmm, 0, sizeof mmm);
  return res;
}
