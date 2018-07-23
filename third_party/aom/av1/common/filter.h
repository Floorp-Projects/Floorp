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

#ifndef AV1_COMMON_FILTER_H_
#define AV1_COMMON_FILTER_H_

#include <assert.h>

#include "config/aom_config.h"

#include "aom/aom_integer.h"
#include "aom_dsp/aom_filter.h"
#include "aom_ports/mem.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_FILTER_TAP 8

typedef enum ATTRIBUTE_PACKED {
  EIGHTTAP_REGULAR,
  EIGHTTAP_SMOOTH,
  MULTITAP_SHARP,
  BILINEAR,
  INTERP_FILTERS_ALL,
  SWITCHABLE_FILTERS = BILINEAR,
  SWITCHABLE = SWITCHABLE_FILTERS + 1, /* the last switchable one */
  EXTRA_FILTERS = INTERP_FILTERS_ALL - SWITCHABLE_FILTERS,
} InterpFilter;

// With CONFIG_DUAL_FILTER, pack two InterpFilter's into a uint32_t: since
// there are at most 10 filters, we can use 16 bits for each and have more than
// enough space. This reduces argument passing and unifies the operation of
// setting a (pair of) filters.
//
// Without CONFIG_DUAL_FILTER,
typedef uint32_t InterpFilters;
static INLINE InterpFilter av1_extract_interp_filter(InterpFilters filters,
                                                     int x_filter) {
  return (InterpFilter)((filters >> (x_filter ? 16 : 0)) & 0xf);
}

static INLINE InterpFilters av1_make_interp_filters(InterpFilter y_filter,
                                                    InterpFilter x_filter) {
  uint16_t y16 = y_filter & 0xf;
  uint16_t x16 = x_filter & 0xf;
  return y16 | ((uint32_t)x16 << 16);
}

static INLINE InterpFilters av1_broadcast_interp_filter(InterpFilter filter) {
  return av1_make_interp_filters(filter, filter);
}

static INLINE InterpFilter av1_unswitchable_filter(InterpFilter filter) {
  return filter == SWITCHABLE ? EIGHTTAP_REGULAR : filter;
}

/* (1 << LOG_SWITCHABLE_FILTERS) > SWITCHABLE_FILTERS */
#define LOG_SWITCHABLE_FILTERS 2

#define MAX_SUBPEL_TAPS 12
#define SWITCHABLE_FILTER_CONTEXTS ((SWITCHABLE_FILTERS + 1) * 4)
#define INTER_FILTER_COMP_OFFSET (SWITCHABLE_FILTERS + 1)
#define INTER_FILTER_DIR_OFFSET ((SWITCHABLE_FILTERS + 1) * 2)

typedef struct InterpFilterParams {
  const int16_t *filter_ptr;
  uint16_t taps;
  uint16_t subpel_shifts;
  InterpFilter interp_filter;
} InterpFilterParams;

DECLARE_ALIGNED(256, static const InterpKernel,
                av1_bilinear_filters[SUBPEL_SHIFTS]) = {
  { 0, 0, 0, 128, 0, 0, 0, 0 },  { 0, 0, 0, 120, 8, 0, 0, 0 },
  { 0, 0, 0, 112, 16, 0, 0, 0 }, { 0, 0, 0, 104, 24, 0, 0, 0 },
  { 0, 0, 0, 96, 32, 0, 0, 0 },  { 0, 0, 0, 88, 40, 0, 0, 0 },
  { 0, 0, 0, 80, 48, 0, 0, 0 },  { 0, 0, 0, 72, 56, 0, 0, 0 },
  { 0, 0, 0, 64, 64, 0, 0, 0 },  { 0, 0, 0, 56, 72, 0, 0, 0 },
  { 0, 0, 0, 48, 80, 0, 0, 0 },  { 0, 0, 0, 40, 88, 0, 0, 0 },
  { 0, 0, 0, 32, 96, 0, 0, 0 },  { 0, 0, 0, 24, 104, 0, 0, 0 },
  { 0, 0, 0, 16, 112, 0, 0, 0 }, { 0, 0, 0, 8, 120, 0, 0, 0 }
};

