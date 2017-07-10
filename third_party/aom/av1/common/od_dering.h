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

#if !defined(_dering_H)
#define _dering_H (1)

#include "odintrin.h"

#define OD_DERING_NBLOCKS (MAX_SB_SIZE / 8)

/* We need to buffer three vertical lines. */
#define OD_FILT_VBORDER (3)
/* We only need to buffer three horizontal pixels too, but let's align to
   16 bytes (8 x 16 bits) to make vectorization easier. */
#define OD_FILT_HBORDER (8)
#define OD_FILT_BSTRIDE ALIGN_POWER_OF_TWO(MAX_SB_SIZE + 2 * OD_FILT_HBORDER, 3)

#define OD_DERING_VERY_LARGE (30000)
#define OD_DERING_INBUF_SIZE \
  (OD_FILT_BSTRIDE * (MAX_SB_SIZE + 2 * OD_FILT_VBORDER))

extern const int OD_DIRECTION_OFFSETS_TABLE[8][3];

typedef struct {
  uint8_t by;
  uint8_t bx;
  uint8_t skip;
} dering_list;

typedef void (*od_filter_dering_direction_func)(uint16_t *y, int ystride,
                                                const uint16_t *in,
                                                int threshold, int dir,
                                                int damping);

int get_filter_skip(int level);

void od_dering(uint8_t *dst, int dstride, uint16_t *y, uint16_t *in, int xdec,
               int ydec, int dir[OD_DERING_NBLOCKS][OD_DERING_NBLOCKS],
               int *dirinit, int var[OD_DERING_NBLOCKS][OD_DERING_NBLOCKS],
               int pli, dering_list *dlist, int dering_count, int level,
               int clpf_strength, int clpf_damping, int dering_damping,
               int coeff_shift, int skip_dering, int hbd);
#endif
