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

/* vis versions of these functions run only on v8+vis or v9+vis CPUs. */

/* c = a * b */
static void 
vis_mpv_mul_d(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
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
static void     
vis_mpv_mul_d_add(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
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
static void     
vis_mpv_mul_d_add_prop(const mp_digit *a, mp_size a_len, mp_digit b, 
			 mp_digit *c)
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

#if defined(SOLARIS2_5)
static int
isSparcV8PlusVis(void)
{
    long buflen;
    int  rv             = 0;    /* false */
    char buf[256];
    buflen = sysinfo(SI_MACHINE, buf, sizeof buf);
    if (buflen > 0) {
        rv = (!strcmp(buf, "sun4u") || !strcmp(buf, "sun4u1"));
    }
    return rv;
}
#else   /* SunOS2.6or higher has SI_ISALIST */

static int
isSparcV8PlusVis(void)
{
    long buflen;
    int  rv             = 0;    /* false */
    char buf[256];
    buflen = sysinfo(SI_ISALIST, buf, sizeof buf);
    if (buflen > 0) {
#if defined(MP_USE_LONG_DIGIT)
        char * found = strstr(buf, "sparcv9+vis");
#else
        char * found = strstr(buf, "sparcv8plus+vis");
#endif
        rv = (found != 0);
    }
    return rv;
}
#endif

typedef void MPVmpy(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c);

/* forward static function declarations */
static MPVmpy sp_mpv_mul_d;
static MPVmpy sp_mpv_mul_d_add;
static MPVmpy sp_mpv_mul_d_add_prop;

static MPVmpy *p_mpv_mul_d		= &sp_mpv_mul_d;
static MPVmpy *p_mpv_mul_d_add		= &sp_mpv_mul_d_add;
static MPVmpy *p_mpv_mul_d_add_prop	= &sp_mpv_mul_d_add_prop;

static void
initPtrs(void)
{
    if (isSparcV8PlusVis()) {
	p_mpv_mul_d = 		&vis_mpv_mul_d;
	p_mpv_mul_d_add = 	&vis_mpv_mul_d_add;
	p_mpv_mul_d_add_prop = 	&vis_mpv_mul_d_add_prop;
    } else {
	p_mpv_mul_d = 		&v8_mpv_mul_d;
	p_mpv_mul_d_add = 	&v8_mpv_mul_d_add;
	p_mpv_mul_d_add_prop = 	&v8_mpv_mul_d_add_prop;
    }
}

static void 
sp_mpv_mul_d(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
    initPtrs();
    (* p_mpv_mul_d)(a, a_len, b, c);
}

static void 
sp_mpv_mul_d_add(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
    initPtrs();
    (* p_mpv_mul_d_add)(a, a_len, b, c);
}

static void 
sp_mpv_mul_d_add_prop(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
    initPtrs();
    (* p_mpv_mul_d_add_prop)(a, a_len, b, c);
}


/* This is the external interface */

void 
s_mpv_mul_d(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
    (* p_mpv_mul_d)(a, a_len, b, c);
}

void 
s_mpv_mul_d_add(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
    (* p_mpv_mul_d_add)(a, a_len, b, c);
}

void 
s_mpv_mul_d_add_prop(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
    (* p_mpv_mul_d_add_prop)(a, a_len, b, c);
}


