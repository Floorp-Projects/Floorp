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
 *  $Id: mpmontg.c,v 1.6 2000/08/08 03:20:35 nelsonb%netscape.com Exp $
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
#define MP_CHECKERR(x) if (0 > (rv = (x))) goto loser
#define STATIC
/* #define DEBUG 1  */

#define WINDOW_BITS 5
#define ODD_INTS    16   /* 2 ** (WINDOW_BITS - 1) */

typedef struct {
  mp_int       N;	/* modulus N */
  mp_digit     n0prime; /* n0' = - (n0 ** -1) mod MP_RADIX */
  mp_size      b;	/* R == 2 ** b,  also b = # significant bits in N */
} mp_mont_modulus;

/* computes T = REDC(T), 2^b == R */
STATIC
mp_err s_mp_redc(mp_int *T, mp_mont_modulus *mmm)
{
  mp_err rv;
  int i;
#ifdef DEBUG
  mp_int m;
#endif

  MP_CHECKOK( s_mp_pad(T, MP_USED(T) + MP_USED(&mmm->N) + 2) );
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
#ifdef DEBUG
  mp_clear(&m);
#endif
  return rv;
}

mp_err mp_to_mont(const mp_int *x, mp_mont_modulus *mmm, mp_int *xMont)
{
  mp_err rv;

  /* xMont = x * R mod N   where  N is modulus */
  MP_CHECKOK( mpl_lsh(x, xMont, mmm->b) );  		/* xMont = x << b */
  MP_CHECKOK( mp_div(xMont, &mmm->N, 0, xMont) );	/*         mod N */
loser:
  return rv;
}


