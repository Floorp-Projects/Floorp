/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2020-2021 Evan Nemerson <evan@nemerson.com>
 */

/* Support for complex math.
 *
 * We try to avoid inculding <complex> (in C++ mode) since it pulls in
 * a *lot* of code.  Unfortunately this only works for GNU modes (i.e.,
 * -std=gnu++14 not -std=c++14) unless you pass -fext-numeric-literals,
 * but there is no way (AFAICT) to detect that flag so we have to rely
 * on __STRICT_ANSI__ to instead detect GNU mode.
 *
 * This header is separate from simde-math.h since there is a good
 * chance it will pull in <complex>, and most of the time we don't need
 * complex math (on x86 only SVML uses it). */

#if !defined(SIMDE_COMPLEX_H)
#define SIMDE_COMPLEX_H 1

#include "simde-math.h"

#if ( \
      HEDLEY_HAS_BUILTIN(__builtin_creal) || \
      HEDLEY_GCC_VERSION_CHECK(4,7,0) || \
      HEDLEY_INTEL_VERSION_CHECK(13,0,0) \
    ) && (!defined(__cplusplus) && !defined(__STRICT_ANSI__))
  HEDLEY_DIAGNOSTIC_PUSH
  SIMDE_DIAGNOSTIC_DISABLE_C99_EXTENSIONS_
    typedef __complex__ float simde_cfloat32;
    typedef __complex__ double simde_cfloat64;
  HEDLEY_DIAGNOSTIC_POP
  #define SIMDE_MATH_CMPLX(x, y) (HEDLEY_STATIC_CAST(double, x) + HEDLEY_STATIC_CAST(double, y) * (__extension__ 1.0j))
  #define SIMDE_MATH_CMPLXF(x, y) (HEDLEY_STATIC_CAST(float, x) + HEDLEY_STATIC_CAST(float, y) * (__extension__ 1.0fj))

  #if !defined(simde_math_creal)
    #define simde_math_crealf(z) __builtin_crealf(z)
  #endif
  #if !defined(simde_math_crealf)
    #define simde_math_creal(z) __builtin_creal(z)
  #endif
  #if !defined(simde_math_cimag)
    #define simde_math_cimagf(z) __builtin_cimagf(z)
  #endif
  #if !defined(simde_math_cimagf)
    #define simde_math_cimag(z) __builtin_cimag(z)
  #endif
  #if !defined(simde_math_cexp)
    #define simde_math_cexp(z) __builtin_cexp(z)
  #endif
  #if !defined(simde_math_cexpf)
    #define simde_math_cexpf(z) __builtin_cexpf(z)
  #endif
#elif !defined(__cplusplus)
  #include <complex.h>

  #if !defined(HEDLEY_MSVC_VERSION)
    typedef float _Complex simde_cfloat32;
    typedef double _Complex simde_cfloat64;
  #else
    typedef _Fcomplex simde_cfloat32;
    typedef _Dcomplex simde_cfloat64;
  #endif

  #if defined(HEDLEY_MSVC_VERSION)
    #define SIMDE_MATH_CMPLX(x, y) ((simde_cfloat64) { (x), (y) })
    #define SIMDE_MATH_CMPLXF(x, y) ((simde_cfloat32) { (x), (y) })
  #elif defined(CMPLX) && defined(CMPLXF)
    #define SIMDE_MATH_CMPLX(x, y) CMPLX(x, y)
    #define SIMDE_MATH_CMPLXF(x, y) CMPLXF(x, y)
  #else
    #define SIMDE_MATH_CMPLX(x, y) (HEDLEY_STATIC_CAST(double, x) + HEDLEY_STATIC_CAST(double, y) * I)
    #define SIMDE_MATH_CMPLXF(x, y) (HEDLEY_STATIC_CAST(float, x) + HEDLEY_STATIC_CAST(float, y) * I)
  #endif

  #if !defined(simde_math_creal)
    #define simde_math_creal(z) creal(z)
  #endif
  #if !defined(simde_math_crealf)
    #define simde_math_crealf(z) crealf(z)
  #endif
  #if !defined(simde_math_cimag)
    #define simde_math_cimag(z) cimag(z)
  #endif
  #if !defined(simde_math_cimagf)
    #define simde_math_cimagf(z) cimagf(z)
  #endif
  #if !defined(simde_math_cexp)
    #define simde_math_cexp(z) cexp(z)
  #endif
  #if !defined(simde_math_cexpf)
    #define simde_math_cexpf(z) cexpf(z)
  #endif
#else
  HEDLEY_DIAGNOSTIC_PUSH
  #if defined(HEDLEY_MSVC_VERSION)
    #pragma warning(disable:4530)
  #endif
  #include <complex>
  HEDLEY_DIAGNOSTIC_POP

  typedef std::complex<float> simde_cfloat32;
  typedef std::complex<double> simde_cfloat64;
  #define SIMDE_MATH_CMPLX(x, y) (std::complex<double>(x, y))
  #define SIMDE_MATH_CMPLXF(x, y) (std::complex<float>(x, y))

  #if !defined(simde_math_creal)
    #define simde_math_creal(z) ((z).real())
  #endif
  #if !defined(simde_math_crealf)
    #define simde_math_crealf(z) ((z).real())
  #endif
  #if !defined(simde_math_cimag)
    #define simde_math_cimag(z) ((z).imag())
  #endif
  #if !defined(simde_math_cimagf)
    #define simde_math_cimagf(z) ((z).imag())
  #endif
  #if !defined(simde_math_cexp)
    #define simde_math_cexp(z) std::exp(z)
  #endif
  #if !defined(simde_math_cexpf)
    #define simde_math_cexpf(z) std::exp(z)
  #endif
#endif

#endif /* !defined(SIMDE_COMPLEX_H) */
