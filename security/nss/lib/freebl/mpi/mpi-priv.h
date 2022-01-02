/*
 *  mpi-priv.h  - Private header file for MPI
 *  Arbitrary precision integer arithmetic library
 *
 *  NOTE WELL: the content of this header file is NOT part of the "public"
 *  API for the MPI library, and may change at any time.
 *  Application programs that use libmpi should NOT include this header file.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _MPI_PRIV_H_
#define _MPI_PRIV_H_ 1

#include "mpi.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if MP_DEBUG
#include <stdio.h>

#define DIAG(T, V)           \
    {                        \
        fprintf(stderr, T);  \
        mp_print(V, stderr); \
        fputc('\n', stderr); \
    }
#else
#define DIAG(T, V)
#endif

/* If we aren't using a wired-in logarithm table, we need to include
   the math library to get the log() function
 */

/* {{{ s_logv_2[] - log table for 2 in various bases */

#if MP_LOGTAB
/*
  A table of the logs of 2 for various bases (the 0 and 1 entries of
  this table are meaningless and should not be referenced).

  This table is used to compute output lengths for the mp_toradix()
  function.  Since a number n in radix r takes up about log_r(n)
  digits, we estimate the output size by taking the least integer
  greater than log_r(n), where:

  log_r(n) = log_2(n) * log_r(2)

  This table, therefore, is a table of log_r(2) for 2 <= r <= 36,
  which are the output bases supported.
 */

extern const float s_logv_2[];
#define LOG_V_2(R) s_logv_2[(R)]

#else

/*
   If MP_LOGTAB is not defined, use the math library to compute the
   logarithms on the fly.  Otherwise, use the table.
   Pick which works best for your system.
 */

#include <math.h>
#define LOG_V_2(R) (log(2.0) / log(R))

#endif /* if MP_LOGTAB */

/* }}} */

/* {{{ Digit arithmetic macros */

/*
  When adding and multiplying digits, the results can be larger than
  can be contained in an mp_digit.  Thus, an mp_word is used.  These
  macros mask off the upper and lower digits of the mp_word (the
  mp_word may be more than 2 mp_digits wide, but we only concern
  ourselves with the low-order 2 mp_digits)
 */

#define CARRYOUT(W) (mp_digit)((W) >> DIGIT_BIT)
#define ACCUM(W) (mp_digit)(W)

#define MP_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MP_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MP_HOWMANY(a, b) (((a) + (b)-1) / (b))
#define MP_ROUNDUP(a, b) (MP_HOWMANY(a, b) * (b))

/* }}} */

/* {{{ Comparison constants */

#define MP_LT -1
#define MP_EQ 0
#define MP_GT 1

/* }}} */

/* {{{ private function declarations */

void s_mp_setz(mp_digit *dp, mp_size count);                     /* zero digits           */
void s_mp_copy(const mp_digit *sp, mp_digit *dp, mp_size count); /* copy */
void *s_mp_alloc(size_t nb, size_t ni);                          /* general allocator     */
void s_mp_free(void *ptr);                                       /* general free function */

mp_err s_mp_grow(mp_int *mp, mp_size min); /* increase allocated size */
mp_err s_mp_pad(mp_int *mp, mp_size min);  /* left pad with zeroes    */

void s_mp_clamp(mp_int *mp); /* clip leading zeroes     */

void s_mp_exch(mp_int *a, mp_int *b); /* swap a and b in place   */

mp_err s_mp_lshd(mp_int *mp, mp_size p);    /* left-shift by p digits  */
void s_mp_rshd(mp_int *mp, mp_size p);      /* right-shift by p digits */
mp_err s_mp_mul_2d(mp_int *mp, mp_digit d); /* multiply by 2^d in place */
void s_mp_div_2d(mp_int *mp, mp_digit d);   /* divide by 2^d in place  */
void s_mp_mod_2d(mp_int *mp, mp_digit d);   /* modulo 2^d in place     */
void s_mp_div_2(mp_int *mp);                /* divide by 2 in place    */
mp_err s_mp_mul_2(mp_int *mp);              /* multiply by 2 in place  */
mp_err s_mp_norm(mp_int *a, mp_int *b, mp_digit *pd);
/* normalize for division  */
mp_err s_mp_add_d(mp_int *mp, mp_digit d); /* unsigned digit addition */
mp_err s_mp_sub_d(mp_int *mp, mp_digit d); /* unsigned digit subtract */
mp_err s_mp_mul_d(mp_int *mp, mp_digit d); /* unsigned digit multiply */
mp_err s_mp_div_d(mp_int *mp, mp_digit d, mp_digit *r);
/* unsigned digit divide   */
mp_err s_mp_reduce(mp_int *x, const mp_int *m, const mp_int *mu);
/* Barrett reduction       */
mp_err s_mp_add(mp_int *a, const mp_int *b); /* magnitude addition      */
mp_err s_mp_add_3arg(const mp_int *a, const mp_int *b, mp_int *c);
mp_err s_mp_sub(mp_int *a, const mp_int *b); /* magnitude subtract      */
mp_err s_mp_sub_3arg(const mp_int *a, const mp_int *b, mp_int *c);
mp_err s_mp_add_offset(mp_int *a, mp_int *b, mp_size offset);
/* a += b * RADIX^offset   */
mp_err s_mp_mul(mp_int *a, const mp_int *b); /* magnitude multiply      */
#if MP_SQUARE
mp_err s_mp_sqr(mp_int *a); /* magnitude square        */
#else
#define s_mp_sqr(a) s_mp_mul(a, a)
#endif
mp_err s_mp_div(mp_int *rem, mp_int *div, mp_int *quot); /* magnitude div */
mp_err s_mp_exptmod(const mp_int *a, const mp_int *b, const mp_int *m, mp_int *c);
mp_err s_mp_2expt(mp_int *a, mp_digit k);       /* a = 2^k                 */
int s_mp_cmp(const mp_int *a, const mp_int *b); /* magnitude comparison */
int s_mp_cmp_d(const mp_int *a, mp_digit d);    /* magnitude digit compare */
int s_mp_ispow2(const mp_int *v);               /* is v a power of 2?      */
int s_mp_ispow2d(mp_digit d);                   /* is d a power of 2?      */

