/*
 *  mpi-priv.h
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
 * may use your version of this file under either the MPL or the GPL.
 *
 *  $Id: mpi-priv.h,v 1.4 2000/08/01 01:38:29 nelsonb%netscape.com Exp $
 */
#ifndef _MPI_PRIV_H_
#define _MPI_PRIV_H_ 1

#include "mpi.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if MP_DEBUG
#include <stdio.h>

#define DIAG(T,V) {fprintf(stderr,T);mp_print(V,stderr);fputc('\n',stderr);}
#else
#define DIAG(T,V)
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
#define LOG_V_2(R)  s_logv_2[(R)]

#else

/* 
   If MP_LOGTAB is not defined, use the math library to compute the
   logarithms on the fly.  Otherwise, use the table.
   Pick which works best for your system.
 */

#include <math.h>
#define LOG_V_2(R)  (log(2.0)/log(R))

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

#define  CARRYOUT(W)  (mp_digit)((W)>>DIGIT_BIT)
#define  ACCUM(W)     (mp_digit)(W)

/* }}} */

/* {{{ Comparison constants */

#define  MP_LT       -1
#define  MP_EQ        0
#define  MP_GT        1

/* }}} */

/* {{{ private function declarations */

/* 
   If MP_MACRO is false, these will be defined as actual functions;
   otherwise, suitable macro definitions will be used.  This works
   around the fact that ANSI C89 doesn't support an 'inline' keyword
   (although I hear C9x will ... about bloody time).  At present, the
   macro definitions are identical to the function bodies, but they'll
   expand in place, instead of generating a function call.

   I chose these particular functions to be made into macros because
   some profiling showed they are called a lot on a typical workload,
   and yet they are primarily housekeeping.
 */
#if MP_MACRO == 0
 void     s_mp_setz(mp_digit *dp, mp_size count); /* zero digits           */
 void     s_mp_copy(const mp_digit *sp, mp_digit *dp, mp_size count); /* copy */
 void    *s_mp_alloc(size_t nb, size_t ni);       /* general allocator     */
 void     s_mp_free(void *ptr);                   /* general free function */
#else

 /* Even if these are defined as macros, we need to respect the settings
    of the MP_MEMSET and MP_MEMCPY configuration options...
  */
 #if MP_MEMSET == 0
  #define  s_mp_setz(dp, count) \
       {int ix;for(ix=0;ix<(count);ix++)(dp)[ix]=0;}
 #else
  #define  s_mp_setz(dp, count) memset(dp, 0, (count) * sizeof(mp_digit))
 #endif /* MP_MEMSET */

 #if MP_MEMCPY == 0
  #define  s_mp_copy(sp, dp, count) \
       {int ix;for(ix=0;ix<(count);ix++)(dp)[ix]=(sp)[ix];}
 #else
  #define  s_mp_copy(sp, dp, count) memcpy(dp, sp, (count) * sizeof(mp_digit))
 #endif /* MP_MEMCPY */

 #define  s_mp_alloc(nb, ni)  calloc(nb, ni)
 #define  s_mp_free(ptr) {if(ptr) free(ptr);}
#endif /* MP_MACRO */

mp_err   s_mp_grow(mp_int *mp, mp_size min);   /* increase allocated size */
mp_err   s_mp_pad(mp_int *mp, mp_size min);    /* left pad with zeroes    */

#if MP_MACRO == 0
 void     s_mp_clamp(mp_int *mp);               /* clip leading zeroes     */
#else
 #define  s_mp_clamp(mp)\
     { while(USED(mp) > 1 && DIGIT((mp), USED(mp) - 1) == 0) USED(mp) -= 1; }
#endif /* MP_MACRO */

void     s_mp_exch(mp_int *a, mp_int *b);      /* swap a and b in place   */

mp_err   s_mp_lshd(mp_int *mp, mp_size p);     /* left-shift by p digits  */
void     s_mp_rshd(mp_int *mp, mp_size p);     /* right-shift by p digits */
void     s_mp_div_2d(mp_int *mp, mp_digit d);  /* divide by 2^d in place  */
void     s_mp_mod_2d(mp_int *mp, mp_digit d);  /* modulo 2^d in place     */
void     s_mp_div_2(mp_int *mp);               /* divide by 2 in place    */
mp_err   s_mp_mul_2(mp_int *mp);               /* multiply by 2 in place  */
mp_digit s_mp_norm(mp_int *a, mp_int *b);      /* normalize for division  */
mp_err   s_mp_add_d(mp_int *mp, mp_digit d);   /* unsigned digit addition */
mp_err   s_mp_sub_d(mp_int *mp, mp_digit d);   /* unsigned digit subtract */
mp_err   s_mp_mul_d(mp_int *mp, mp_digit d);   /* unsigned digit multiply */
mp_err   s_mp_div_d(mp_int *mp, mp_digit d, mp_digit *r);
		                               /* unsigned digit divide   */
mp_err   s_mp_mod_d(mp_int *mp, mp_digit d, mp_digit *r);
                                               /* unsigned digit rem      */
mp_err   s_mp_reduce(mp_int *x, const mp_int *m, const mp_int *mu);
                                               /* Barrett reduction       */
mp_err   s_mp_add(mp_int *a, const mp_int *b); /* magnitude addition      */
mp_err   s_mp_add_offset(mp_int *a, mp_int *b, mp_size offset);
                                               /* a += b * RADIX^offset   */
mp_err   s_mp_sub(mp_int *a, const mp_int *b); /* magnitude subtract      */
mp_err   s_mp_mul(mp_int *a, const mp_int *b); /* magnitude multiply      */
mp_err   s_mp_mul_d_add_offset(mp_int *a, mp_digit b, mp_int *c, mp_size off);
                                      /* c += a * b * (MP_RADIX ** offset);  */
#if MP_SQUARE
mp_err   s_mp_sqr(mp_int *a);                  /* magnitude square        */
#else
#define  s_mp_sqr(a) s_mp_mul(a, a)
#endif
mp_err   s_mp_div(mp_int *a, mp_int *b);       /* magnitude divide        */
mp_err   s_mp_exptmod(const mp_int *a, const mp_int *b, const mp_int *m, mp_int *c);
mp_err   s_mp_2expt(mp_int *a, mp_digit k);    /* a = 2^k                 */
int      s_mp_cmp(const mp_int *a, const mp_int *b); /* magnitude comparison */
int      s_mp_cmp_d(const mp_int *a, mp_digit d); /* magnitude digit compare */
int      s_mp_ispow2(mp_int *v);               /* is v a power of 2?      */
int      s_mp_ispow2d(mp_digit d);             /* is d a power of 2?      */

int      s_mp_tovalue(char ch, int r);          /* convert ch to value    */
char     s_mp_todigit(mp_digit val, int r, int low); /* convert val to digit */
int      s_mp_outlen(int bits, int r);          /* output length in bytes */

/* }}} */
#endif
