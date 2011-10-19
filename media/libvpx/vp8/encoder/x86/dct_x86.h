/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef DCT_X86_H
#define DCT_X86_H


/* Note:
 *
 * This platform is commonly built for runtime CPU detection. If you modify
 * any of the function mappings present in this file, be sure to also update
 * them in the function pointer initialization code
 */
#if HAVE_MMX
extern prototype_fdct(vp8_short_fdct4x4_mmx);
extern prototype_fdct(vp8_short_fdct8x4_mmx);

#if !CONFIG_RUNTIME_CPU_DETECT

#undef  vp8_fdct_short4x4
#define vp8_fdct_short4x4 vp8_short_fdct4x4_mmx

#undef  vp8_fdct_short8x4
#define vp8_fdct_short8x4 vp8_short_fdct8x4_mmx

#undef  vp8_fdct_fast4x4
#define vp8_fdct_fast4x4 vp8_short_fdct4x4_mmx

#undef  vp8_fdct_fast8x4
#define vp8_fdct_fast8x4 vp8_short_fdct8x4_mmx

#endif

#endif


#if HAVE_SSE2
extern prototype_fdct(vp8_short_fdct8x4_sse2);
extern prototype_fdct(vp8_short_walsh4x4_sse2);

extern prototype_fdct(vp8_short_fdct4x4_sse2);

#if !CONFIG_RUNTIME_CPU_DETECT

#undef  vp8_fdct_short4x4
#define vp8_fdct_short4x4 vp8_short_fdct4x4_sse2

#undef  vp8_fdct_short8x4
#define vp8_fdct_short8x4 vp8_short_fdct8x4_sse2

#undef  vp8_fdct_fast4x4
#define vp8_fdct_fast4x4 vp8_short_fdct4x4_sse2

#undef  vp8_fdct_fast8x4
#define vp8_fdct_fast8x4 vp8_short_fdct8x4_sse2

#undef  vp8_fdct_walsh_short4x4
#define vp8_fdct_walsh_short4x4  vp8_short_walsh4x4_sse2

#endif


#endif

#endif
