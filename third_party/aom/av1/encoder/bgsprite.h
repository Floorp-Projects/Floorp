/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AV1_ENCODER_BGSPRITE_H_
#define AV1_ENCODER_BGSPRITE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "av1/encoder/encoder.h"

// Creates alternate reference frame staring from source image + frames up to
// 'distance' past source frame.
// Returns 0 on success and 1 on failure.
int av1_background_sprite(AV1_COMP *cpi, int distance);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_BGSPRITE_H_
