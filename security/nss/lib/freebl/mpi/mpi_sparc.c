/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id$ */

/* Multiplication performance enhancements for sparc v8+vis CPUs. */

#include "mpi-priv.h"
#include <stddef.h>
#include <sys/systeminfo.h>
#include <strings.h>

/* In the functions below, */
/* vector y must be 8-byte aligned, and n must be even */
/* returns carry out of high order word of result */
/* maximum n is 256 */

/* vector x += vector y * scaler a; where y is of length n words. */
extern mp_digit mul_add_inp(mp_digit *x, const mp_digit *y, int n, mp_digit a);

/* vector z = vector x + vector y * scaler a; where y is of length n words. */
extern mp_digit mul_add(mp_digit *z, const mp_digit *x, const mp_digit *y, 
			int n, mp_digit a);

/* v8 versions of these functions run on any Sparc v8 CPU. */

/* This trick works on Sparc V8 CPUs with the Workshop compilers. */
#define MP_MUL_DxD(a, b, Phi, Plo) \
  { unsigned long long product = (unsigned long long)a * b; \
    Plo = (mp_digit)product; \
    Phi = (mp_digit)(product >> MP_DIGIT_BIT); }

/* c = a * b */
static void 
v8_mpv_mul_d(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
#if !defined(MP_NO_MP_WORD)
  mp_digit   d = 0;

  /* Inner product:  Digits of a */
  while (a_len--) {
    mp_word w = ((mp_word)b * *a++) + d;
    *c++ = ACCUM(w);
    d = CARRYOUT(w);
  }
  *c = d;
#else
  mp_digit carry = 0;
  while (a_len--) {
    mp_digit a_i = *a++;
    mp_digit a0b0, a1b1;

    MP_MUL_DxD(a_i, b, a1b1, a0b0);

    a0b0 += carry;
    if (a0b0 < carry)
      ++a1b1;
    *c++ = a0b0;
    carry = a1b1;
  }
  *c = carry;
#endif
}

/* c += a * b */
static void 
v8_mpv_mul_d_add(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
#if !defined(MP_NO_MP_WORD)
  mp_digit   d = 0;

  /* Inner product:  Digits of a */
  while (a_len--) {
    mp_word w = ((mp_word)b * *a++) + *c + d;
    *c++ = ACCUM(w);
    d = CARRYOUT(w);
  }
  *c = d;
#else
  mp_digit carry = 0;
  while (a_len--) {
    mp_digit a_i = *a++;
    mp_digit a0b0, a1b1;

    MP_MUL_DxD(a_i, b, a1b1, a0b0);

    a0b0 += carry;
    if (a0b0 < carry)
      ++a1b1;
    a0b0 += a_i = *c;
    if (a0b0 < a_i)
      ++a1b1;
    *c++ = a0b0;
    carry = a1b1;
  }
  *c = carry;
#endif
}

/* Presently, this is only used by the Montgomery arithmetic code. */
/* c += a * b */
static void 
v8_mpv_mul_d_add_prop(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
#if !defined(MP_NO_MP_WORD)
  mp_digit   d = 0;

  /* Inner product:  Digits of a */
  while (a_len--) {
    mp_word w = ((mp_word)b * *a++) + *c + d;
    *c++ = ACCUM(w);
    d = CARRYOUT(w);
  }

  while (d) {
    mp_word w = (mp_word)*c + d;
    *c++ = ACCUM(w);
    d = CARRYOUT(w);
  }
#else
  mp_digit carry = 0;
  while (a_len--) {
    mp_digit a_i = *a++;
    mp_digit a0b0, a1b1;

    MP_MUL_DxD(a_i, b, a1b1, a0b0);

    a0b0 += carry;
    if (a0b0 < carry)
      ++a1b1;

    a0b0 += a_i = *c;
    if (a0b0 < a_i)
      ++a1b1;

    *c++ = a0b0;
    carry = a1b1;
  }
  while (carry) {
    mp_digit c_i = *c;
    carry += c_i;
    *c++ = carry;
    carry = carry < c_i;
  }
#endif
}

/* These functions run only on v8plus+vis or v9+vis CPUs. */

/* c = a * b */
void 
s_mpv_mul_d(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
    mp_digit d;
    mp_digit x[258];
    if (a_len <= 256) {
	if (a == c || ((ptrdiff_t)a & 0x7) != 0 || (a_len & 1) != 0) {
	    mp_digit * px;
	    px = (((ptrdiff_t)x & 0x7) != 0) ? x + 1 : x;
	    memcpy(px, a, a_len * sizeof(*a));
	    a = px;
	    if (a_len & 1) {
		px[a_len] = 0;
	    }
	}
	s_mp_setz(c, a_len + 1);
	d = mul_add_inp(c, a, a_len, b);
	c[a_len] = d;
    } else {
	v8_mpv_mul_d(a, a_len, b, c);
    }
}

/* c += a * b, where a is a_len words long. */
void     
s_mpv_mul_d_add(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
    mp_digit d;
    mp_digit x[258];
    if (a_len <= 256) {
	if (((ptrdiff_t)a & 0x7) != 0 || (a_len & 1) != 0) {
	    mp_digit * px;
	    px = (((ptrdiff_t)x & 0x7) != 0) ? x + 1 : x;
	    memcpy(px, a, a_len * sizeof(*a));
	    a = px;
	    if (a_len & 1) {
		px[a_len] = 0;
	    }
	}
	d = mul_add_inp(c, a, a_len, b);
	c[a_len] = d;
    } else {
	v8_mpv_mul_d_add(a, a_len, b, c);
    }
}

/* c += a * b, where a is y words long. */
void     
s_mpv_mul_d_add_prop(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
    mp_digit d;
    mp_digit x[258];
    if (a_len <= 256) {
	if (((ptrdiff_t)a & 0x7) != 0 || (a_len & 1) != 0) {
	    mp_digit * px;
	    px = (((ptrdiff_t)x & 0x7) != 0) ? x + 1 : x;
	    memcpy(px, a, a_len * sizeof(*a));
	    a = px;
	    if (a_len & 1) {
		px[a_len] = 0;
	    }
	}
	d = mul_add_inp(c, a, a_len, b);
	if (d) {
	    c += a_len;
	    do {
		mp_digit sum = d + *c;
		*c++ = sum;
		d = sum < d;
	    } while (d);
	}
    } else {
	v8_mpv_mul_d_add_prop(a, a_len, b, c);
    }
}
