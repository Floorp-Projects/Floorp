/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_SCAN_H_
#define VP9_COMMON_VP9_SCAN_H_

#include "vpx/vpx_integer.h"
#include "vpx_ports/mem.h"

#include "vp9/common/vp9_enums.h"

#define MAX_NEIGHBORS 2

extern DECLARE_ALIGNED(16, const int16_t, vp9_default_scan_4x4[16]);
extern DECLARE_ALIGNED(16, const int16_t, vp9_col_scan_4x4[16]);
extern DECLARE_ALIGNED(16, const int16_t, vp9_row_scan_4x4[16]);

extern DECLARE_ALIGNED(16, const int16_t, vp9_default_scan_8x8[64]);
extern DECLARE_ALIGNED(16, const int16_t, vp9_col_scan_8x8[64]);
extern DECLARE_ALIGNED(16, const int16_t, vp9_row_scan_8x8[64]);

extern DECLARE_ALIGNED(16, const int16_t, vp9_default_scan_16x16[256]);
extern DECLARE_ALIGNED(16, const int16_t, vp9_col_scan_16x16[256]);
extern DECLARE_ALIGNED(16, const int16_t, vp9_row_scan_16x16[256]);

extern DECLARE_ALIGNED(16, const int16_t, vp9_default_scan_32x32[1024]);

extern DECLARE_ALIGNED(16, int16_t, vp9_default_iscan_4x4[16]);
extern DECLARE_ALIGNED(16, int16_t, vp9_col_iscan_4x4[16]);
extern DECLARE_ALIGNED(16, int16_t, vp9_row_iscan_4x4[16]);

extern DECLARE_ALIGNED(16, int16_t, vp9_default_iscan_8x8[64]);
extern DECLARE_ALIGNED(16, int16_t, vp9_col_iscan_8x8[64]);
extern DECLARE_ALIGNED(16, int16_t, vp9_row_iscan_8x8[64]);

extern DECLARE_ALIGNED(16, int16_t, vp9_default_iscan_16x16[256]);
extern DECLARE_ALIGNED(16, int16_t, vp9_col_iscan_16x16[256]);
extern DECLARE_ALIGNED(16, int16_t, vp9_row_iscan_16x16[256]);

extern DECLARE_ALIGNED(16, int16_t, vp9_default_iscan_32x32[1024]);

extern DECLARE_ALIGNED(16, int16_t,
                       vp9_default_scan_4x4_neighbors[17 * MAX_NEIGHBORS]);
extern DECLARE_ALIGNED(16, int16_t,
                       vp9_col_scan_4x4_neighbors[17 * MAX_NEIGHBORS]);
extern DECLARE_ALIGNED(16, int16_t,
                       vp9_row_scan_4x4_neighbors[17 * MAX_NEIGHBORS]);
extern DECLARE_ALIGNED(16, int16_t,
                       vp9_col_scan_8x8_neighbors[65 * MAX_NEIGHBORS]);
extern DECLARE_ALIGNED(16, int16_t,
                       vp9_row_scan_8x8_neighbors[65 * MAX_NEIGHBORS]);
extern DECLARE_ALIGNED(16, int16_t,
                       vp9_default_scan_8x8_neighbors[65 * MAX_NEIGHBORS]);
extern DECLARE_ALIGNED(16, int16_t,
                       vp9_col_scan_16x16_neighbors[257 * MAX_NEIGHBORS]);
extern DECLARE_ALIGNED(16, int16_t,
                       vp9_row_scan_16x16_neighbors[257 * MAX_NEIGHBORS]);
extern DECLARE_ALIGNED(16, int16_t,
                       vp9_default_scan_16x16_neighbors[257 * MAX_NEIGHBORS]);
extern DECLARE_ALIGNED(16, int16_t,
                       vp9_default_scan_32x32_neighbors[1025 * MAX_NEIGHBORS]);


void vp9_init_neighbors();