DECLARE_ALIGNED(256, static const InterpKernel,
                av1_sub_pel_filters_8[SUBPEL_SHIFTS]) = {
  { 0, 0, 0, 128, 0, 0, 0, 0 },      { 0, 2, -6, 126, 8, -2, 0, 0 },
  { 0, 2, -10, 122, 18, -4, 0, 0 },  { 0, 2, -12, 116, 28, -8, 2, 0 },
  { 0, 2, -14, 110, 38, -10, 2, 0 }, { 0, 2, -14, 102, 48, -12, 2, 0 },
  { 0, 2, -16, 94, 58, -12, 2, 0 },  { 0, 2, -14, 84, 66, -12, 2, 0 },
  { 0, 2, -14, 76, 76, -14, 2, 0 },  { 0, 2, -12, 66, 84, -14, 2, 0 },
  { 0, 2, -12, 58, 94, -16, 2, 0 },  { 0, 2, -12, 48, 102, -14, 2, 0 },
  { 0, 2, -10, 38, 110, -14, 2, 0 }, { 0, 2, -8, 28, 116, -12, 2, 0 },
  { 0, 0, -4, 18, 122, -10, 2, 0 },  { 0, 0, -2, 8, 126, -6, 2, 0 }
};

DECLARE_ALIGNED(256, static const InterpKernel,
                av1_sub_pel_filters_8sharp[SUBPEL_SHIFTS]) = {
  { 0, 0, 0, 128, 0, 0, 0, 0 },         { -2, 2, -6, 126, 8, -2, 2, 0 },
  { -2, 6, -12, 124, 16, -6, 4, -2 },   { -2, 8, -18, 120, 26, -10, 6, -2 },
  { -4, 10, -22, 116, 38, -14, 6, -2 }, { -4, 10, -22, 108, 48, -18, 8, -2 },
  { -4, 10, -24, 100, 60, -20, 8, -2 }, { -4, 10, -24, 90, 70, -22, 10, -2 },
  { -4, 12, -24, 80, 80, -24, 12, -4 }, { -2, 10, -22, 70, 90, -24, 10, -4 },
  { -2, 8, -20, 60, 100, -24, 10, -4 }, { -2, 8, -18, 48, 108, -22, 10, -4 },
  { -2, 6, -14, 38, 116, -22, 10, -4 }, { -2, 6, -10, 26, 120, -18, 8, -2 },
  { -2, 4, -6, 16, 124, -12, 6, -2 },   { 0, 2, -2, 8, 126, -6, 2, -2 }
};

DECLARE_ALIGNED(256, static const InterpKernel,
                av1_sub_pel_filters_8smooth[SUBPEL_SHIFTS]) = {
  { 0, 0, 0, 128, 0, 0, 0, 0 },     { 0, 2, 28, 62, 34, 2, 0, 0 },
  { 0, 0, 26, 62, 36, 4, 0, 0 },    { 0, 0, 22, 62, 40, 4, 0, 0 },
  { 0, 0, 20, 60, 42, 6, 0, 0 },    { 0, 0, 18, 58, 44, 8, 0, 0 },
  { 0, 0, 16, 56, 46, 10, 0, 0 },   { 0, -2, 16, 54, 48, 12, 0, 0 },
  { 0, -2, 14, 52, 52, 14, -2, 0 }, { 0, 0, 12, 48, 54, 16, -2, 0 },
  { 0, 0, 10, 46, 56, 16, 0, 0 },   { 0, 0, 8, 44, 58, 18, 0, 0 },
  { 0, 0, 6, 42, 60, 20, 0, 0 },    { 0, 0, 4, 40, 62, 22, 0, 0 },
  { 0, 0, 4, 36, 62, 26, 0, 0 },    { 0, 0, 2, 34, 62, 28, 2, 0 }
};

static const InterpFilterParams
    av1_interp_filter_params_list[SWITCHABLE_FILTERS + 1] = {
      { (const int16_t *)av1_sub_pel_filters_8, SUBPEL_TAPS, SUBPEL_SHIFTS,
        EIGHTTAP_REGULAR },
      { (const int16_t *)av1_sub_pel_filters_8smooth, SUBPEL_TAPS,
        SUBPEL_SHIFTS, EIGHTTAP_SMOOTH },
      { (const int16_t *)av1_sub_pel_filters_8sharp, SUBPEL_TAPS, SUBPEL_SHIFTS,
        MULTITAP_SHARP },
      { (const int16_t *)av1_bilinear_filters, SUBPEL_TAPS, SUBPEL_SHIFTS,
        BILINEAR }
    };

