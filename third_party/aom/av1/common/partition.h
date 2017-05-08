/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/* clang-format off */

#if !defined(_partition_H)
# define _partition_H

#include "av1/common/enums.h"
#include "odintrin.h"

typedef unsigned char index_pair[2];

typedef struct {
  const index_pair **const dst_table;
  int size;
  int nb_bands;
  const int *const band_offsets;
} band_layout;

extern const int *const OD_BAND_OFFSETS[OD_TXSIZES + 1];

void od_raster_to_coding_order(tran_low_t *dst, int n,  TX_TYPE ty_type,
 const tran_low_t *src, int stride);

void od_coding_order_to_raster(tran_low_t *dst, int stride,  TX_TYPE ty_type,
 const tran_low_t *src, int n);

void od_raster_to_coding_order_16(int16_t *dst, int n, const int16_t *src,
 int stride);

#endif