static INLINE const int16_t* get_scan_4x4(TX_TYPE tx_type) {
  switch (tx_type) {
    case ADST_DCT:
      return vp9_row_scan_4x4;
    case DCT_ADST:
      return vp9_col_scan_4x4;
    default:
      return vp9_default_scan_4x4;
  }
}

static INLINE void get_scan_nb_4x4(TX_TYPE tx_type,
                                   const int16_t **scan, const int16_t **nb) {
  switch (tx_type) {
    case ADST_DCT:
      *scan = vp9_row_scan_4x4;
      *nb = vp9_row_scan_4x4_neighbors;
      break;
    case DCT_ADST:
      *scan = vp9_col_scan_4x4;
      *nb = vp9_col_scan_4x4_neighbors;
      break;
    default:
      *scan = vp9_default_scan_4x4;
      *nb = vp9_default_scan_4x4_neighbors;
      break;
  }
}

static INLINE const int16_t* get_iscan_4x4(TX_TYPE tx_type) {
  switch (tx_type) {
    case ADST_DCT:
      return vp9_row_iscan_4x4;
    case DCT_ADST:
      return vp9_col_iscan_4x4;
    default:
      return vp9_default_iscan_4x4;
  }
}

static INLINE const int16_t* get_scan_8x8(TX_TYPE tx_type) {
  switch (tx_type) {
    case ADST_DCT:
      return vp9_row_scan_8x8;
    case DCT_ADST:
      return vp9_col_scan_8x8;
    default:
      return vp9_default_scan_8x8;
  }
}

static INLINE void get_scan_nb_8x8(TX_TYPE tx_type,
                                   const int16_t **scan, const int16_t **nb) {
  switch (tx_type) {
    case ADST_DCT:
      *scan = vp9_row_scan_8x8;
      *nb = vp9_row_scan_8x8_neighbors;
      break;
    case DCT_ADST:
      *scan = vp9_col_scan_8x8;
      *nb = vp9_col_scan_8x8_neighbors;
      break;
    default:
      *scan = vp9_default_scan_8x8;
      *nb = vp9_default_scan_8x8_neighbors;
      break;
  }
}

static INLINE const int16_t* get_iscan_8x8(TX_TYPE tx_type) {
  switch (tx_type) {
    case ADST_DCT:
      return vp9_row_iscan_8x8;
    case DCT_ADST:
      return vp9_col_iscan_8x8;
    default:
      return vp9_default_iscan_8x8;
  }
}

static INLINE const int16_t* get_scan_16x16(TX_TYPE tx_type) {
  switch (tx_type) {
    case ADST_DCT:
      return vp9_row_scan_16x16;
    case DCT_ADST:
      return vp9_col_scan_16x16;
    default:
      return vp9_default_scan_16x16;
  }
}

static INLINE void get_scan_nb_16x16(TX_TYPE tx_type,
                                     const int16_t **scan, const int16_t **nb) {
  switch (tx_type) {
    case ADST_DCT:
      *scan = vp9_row_scan_16x16;
      *nb = vp9_row_scan_16x16_neighbors;
      break;
    case DCT_ADST:
      *scan = vp9_col_scan_16x16;
      *nb = vp9_col_scan_16x16_neighbors;
      break;
    default:
      *scan = vp9_default_scan_16x16;
      *nb = vp9_default_scan_16x16_neighbors;
      break;
  }
}

static INLINE const int16_t* get_iscan_16x16(TX_TYPE tx_type) {
  switch (tx_type) {
    case ADST_DCT:
      return vp9_row_iscan_16x16;
    case DCT_ADST:
      return vp9_col_iscan_16x16;
    default:
      return vp9_default_iscan_16x16;
  }
}

static INLINE int get_coef_context(const int16_t *neighbors,
                                   const uint8_t *token_cache, int c) {
  return (1 + token_cache[neighbors[MAX_NEIGHBORS * c + 0]] +
          token_cache[neighbors[MAX_NEIGHBORS * c + 1]]) >> 1;
}

#endif  // VP9_COMMON_VP9_SCAN_H_
