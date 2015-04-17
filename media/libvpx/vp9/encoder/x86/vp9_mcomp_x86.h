/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP9_ENCODER_X86_VP9_MCOMP_X86_H_
#define VP9_ENCODER_X86_VP9_MCOMP_X86_H_

#ifdef __cplusplus
extern "C" {
#endif

#if HAVE_SSE3
#if !CONFIG_RUNTIME_CPU_DETECT

#undef  vp9_search_full_search
#define vp9_search_full_search vp9_full_search_sadx3

#undef  vp9_search_refining_search
#define vp9_search_refining_search vp9_refining_search_sadx4

#undef  vp9_search_diamond_search
#define vp9_search_diamond_search vp9_diamond_search_sadx4

#endif
#endif

#if HAVE_SSE4_1
#if !CONFIG_RUNTIME_CPU_DETECT

#undef  vp9_search_full_search
#define vp9_search_full_search vp9_full_search_sadx8

#endif
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_X86_VP9_MCOMP_X86_H_

