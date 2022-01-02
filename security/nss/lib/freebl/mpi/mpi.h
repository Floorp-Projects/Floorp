/*
 *  mpi.h
 *
 *  Arbitrary precision integer arithmetic library
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _H_MPI_
#define _H_MPI_

#include "mpi-config.h"

#include "seccomon.h"
SEC_BEGIN_PROTOS

#if MP_DEBUG
#undef MP_IOFUNC
#define MP_IOFUNC 1
#endif

#if MP_IOFUNC
#include <stdio.h>
#include <ctype.h>
#endif

#include <limits.h>

#if defined(BSDI)
#undef ULLONG_MAX
#endif

#include <sys/types.h>

#define MP_NEG 1
#define MP_ZPOS 0

#define MP_OKAY 0    /* no error, all is well */
#define MP_YES 0     /* yes (boolean result)  */
#define MP_NO -1     /* no (boolean result)   */
#define MP_MEM -2    /* out of memory         */
#define MP_RANGE -3  /* argument out of range */
#define MP_BADARG -4 /* invalid parameter     */
#define MP_UNDEF -5  /* answer is undefined   */
#define MP_LAST_CODE MP_UNDEF

typedef unsigned int mp_sign;
typedef unsigned int mp_size;
typedef int mp_err;

#define MP_32BIT_MAX 4294967295U

#if !defined(ULONG_MAX)
#error "ULONG_MAX not defined"
#elif !defined(UINT_MAX)
#error "UINT_MAX not defined"
#elif !defined(USHRT_MAX)
#error "USHRT_MAX not defined"
#endif

#if defined(ULLONG_MAX) /* C99, Solaris */
#define MP_ULONG_LONG_MAX ULLONG_MAX
/* MP_ULONG_LONG_MAX was defined to be ULLONG_MAX */
#elif defined(ULONG_LONG_MAX) /* HPUX */
#define MP_ULONG_LONG_MAX ULONG_LONG_MAX
#elif defined(ULONGLONG_MAX) /* IRIX, AIX */
#define MP_ULONG_LONG_MAX ULONGLONG_MAX
#endif

/* We only use unsigned long for mp_digit iff long is more than 32 bits. */
#if !defined(MP_USE_UINT_DIGIT) && ULONG_MAX > MP_32BIT_MAX
typedef unsigned long mp_digit;
#define MP_DIGIT_MAX ULONG_MAX
#define MP_DIGIT_FMT "%016lX" /* printf() format for 1 digit */
#define MP_HALF_DIGIT_MAX UINT_MAX
#undef MP_NO_MP_WORD
#define MP_NO_MP_WORD 1
#undef MP_USE_LONG_DIGIT
#define MP_USE_LONG_DIGIT 1
#undef MP_USE_LONG_LONG_DIGIT

#elif !defined(MP_USE_UINT_DIGIT) && defined(MP_ULONG_LONG_MAX)
typedef unsigned long long mp_digit;
#define MP_DIGIT_MAX MP_ULONG_LONG_MAX
#define MP_DIGIT_FMT "%016llX" /* printf() format for 1 digit */
#define MP_HALF_DIGIT_MAX UINT_MAX
#undef MP_NO_MP_WORD
#define MP_NO_MP_WORD 1
#undef MP_USE_LONG_LONG_DIGIT
#define MP_USE_LONG_LONG_DIGIT 1
#undef MP_USE_LONG_DIGIT

#else
typedef unsigned int mp_digit;
#define MP_DIGIT_MAX UINT_MAX
#define MP_DIGIT_FMT "%08X" /* printf() format for 1 digit */
#define MP_HALF_DIGIT_MAX USHRT_MAX
#undef MP_USE_UINT_DIGIT
#define MP_USE_UINT_DIGIT 1
#undef MP_USE_LONG_LONG_DIGIT
#undef MP_USE_LONG_DIGIT
#endif

#if !defined(MP_NO_MP_WORD)
#if defined(MP_USE_UINT_DIGIT) && \
    (defined(MP_ULONG_LONG_MAX) || (ULONG_MAX > UINT_MAX))

#if (ULONG_MAX > UINT_MAX)
typedef unsigned long mp_word;
typedef long mp_sword;
#define MP_WORD_MAX ULONG_MAX

#else
typedef unsigned long long mp_word;
typedef long long mp_sword;
#define MP_WORD_MAX MP_ULONG_LONG_MAX
#endif

#else
#define MP_NO_MP_WORD 1
#endif
#endif /* !defined(MP_NO_MP_WORD) */

#if !defined(MP_WORD_MAX) && defined(MP_DEFINE_SMALL_WORD)
typedef unsigned int mp_word;
typedef int mp_sword;
#define MP_WORD_MAX UINT_MAX
#endif

