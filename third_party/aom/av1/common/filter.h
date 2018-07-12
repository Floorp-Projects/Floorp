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

#include "./aom_config.h"
#include "aom/aom_integer.h"
#include "aom_dsp/aom_filter.h"
#include "aom_ports/mem.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USE_TEMPORALFILTER_12TAP 1
#define MAX_FILTER_TAP 12

#define USE_12TAP_FILTER 0
#define USE_EXTRA_FILTER 0

typedef enum {
  EIGHTTAP_REGULAR,
  EIGHTTAP_SMOOTH,
#if USE_EXTRA_FILTER
  EIGHTTAP_SMOOTH2,
#endif  // USE_EXTRA_FILTER
  MULTITAP_SHARP,
  BILINEAR,
#if USE_EXTRA_FILTER
  EIGHTTAP_SHARP,
  FILTER_REGULAR_UV,
  FILTER_SMOOTH_UV,
  FILTER_SHARP_UV,
  FILTER_SMOOTH2_UV,
#endif  // USE_EXTRA_FILTER
  INTERP_FILTERS_ALL,
  SWITCHABLE_FILTERS = BILINEAR,
  SWITCHABLE = SWITCHABLE_FILTERS + 1, /* the last switchable one */
  EXTRA_FILTERS = INTERP_FILTERS_ALL - SWITCHABLE_FILTERS,
#if USE_TEMPORALFILTER_12TAP
  TEMPORALFILTER_12TAP = SWITCHABLE_FILTERS + EXTRA_FILTERS,
#endif
} InterpFilter;

// With CONFIG_DUAL_FILTER, pack two InterpFilter's into a uint32_t: since
// there are at most 10 filters, we can use 16 bits for each and have more than
// enough space. This reduces argument passing and unifies the operation of
// setting a (pair of) filters.
//
// Without CONFIG_DUAL_FILTER,
#if CONFIG_DUAL_FILTER
typedef uint32_t InterpFilters;
static INLINE InterpFilter av1_extract_interp_filter(InterpFilters filters,
                                                     int x_filter) {
  return (InterpFilter)((filters >> (x_filter ? 16 : 0)) & 0xffff);
}

static INLINE InterpFilters av1_make_interp_filters(InterpFilter y_filter,
                                                    InterpFilter x_filter) {
  uint16_t y16 = y_filter & 0xffff;
  uint16_t x16 = x_filter & 0xffff;
  return y16 | ((uint32_t)x16 << 16);
}

static INLINE InterpFilters av1_broadcast_interp_filter(InterpFilter filter) {
  return av1_make_interp_filters(filter, filter);
}
#else
typedef InterpFilter InterpFilters;
static INLINE InterpFilter av1_extract_interp_filter(InterpFilters filters,
                                                     int x_filter) {
#ifdef NDEBUG
  (void)x_filter;
#endif
  assert(!x_filter);
  return filters;
}

static INLINE InterpFilters av1_broadcast_interp_filter(InterpFilter filter) {
  return filter;
}
#endif

static INLINE InterpFilter av1_unswitchable_filter(InterpFilter filter) {
  return filter == SWITCHABLE ? EIGHTTAP_REGULAR : filter;
}

#if USE_EXTRA_FILTER
#define LOG_SWITCHABLE_FILTERS \
  3 /* (1 << LOG_SWITCHABLE_FILTERS) > SWITCHABLE_FILTERS */
#else
#define LOG_SWITCHABLE_FILTERS \
  2 /* (1 << LOG_SWITCHABLE_FILTERS) > SWITCHABLE_FILTERS */
#endif

#if CONFIG_DUAL_FILTER
#define MAX_SUBPEL_TAPS 12
#define SWITCHABLE_FILTER_CONTEXTS ((SWITCHABLE_FILTERS + 1) * 4)
#define INTER_FILTER_COMP_OFFSET (SWITCHABLE_FILTERS + 1)
#define INTER_FILTER_DIR_OFFSET ((SWITCHABLE_FILTERS + 1) * 2)
#else  // CONFIG_DUAL_FILTER
#define SWITCHABLE_FILTER_CONTEXTS (SWITCHABLE_FILTERS + 1)
#endif  // CONFIG_DUAL_FILTER

#if CONFIG_EXT_INTRA
#if CONFIG_INTRA_INTERP
typedef enum {
  INTRA_FILTER_LINEAR,
  INTRA_FILTER_8TAP,
  INTRA_FILTER_8TAP_SHARP,
  INTRA_FILTER_8TAP_SMOOTH,
  INTRA_FILTERS,
} INTRA_FILTER;

extern const InterpKernel *av1_intra_filter_kernels[INTRA_FILTERS];
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA

typedef struct InterpFilterParams {
  const int16_t *filter_ptr;
  uint16_t taps;
  uint16_t subpel_shifts;
  InterpFilter interp_filter;
} InterpFilterParams;

InterpFilterParams av1_get_interp_filter_params(
    const InterpFilter interp_filter);

const int16_t *av1_get_interp_filter_kernel(const InterpFilter interp_filter);

static INLINE const int16_t *av1_get_interp_filter_subpel_kernel(
    const InterpFilterParams filter_params, const int subpel) {
  return filter_params.filter_ptr + filter_params.taps * subpel;
}

static INLINE int av1_is_interpolating_filter(
    const InterpFilter interp_filter) {
  const InterpFilterParams ip = av1_get_interp_filter_params(interp_filter);
  return (ip.filter_ptr[ip.taps / 2 - 1] == 128);
}

#if CONFIG_DUAL_FILTER
InterpFilter av1_get_plane_interp_filter(InterpFilter interp_filter, int plane);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_FILTER_H_
