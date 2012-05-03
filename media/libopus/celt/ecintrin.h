/* Copyright (c) 2003-2008 Timothy B. Terriberry
   Copyright (c) 2008 Xiph.Org Foundation */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*Some common macros for potential platform-specific optimization.*/
#include "opus_types.h"
#include <math.h>
#include <limits.h>
#include "arch.h"
#if !defined(_ecintrin_H)
# define _ecintrin_H (1)

/*Some specific platforms may have optimized intrinsic or inline assembly
   versions of these functions which can substantially improve performance.
  We define macros for them to allow easy incorporation of these non-ANSI
   features.*/

/*Note that we do not provide a macro for abs(), because it is provided as a
   library function, which we assume is translated into an intrinsic to avoid
   the function call overhead and then implemented in the smartest way for the
   target platform.
  With modern gcc (4.x), this is true: it uses cmov instructions if the
   architecture supports it and branchless bit-twiddling if it does not (the
   speed difference between the two approaches is not measurable).
  Interestingly, the bit-twiddling method was patented in 2000 (US 6,073,150)
   by Sun Microsystems, despite prior art dating back to at least 1996:
   http://web.archive.org/web/19961201174141/www.x86.org/ftp/articles/pentopt/PENTOPT.TXT
  On gcc 3.x, however, our assumption is not true, as abs() is translated to a
   conditional jump, which is horrible on deeply piplined architectures (e.g.,
   all consumer architectures for the past decade or more) when the sign cannot
   be reliably predicted.*/

/*Modern gcc (4.x) can compile the naive versions of min and max with cmov if
   given an appropriate architecture, but the branchless bit-twiddling versions
   are just as fast, and do not require any special target architecture.
  Earlier gcc versions (3.x) compiled both code to the same assembly
   instructions, because of the way they represented ((_b)>(_a)) internally.*/
# define EC_MINI(_a,_b)      ((_a)+(((_b)-(_a))&-((_b)<(_a))))

/*Count leading zeros.
  This macro should only be used for implementing ec_ilog(), if it is defined.
  All other code should use EC_ILOG() instead.*/
#if defined(_MSC_VER)
# include <intrin.h>
/*In _DEBUG mode this is not an intrinsic by default.*/
# pragma intrinsic(_BitScanReverse)

static __inline int ec_bsr(unsigned long _x){
  unsigned long ret;
  _BitScanReverse(&ret,_x);
  return (int)ret;
}
# define EC_CLZ0    (1)
# define EC_CLZ(_x) (-ec_bsr(_x))
#elif defined(ENABLE_TI_DSPLIB)
# include "dsplib.h"
# define EC_CLZ0    (31)
# define EC_CLZ(_x) (_lnorm(_x))
#elif __GNUC_PREREQ(3,4)
# if INT_MAX>=2147483647
#  define EC_CLZ0    ((int)sizeof(unsigned)*CHAR_BIT)
#  define EC_CLZ(_x) (__builtin_clz(_x))
# elif LONG_MAX>=2147483647L
#  define EC_CLZ0    ((int)sizeof(unsigned long)*CHAR_BIT)
#  define EC_CLZ(_x) (__builtin_clzl(_x))
# endif
#endif

#if defined(EC_CLZ)
/*Note that __builtin_clz is not defined when _x==0, according to the gcc
   documentation (and that of the BSR instruction that implements it on x86).
  The majority of the time we can never pass it zero.
  When we need to, it can be special cased.*/
# define EC_ILOG(_x) (EC_CLZ0-EC_CLZ(_x))
#else
int ec_ilog(opus_uint32 _v);
# define EC_ILOG(_x) (ec_ilog(_x))
#endif
#endif
