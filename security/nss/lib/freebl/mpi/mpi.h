/*
 *  mpi.h
 *
 *  Arbitrary precision integer arithmetic library
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
 * Copyright (C) 1998, 1999, 2000 Michael J. Fromberger. 
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
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 *  $Id: mpi.h,v 1.3 2000/07/19 23:18:08 nelsonb%netscape.com Exp $
 */

#ifndef _H_MPI_
#define _H_MPI_

#include "mpi-config.h"

#if MP_DEBUG
#undef MP_IOFUNC
#define MP_IOFUNC 1
#endif

#if MP_IOFUNC
#include <stdio.h>
#include <ctype.h>
#endif

#include <limits.h>

#define  MP_NEG    1
#define  MP_ZPOS   0

#define  MP_OKAY          0 /* no error, all is well */
#define  MP_YES           0 /* yes (boolean result)  */
#define  MP_NO           -1 /* no (boolean result)   */
#define  MP_MEM          -2 /* out of memory         */
#define  MP_RANGE        -3 /* argument out of range */
#define  MP_BADARG       -4 /* invalid parameter     */
#define  MP_UNDEF        -5 /* answer is undefined   */
#define  MP_LAST_CODE    MP_UNDEF

typedef char              mp_sign;
typedef unsigned int      mp_size;
typedef int               mp_err;

#ifndef MP_USE_32
#if defined(ULONG_LONG_MAX)			/* GCC, HPUX */
#define MP_ULONG_LONG_MAX ULONG_LONG_MAX
#elif defined(ULLONG_MAX)			/* Solaris */
#define MP_ULONG_LONG_MAX ULLONG_MAX
#elif defined(ULONGLONG_MAX)			/* IRIX, AIX */
#define MP_ULONG_LONG_MAX ULONGLONG_MAX
#endif

#if defined(MP_ULONG_LONG_MAX) && MP_ULONG_LONG_MAX > UINT_MAX
#if MP_ULONG_LONG_MAX == ULONG_MAX
typedef unsigned int      mp_digit;
typedef unsigned long     mp_word;
#define MP_DIGIT_MAX      UINT_MAX
#define MP_WORD_MAX       ULONG_MAX
#else
typedef unsigned long     mp_digit;
typedef unsigned long long mp_word;
#define MP_DIGIT_MAX      ULONG_MAX
#define MP_WORD_MAX       MP_ULONG_LONG_MAX
#endif
#endif
#endif /* !USE_32 */

#if !defined(MP_DIGIT_MAX)
#if ULONG_MAX == UINT_MAX
typedef unsigned short    mp_digit;
typedef unsigned int      mp_word;
#define MP_DIGIT_MAX      USHRT_MAX
#define MP_WORD_MAX       UINT_MAX
#else
typedef unsigned int      mp_digit;
typedef unsigned long     mp_word;
#define MP_DIGIT_MAX      UINT_MAX
#define MP_WORD_MAX       ULONG_MAX
#endif
#endif

#define MP_DIGIT_BIT      (CHAR_BIT*sizeof(mp_digit))
#define MP_WORD_BIT       (CHAR_BIT*sizeof(mp_word))
#define MP_RADIX          (1+(mp_word)MP_DIGIT_MAX)

#define MP_DIGIT_FMT      "%04X"     /* printf() format for 1 digit */

/* Macros for accessing the mp_int internals           */
#define  MP_SIGN(MP)     ((MP)->sign)
#define  MP_USED(MP)     ((MP)->used)
#define  MP_ALLOC(MP)    ((MP)->alloc)
#define  MP_DIGITS(MP)   ((MP)->dp)
#define  MP_DIGIT(MP,N)  (MP)->dp[(N)]

/* This defines the maximum I/O base (minimum is 2)   */
#define MP_MAX_RADIX         64

typedef struct {
  mp_sign       sign;    /* sign of this quantity      */
  mp_size       alloc;   /* how many digits allocated  */
  mp_size       used;    /* how many digits used       */
  mp_digit     *dp;      /* the digits themselves      */
} mp_int;

/* Default precision       */
unsigned int mp_get_prec(void);
void         mp_set_prec(unsigned int prec);

/* Memory management       */
mp_err mp_init(mp_int *mp);
mp_err mp_init_size(mp_int *mp, mp_size prec);
mp_err mp_init_copy(mp_int *mp, mp_int *from);
mp_err mp_copy(mp_int *from, mp_int *to);
void   mp_exch(mp_int *mp1, mp_int *mp2);
void   mp_clear(mp_int *mp);
void   mp_zero(mp_int *mp);
void   mp_set(mp_int *mp, mp_digit d);
mp_err mp_set_int(mp_int *mp, long z);

/* Single digit arithmetic */
mp_err mp_add_d(mp_int *a, mp_digit d, mp_int *b);
mp_err mp_sub_d(mp_int *a, mp_digit d, mp_int *b);
mp_err mp_mul_d(mp_int *a, mp_digit d, mp_int *b);
mp_err mp_mul_2(mp_int *a, mp_int *c);
mp_err mp_div_d(mp_int *a, mp_digit d, mp_int *q, mp_digit *r);
mp_err mp_div_2(mp_int *a, mp_int *c);
mp_err mp_expt_d(mp_int *a, mp_digit d, mp_int *c);

/* Sign manipulations      */
mp_err mp_abs(mp_int *a, mp_int *b);
mp_err mp_neg(mp_int *a, mp_int *b);

