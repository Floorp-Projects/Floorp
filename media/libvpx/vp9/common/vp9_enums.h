/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_ENUMS_H_
#define VP9_COMMON_VP9_ENUMS_H_

#include "./vpx_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MI_SIZE_LOG2 3
#define MI_BLOCK_SIZE_LOG2 (6 - MI_SIZE_LOG2)  // 64 = 2^6

#define MI_SIZE (1 << MI_SIZE_LOG2)  // pixels per mi-unit
#define MI_BLOCK_SIZE (1 << MI_BLOCK_SIZE_LOG2)  // mi-units per max block

#define MI_MASK (MI_BLOCK_SIZE - 1)


typedef enum BLOCK_SIZE {
  BLOCK_4X4,
  BLOCK_4X8,
  BLOCK_8X4,
  BLOCK_8X8,
  BLOCK_8X16,
  BLOCK_16X8,
  BLOCK_16X16,
  BLOCK_16X32,
  BLOCK_32X16,
  BLOCK_32X32,
  BLOCK_32X64,
  BLOCK_64X32,
  BLOCK_64X64,
  BLOCK_SIZES,
  BLOCK_INVALID = BLOCK_SIZES
} BLOCK_SIZE;

typedef enum PARTITION_TYPE {
  PARTITION_NONE,
  PARTITION_HORZ,
  PARTITION_VERT,
  PARTITION_SPLIT,
  PARTITION_TYPES,
  PARTITION_INVALID = PARTITION_TYPES
} PARTITION_TYPE;

#define PARTITION_PLOFFSET   4  // number of probability models per block size
#define PARTITION_CONTEXTS (4 * PARTITION_PLOFFSET)

// block transform size
typedef enum {
  TX_4X4 = 0,                      // 4x4 transform
  TX_8X8 = 1,                      // 8x8 transform
  TX_16X16 = 2,                    // 16x16 transform
  TX_32X32 = 3,                    // 32x32 transform
  TX_SIZES
} TX_SIZE;

// frame transform mode
typedef enum {
  ONLY_4X4            = 0,        // only 4x4 transform used
  ALLOW_8X8           = 1,        // allow block transform size up to 8x8
  ALLOW_16X16         = 2,        // allow block transform size up to 16x16
  ALLOW_32X32         = 3,        // allow block transform size up to 32x32
  TX_MODE_SELECT      = 4,        // transform specified for each block
  TX_MODES            = 5,
} TX_MODE;

typedef enum {
  DCT_DCT   = 0,                      // DCT  in both horizontal and vertical
  ADST_DCT  = 1,                      // ADST in vertical, DCT in horizontal
  DCT_ADST  = 2,                      // DCT  in vertical, ADST in horizontal
  ADST_ADST = 3,                      // ADST in both directions
  TX_TYPES = 4
} TX_TYPE;

typedef enum {
  UNKNOWN    = 0,
  BT_601     = 1,  // YUV
  BT_709     = 2,  // YUV
  SMPTE_170  = 3,  // YUV
  SMPTE_240  = 4,  // YUV
  RESERVED_1 = 5,
  RESERVED_2 = 6,
  SRGB       = 7   // RGB
} COLOR_SPACE;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_COMMON_VP9_ENUMS_H_