int s_mp_tovalue(char ch, int r);                /* convert ch to value    */
char s_mp_todigit(mp_digit val, int r, int low); /* convert val to digit */
int s_mp_outlen(int bits, int r);                /* output length in bytes */
mp_digit s_mp_invmod_radix(mp_digit P);          /* returns (P ** -1) mod RADIX */
mp_err s_mp_invmod_odd_m(const mp_int *a, const mp_int *m, mp_int *c);
mp_err s_mp_invmod_2d(const mp_int *a, mp_size k, mp_int *c);
mp_err s_mp_invmod_even_m(const mp_int *a, const mp_int *m, mp_int *c);

#ifdef NSS_USE_COMBA
PR_STATIC_ASSERT(sizeof(mp_digit) == 8);
#define IS_POWER_OF_2(a) ((a) && !((a) & ((a)-1)))

void s_mp_mul_comba_4(const mp_int *A, const mp_int *B, mp_int *C);
void s_mp_mul_comba_8(const mp_int *A, const mp_int *B, mp_int *C);
void s_mp_mul_comba_16(const mp_int *A, const mp_int *B, mp_int *C);
void s_mp_mul_comba_32(const mp_int *A, const mp_int *B, mp_int *C);

void s_mp_sqr_comba_4(const mp_int *A, mp_int *B);
void s_mp_sqr_comba_8(const mp_int *A, mp_int *B);
void s_mp_sqr_comba_16(const mp_int *A, mp_int *B);
void s_mp_sqr_comba_32(const mp_int *A, mp_int *B);

#endif /* end NSS_USE_COMBA */

/* ------ mpv functions, operate on arrays of digits, not on mp_int's ------ */
#if defined(__OS2__) && defined(__IBMC__)
#define MPI_ASM_DECL __cdecl
#else
#define MPI_ASM_DECL
#endif

#ifdef MPI_AMD64

mp_digit MPI_ASM_DECL s_mpv_mul_set_vec64(mp_digit *, mp_digit *, mp_size, mp_digit);
mp_digit MPI_ASM_DECL s_mpv_mul_add_vec64(mp_digit *, const mp_digit *, mp_size, mp_digit);

/* c = a * b */
#define s_mpv_mul_d(a, a_len, b, c) \
    ((mp_digit *)c)[a_len] = s_mpv_mul_set_vec64(c, a, a_len, b)

/* c += a * b */
#define s_mpv_mul_d_add(a, a_len, b, c) \
    ((mp_digit *)c)[a_len] = s_mpv_mul_add_vec64(c, a, a_len, b)

#else

void MPI_ASM_DECL s_mpv_mul_d(const mp_digit *a, mp_size a_len,
                              mp_digit b, mp_digit *c);
void MPI_ASM_DECL s_mpv_mul_d_add(const mp_digit *a, mp_size a_len,
                                  mp_digit b, mp_digit *c);

#endif

void MPI_ASM_DECL s_mpv_mul_d_add_prop(const mp_digit *a,
                                       mp_size a_len, mp_digit b,
                                       mp_digit *c);
void MPI_ASM_DECL s_mpv_sqr_add_prop(const mp_digit *a,
                                     mp_size a_len,
                                     mp_digit *sqrs);

mp_err MPI_ASM_DECL s_mpv_div_2dx1d(mp_digit Nhi, mp_digit Nlo,
                                    mp_digit divisor, mp_digit *quot, mp_digit *rem);

/* c += a * b * (MP_RADIX ** offset);  */
/* Callers of this macro should be aware that the return type might vary;
 * it should be treated as a void function. */
#define s_mp_mul_d_add_offset(a, b, c, off) \
    s_mpv_mul_d_add_prop(MP_DIGITS(a), MP_USED(a), b, MP_DIGITS(c) + off)

typedef struct {
    mp_int N;         /* modulus N */
    mp_digit n0prime; /* n0' = - (n0 ** -1) mod MP_RADIX */
} mp_mont_modulus;

mp_err s_mp_mul_mont(const mp_int *a, const mp_int *b, mp_int *c,
                     mp_mont_modulus *mmm);
mp_err s_mp_redc(mp_int *T, mp_mont_modulus *mmm);

/*
 * s_mpi_getProcessorLineSize() returns the size in bytes of the cache line
 * if a cache exists, or zero if there is no cache. If more than one
 * cache line exists, it should return the smallest line size (which is
 * usually the L1 cache).
 *
 * mp_modexp uses this information to make sure that private key information
 * isn't being leaked through the cache.
 *
 * see mpcpucache.c for the implementation.
 */
unsigned long s_mpi_getProcessorLineSize();

/* }}} */
#endif
