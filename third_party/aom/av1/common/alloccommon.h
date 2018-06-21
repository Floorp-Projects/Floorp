/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AV1_COMMON_ALLOCCOMMON_H_
#define AV1_COMMON_ALLOCCOMMON_H_

#define INVALID_IDX -1  // Invalid buffer index.

#ifdef __cplusplus
extern "C" {
#endif

struct AV1Common;
struct BufferPool;

void av1_remove_common(struct AV1Common *cm);

int av1_alloc_above_context_buffers(struct AV1Common *cm,
                                    int num_alloc_above_contexts);
void av1_free_above_context_buffers(struct AV1Common *cm,
                                    int num_free_above_contexts);
int av1_alloc_context_buffers(struct AV1Common *cm, int width, int height);
void av1_init_context_buffers(struct AV1Common *cm);
void av1_free_context_buffers(struct AV1Common *cm);

void av1_free_ref_frame_buffers(struct BufferPool *pool);
void av1_alloc_restoration_buffers(struct AV1Common *cm);
void av1_free_restoration_buffers(struct AV1Common *cm);

int av1_alloc_state_buffers(struct AV1Common *cm, int width, int height);
void av1_free_state_buffers(struct AV1Common *cm);

void av1_set_mb_mi(struct AV1Common *cm, int width, int height);
int av1_get_MBs(int width, int height);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_ALLOCCOMMON_H_
