/**************************************************************************
 *
 * Copyright 2007-2015 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

/**
 * Wrapper for math.h which makes sure we have definitions of all the c99
 * functions.
 */


#ifndef _C99_MATH_H_
#define _C99_MATH_H_

#include <math.h>
#include "c99_compat.h"


/* This is to ensure that we get M_PI, etc. definitions */
#if defined(_MSC_VER) && !defined(_USE_MATH_DEFINES)
#error _USE_MATH_DEFINES define required when building with MSVC
#endif 


#if !defined(_MSC_VER) && \
    __STDC_VERSION__ < 199901L && \
    (!defined(_XOPEN_SOURCE) || _XOPEN_SOURCE < 600) && \
    !defined(__cplusplus)

static inline long int
lrint(double d)
{
   long int rounded = (long int)(d + 0.5);

   if (d - floor(d) == 0.5) {
      if (rounded % 2 != 0)
         rounded += (d > 0) ? -1 : 1;
   }

   return rounded;
}

static inline long int
lrintf(float f)
{
   long int rounded = (long int)(f + 0.5f);

   if (f - floorf(f) == 0.5f) {
      if (rounded % 2 != 0)
         rounded += (f > 0) ? -1 : 1;
   }

   return rounded;
}

static inline long long int
llrint(double d)
{
   long long int rounded = (long long int)(d + 0.5);

   if (d - floor(d) == 0.5) {
      if (rounded % 2 != 0)
         rounded += (d > 0) ? -1 : 1;
   }

   return rounded;
}

static inline long long int
llrintf(float f)
{
   long long int rounded = (long long int)(f + 0.5f);

   if (f - floorf(f) == 0.5f) {
      if (rounded % 2 != 0)
         rounded += (f > 0) ? -1 : 1;
   }

   return rounded;
}

static inline float
exp2f(float f)
{
   return powf(2.0f, f);
}

static inline double
exp2(double d)
{
   return pow(2.0, d);
}

#endif /* C99 */


/*
 * signbit() is a macro on Linux.  Not available on Windows.
 */
#ifndef signbit
#define signbit(x) ((x) < 0.0f)
#endif


#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

#ifndef M_E
#define M_E (2.7182818284590452354)
#endif

#ifndef M_LOG2E
#define M_LOG2E (1.4426950408889634074)
#endif

#ifndef FLT_MAX_EXP
#define FLT_MAX_EXP 128
#endif


#if defined(fpclassify)
/* ISO C99 says that fpclassify is a macro.  Assume that any implementation
 * of fpclassify, whether it's in a C99 compiler or not, will be a macro.
 */
#elif defined(__cplusplus)
/* For C++, fpclassify() should be defined in <cmath> */
#elif defined(_MSC_VER)
/* Not required on VS2013 and above.  Oddly, the fpclassify() function
 * doesn't exist in such a form on MSVC.  This is an implementation using
 * slightly different lower-level Windows functions.
 */
#include <float.h>

static inline enum {FP_NAN, FP_INFINITE, FP_ZERO, FP_SUBNORMAL, FP_NORMAL}
fpclassify(double x)
{
   switch(_fpclass(x)) {
   case _FPCLASS_SNAN: /* signaling NaN */
   case _FPCLASS_QNAN: /* quiet NaN */
      return FP_NAN;
   case _FPCLASS_NINF: /* negative infinity */
   case _FPCLASS_PINF: /* positive infinity */
      return FP_INFINITE;
   case _FPCLASS_NN:   /* negative normal */
   case _FPCLASS_PN:   /* positive normal */
      return FP_NORMAL;
   case _FPCLASS_ND:   /* negative denormalized */
   case _FPCLASS_PD:   /* positive denormalized */
      return FP_SUBNORMAL;
   case _FPCLASS_NZ:   /* negative zero */
   case _FPCLASS_PZ:   /* positive zero */
      return FP_ZERO;
   default:
      /* Should never get here; but if we do, this will guarantee
       * that the pattern is not treated like a number.
       */
      return FP_NAN;
   }
}
#else
#error "Need to include or define an fpclassify function"
#endif


/* Since C++11, the following functions are part of the std namespace. Their C
 * counteparts should still exist in the global namespace, however cmath
 * undefines those functions, which in glibc 2.23, are defined as macros rather
 * than functions as in glibc 2.22.
 */
#if __cplusplus >= 201103L && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 23))
#include <cmath>

using std::fpclassify;
using std::isfinite;
using std::isinf;
using std::isnan;
using std::isnormal;
using std::signbit;
using std::isgreater;
using std::isgreaterequal;
using std::isless;
using std::islessequal;
using std::islessgreater;
using std::isunordered;
#endif


#endif /* #define _C99_MATH_H_ */
