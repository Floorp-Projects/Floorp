/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sheueling Chang Shantz <sheueling.chang@sun.com>,
 *   Stephen Fung <stephen.fung@sun.com>, and
 *   Douglas Stebila <douglas@stebila.ca> of Sun Laboratories.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* This file implements moduluar exponentiation using Montgomery's
 * method for modular reduction.  This file implements the method
 * described as "Improvement 1" in the paper "A Cryptogrpahic Library for
 * the Motorola DSP56000" by Stephen R. Dusse' and Burton S. Kaliski Jr.
 * published in "Advances in Cryptology: Proceedings of EUROCRYPT '90"
 * "Lecture Notes in Computer Science" volume 473, 1991, pg 230-244,
 * published by Springer Verlag.
 */

/* #define MP_USING_MONT_MULF 1 */
#include <string.h>
#include "mpi-priv.h"
#include "mplogic.h"
#include "mpprime.h"
#ifdef MP_USING_MONT_MULF
#include "montmulf.h"
#endif

#define STATIC
/* #define DEBUG 1  */

#define MAX_WINDOW_BITS 6
#define MAX_ODD_INTS    32   /* 2 ** (WINDOW_BITS - 1) */

#if defined(_WIN32_WCE)
#define ABORT  res = MP_UNDEF; goto CLEANUP
#else
#define ABORT abort()
#endif

