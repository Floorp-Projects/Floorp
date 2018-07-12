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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "enums.h"
#include "odintrin.h"
#include "partition.h"
#include "zigzag.h"

OD_EXTERN const index_pair *OD_ZIGZAG4[4] = {
  OD_ZIGZAG4_DCT_DCT,
  OD_ZIGZAG4_ADST_DCT,
  OD_ZIGZAG4_DCT_ADST,
  OD_ZIGZAG4_ADST_ADST
};

OD_EXTERN const index_pair *OD_ZIGZAG8[4] = {
  OD_ZIGZAG8_DCT_DCT,
  OD_ZIGZAG8_ADST_DCT,
  OD_ZIGZAG8_DCT_ADST,
  OD_ZIGZAG8_ADST_ADST
};

OD_EXTERN const index_pair *OD_ZIGZAG16[4] = {
  OD_ZIGZAG16_DCT_DCT,
  OD_ZIGZAG16_ADST_DCT,
  OD_ZIGZAG16_DCT_ADST,
  OD_ZIGZAG16_ADST_ADST
};

OD_EXTERN const index_pair *OD_ZIGZAG32[4] = {
  OD_ZIGZAG32_DCT_DCT,
  OD_ZIGZAG32_DCT_DCT,
  OD_ZIGZAG32_DCT_DCT,
  OD_ZIGZAG32_DCT_DCT
};

/* The tables below specify how coefficient blocks are translated to
   and from PVQ partition coding scan order for 4x4, 8x8 and 16x16 */

static const int OD_LAYOUT32_OFFSETS[4] = { 0, 128, 256, 768 };
const band_layout OD_LAYOUT32 = {
  OD_ZIGZAG32,
  32,
  3,
  OD_LAYOUT32_OFFSETS
};

static const int OD_LAYOUT16_OFFSETS[4] = { 0, 32, 64, 192 };
const band_layout OD_LAYOUT16 = {
  OD_ZIGZAG16,
  16,
  3,
  OD_LAYOUT16_OFFSETS
};

const int OD_LAYOUT8_OFFSETS[4] = { 0, 8, 16, 48 };
const band_layout OD_LAYOUT8 = {
  OD_ZIGZAG8,
  8,
  3,
  OD_LAYOUT8_OFFSETS
};

static const int OD_LAYOUT4_OFFSETS[2] = { 0, 15 };
const band_layout OD_LAYOUT4 = {
  OD_ZIGZAG4,
  4,
  1,
  OD_LAYOUT4_OFFSETS
};

/* First element is the number of bands, followed by the list all the band
  boundaries. */
static const int OD_BAND_OFFSETS4[] = {1, 1, 16};
static const int OD_BAND_OFFSETS8[] = {4, 1, 16, 24, 32, 64};
static const int OD_BAND_OFFSETS16[] = {7, 1, 16, 24, 32, 64, 96, 128, 256};
static const int OD_BAND_OFFSETS32[] = {10, 1, 16, 24, 32, 64, 96, 128, 256,
 384, 512, 1024};
static const int OD_BAND_OFFSETS64[] = {13, 1, 16, 24, 32, 64, 96, 128, 256,
 384, 512, 1024, 1536, 2048, 4096};

const int *const OD_BAND_OFFSETS[OD_TXSIZES + 1] = {
  OD_BAND_OFFSETS4,
  OD_BAND_OFFSETS8,
  OD_BAND_OFFSETS16,
  OD_BAND_OFFSETS32,
  OD_BAND_OFFSETS64
};

/** Perform a single stage of conversion from a coefficient block in
 * raster order into coding scan order
 *
 * @param [in]     layout  scan order specification
 * @param [out]    dst     destination vector
 * @param [in]     src     source coefficient block
 * @param [int]    int     source vector row stride
 */
static void od_band_from_raster(const band_layout *layout, tran_low_t *dst,
 const tran_low_t *src, int stride, TX_TYPE tx_type) {
  int i;
  int len;
  len = layout->band_offsets[layout->nb_bands];
  for (i = 0; i < len; i++) {
    dst[i] = src[layout->dst_table[tx_type][i][1]*stride + layout->dst_table[tx_type][i][0]];
  }
}

/** Perform a single stage of conversion from a vector in coding scan
    order back into a coefficient block in raster order
 *
 * @param [in]     layout  scan order specification
 * @param [out]    dst     destination coefficient block
 * @param [in]     src     source vector
 * @param [int]    stride  destination vector row stride
 */
static void od_raster_from_band(const band_layout *layout, tran_low_t *dst,
 int stride, TX_TYPE tx_type, const tran_low_t *src) {
  int i;
  int len;
  len = layout->band_offsets[layout->nb_bands];
  for (i = 0; i < len; i++) {
    dst[layout->dst_table[tx_type][i][1]*stride + layout->dst_table[tx_type][i][0]] = src[i];
  }
}

static const band_layout *const OD_LAYOUTS[] = {&OD_LAYOUT4, &OD_LAYOUT8,
 &OD_LAYOUT16, &OD_LAYOUT32};

