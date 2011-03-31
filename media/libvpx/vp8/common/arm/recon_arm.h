/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef RECON_ARM_H
#define RECON_ARM_H

#if HAVE_ARMV6
extern prototype_recon_block(vp8_recon_b_armv6);
extern prototype_recon_block(vp8_recon2b_armv6);
extern prototype_recon_block(vp8_recon4b_armv6);

extern prototype_copy_block(vp8_copy_mem8x8_v6);
extern prototype_copy_block(vp8_copy_mem8x4_v6);
extern prototype_copy_block(vp8_copy_mem16x16_v6);

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_recon_recon
#define vp8_recon_recon vp8_recon_b_armv6

#undef  vp8_recon_recon2
#define vp8_recon_recon2 vp8_recon2b_armv6

#undef  vp8_recon_recon4
#define vp8_recon_recon4 vp8_recon4b_armv6

#undef  vp8_recon_copy8x8
#define vp8_recon_copy8x8 vp8_copy_mem8x8_v6

#undef  vp8_recon_copy8x4
#define vp8_recon_copy8x4 vp8_copy_mem8x4_v6

#undef  vp8_recon_copy16x16
#define vp8_recon_copy16x16 vp8_copy_mem16x16_v6
#endif
#endif

#if HAVE_ARMV7
extern prototype_recon_block(vp8_recon_b_neon);
extern prototype_recon_block(vp8_recon2b_neon);
extern prototype_recon_block(vp8_recon4b_neon);

extern prototype_copy_block(vp8_copy_mem8x8_neon);
extern prototype_copy_block(vp8_copy_mem8x4_neon);
extern prototype_copy_block(vp8_copy_mem16x16_neon);

extern prototype_recon_macroblock(vp8_recon_mb_neon);

extern prototype_build_intra_predictors(vp8_build_intra_predictors_mby_neon);
extern prototype_build_intra_predictors(vp8_build_intra_predictors_mby_s_neon);

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_recon_recon
#define vp8_recon_recon vp8_recon_b_neon

#undef  vp8_recon_recon2
#define vp8_recon_recon2 vp8_recon2b_neon

#undef  vp8_recon_recon4
#define vp8_recon_recon4 vp8_recon4b_neon

#undef  vp8_recon_copy8x8
#define vp8_recon_copy8x8 vp8_copy_mem8x8_neon

#undef  vp8_recon_copy8x4
#define vp8_recon_copy8x4 vp8_copy_mem8x4_neon

#undef  vp8_recon_copy16x16
#define vp8_recon_copy16x16 vp8_copy_mem16x16_neon

#undef  vp8_recon_recon_mb
#define vp8_recon_recon_mb vp8_recon_mb_neon

#undef  vp8_recon_build_intra_predictors_mby
#define vp8_recon_build_intra_predictors_mby vp8_build_intra_predictors_mby_neon

#undef  vp8_recon_build_intra_predictors_mby_s
#define vp8_recon_build_intra_predictors_mby_s vp8_build_intra_predictors_mby_s_neon

#endif
#endif

#endif
