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

#ifndef AOM_DSP_AOM_DSP_COMMON_H_
#define AOM_DSP_AOM_DSP_COMMON_H_

#include "./aom_config.h"
#include "aom/aom_integer.h"
#include "aom_ports/mem.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAX_SB_SIZE
#if CONFIG_AV1 && CONFIG_EXT_PARTITION
#define MAX_SB_SIZE 128
#else
#define MAX_SB_SIZE 64
#endif  // CONFIG_AV1 && CONFIG_EXT_PARTITION
#endif  // ndef MAX_SB_SIZE

#define AOMMIN(x, y) (((x) < (y)) ? (x) : (y))
#define AOMMAX(x, y) (((x) > (y)) ? (x) : (y))

#define IMPLIES(a, b) (!(a) || (b))  //  Logical 'a implies b' (or 'a -> b')

#define IS_POWER_OF_TWO(x) (((x) & ((x)-1)) == 0)

/* Left shifting a negative value became undefined behavior in C99 (downgraded
   from merely implementation-defined in C89). This should still compile to the
   correct thing on any two's-complement machine, but avoid ubsan warnings.*/
#define AOM_SIGNED_SHL(x, shift) ((x) * (((x)*0 + 1) << (shift)))

// These can be used to give a hint about branch outcomes.
// This can have an effect, even if your target processor has a
// good branch predictor, as these hints can affect basic block
// ordering by the compiler.
#ifdef __GNUC__
#define LIKELY(v) __builtin_expect(v, 1)
#define UNLIKELY(v) __builtin_expect(v, 0)
#else
#define LIKELY(v) (v)
#define UNLIKELY(v) (v)
#endif

typedef uint16_t qm_val_t;
#define AOM_QM_BITS 5

#if CONFIG_HIGHBITDEPTH
// Note:
// tran_low_t  is the datatype used for final transform coefficients.
// tran_high_t is the datatype used for intermediate transform stages.
typedef int64_t tran_high_t;
typedef int32_t tran_low_t;
#else
// Note:
// tran_low_t  is the datatype used for final transform coefficients.
// tran_high_t is the datatype used for intermediate transform stages.
typedef int32_t tran_high_t;
typedef int16_t tran_low_t;
#endif  // CONFIG_HIGHBITDEPTH

static INLINE uint8_t clip_pixel(int val) {
  return (val > 255) ? 255 : (val < 0) ? 0 : val;
}

static INLINE int clamp(int value, int low, int high) {
  return value < low ? low : (value > high ? high : value);
}

static INLINE uint32_t clamp32u(uint32_t value, uint32_t low, uint32_t high) {
  return value < low ? low : (value > high ? high : value);
}

static INLINE int64_t clamp64(int64_t value, int64_t low, int64_t high) {
  return value < low ? low : (value > high ? high : value);
}

static INLINE double fclamp(double value, double low, double high) {
  return value < low ? low : (value > high ? high : value);
}

static INLINE uint16_t clip_pixel_highbd(int val, int bd) {
  switch (bd) {
    case 8:
    default: return (uint16_t)clamp(val, 0, 255);
    case 10: return (uint16_t)clamp(val, 0, 1023);
    case 12: return (uint16_t)clamp(val, 0, 4095);
  }
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_DSP_AOM_DSP_COMMON_H_
