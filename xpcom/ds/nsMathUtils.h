/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathUtils_h__
#define nsMathUtils_h__

#define _USE_MATH_DEFINES /* needed for M_ constants on Win32 */

#include "nscore.h"
#include <cmath>
#include <float.h>

#ifdef SOLARIS
#include <ieeefp.h>
#endif

/*
 * round
 */
inline double
NS_round(double aNum)
{
  return aNum >= 0.0 ? floor(aNum + 0.5) : ceil(aNum - 0.5);
}
inline float
NS_roundf(float aNum)
{
  return aNum >= 0.0f ? floorf(aNum + 0.5f) : ceilf(aNum - 0.5f);
}
inline int32_t
NS_lround(double aNum)
{
  return aNum >= 0.0 ? int32_t(aNum + 0.5) : int32_t(aNum - 0.5);
}

/* NS_roundup30 rounds towards infinity for positive and       */
/* negative numbers.                                           */

#if defined(XP_WIN32) && defined(_M_IX86) && !defined(__GNUC__) && !defined(__clang__)
inline int32_t NS_lroundup30(float x)
{
  /* Code derived from Laurent de Soras' paper at             */
  /* http://ldesoras.free.fr/doc/articles/rounding_en.pdf     */

  /* Rounding up on Windows is expensive using the float to   */
  /* int conversion and the floor function. A faster          */
  /* approach is to use f87 rounding while assuming the       */
  /* default rounding mode of rounding to the nearest         */
  /* integer. This rounding mode, however, actually rounds    */
  /* to the nearest integer so we add the floating point      */
  /* number to itself and add our rounding factor before      */
  /* doing the conversion to an integer. We then do a right   */
  /* shift of one bit on the integer to divide by two.        */

  /* This routine doesn't handle numbers larger in magnitude  */
  /* than 2^30 but this is fine for NSToCoordRound because    */
  /* Coords are limited to 2^30 in magnitude.                 */

  static const double round_to_nearest = 0.5f;
  int i;

  __asm {
    fld     x                   ; load fp argument
    fadd    st, st(0)           ; double it
    fadd    round_to_nearest    ; add the rounding factor
    fistp   dword ptr i         ; convert the result to int
  }
  return i >> 1;                /* divide by 2 */
}
#endif /* XP_WIN32 && _M_IX86 && !__GNUC__ */

inline int32_t
NS_lroundf(float aNum)
{
  return aNum >= 0.0f ? int32_t(aNum + 0.5f) : int32_t(aNum - 0.5f);
}

/*
 * hypot.  We don't need a super accurate version of this, if a platform
 * turns up with none of the possibilities below it would be okay to fall
 * back to sqrt(x*x + y*y).
 */
inline double
NS_hypot(double aNum1, double aNum2)
{
#ifdef __GNUC__
  return __builtin_hypot(aNum1, aNum2);
#elif defined _WIN32
  return _hypot(aNum1, aNum2);
#else
  return hypot(aNum1, aNum2);
#endif
}

/**
 * Check whether a floating point number is finite (not +/-infinity and not a
 * NaN value).
 */
inline bool
NS_finite(double aNum)
{
#ifdef WIN32
  // NOTE: '!!' casts an int to bool without spamming MSVC warning C4800.
  return !!_finite(aNum);
#elif defined(XP_DARWIN)
  // Darwin has deprecated |finite| and recommends |isfinite|. The former is
  // not present in the iOS SDK.
  return std::isfinite(aNum);
#else
  return finite(aNum);
#endif
}

/**
 * Returns the result of the modulo of x by y using a floored division.
 * fmod(x, y) is using a truncated division.
 * The main difference is that the result of this method will have the sign of
 * y while the result of fmod(x, y) will have the sign of x.
 */
inline double
NS_floorModulo(double aNum1, double aNum2)
{
  return (aNum1 - aNum2 * floor(aNum1 / aNum2));
}

#endif
