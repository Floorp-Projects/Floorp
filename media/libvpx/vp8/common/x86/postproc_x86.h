/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef POSTPROC_X86_H
#define POSTPROC_X86_H

/* Note:
 *
 * This platform is commonly built for runtime CPU detection. If you modify
 * any of the function mappings present in this file, be sure to also update
 * them in the function pointer initialization code
 */

#if HAVE_MMX
extern prototype_postproc_inplace(vp8_mbpost_proc_down_mmx);
extern prototype_postproc(vp8_post_proc_down_and_across_mmx);
extern prototype_postproc_addnoise(vp8_plane_add_noise_mmx);

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_postproc_down
#define vp8_postproc_down vp8_mbpost_proc_down_mmx

#undef  vp8_postproc_downacross
#define vp8_postproc_downacross vp8_post_proc_down_and_across_mmx

#undef  vp8_postproc_addnoise
#define vp8_postproc_addnoise vp8_plane_add_noise_mmx

#endif
#endif


#if HAVE_SSE2
extern prototype_postproc_inplace(vp8_mbpost_proc_down_xmm);
extern prototype_postproc_inplace(vp8_mbpost_proc_across_ip_xmm);
extern prototype_postproc(vp8_post_proc_down_and_across_xmm);
extern prototype_postproc_addnoise(vp8_plane_add_noise_wmt);

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_postproc_down
#define vp8_postproc_down vp8_mbpost_proc_down_xmm

#undef  vp8_postproc_across
#define vp8_postproc_across vp8_mbpost_proc_across_ip_xmm

#undef  vp8_postproc_downacross
#define vp8_postproc_downacross vp8_post_proc_down_and_across_xmm

#undef  vp8_postproc_addnoise
#define vp8_postproc_addnoise vp8_plane_add_noise_wmt


#endif
#endif

#endif
