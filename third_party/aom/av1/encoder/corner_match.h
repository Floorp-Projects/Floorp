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
#ifndef AV1_ENCODER_CORNER_MATCH_H_
#define AV1_ENCODER_CORNER_MATCH_H_

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#define MATCH_SZ 13
#define MATCH_SZ_BY2 ((MATCH_SZ - 1) / 2)
#define MATCH_SZ_SQ (MATCH_SZ * MATCH_SZ)

typedef struct {
  int x, y;
  int rx, ry;
} Correspondence;

int determine_correspondence(unsigned char *frm, int *frm_corners,
                             int num_frm_corners, unsigned char *ref,
                             int *ref_corners, int num_ref_corners, int width,
                             int height, int frm_stride, int ref_stride,
                             int *correspondence_pts);

#endif  // AV1_ENCODER_CORNER_MATCH_H_
