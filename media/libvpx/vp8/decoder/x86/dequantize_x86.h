/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license 
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may 
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef DEQUANTIZE_X86_H
#define DEQUANTIZE_X86_H


/* Note:
 *
 * This platform is commonly built for runtime CPU detection. If you modify
 * any of the function mappings present in this file, be sure to also update
 * them in the function pointer initialization code
 */
#if HAVE_MMX
extern prototype_dequant_block(vp8_dequantize_b_mmx);
extern prototype_dequant_idct(vp8_dequant_idct_mmx);
extern prototype_dequant_idct_dc(vp8_dequant_dc_idct_mmx);


#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_dequant_block
#define vp8_dequant_block vp8_dequantize_b_mmx

#undef  vp8_dequant_idct
#define vp8_dequant_idct vp8_dequant_idct_mmx

#undef  vp8_dequant_idct_dc
#define vp8_dequant_idct_dc vp8_dequant_dc_idct_mmx

#endif
#endif

#endif
