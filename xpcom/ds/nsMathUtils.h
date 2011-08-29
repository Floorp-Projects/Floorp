/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
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

#ifndef nsMathUtils_h__
#define nsMathUtils_h__

#define _USE_MATH_DEFINES /* needed for M_ constants on Win32 */

#include "nscore.h"
#include <math.h>
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
inline NS_HIDDEN_(PRInt32) NS_lround(double x)
{
    return x >= 0.0 ? PRInt32(x + 0.5) : PRInt32(x - 0.5);
}

/* NS_roundup30 rounds towards infinity for positive and       */
/* negative numbers.                                           */

#if defined(XP_WIN32) && defined(_M_IX86) && !defined(__GNUC__)
inline NS_HIDDEN_(PRInt32) NS_lroundup30(float x)
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

inline NS_HIDDEN_(PRInt32) NS_lroundf(float x)
{
    return x >= 0.0f ? PRInt32(x + 0.5f) : PRInt32(x - 0.5f);
}

/*
 * hypot.  We don't need a super accurate version of this, if a platform
 * turns up with none of the possibilities below it would be okay to fall
 * back to sqrt(x*x + y*y).
 */
inline NS_HIDDEN_(double) NS_hypot(double x, double y)
{
#if __GNUC__ >= 4
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
    return isfinite(d);
#else
    return finite(d);
#endif
}

#endif
