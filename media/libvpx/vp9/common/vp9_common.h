/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_COMMON_H_
#define VP9_COMMON_VP9_COMMON_H_

/* Interface header for common constant data structures and lookup tables */

#include <assert.h>

#include "./vpx_config.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx/vpx_integer.h"
#include "vp9/common/vp9_systemdependent.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#define ROUND_POWER_OF_TWO(value, n) \
    (((value) + (1 << ((n) - 1))) >> (n))

#define ALIGN_POWER_OF_TWO(value, n) \
    (((value) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

// Only need this for fixed-size arrays, for structs just assign.
#define vp9_copy(dest, src) {            \
    assert(sizeof(dest) == sizeof(src)); \
    vpx_memcpy(dest, src, sizeof(src));  \
  }

// Use this for variably-sized arrays.
#define vp9_copy_array(dest, src, n) {       \
    assert(sizeof(*dest) == sizeof(*src));   \
    vpx_memcpy(dest, src, n * sizeof(*src)); \
  }

#define vp9_zero(dest) vpx_memset(&(dest), 0, sizeof(dest))
#define vp9_zero_array(dest, n) vpx_memset(dest, 0, n * sizeof(*dest))

static INLINE uint8_t clip_pixel(int val) {
  return (val > 255) ? 255 : (val < 0) ? 0 : val;
}

static INLINE int clamp(int value, int low, int high) {
  return value < low ? low : (value > high ? high : value);
}

static INLINE double fclamp(double value, double low, double high) {
  return value < low ? low : (value > high ? high : value);
}

static INLINE int get_unsigned_bits(unsigned int num_values) {
  return num_values > 0 ? get_msb(num_values) + 1 : 0;
}

#if CONFIG_VP9_HIGHBITDEPTH
static INLINE uint16_t clip_pixel_high(int val, int bd) {
  switch (bd) {
    case 8:
    default:
      return (uint16_t)clamp(val, 0, 255);
    case 10:
      return (uint16_t)clamp(val, 0, 1023);
    case 12:
      return (uint16_t)clamp(val, 0, 4095);
  }
}

#define CONVERT_TO_SHORTPTR(x) ((uint16_t*)(((uintptr_t)x) << 1))
#define CONVERT_TO_BYTEPTR(x) ((uint8_t*)(((uintptr_t)x) >> 1 ))
#endif  // CONFIG_VP9_HIGHBITDEPTH

#if CONFIG_DEBUG
#define CHECK_MEM_ERROR(cm, lval, expr) do { \
  lval = (expr); \
  if (!lval) \
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR, \
                       "Failed to allocate "#lval" at %s:%d", \
                       __FILE__, __LINE__); \
  } while (0)
#else
#define CHECK_MEM_ERROR(cm, lval, expr) do { \
  lval = (expr); \
  if (!lval) \
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR, \
                       "Failed to allocate "#lval); \
  } while (0)
#endif

#define VP9_SYNC_CODE_0 0x49
#define VP9_SYNC_CODE_1 0x83
#define VP9_SYNC_CODE_2 0x42

#define VP9_FRAME_MARKER 0x2


#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_COMMON_VP9_COMMON_H_