DECLARE_ALIGNED(256, static const InterpKernel,
                av1_sub_pel_filters_4[SUBPEL_SHIFTS]) = {
  { 0, 0, 0, 128, 0, 0, 0, 0 },     { 0, 0, -4, 126, 8, -2, 0, 0 },
  { 0, 0, -8, 122, 18, -4, 0, 0 },  { 0, 0, -10, 116, 28, -6, 0, 0 },
  { 0, 0, -12, 110, 38, -8, 0, 0 }, { 0, 0, -12, 102, 48, -10, 0, 0 },
  { 0, 0, -14, 94, 58, -10, 0, 0 }, { 0, 0, -12, 84, 66, -10, 0, 0 },
  { 0, 0, -12, 76, 76, -12, 0, 0 }, { 0, 0, -10, 66, 84, -12, 0, 0 },
  { 0, 0, -10, 58, 94, -14, 0, 0 }, { 0, 0, -10, 48, 102, -12, 0, 0 },
  { 0, 0, -8, 38, 110, -12, 0, 0 }, { 0, 0, -6, 28, 116, -10, 0, 0 },
  { 0, 0, -4, 18, 122, -8, 0, 0 },  { 0, 0, -2, 8, 126, -4, 0, 0 }
};
DECLARE_ALIGNED(256, static const InterpKernel,
                av1_sub_pel_filters_4smooth[SUBPEL_SHIFTS]) = {
  { 0, 0, 0, 128, 0, 0, 0, 0 },   { 0, 0, 30, 62, 34, 2, 0, 0 },
  { 0, 0, 26, 62, 36, 4, 0, 0 },  { 0, 0, 22, 62, 40, 4, 0, 0 },
  { 0, 0, 20, 60, 42, 6, 0, 0 },  { 0, 0, 18, 58, 44, 8, 0, 0 },
  { 0, 0, 16, 56, 46, 10, 0, 0 }, { 0, 0, 14, 54, 48, 12, 0, 0 },
  { 0, 0, 12, 52, 52, 12, 0, 0 }, { 0, 0, 12, 48, 54, 14, 0, 0 },
  { 0, 0, 10, 46, 56, 16, 0, 0 }, { 0, 0, 8, 44, 58, 18, 0, 0 },
  { 0, 0, 6, 42, 60, 20, 0, 0 },  { 0, 0, 4, 40, 62, 22, 0, 0 },
  { 0, 0, 4, 36, 62, 26, 0, 0 },  { 0, 0, 2, 34, 62, 30, 0, 0 }
};

// For w<=4, MULTITAP_SHARP is the same as EIGHTTAP_REGULAR
static const InterpFilterParams av1_interp_4tap[SWITCHABLE_FILTERS + 1] = {
  { (const int16_t *)av1_sub_pel_filters_4, SUBPEL_TAPS, SUBPEL_SHIFTS,
    EIGHTTAP_REGULAR },
  { (const int16_t *)av1_sub_pel_filters_4smooth, SUBPEL_TAPS, SUBPEL_SHIFTS,
    EIGHTTAP_SMOOTH },
  { (const int16_t *)av1_sub_pel_filters_4, SUBPEL_TAPS, SUBPEL_SHIFTS,
    EIGHTTAP_REGULAR },
  { (const int16_t *)av1_bilinear_filters, SUBPEL_TAPS, SUBPEL_SHIFTS,
    BILINEAR },
};

static INLINE const InterpFilterParams *
av1_get_interp_filter_params_with_block_size(const InterpFilter interp_filter,
                                             const int w) {
  if (w <= 4) return &av1_interp_4tap[interp_filter];
  return &av1_interp_filter_params_list[interp_filter];
}

static INLINE const int16_t *av1_get_interp_filter_kernel(
    const InterpFilter interp_filter) {
  return av1_interp_filter_params_list[interp_filter].filter_ptr;
}

static INLINE const int16_t *av1_get_interp_filter_subpel_kernel(
    const InterpFilterParams *const filter_params, const int subpel) {
  return filter_params->filter_ptr + filter_params->taps * subpel;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_FILTER_H_
