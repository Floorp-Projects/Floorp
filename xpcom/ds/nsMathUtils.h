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
inline NS_HIDDEN_(double) NS_round(double x)
{
    return x >= 0.0 ? floor(x + 0.5) : ceil(x - 0.5);
}
inline NS_HIDDEN_(float) NS_roundf(float x)
{
    return x >= 0.0f ? floorf(x + 0.5f) : ceilf(x - 0.5f);
}
inline NS_HIDDEN_(int32_t) NS_lround(double x)
{
    return x >= 0.0 ? int32_t(x + 0.5) : int32_t(x - 0.5);
}

/* NS_roundup30 rounds towards infinity for positive and       */
/* negative numbers.                                           */

#if defined(XP_WIN32) && defined(_M_IX86) && !defined(__GNUC__)
inline NS_HIDDEN_(int32_t) NS_lroundup30(float x)
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

inline NS_HIDDEN_(int32_t) NS_lroundf(float x)
{
    return x >= 0.0f ? int32_t(x + 0.5f) : int32_t(x - 0.5f);
}

/*
 * hypot.  We don't need a super accurate version of this, if a platform
 * turns up with none of the possibilities below it would be okay to fall
 * back to sqrt(x*x + y*y).
 */
inline NS_HIDDEN_(double) NS_hypot(double x, double y)
{
#ifdef __GNUC__
    return __builtin_hypot(x, y);
#elif defined _WIN32
    return _hypot(x, y);
#else
    return hypot(x, y);
#endif
}

/**
 * Check whether a floating point number is finite (not +/-infinity and not a
 * NaN value).
 */
inline NS_HIDDEN_(bool) NS_finite(double d)
{
#ifdef WIN32
    // NOTE: '!!' casts an int to bool without spamming MSVC warning C4800.
    return !!_finite(d);
#elif defined(XP_DARWIN)
    // Darwin has deprecated |finite| and recommends |isfinite|. The former is
    // not present in the iOS SDK.
    return std::isfinite(d);
#else
    return finite(d);
#endif
}

/**
 * Returns the result of the modulo of x by y using a floored division.
 * fmod(x, y) is using a truncated division.
 * The main difference is that the result of this method will have the sign of
 * y while the result of fmod(x, y) will have the sign of x.
 */
inline NS_HIDDEN_(double) NS_floorModulo(double x, double y)
{
  return (x - y * floor(x / y));
}

#endif