/** Converts a coefficient block in raster order into a vector in
 * coding scan order with the PVQ partitions laid out one after
 * another.  This works in stages; the 4x4 conversion is applied to
 * the coefficients nearest DC, then the 8x8 applied to the 8x8 block
 * nearest DC that was not already coded by 4x4, then 16x16 following
 * the same pattern.
 *
 * @param [out]    dst        destination vector
 * @param [in]     n          block size (along one side)
 * @param [in]     ty_type    transfrom type
 * @param [in]     src        source coefficient block
 * @param [in]     stride     source vector row stride
 */
void od_raster_to_coding_order(tran_low_t *dst, int n, TX_TYPE ty_type,
 const tran_low_t *src, int stride) {
  int bs;
  /* dst + 1 because DC is not included for 4x4 blocks. */
  od_band_from_raster(OD_LAYOUTS[0], dst + 1, src, stride, ty_type);
  for (bs = 1; bs < OD_TXSIZES; bs++) {
    int size;
    int offset;
    /* Length of block size > 4. */
    size = 1 << (OD_LOG_BSIZE0 + bs);
    /* Offset is the size of the previous block squared. */
    offset = 1 << 2*(OD_LOG_BSIZE0 - 1 + bs);
    if (n >= size) {
      /* 3 16x16 bands come after 3 8x8 bands, which come after 2 4x4 bands. */
      od_band_from_raster(OD_LAYOUTS[bs], dst + offset, src, stride, ty_type);
    }
  }
  dst[0] = src[0];
}

/** Converts a vector in coding scan order witht he PVQ partitions
 * laid out one after another into a coefficient block in raster
 * order. This works in stages in the reverse order of raster->scan
 * order; the 16x16 conversion is applied to the coefficients that
 * don't appear in an 8x8 block, then the 8x8 applied to the 8x8 block
 * sans the 4x4 block it contains, then 4x4 is converted sans DC.
 *
 * @param [out]    dst        destination coefficient block
 * @param [in]     stride     destination vector row stride
 * @param [in]     src        source vector
 * @param [in]     n          block size (along one side)
 */
void od_coding_order_to_raster(tran_low_t *dst, int stride, TX_TYPE ty_type,
 const tran_low_t *src, int n) {
  int bs;
  /* src + 1 because DC is not included for 4x4 blocks. */
  od_raster_from_band(OD_LAYOUTS[0], dst, stride, ty_type, src + 1);
  for (bs = 1; bs < OD_TXSIZES; bs++) {
    int size;
    int offset;
    /* Length of block size > 4 */
    size = 1 << (OD_LOG_BSIZE0 + bs);
    /* Offset is the size of the previous block squared. */
    offset = 1 << 2*(OD_LOG_BSIZE0 - 1 + bs);
    if (n >= size) {
      /* 3 16x16 bands come after 3 8x8 bands, which come after 2 4x4 bands. */
      od_raster_from_band(OD_LAYOUTS[bs], dst, stride, ty_type, src + offset);
    }
  }
  dst[0] = src[0];
}

/** Perform a single stage of conversion from a coefficient block in
 * raster order into coding scan order
 *
 * @param [in]     layout  scan order specification
 * @param [out]    dst     destination vector
 * @param [in]     src     source coefficient block
 * @param [int]    int     source vector row stride
 */
static void od_band_from_raster_16(const band_layout *layout, int16_t *dst,
 const int16_t *src, int stride) {
  int i;
  int len;
  len = layout->band_offsets[layout->nb_bands];
  for (i = 0; i < len; i++) {
    dst[i] = src[layout->dst_table[DCT_DCT][i][1]*stride + layout->dst_table[DCT_DCT][i][0]];
  }
}

/** Converts a coefficient block in raster order into a vector in
 * coding scan order with the PVQ partitions laid out one after
 * another.  This works in stages; the 4x4 conversion is applied to
 * the coefficients nearest DC, then the 8x8 applied to the 8x8 block
 * nearest DC that was not already coded by 4x4, then 16x16 following
 * the same pattern.
 *
 * @param [out]    dst        destination vector
 * @param [in]     n          block size (along one side)
 * @param [in]     src        source coefficient block
 * @param [in]     stride     source vector row stride
 */
void od_raster_to_coding_order_16(int16_t *dst, int n, const int16_t *src,
 int stride) {
  int bs;
  /* dst + 1 because DC is not included for 4x4 blocks. */
  od_band_from_raster_16(OD_LAYOUTS[0], dst + 1, src, stride);
  for (bs = 1; bs < OD_TXSIZES; bs++) {
    int size;
    int offset;
    /* Length of block size > 4. */
    size = 1 << (OD_LOG_BSIZE0 + bs);
    /* Offset is the size of the previous block squared. */
    offset = 1 << 2*(OD_LOG_BSIZE0 - 1 + bs);
    if (n >= size) {
      /* 3 16x16 bands come after 3 8x8 bands, which come after 2 4x4 bands. */
      od_band_from_raster_16(OD_LAYOUTS[bs], dst + offset, src, stride);
    }
  }
  dst[0] = src[0];
}
