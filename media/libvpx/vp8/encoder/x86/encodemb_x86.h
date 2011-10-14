/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef ENCODEMB_X86_H
#define ENCODEMB_X86_H


/* Note:
 *
 * This platform is commonly built for runtime CPU detection. If you modify
 * any of the function mappings present in this file, be sure to also update
 * them in the function pointer initialization code
 */
#if HAVE_MMX
extern prototype_berr(vp8_block_error_mmx);
extern prototype_mberr(vp8_mbblock_error_mmx);
extern prototype_mbuverr(vp8_mbuverror_mmx);
extern prototype_subb(vp8_subtract_b_mmx);
extern prototype_submby(vp8_subtract_mby_mmx);
extern prototype_submbuv(vp8_subtract_mbuv_mmx);


#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_encodemb_berr
#define vp8_encodemb_berr vp8_block_error_mmx

#undef  vp8_encodemb_mberr
#define vp8_encodemb_mberr vp8_mbblock_error_mmx

#undef  vp8_encodemb_mbuverr
#define vp8_encodemb_mbuverr vp8_mbuverror_mmx

#undef  vp8_encodemb_subb
#define vp8_encodemb_subb vp8_subtract_b_mmx

#undef  vp8_encodemb_submby
#define vp8_encodemb_submby vp8_subtract_mby_mmx

#undef  vp8_encodemb_submbuv
#define vp8_encodemb_submbuv vp8_subtract_mbuv_mmx

#endif
#endif


#if HAVE_SSE2
extern prototype_berr(vp8_block_error_xmm);
extern prototype_mberr(vp8_mbblock_error_xmm);
extern prototype_mbuverr(vp8_mbuverror_xmm);
extern prototype_subb(vp8_subtract_b_sse2);
extern prototype_submby(vp8_subtract_mby_sse2);
extern prototype_submbuv(vp8_subtract_mbuv_sse2);

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_encodemb_berr
#define vp8_encodemb_berr vp8_block_error_xmm

#undef  vp8_encodemb_mberr
#define vp8_encodemb_mberr vp8_mbblock_error_xmm

#undef  vp8_encodemb_mbuverr
#define vp8_encodemb_mbuverr vp8_mbuverror_xmm

#undef  vp8_encodemb_subb
#define vp8_encodemb_subb vp8_subtract_b_sse2

#undef  vp8_encodemb_submby
#define vp8_encodemb_submby vp8_subtract_mby_sse2

#undef  vp8_encodemb_submbuv
#define vp8_encodemb_submbuv vp8_subtract_mbuv_sse2

#endif
#endif


#endif