mp_err mp_exptmod(const mp_int *inBase, const mp_int *exponent, 
		  const mp_int *modulus, mp_int *result)
{
  const mp_int *base;
  mp_size bits_in_exponent;
  mp_size i;
  mp_err rv;
  mp_int square, accum, goodBase, tmp;
  mp_mont_modulus mmm;

  /* function for computing n0prime only works if n0 is odd */
  if (!mp_isodd(modulus))
    return s_mp_exptmod(inBase, exponent, modulus, result);

  if (mp_cmp(inBase, modulus) < 0) {
    base = inBase;
    MP_DIGITS(&goodBase) = 0;
  } else {
    mp_init(&goodBase);
    base = &goodBase;
    MP_CHECKOK( mp_mod(inBase, modulus, &goodBase) );
  }

  mp_init_size(&square, 2 * MP_USED(modulus) + 2);
  mp_init_size(&accum,  3 * MP_USED(modulus) + 2);
  mp_init_size(&tmp,    3 * MP_USED(modulus) + 2);

  mmm.N = *modulus;			/* a copy of the mp_int struct */
  i = mpl_significant_bits(modulus);
  i += MP_DIGIT_BIT - 1;
  mmm.b = i - i % MP_DIGIT_BIT;

  /* compute n0', given n0, n0' = -(n0 ** -1) mod MP_RADIX
  **		where n0 = least significant mp_digit of N, the modulus.
  */
  mmm.n0prime = 0 - s_mp_invmod_32b( MP_DIGIT(modulus, 0) );

  MP_CHECKOK( mp_to_mont(base, &mmm, &square) );

  bits_in_exponent = mpl_significant_bits(exponent);
  i = bits_in_exponent % WINDOW_BITS;
  if (i != 0) {
    bits_in_exponent += WINDOW_BITS - i;
  } 
  {
    /* oddPowers[i] = base ** (2*i + 1); */
    /* power2 = base ** 2; */
    int expOff;
    mp_int power2, oddPowers[ODD_INTS];

    MP_CHECKOK( mp_init_copy(oddPowers, &square) );

    mp_init_size(&power2, MP_USED(modulus) + 2 * MP_USED(&square) + 2);
    MP_CHECKOK( mp_sqr(&square, &power2) );	/* square = square ** 2 */
    MP_CHECKOK( s_mp_redc(&power2, &mmm) );

    for (i = 1; i < ODD_INTS; ++i) {
      mp_init_size(oddPowers + i, MP_USED(modulus) + 2 * MP_USED(&power2) + 2);
      MP_CHECKOK( mp_mul(oddPowers + (i - 1), &power2, oddPowers + i) );
      MP_CHECKOK( s_mp_redc(oddPowers + i, &mmm) );
    }
    mp_set(&accum, 1);
    MP_CHECKOK( mp_to_mont(&accum, &mmm, &accum) );

#define SQUARE \
    MP_CHECKOK( mp_sqr(&accum, &tmp) );\
    mp_exch(&accum, &tmp); \
    MP_CHECKOK( s_mp_redc(&accum, &mmm) )

#define MUL(x) \
    MP_CHECKOK( mp_mul(&accum, oddPowers + (x), &tmp) ); \
    mp_exch(&accum, &tmp); \
    MP_CHECKOK( s_mp_redc(&accum, &mmm))

    for (expOff = bits_in_exponent - WINDOW_BITS; expOff >= 0; expOff -= WINDOW_BITS) {
      mp_size smallExp;
      MP_CHECKERR( mpl_get_bits(exponent, expOff, WINDOW_BITS) );
      smallExp = (mp_size)rv;

#if WINDOW_BITS == 4
      if (!smallExp) {
		SQUARE; SQUARE; SQUARE; SQUARE;
      } else if (smallExp & 1) {
		SQUARE; SQUARE; SQUARE; SQUARE; MUL(smallExp/2);
      } else if (smallExp & 2) {
		SQUARE; SQUARE; SQUARE; MUL(smallExp/4); SQUARE;
      } else if (smallExp & 4) {
		SQUARE; SQUARE; MUL(smallExp/8); SQUARE; SQUARE;
      } else if (smallExp & 8) {
		SQUARE; MUL(smallExp/16); SQUARE; SQUARE; SQUARE;
      } else {
		abort();
      }
#elif WINDOW_BITS == 5
      if (!smallExp) {
		SQUARE; SQUARE; SQUARE; SQUARE; SQUARE;
      } else if (smallExp & 1) {
		SQUARE; SQUARE; SQUARE; SQUARE; SQUARE; MUL(smallExp/2);
      } else if (smallExp & 2) {
		SQUARE; SQUARE; SQUARE; SQUARE; MUL(smallExp/4); SQUARE;
      } else if (smallExp & 4) {
		SQUARE; SQUARE; SQUARE; MUL(smallExp/8); SQUARE; SQUARE;
      } else if (smallExp & 8) {
		SQUARE; SQUARE; MUL(smallExp/16); SQUARE; SQUARE; SQUARE;
      } else if (smallExp & 0x10) {
		SQUARE; MUL(smallExp/32); SQUARE; SQUARE; SQUARE; SQUARE;
      } else {
		abort();
      }
#elif WINDOW_BITS == 6
      if (!smallExp) {
	      SQUARE; SQUARE; SQUARE; SQUARE; SQUARE; SQUARE;
      } else if (smallExp & 1) {
	      SQUARE; SQUARE; SQUARE; SQUARE; SQUARE; SQUARE; MUL(smallExp/2);
      } else if (smallExp & 2) {
	      SQUARE; SQUARE; SQUARE; SQUARE; SQUARE; MUL(smallExp/4); SQUARE;
      } else if (smallExp & 4) {
	      SQUARE; SQUARE; SQUARE; SQUARE; MUL(smallExp/8); SQUARE; SQUARE;
      } else if (smallExp & 8) {
	      SQUARE; SQUARE; SQUARE; MUL(smallExp/16); SQUARE; SQUARE; SQUARE;
      } else if (smallExp & 0x10) {
	      SQUARE; SQUARE; MUL(smallExp/32); SQUARE; SQUARE; SQUARE; SQUARE;
      } else if (smallExp & 0x20) {
	      SQUARE; MUL(smallExp/64); SQUARE; SQUARE; SQUARE; SQUARE; SQUARE;
      } else {
		abort();
      }
#else
#error "Unknown value for WINDOW_BITS"
#endif
    }

    mp_clear(&power2);
    for (i = 0; i < ODD_INTS; ++i) {
      mp_clear(oddPowers + i);
    }
  }
  rv = s_mp_redc(&accum, &mmm);
  mp_exch(&accum, result);
loser:
  mp_clear(&square);
  mp_clear(&accum);
  mp_clear(&goodBase);
  mp_clear(&tmp);
  /* Don't mp_clear mmm.N because it is merely a copy of modulus.
  ** Just zap it.
  */
  memset(&mmm, 0, sizeof mmm);
  return rv;
}
