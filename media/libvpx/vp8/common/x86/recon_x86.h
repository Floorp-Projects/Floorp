/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef RECON_X86_H
#define RECON_X86_H

/* Note:
 *
 * This platform is commonly built for runtime CPU detection. If you modify
 * any of the function mappings present in this file, be sure to also update
 * them in the function pointer initialization code
 */

#if HAVE_MMX
extern prototype_copy_block(vp8_copy_mem8x8_mmx);
extern prototype_copy_block(vp8_copy_mem8x4_mmx);
extern prototype_copy_block(vp8_copy_mem16x16_mmx);


#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_recon_copy8x8
#define vp8_recon_copy8x8 vp8_copy_mem8x8_mmx

#undef  vp8_recon_copy8x4
#define vp8_recon_copy8x4 vp8_copy_mem8x4_mmx

#undef  vp8_recon_copy16x16
#define vp8_recon_copy16x16 vp8_copy_mem16x16_mmx

#endif
#endif

#if HAVE_SSE2
extern prototype_copy_block(vp8_copy_mem16x16_sse2);
extern prototype_build_intra_predictors(vp8_build_intra_predictors_mbuv_sse2);
extern prototype_build_intra_predictors(vp8_build_intra_predictors_mbuv_s_sse2);
extern prototype_build_intra_predictors(vp8_build_intra_predictors_mby_sse2);
extern prototype_build_intra_predictors(vp8_build_intra_predictors_mby_s_sse2);

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_recon_copy16x16
#define vp8_recon_copy16x16 vp8_copy_mem16x16_sse2

#undef  vp8_recon_build_intra_predictors_mbuv
#define vp8_recon_build_intra_predictors_mbuv vp8_build_intra_predictors_mbuv_sse2

#undef  vp8_recon_build_intra_predictors_mbuv_s
#define vp8_recon_build_intra_predictors_mbuv_s vp8_build_intra_predictors_mbuv_s_sse2

#undef  vp8_recon_build_intra_predictors_mby
#define vp8_recon_build_intra_predictors_mby vp8_build_intra_predictors_mby_sse2

#undef  vp8_recon_build_intra_predictors_mby_s
#define vp8_recon_build_intra_predictors_mby_s vp8_build_intra_predictors_mby_s_sse2

#endif
#endif

#if HAVE_SSSE3
extern prototype_build_intra_predictors(vp8_build_intra_predictors_mbuv_ssse3);
extern prototype_build_intra_predictors(vp8_build_intra_predictors_mbuv_s_ssse3);
extern prototype_build_intra_predictors(vp8_build_intra_predictors_mby_ssse3);
extern prototype_build_intra_predictors(vp8_build_intra_predictors_mby_s_ssse3);

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_recon_build_intra_predictors_mbuv
#define vp8_recon_build_intra_predictors_mbuv vp8_build_intra_predictors_mbuv_ssse3

#undef  vp8_recon_build_intra_predictors_mbuv_s
#define vp8_recon_build_intra_predictors_mbuv_s vp8_build_intra_predictors_mbuv_s_ssse3

#undef  vp8_recon_build_intra_predictors_mby
#define vp8_recon_build_intra_predictors_mby vp8_build_intra_predictors_mby_ssse3

#undef  vp8_recon_build_intra_predictors_mby_s
#define vp8_recon_build_intra_predictors_mby_s vp8_build_intra_predictors_mby_s_ssse3

#endif
#endif
#endif
