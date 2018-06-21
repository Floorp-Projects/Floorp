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

#if !defined(_CDEF_BLOCK_H)
#define _CDEF_BLOCK_H (1)

#include "av1/common/odintrin.h"

#define CDEF_BLOCKSIZE 64
#define CDEF_BLOCKSIZE_LOG2 6
#define CDEF_NBLOCKS ((1 << MAX_SB_SIZE_LOG2) / 8)
#define CDEF_SB_SHIFT (MAX_SB_SIZE_LOG2 - CDEF_BLOCKSIZE_LOG2)

/* We need to buffer three vertical lines. */
#define CDEF_VBORDER (3)
/* We only need to buffer three horizontal pixels too, but let's align to
   16 bytes (8 x 16 bits) to make vectorization easier. */
#define CDEF_HBORDER (8)
#define CDEF_BSTRIDE \
  ALIGN_POWER_OF_TWO((1 << MAX_SB_SIZE_LOG2) + 2 * CDEF_HBORDER, 3)

#define CDEF_VERY_LARGE (30000)
#define CDEF_INBUF_SIZE \
  (CDEF_BSTRIDE * ((1 << MAX_SB_SIZE_LOG2) + 2 * CDEF_VBORDER))

extern const int cdef_pri_taps[2][2];
extern const int cdef_sec_taps[2][2];
DECLARE_ALIGNED(16, extern const int, cdef_directions[8][2]);

typedef struct {
  uint8_t by;
  uint8_t bx;
  uint8_t skip;
} cdef_list;

typedef void (*cdef_filter_block_func)(uint8_t *dst8, uint16_t *dst16,
                                       int dstride, const uint16_t *in,
                                       int pri_strength, int sec_strength,
                                       int dir, int pri_damping,
                                       int sec_damping, int bsize, int max,
                                       int coeff_shift);
void copy_cdef_16bit_to_16bit(uint16_t *dst, int dstride, uint16_t *src,
                              cdef_list *dlist, int cdef_count, int bsize);

void cdef_filter_fb(uint8_t *dst8, uint16_t *dst16, int dstride, uint16_t *in,
                    int xdec, int ydec, int dir[CDEF_NBLOCKS][CDEF_NBLOCKS],
                    int *dirinit, int var[CDEF_NBLOCKS][CDEF_NBLOCKS], int pli,
                    cdef_list *dlist, int cdef_count, int level,
                    int sec_strength, int pri_damping, int sec_damping,
                    int coeff_shift);
#endif
