/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license and patent
 *  grant that can be found in the LICENSE file in the root of the source
 *  tree. All contributing project authors may be found in the AUTHORS
 *  file in the root of the source tree.
 */

#ifndef QUANTIZE_X86_H
#define QUANTIZE_X86_H


/* Note:
 *
 * This platform is commonly built for runtime CPU detection. If you modify
 * any of the function mappings present in this file, be sure to also update
 * them in the function pointer initialization code
 */
#if HAVE_MMX

#endif /* HAVE_MMX */


#if HAVE_SSE2
extern prototype_quantize_block(vp8_regular_quantize_b_sse2);
extern prototype_quantize_block(vp8_fast_quantize_b_sse2);

#if !CONFIG_RUNTIME_CPU_DETECT

#undef vp8_quantize_quantb
#define vp8_quantize_quantb vp8_regular_quantize_b_sse2

#undef vp8_quantize_fastquantb
#define vp8_quantize_fastquantb vp8_fast_quantize_b_sse2

#endif /* !CONFIG_RUNTIME_CPU_DETECT */

#endif /* HAVE_SSE2 */


#if HAVE_SSSE3
extern prototype_quantize_block(vp8_fast_quantize_b_ssse3);

#if !CONFIG_RUNTIME_CPU_DETECT

#undef vp8_quantize_fastquantb
#define vp8_quantize_fastquantb vp8_fast_quantize_b_ssse3

#endif /* !CONFIG_RUNTIME_CPU_DETECT */

#endif /* HAVE_SSSE3 */


#if HAVE_SSE4_1
extern prototype_quantize_block(vp8_regular_quantize_b_sse4);

#if !CONFIG_RUNTIME_CPU_DETECT

#undef vp8_quantize_quantb
#define vp8_quantize_quantb vp8_regular_quantize_b_sse4

#endif /* !CONFIG_RUNTIME_CPU_DETECT */

#endif /* HAVE_SSE4_1 */

#endif /* QUANTIZE_X86_H */
