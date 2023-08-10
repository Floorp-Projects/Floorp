/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is intended for use only with vendored C libraries where editing the
 * source files to explicitly use fdblim is not desirable due to maintenance
 * burden.  cmath for C++ is not supported.
 *
 * Use LOCAL_INCLUDES += ['/modules/fdlibm/inexact-math-override'] to
 * substitute fdlibm math functions for inexact functions in system libraries.
 *
 * This is intended to remove fingerprinting entropy due to variations in
 * return values from system libaries.  Exact functions are not substituted
 * because these are expected to return consistent results regardless of
 * implementation.  sqrt() is considered exact as correct rounding is required
 * by IEEE-754.
 */

#ifndef MODULES_FDLIBM_INEXACT_MATH_OVERRIDE_MATH_H_
#define MODULES_FDLIBM_INEXACT_MATH_OVERRIDE_MATH_H_

#include_next <math.h>

#include <fdlibm.h>

#define acos fdlibm_acos
#define acosf fdlibm_acosf
#define asin fdlibm_asin
#define asinf fdlibm_asinf
#define atan fdlibm_atan
#define atanf fdlibm_atanf
#define atan2 fdlibm_atan2
#define cos fdlibm_cos
#define cosf fdlibm_cosf
#define sin fdlibm_sin
#define sinf fdlibm_sinf
#define tan fdlibm_tan
#define tanf fdlibm_tanf
#define cosh fdlibm_cosh
#define sinh fdlibm_sinh
#define tanh fdlibm_tanh
#define exp fdlibm_exp
#define expf fdlibm_expf
#define exp2 fdlibm_exp2
#define exp2f fdlibm_exp2f
#define log fdlibm_log
#define logf fdlibm_logf
#define log10 fdlibm_log10
#define log10f fdlibm_log10f
#define pow fdlibm_pow
#define powf fdlibm_powf
#define acosh fdlibm_acosh
#define asinh fdlibm_asinh
#define atanh fdlibm_atanh
#define cbrt fdlibm_cbrt
#define expm1 fdlibm_expm1
#define hypot fdlibm_hypot
#define hypotf fdlibm_hypotf
#define log1p fdlibm_log1p
#define log2 fdlibm_log2

// These functions are not yet vendored from fdlibm but are defined here to
// catch any uses that would imply that they should be vendored.
#define coshf fdlibm_coshf
#define sinhf fdlibm_sinhf
#define tanhf fdlibm_tanhf
#define acoshf fdlibm_acoshf
#define asinhf fdlibm_asinhf
#define atanhf fdlibm_atanhf
#define cbrtf fdlibm_cbrtf
#define expm1f fdlibm_expm1f
#define log1pf fdlibm_log1pf
#define log2f fdlibm_log2f
#define erf fdlibm_erf
#define erff fdlibm_erff
#define erfc fdlibm_erfc
#define erfcf fdlibm_erfcf
#define tgamma fdlibm_tgamma
#define tgammaf fdlibm_tgammaf
#define lgamma fdlibm_lgamma
#define lgammaf fdlibm_lgammaf

#endif  // MODULES_FDLIBM_INEXACT_MATH_OVERRIDE_MATH_H_
