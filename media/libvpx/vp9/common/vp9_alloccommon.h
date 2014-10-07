/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP9_COMMON_VP9_ALLOCCOMMON_H_
#define VP9_COMMON_VP9_ALLOCCOMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

struct VP9Common;

void vp9_remove_common(struct VP9Common *cm);

int vp9_alloc_context_buffers(struct VP9Common *cm, int width, int height);
void vp9_init_context_buffers(struct VP9Common *cm);
void vp9_free_context_buffers(struct VP9Common *cm);

int vp9_alloc_ref_frame_buffers(struct VP9Common *cm, int width, int height);
void vp9_free_ref_frame_buffers(struct VP9Common *cm);

int vp9_alloc_state_buffers(struct VP9Common *cm, int width, int height);
void vp9_free_state_buffers(struct VP9Common *cm);

void vp9_set_mb_mi(struct VP9Common *cm, int width, int height);
void vp9_swap_mi_and_prev_mi(struct VP9Common *cm);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_COMMON_VP9_ALLOCCOMMON_H_
