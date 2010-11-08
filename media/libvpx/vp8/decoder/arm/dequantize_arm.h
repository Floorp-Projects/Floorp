/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef DEQUANTIZE_ARM_H
#define DEQUANTIZE_ARM_H

#if HAVE_ARMV6
extern prototype_dequant_block(vp8_dequantize_b_v6);
extern prototype_dequant_idct_add(vp8_dequant_idct_add_v6);
extern prototype_dequant_dc_idct_add(vp8_dequant_dc_idct_add_v6);
extern prototype_dequant_dc_idct_add_y_block(vp8_dequant_dc_idct_add_y_block_v6);
extern prototype_dequant_idct_add_y_block(vp8_dequant_idct_add_y_block_v6);
extern prototype_dequant_idct_add_uv_block(vp8_dequant_idct_add_uv_block_v6);

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_dequant_block
#define vp8_dequant_block vp8_dequantize_b_v6

#undef vp8_dequant_idct_add
#define vp8_dequant_idct_add vp8_dequant_idct_add_v6

#undef vp8_dequant_dc_idct_add
#define vp8_dequant_dc_idct_add vp8_dequant_dc_idct_add_v6

#undef vp8_dequant_dc_idct_add_y_block
#define vp8_dequant_dc_idct_add_y_block vp8_dequant_dc_idct_add_y_block_v6

#undef vp8_dequant_idct_add_y_block
#define vp8_dequant_idct_add_y_block vp8_dequant_idct_add_y_block_v6

#undef vp8_dequant_idct_add_uv_block
#define vp8_dequant_idct_add_uv_block vp8_dequant_idct_add_uv_block_v6
#endif
#endif

#if HAVE_ARMV7
extern prototype_dequant_block(vp8_dequantize_b_neon);
extern prototype_dequant_idct_add(vp8_dequant_idct_add_neon);
extern prototype_dequant_dc_idct_add(vp8_dequant_dc_idct_add_neon);
extern prototype_dequant_dc_idct_add_y_block(vp8_dequant_dc_idct_add_y_block_neon);
extern prototype_dequant_idct_add_y_block(vp8_dequant_idct_add_y_block_neon);
extern prototype_dequant_idct_add_uv_block(vp8_dequant_idct_add_uv_block_neon);

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_dequant_block
#define vp8_dequant_block vp8_dequantize_b_neon

#undef vp8_dequant_idct_add
#define vp8_dequant_idct_add vp8_dequant_idct_add_neon

#undef vp8_dequant_dc_idct_add
#define vp8_dequant_dc_idct_add vp8_dequant_dc_idct_add_neon

#undef vp8_dequant_dc_idct_add_y_block
#define vp8_dequant_dc_idct_add_y_block vp8_dequant_dc_idct_add_y_block_neon

#undef vp8_dequant_idct_add_y_block
#define vp8_dequant_idct_add_y_block vp8_dequant_idct_add_y_block_neon

#undef vp8_dequant_idct_add_uv_block
#define vp8_dequant_idct_add_uv_block vp8_dequant_idct_add_uv_block_neon
#endif
#endif

#endif
