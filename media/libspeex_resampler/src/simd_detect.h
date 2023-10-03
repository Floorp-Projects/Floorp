/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef simd_detect_h
#define simd_detect_h

#include "speex_resampler.h"
#include "arch.h"

#ifdef __cplusplus
extern "C" {
#endif

int moz_speex_have_single_simd();
int moz_speex_have_double_simd();

#if defined(USE_SSE) || defined(USE_NEON)
#define OVERRIDE_INNER_PRODUCT_SINGLE
#define inner_product_single CAT_PREFIX(RANDOM_PREFIX,_inner_product_single)
spx_word32_t inner_product_single(const spx_word16_t *a, const spx_word16_t *b, unsigned int len);
#endif
#if defined(USE_SSE)
#define OVERRIDE_INTERPOLATE_PRODUCT_SINGLE
#define interpolate_product_single CAT_PREFIX(RANDOM_PREFIX,_interpolate_product_single)
spx_word32_t interpolate_product_single(const spx_word16_t *a, const spx_word16_t *b, unsigned int len, const spx_uint32_t oversample, float *frac);
#endif

#if defined(USE_SSE2)
#define OVERRIDE_INNER_PRODUCT_DOUBLE
#define inner_product_double CAT_PREFIX(RANDOM_PREFIX,_inner_product_double)
double inner_product_double(const float *a, const float *b, unsigned int len);
#define OVERRIDE_INTERPOLATE_PRODUCT_DOUBLE
#define interpolate_product_double CAT_PREFIX(RANDOM_PREFIX,_interpolate_product_double)
double interpolate_product_double(const float *a, const float *b, unsigned int len, const spx_uint32_t oversample, float *frac);
#endif

#ifdef __cplusplus
}
#endif

#endif // simd_detect_h