#define MP_DIGIT_SIZE sizeof(mp_digit)
#define MP_DIGIT_BIT (CHAR_BIT * MP_DIGIT_SIZE)
#define MP_WORD_BIT (CHAR_BIT * sizeof(mp_word))
#define MP_RADIX (1 + (mp_word)MP_DIGIT_MAX)

#define MP_HALF_DIGIT_BIT (MP_DIGIT_BIT / 2)
#define MP_HALF_RADIX (1 + (mp_digit)MP_HALF_DIGIT_MAX)
/* MP_HALF_RADIX really ought to be called MP_SQRT_RADIX, but it's named
** MP_HALF_RADIX because it's the radix for MP_HALF_DIGITs, and it's
** consistent with the other _HALF_ names.
*/

/* Macros for accessing the mp_int internals           */
#define MP_SIGN(MP) ((MP)->sign)
#define MP_USED(MP) ((MP)->used)
#define MP_ALLOC(MP) ((MP)->alloc)
#define MP_DIGITS(MP) ((MP)->dp)
#define MP_DIGIT(MP, N) (MP)->dp[(N)]

/* This defines the maximum I/O base (minimum is 2)   */
#define MP_MAX_RADIX 64

typedef struct {
    mp_sign sign;  /* sign of this quantity      */
    mp_size alloc; /* how many digits allocated  */
    mp_size used;  /* how many digits used       */
    mp_digit *dp;  /* the digits themselves      */
} mp_int;

/* Default precision       */
mp_size mp_get_prec(void);
void mp_set_prec(mp_size prec);

/* Memory management       */
mp_err mp_init(mp_int *mp);
mp_err mp_init_size(mp_int *mp, mp_size prec);
mp_err mp_init_copy(mp_int *mp, const mp_int *from);
mp_err mp_copy(const mp_int *from, mp_int *to);
void mp_exch(mp_int *mp1, mp_int *mp2);
void mp_clear(mp_int *mp);
void mp_zero(mp_int *mp);
void mp_set(mp_int *mp, mp_digit d);
mp_err mp_set_int(mp_int *mp, long z);
#define mp_set_long(mp, z) mp_set_int(mp, z)
mp_err mp_set_ulong(mp_int *mp, unsigned long z);

/* Single digit arithmetic */
mp_err mp_add_d(const mp_int *a, mp_digit d, mp_int *b);
mp_err mp_sub_d(const mp_int *a, mp_digit d, mp_int *b);
mp_err mp_mul_d(const mp_int *a, mp_digit d, mp_int *b);
mp_err mp_mul_2(const mp_int *a, mp_int *c);
mp_err mp_div_d(const mp_int *a, mp_digit d, mp_int *q, mp_digit *r);
mp_err mp_div_2(const mp_int *a, mp_int *c);
mp_err mp_expt_d(const mp_int *a, mp_digit d, mp_int *c);

/* Sign manipulations      */
mp_err mp_abs(const mp_int *a, mp_int *b);
mp_err mp_neg(const mp_int *a, mp_int *b);

/* Full arithmetic         */
mp_err mp_add(const mp_int *a, const mp_int *b, mp_int *c);
mp_err mp_sub(const mp_int *a, const mp_int *b, mp_int *c);
mp_err mp_mul(const mp_int *a, const mp_int *b, mp_int *c);
#if MP_SQUARE
mp_err mp_sqr(const mp_int *a, mp_int *b);
#else
#define mp_sqr(a, b) mp_mul(a, a, b)
#endif
mp_err mp_div(const mp_int *a, const mp_int *b, mp_int *q, mp_int *r);
mp_err mp_div_2d(const mp_int *a, mp_digit d, mp_int *q, mp_int *r);
mp_err mp_expt(mp_int *a, mp_int *b, mp_int *c);
mp_err mp_2expt(mp_int *a, mp_digit k);

/* Modular arithmetic      */
#if MP_MODARITH
mp_err mp_mod(const mp_int *a, const mp_int *m, mp_int *c);
mp_err mp_mod_d(const mp_int *a, mp_digit d, mp_digit *c);
mp_err mp_addmod(const mp_int *a, const mp_int *b, const mp_int *m, mp_int *c);
mp_err mp_submod(const mp_int *a, const mp_int *b, const mp_int *m, mp_int *c);
mp_err mp_mulmod(const mp_int *a, const mp_int *b, const mp_int *m, mp_int *c);
#if MP_SQUARE
mp_err mp_sqrmod(const mp_int *a, const mp_int *m, mp_int *c);
#else
#define mp_sqrmod(a, m, c) mp_mulmod(a, a, m, c)
#endif
mp_err mp_exptmod(const mp_int *a, const mp_int *b, const mp_int *m, mp_int *c);
mp_err mp_exptmod_d(const mp_int *a, mp_digit d, const mp_int *m, mp_int *c);
#endif /* MP_MODARITH */

