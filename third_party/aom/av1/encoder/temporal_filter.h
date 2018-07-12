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

#ifndef AV1_ENCODER_TEMPORAL_FILTER_H_
#define AV1_ENCODER_TEMPORAL_FILTER_H_

#ifdef __cplusplus
extern "C" {
#endif

void av1_temporal_filter(AV1_COMP *cpi,
#if CONFIG_BGSPRITE
                         YV12_BUFFER_CONFIG *bg, YV12_BUFFER_CONFIG *target,
#endif  // CONFIG_BGSPRITE
                         int distance);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_TEMPORAL_FILTER_H_