/* computes T = REDC(T), 2^b == R */
mp_err s_mp_redc(mp_int *T, mp_mont_modulus *mmm)
{
  mp_err res;
  mp_size i;

  i = MP_USED(T) + MP_USED(&mmm->N) + 2;
  MP_CHECKOK( s_mp_pad(T, i) );
  for (i = 0; i < MP_USED(&mmm->N); ++i ) {
    mp_digit m_i = MP_DIGIT(T, i) * mmm->n0prime;
    /* T += N * m_i * (MP_RADIX ** i); */
    MP_CHECKOK( s_mp_mul_d_add_offset(&mmm->N, m_i, T, i) );
  }
  s_mp_clamp(T);

  /* T /= R */
  s_mp_div_2d(T, mmm->b); 

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

#ifdef MP_USING_MONT_MULF

unsigned int mp_using_mont_mulf = 1;

/* computes montgomery square of the integer in mResult */
#define SQR \
  conv_i32_to_d32_and_d16(dm1, d16Tmp, mResult, nLen); \
  mont_mulf_noconv(mResult, dm1, d16Tmp, \
		   dTmp, dn, MP_DIGITS(modulus), nLen, dn0)

/* computes montgomery product of x and the integer in mResult */
#define MUL(x) \
  conv_i32_to_d32(dm1, mResult, nLen); \
  mont_mulf_noconv(mResult, dm1, oddPowers[x], \
		   dTmp, dn, MP_DIGITS(modulus), nLen, dn0)

/* Do modular exponentiation using floating point multiply code. */
mp_err mp_exptmod_f(const mp_int *   montBase, 
                    const mp_int *   exponent, 
		    const mp_int *   modulus, 
		    mp_int *         result, 
		    mp_mont_modulus *mmm, 
		    int              nLen, 
		    mp_size          bits_in_exponent, 
		    mp_size          window_bits,
		    mp_size          odd_ints)
{
  mp_digit *mResult;
  double   *dBuf = 0, *dm1, *dn, *dSqr, *d16Tmp, *dTmp;
  double    dn0;
  mp_size   i;
  mp_err    res;
  int       expOff;
  int       dSize = 0, oddPowSize, dTmpSize;
  mp_int    accum1;
  double   *oddPowers[MAX_ODD_INTS];

  /* function for computing n0prime only works if n0 is odd */

  MP_DIGITS(&accum1) = 0;

  for (i = 0; i < MAX_ODD_INTS; ++i)
    oddPowers[i] = 0;

  MP_CHECKOK( mp_init_size(&accum1, 3 * nLen + 2) );

  mp_set(&accum1, 1);
  MP_CHECKOK( s_mp_to_mont(&accum1, mmm, &accum1) );
  MP_CHECKOK( s_mp_pad(&accum1, nLen) );

  oddPowSize = 2 * nLen + 1;
  dTmpSize   = 2 * oddPowSize;
  dSize = sizeof(double) * (nLen * 4 + 1 + 
			    ((odd_ints + 1) * oddPowSize) + dTmpSize);
  dBuf   = (double *)malloc(dSize);
  dm1    = dBuf;		/* array of d32 */
  dn     = dBuf   + nLen;	/* array of d32 */
  dSqr   = dn     + nLen;    	/* array of d32 */
  d16Tmp = dSqr   + nLen;	/* array of d16 */
  dTmp   = d16Tmp + oddPowSize;

  for (i = 0; i < odd_ints; ++i) {
      oddPowers[i] = dTmp;
      dTmp += oddPowSize;
  }
  mResult = (mp_digit *)(dTmp + dTmpSize);	/* size is nLen + 1 */

  /* Make dn and dn0 */
  conv_i32_to_d32(dn, MP_DIGITS(modulus), nLen);
  dn0 = (double)(mmm->n0prime & 0xffff);

  /* Make dSqr */
  conv_i32_to_d32_and_d16(dm1, oddPowers[0], MP_DIGITS(montBase), nLen);
  mont_mulf_noconv(mResult, dm1, oddPowers[0], 
		   dTmp, dn, MP_DIGITS(modulus), nLen, dn0);
  conv_i32_to_d32(dSqr, mResult, nLen);

  for (i = 1; i < odd_ints; ++i) {
    mont_mulf_noconv(mResult, dSqr, oddPowers[i - 1], 
		     dTmp, dn, MP_DIGITS(modulus), nLen, dn0);
    conv_i32_to_d16(oddPowers[i], mResult, nLen);
  }

  s_mp_copy(MP_DIGITS(&accum1), mResult, nLen); /* from, to, len */

  for (expOff = bits_in_exponent - window_bits; expOff >= 0; expOff -= window_bits) {
    mp_size smallExp;
    MP_CHECKOK( mpl_get_bits(exponent, expOff, window_bits) );
    smallExp = (mp_size)res;

    if (window_bits == 1) {
      if (!smallExp) {
	SQR;
      } else if (smallExp & 1) {
	SQR; MUL(0); 
      } else {
	ABORT;
      }
    } else if (window_bits == 4) {
      if (!smallExp) {
	SQR; SQR; SQR; SQR;
      } else if (smallExp & 1) {
	SQR; SQR; SQR; SQR; MUL(smallExp/2); 
      } else if (smallExp & 2) {
	SQR; SQR; SQR; MUL(smallExp/4); SQR; 
      } else if (smallExp & 4) {
	SQR; SQR; MUL(smallExp/8); SQR; SQR; 
      } else if (smallExp & 8) {
	SQR; MUL(smallExp/16); SQR; SQR; SQR; 
      } else {
	ABORT;
      }
    } else if (window_bits == 5) {
      if (!smallExp) {
	SQR; SQR; SQR; SQR; SQR; 
      } else if (smallExp & 1) {
	SQR; SQR; SQR; SQR; SQR; MUL(smallExp/2);
      } else if (smallExp & 2) {
	SQR; SQR; SQR; SQR; MUL(smallExp/4); SQR;
      } else if (smallExp & 4) {
	SQR; SQR; SQR; MUL(smallExp/8); SQR; SQR;
      } else if (smallExp & 8) {
	SQR; SQR; MUL(smallExp/16); SQR; SQR; SQR;
      } else if (smallExp & 0x10) {
	SQR; MUL(smallExp/32); SQR; SQR; SQR; SQR;
      } else {
	ABORT;
      }
    } else if (window_bits == 6) {
      if (!smallExp) {
	SQR; SQR; SQR; SQR; SQR; SQR;
      } else if (smallExp & 1) {
	SQR; SQR; SQR; SQR; SQR; SQR; MUL(smallExp/2); 
      } else if (smallExp & 2) {
	SQR; SQR; SQR; SQR; SQR; MUL(smallExp/4); SQR; 
      } else if (smallExp & 4) {
	SQR; SQR; SQR; SQR; MUL(smallExp/8); SQR; SQR; 
      } else if (smallExp & 8) {
	SQR; SQR; SQR; MUL(smallExp/16); SQR; SQR; SQR; 
      } else if (smallExp & 0x10) {
	SQR; SQR; MUL(smallExp/32); SQR; SQR; SQR; SQR; 
      } else if (smallExp & 0x20) {
	SQR; MUL(smallExp/64); SQR; SQR; SQR; SQR; SQR; 
      } else {
	ABORT;
      }
    } else {
      ABORT;
    }
  }

  s_mp_copy(mResult, MP_DIGITS(&accum1), nLen); /* from, to, len */

  res = s_mp_redc(&accum1, mmm);
  mp_exch(&accum1, result);

CLEANUP:
  mp_clear(&accum1);
  if (dBuf) {
    if (dSize)
      memset(dBuf, 0, dSize);
    free(dBuf);
  }

  return res;
}
#undef SQR
#undef MUL
#endif

#define SQR(a,b) \
  MP_CHECKOK( mp_sqr(a, b) );\
  MP_CHECKOK( s_mp_redc(b, mmm) )

#if defined(MP_MONT_USE_MP_MUL)
#define MUL(x,a,b) \
  MP_CHECKOK( mp_mul(a, oddPowers + (x), b) ); \
  MP_CHECKOK( s_mp_redc(b, mmm) ) 
#else
#define MUL(x,a,b) \
  MP_CHECKOK( s_mp_mul_mont(a, oddPowers + (x), b, mmm) )
#endif

#define SWAPPA ptmp = pa1; pa1 = pa2; pa2 = ptmp

/* Do modular exponentiation using integer multiply code. */
mp_err mp_exptmod_i(const mp_int *   montBase, 
                    const mp_int *   exponent, 
		    const mp_int *   modulus, 
		    mp_int *         result, 
		    mp_mont_modulus *mmm, 
		    int              nLen, 
		    mp_size          bits_in_exponent, 
		    mp_size          window_bits,
		    mp_size          odd_ints)
{
  mp_int *pa1, *pa2, *ptmp;
  mp_size i;
  mp_err  res;
  int     expOff;
  mp_int  accum1, accum2, power2, oddPowers[MAX_ODD_INTS];

  /* power2 = base ** 2; oddPowers[i] = base ** (2*i + 1); */
  /* oddPowers[i] = base ** (2*i + 1); */

  MP_DIGITS(&accum1) = 0;
  MP_DIGITS(&accum2) = 0;
  MP_DIGITS(&power2) = 0;
  for (i = 0; i < MAX_ODD_INTS; ++i) {
    MP_DIGITS(oddPowers + i) = 0;
  }

  MP_CHECKOK( mp_init_size(&accum1, 3 * nLen + 2) );
  MP_CHECKOK( mp_init_size(&accum2, 3 * nLen + 2) );

  MP_CHECKOK( mp_init_copy(&oddPowers[0], montBase) );

  mp_init_size(&power2, nLen + 2 * MP_USED(montBase) + 2);
  MP_CHECKOK( mp_sqr(montBase, &power2) );	/* power2 = montBase ** 2 */
  MP_CHECKOK( s_mp_redc(&power2, mmm) );

  for (i = 1; i < odd_ints; ++i) {
    mp_init_size(oddPowers + i, nLen + 2 * MP_USED(&power2) + 2);
    MP_CHECKOK( mp_mul(oddPowers + (i - 1), &power2, oddPowers + i) );
    MP_CHECKOK( s_mp_redc(oddPowers + i, mmm) );
  }

  /* set accumulator to montgomery residue of 1 */
  mp_set(&accum1, 1);
  MP_CHECKOK( s_mp_to_mont(&accum1, mmm, &accum1) );
  pa1 = &accum1;
  pa2 = &accum2;

  for (expOff = bits_in_exponent - window_bits; expOff >= 0; expOff -= window_bits) {
    mp_size smallExp;
    MP_CHECKOK( mpl_get_bits(exponent, expOff, window_bits) );
    smallExp = (mp_size)res;

    if (window_bits == 1) {
      if (!smallExp) {
	SQR(pa1,pa2); SWAPPA;
      } else if (smallExp & 1) {
	SQR(pa1,pa2); MUL(0,pa2,pa1);
      } else {
	ABORT;
      }
    } else if (window_bits == 4) {
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
	ABORT;
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
	ABORT;
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
	ABORT;
      }
    } else {
      ABORT;
    }
  }

  res = s_mp_redc(pa1, mmm);
  mp_exch(pa1, result);

CLEANUP:
  mp_clear(&accum1);
  mp_clear(&accum2);
  mp_clear(&power2);
  for (i = 0; i < odd_ints; ++i) {
    mp_clear(oddPowers + i);
  }
  return res;
}
#undef SQR
#undef MUL