/* Comparisons             */
int mp_cmp_z(const mp_int *a);
int mp_cmp_d(const mp_int *a, mp_digit d);
int mp_cmp(const mp_int *a, const mp_int *b);
int mp_cmp_mag(const mp_int *a, const mp_int *b);
int mp_isodd(const mp_int *a);
int mp_iseven(const mp_int *a);

/* Number theoretic        */
mp_err mp_gcd(mp_int *a, mp_int *b, mp_int *c);
mp_err mp_lcm(mp_int *a, mp_int *b, mp_int *c);
mp_err mp_xgcd(const mp_int *a, const mp_int *b, mp_int *g, mp_int *x, mp_int *y);
mp_err mp_invmod(const mp_int *a, const mp_int *m, mp_int *c);
mp_err mp_invmod_xgcd(const mp_int *a, const mp_int *m, mp_int *c);

/* Input and output        */
#if MP_IOFUNC
void mp_print(mp_int *mp, FILE *ofp);
#endif /* end MP_IOFUNC */

/* Base conversion         */
mp_err mp_read_raw(mp_int *mp, char *str, int len);
int mp_raw_size(mp_int *mp);
mp_err mp_toraw(mp_int *mp, char *str);
mp_err mp_read_radix(mp_int *mp, const char *str, int radix);
mp_err mp_read_variable_radix(mp_int *a, const char *str, int default_radix);
int mp_radix_size(mp_int *mp, int radix);
mp_err mp_toradix(mp_int *mp, char *str, int radix);
int mp_tovalue(char ch, int r);

#define mp_tobinary(M, S) mp_toradix((M), (S), 2)
#define mp_tooctal(M, S) mp_toradix((M), (S), 8)
#define mp_todecimal(M, S) mp_toradix((M), (S), 10)
#define mp_tohex(M, S) mp_toradix((M), (S), 16)

/* Error strings           */
const char *mp_strerror(mp_err ec);

/* Octet string conversion functions */
mp_err mp_read_unsigned_octets(mp_int *mp, const unsigned char *str, mp_size len);
unsigned int mp_unsigned_octet_size(const mp_int *mp);
mp_err mp_to_unsigned_octets(const mp_int *mp, unsigned char *str, mp_size maxlen);
mp_err mp_to_signed_octets(const mp_int *mp, unsigned char *str, mp_size maxlen);
mp_err mp_to_fixlen_octets(const mp_int *mp, unsigned char *str, mp_size len);

/* Miscellaneous */
mp_size mp_trailing_zeros(const mp_int *mp);
void freebl_cpuid(unsigned long op, unsigned long *eax,
                  unsigned long *ebx, unsigned long *ecx,
                  unsigned long *edx);
mp_err mp_cswap(mp_digit condition, mp_int *a, mp_int *b, mp_size numdigits);

#define MP_CHECKOK(x)          \
    if (MP_OKAY > (res = (x))) \
    goto CLEANUP
#define MP_CHECKERR(x)         \
    if (MP_OKAY > (res = (x))) \
    goto CLEANUP

#define NEG MP_NEG
#define ZPOS MP_ZPOS
#define DIGIT_MAX MP_DIGIT_MAX
#define DIGIT_BIT MP_DIGIT_BIT
#define DIGIT_FMT MP_DIGIT_FMT
#define RADIX MP_RADIX
#define MAX_RADIX MP_MAX_RADIX
#define SIGN(MP) MP_SIGN(MP)
#define USED(MP) MP_USED(MP)
#define ALLOC(MP) MP_ALLOC(MP)
#define DIGITS(MP) MP_DIGITS(MP)
#define DIGIT(MP, N) MP_DIGIT(MP, N)

/* Functions which return an mp_err value will NULL-check their arguments via
 * ARGCHK(condition, return), where the caller is responsible for checking the
 * mp_err return code. For functions that return an integer type, the caller 
 * has no way to tell if the value is an error code or a legitimate value. 
 * Therefore, ARGMPCHK(condition) will trigger an assertion failure on debug
 * builds, but no-op in optimized builds. */
#if MP_ARGCHK == 1
#define ARGMPCHK(X) /* */
#define ARGCHK(X, Y)    \
    {                   \
        if (!(X)) {     \
            return (Y); \
        }               \
    }
#elif MP_ARGCHK == 2
#include <assert.h>
#define ARGMPCHK(X) assert(X)
#define ARGCHK(X, Y) assert(X)
#else
#define ARGMPCHK(X)  /* */
#define ARGCHK(X, Y) /* */
#endif

#ifdef CT_VERIF
void mp_taint(mp_int *mp);
void mp_untaint(mp_int *mp);
#endif

SEC_END_PROTOS

#endif /* end _H_MPI_ */
