/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef IDCT_ARM_H
#define IDCT_ARM_H

#if HAVE_ARMV6
extern prototype_idct(vp8_short_idct4x4llm_v6_dual);
extern prototype_idct_scalar_add(vp8_dc_only_idct_add_v6);
extern prototype_second_order(vp8_short_inv_walsh4x4_1_v6);
extern prototype_second_order(vp8_short_inv_walsh4x4_v6);

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_idct_idct16
#define vp8_idct_idct16 vp8_short_idct4x4llm_v6_dual

#undef  vp8_idct_idct1_scalar_add
#define vp8_idct_idct1_scalar_add vp8_dc_only_idct_add_v6

#undef  vp8_idct_iwalsh16
#define vp8_idct_iwalsh16 vp8_short_inv_walsh4x4_v6
#endif
#endif

#if HAVE_ARMV7
extern prototype_idct(vp8_short_idct4x4llm_neon);
extern prototype_idct_scalar_add(vp8_dc_only_idct_add_neon);
extern prototype_second_order(vp8_short_inv_walsh4x4_1_neon);
extern prototype_second_order(vp8_short_inv_walsh4x4_neon);

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_idct_idct16
#define vp8_idct_idct16 vp8_short_idct4x4llm_neon

#undef  vp8_idct_idct1_scalar_add
#define vp8_idct_idct1_scalar_add vp8_dc_only_idct_add_neon

#undef  vp8_idct_iwalsh16
#define vp8_idct_iwalsh16 vp8_short_inv_walsh4x4_neon
#endif
#endif

#endif