mp_err mp_exptmod(const mp_int *inBase, const mp_int *exponent, 
		  const mp_int *modulus, mp_int *result)
{
  const mp_int *base;
  mp_size bits_in_exponent, i, window_bits, odd_ints;
  mp_err  res;
  int     nLen;
  mp_int  montBase, goodBase;
  mp_mont_modulus mmm;

  /* function for computing n0prime only works if n0 is odd */
  if (!mp_isodd(modulus))
    return s_mp_exptmod(inBase, exponent, modulus, result);

  MP_DIGITS(&montBase) = 0;
  MP_DIGITS(&goodBase) = 0;

  if (mp_cmp(inBase, modulus) < 0) {
    base = inBase;
  } else {
    MP_CHECKOK( mp_init(&goodBase) );
    base = &goodBase;
    MP_CHECKOK( mp_mod(inBase, modulus, &goodBase) );
  }

  nLen  = MP_USED(modulus);
  MP_CHECKOK( mp_init_size(&montBase, 2 * nLen + 2) );

  mmm.N = *modulus;			/* a copy of the mp_int struct */
  i = mpl_significant_bits(modulus);
  i += MP_DIGIT_BIT - 1;
  mmm.b = i - i % MP_DIGIT_BIT;

  /* compute n0', given n0, n0' = -(n0 ** -1) mod MP_RADIX
  **		where n0 = least significant mp_digit of N, the modulus.
  */
  mmm.n0prime = 0 - s_mp_invmod_radix( MP_DIGIT(modulus, 0) );

  MP_CHECKOK( s_mp_to_mont(base, &mmm, &montBase) );

  bits_in_exponent = mpl_significant_bits(exponent);
  if (bits_in_exponent > 480)
    window_bits = 6;
  else if (bits_in_exponent > 160)
    window_bits = 5;
  else if (bits_in_exponent > 20)
    window_bits = 4;
  /* RSA public key exponents are typically under 20 bits (common values 
   * are: 3, 17, 65537) and a 4-bit window is inefficient
   */
  else 
    window_bits = 1;
  odd_ints = 1 << (window_bits - 1);
  i = bits_in_exponent % window_bits;
  if (i != 0) {
    bits_in_exponent += window_bits - i;
  } 

#ifdef MP_USING_MONT_MULF
  if (mp_using_mont_mulf) {
    MP_CHECKOK( s_mp_pad(&montBase, nLen) );
    res = mp_exptmod_f(&montBase, exponent, modulus, result, &mmm, nLen, 
		     bits_in_exponent, window_bits, odd_ints);
  } else
#endif
  res = mp_exptmod_i(&montBase, exponent, modulus, result, &mmm, nLen, 
		     bits_in_exponent, window_bits, odd_ints);

CLEANUP:
  mp_clear(&montBase);
  mp_clear(&goodBase);
  /* Don't mp_clear mmm.N because it is merely a copy of modulus.
  ** Just zap it.
  */
  memset(&mmm, 0, sizeof mmm);
  return res;
}
