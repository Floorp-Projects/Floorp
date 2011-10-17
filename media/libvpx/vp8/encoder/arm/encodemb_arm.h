/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef ENCODEMB_ARM_H
#define ENCODEMB_ARM_H

#if HAVE_ARMV6
extern prototype_subb(vp8_subtract_b_armv6);
extern prototype_submby(vp8_subtract_mby_armv6);
extern prototype_submbuv(vp8_subtract_mbuv_armv6);

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_encodemb_subb
#define vp8_encodemb_subb vp8_subtract_b_armv6

#undef  vp8_encodemb_submby
#define vp8_encodemb_submby vp8_subtract_mby_armv6

#undef  vp8_encodemb_submbuv
#define vp8_encodemb_submbuv vp8_subtract_mbuv_armv6
#endif

#endif /* HAVE_ARMV6 */

#if HAVE_ARMV7
//extern prototype_berr(vp8_block_error_c);
//extern prototype_mberr(vp8_mbblock_error_c);
//extern prototype_mbuverr(vp8_mbuverror_c);

extern prototype_subb(vp8_subtract_b_neon);
extern prototype_submby(vp8_subtract_mby_neon);
extern prototype_submbuv(vp8_subtract_mbuv_neon);

//#undef  vp8_encodemb_berr
//#define vp8_encodemb_berr vp8_block_error_c

//#undef  vp8_encodemb_mberr
//#define vp8_encodemb_mberr vp8_mbblock_error_c

//#undef  vp8_encodemb_mbuverr
//#define vp8_encodemb_mbuverr vp8_mbuverror_c

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_encodemb_subb
#define vp8_encodemb_subb vp8_subtract_b_neon

#undef  vp8_encodemb_submby
#define vp8_encodemb_submby vp8_subtract_mby_neon

#undef  vp8_encodemb_submbuv
#define vp8_encodemb_submbuv vp8_subtract_mbuv_neon
#endif

#endif

#endif