/* Full arithmetic         */
mp_err mp_add(mp_int *a, mp_int *b, mp_int *c);
mp_err mp_sub(mp_int *a, mp_int *b, mp_int *c);
mp_err mp_mul(mp_int *a, mp_int *b, mp_int *c);
#if MP_SQUARE
mp_err mp_sqr(mp_int *a, mp_int *b);
#else
#define mp_sqr(a, b) mp_mul(a, a, b)
#endif
mp_err mp_div(mp_int *a, mp_int *b, mp_int *q, mp_int *r);
mp_err mp_div_2d(mp_int *a, mp_digit d, mp_int *q, mp_int *r);
mp_err mp_expt(mp_int *a, mp_int *b, mp_int *c);
mp_err mp_2expt(mp_int *a, mp_digit k);
mp_err mp_sqrt(mp_int *a, mp_int *b);

/* Modular arithmetic      */
#if MP_MODARITH
mp_err mp_mod(mp_int *a, mp_int *m, mp_int *c);
mp_err mp_mod_d(mp_int *a, mp_digit d, mp_digit *c);
mp_err mp_addmod(mp_int *a, mp_int *b, mp_int *m, mp_int *c);
mp_err mp_submod(mp_int *a, mp_int *b, mp_int *m, mp_int *c);
mp_err mp_mulmod(mp_int *a, mp_int *b, mp_int *m, mp_int *c);
#if MP_SQUARE
mp_err mp_sqrmod(mp_int *a, mp_int *m, mp_int *c);
#else
#define mp_sqrmod(a, m, c) mp_mulmod(a, a, m, c)
#endif
mp_err mp_exptmod(mp_int *a, mp_int *b, mp_int *m, mp_int *c);
mp_err mp_exptmod_d(mp_int *a, mp_digit d, mp_int *m, mp_int *c);
#endif /* MP_MODARITH */

/* Comparisons             */
int    mp_cmp_z(mp_int *a);
int    mp_cmp_d(mp_int *a, mp_digit d);
int    mp_cmp(mp_int *a, mp_int *b);
int    mp_cmp_mag(mp_int *a, mp_int *b);
int    mp_cmp_int(mp_int *a, long z);
int    mp_isodd(mp_int *a);
int    mp_iseven(mp_int *a);

/* Number theoretic        */
#if MP_NUMTH
mp_err mp_gcd(mp_int *a, mp_int *b, mp_int *c);
mp_err mp_lcm(mp_int *a, mp_int *b, mp_int *c);
mp_err mp_xgcd(mp_int *a, mp_int *b, mp_int *g, mp_int *x, mp_int *y);
mp_err mp_invmod(mp_int *a, mp_int *m, mp_int *c);
#endif /* end MP_NUMTH */

/* Input and output        */
#if MP_IOFUNC
void   mp_print(mp_int *mp, FILE *ofp);
#endif /* end MP_IOFUNC */

/* Base conversion         */
mp_err mp_read_raw(mp_int *mp, char *str, int len);
int    mp_raw_size(mp_int *mp);
mp_err mp_toraw(mp_int *mp, char *str);
mp_err mp_read_radix(mp_int *mp, char *str, int radix);
int    mp_radix_size(mp_int *mp, int radix);
mp_err mp_toradix(mp_int *mp, char *str, int radix);
int    mp_tovalue(char ch, int r);

#define mp_tobinary(M, S)  mp_toradix((M), (S), 2)
#define mp_tooctal(M, S)   mp_toradix((M), (S), 8)
#define mp_todecimal(M, S) mp_toradix((M), (S), 10)
#define mp_tohex(M, S)     mp_toradix((M), (S), 16)

/* Error strings           */
const  char  *mp_strerror(mp_err ec);

/* Octet string conversion functions */
mp_err mp_read_unsigned_octets(mp_int *mp, const unsigned char *str, int len);
int    mp_unsigned_octet_size(const mp_int *mp);
mp_err mp_to_unsigned_octets(const mp_int *mp, unsigned char *str, int maxlen);
mp_err mp_to_signed_octets(const mp_int *mp, unsigned char *str, int maxlen);
mp_err mp_to_fixlen_octets(const mp_int *mp, unsigned char *str, int len);

#if defined(MP_API_COMPATIBLE)
#define NEG             MP_NEG
#define ZPOS            MP_ZPOS
#define DIGIT_MAX       MP_DIGIT_MAX
#define DIGIT_BIT       MP_DIGIT_BIT
#define DIGIT_FMT       MP_DIGIT_FMT
#define RADIX           MP_RADIX
#define MAX_RADIX       MP_MAX_RADIX
#define SIGN(MP)        MP_SIGN(MP)
#define USED(MP)        MP_USED(MP)
#define ALLOC(MP)       MP_ALLOC(MP)
#define DIGITS(MP)      MP_DIGITS(MP)
#define DIGIT(MP,N)     MP_DIGIT(MP,N)

#if MP_ARGCHK == 1
#define  ARGCHK(X,Y)  {if(!(X)){return (Y);}}
#elif MP_ARGCHK == 2
#include <assert.h>
#define  ARGCHK(X,Y)  assert(X)
#else
#define  ARGCHK(X,Y)  /*  */
#endif
#endif /* defined MP_API_COMPATIBLE */

#endif /* end _H_MPI_ */
