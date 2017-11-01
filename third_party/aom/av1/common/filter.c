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

#include <assert.h>

#include "av1/common/filter.h"

DECLARE_ALIGNED(256, static const InterpKernel,
                bilinear_filters[SUBPEL_SHIFTS]) = {
  { 0, 0, 0, 128, 0, 0, 0, 0 },  { 0, 0, 0, 120, 8, 0, 0, 0 },
  { 0, 0, 0, 112, 16, 0, 0, 0 }, { 0, 0, 0, 104, 24, 0, 0, 0 },
  { 0, 0, 0, 96, 32, 0, 0, 0 },  { 0, 0, 0, 88, 40, 0, 0, 0 },
  { 0, 0, 0, 80, 48, 0, 0, 0 },  { 0, 0, 0, 72, 56, 0, 0, 0 },
  { 0, 0, 0, 64, 64, 0, 0, 0 },  { 0, 0, 0, 56, 72, 0, 0, 0 },
  { 0, 0, 0, 48, 80, 0, 0, 0 },  { 0, 0, 0, 40, 88, 0, 0, 0 },
  { 0, 0, 0, 32, 96, 0, 0, 0 },  { 0, 0, 0, 24, 104, 0, 0, 0 },
  { 0, 0, 0, 16, 112, 0, 0, 0 }, { 0, 0, 0, 8, 120, 0, 0, 0 }
};

#if USE_TEMPORALFILTER_12TAP
DECLARE_ALIGNED(16, static const int16_t,
                sub_pel_filters_temporalfilter_12[SUBPEL_SHIFTS][12]) = {
  // intfilt 0.8
  { 0, 0, 0, 0, 0, 128, 0, 0, 0, 0, 0, 0 },
  { 0, 1, -1, 3, -7, 127, 8, -4, 2, -1, 0, 0 },
  { 0, 1, -3, 5, -12, 124, 18, -8, 4, -2, 1, 0 },
  { -1, 2, -4, 8, -17, 120, 28, -11, 6, -3, 1, -1 },
  { -1, 2, -4, 10, -21, 114, 38, -15, 8, -4, 2, -1 },
  { -1, 3, -5, 11, -23, 107, 49, -18, 9, -5, 2, -1 },
  { -1, 3, -6, 12, -25, 99, 60, -21, 11, -6, 3, -1 },
  { -1, 3, -6, 12, -25, 90, 70, -23, 12, -6, 3, -1 },
  { -1, 3, -6, 12, -24, 80, 80, -24, 12, -6, 3, -1 },
  { -1, 3, -6, 12, -23, 70, 90, -25, 12, -6, 3, -1 },
  { -1, 3, -6, 11, -21, 60, 99, -25, 12, -6, 3, -1 },
  { -1, 2, -5, 9, -18, 49, 107, -23, 11, -5, 3, -1 },
  { -1, 2, -4, 8, -15, 38, 114, -21, 10, -4, 2, -1 },
  { -1, 1, -3, 6, -11, 28, 120, -17, 8, -4, 2, -1 },
  { 0, 1, -2, 4, -8, 18, 124, -12, 5, -3, 1, 0 },
  { 0, 0, -1, 2, -4, 8, 127, -7, 3, -1, 1, 0 },
};
#endif  // USE_TEMPORALFILTER_12TAP

#if USE_EXTRA_FILTER
DECLARE_ALIGNED(256, static const InterpKernel,
                sub_pel_filters_8[SUBPEL_SHIFTS]) = {
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
                sub_pel_filters_regular_uv[SUBPEL_SHIFTS]) = {
  { 0, 0, 0, 128, 0, 0, 0, 0 },      { 0, 2, -6, 126, 8, -2, 0, 0 },
  { 0, 2, -10, 122, 18, -4, 0, 0 },  { 0, 2, -12, 116, 28, -8, 2, 0 },
  { 0, 2, -14, 110, 38, -10, 2, 0 }, { 0, 2, -14, 102, 48, -12, 2, 0 },
  { 0, 2, -16, 94, 58, -12, 2, 0 },  { 0, 2, -14, 84, 66, -12, 2, 0 },
  { 0, 2, -14, 76, 76, -14, 2, 0 },  { 0, 2, -12, 66, 84, -14, 2, 0 },
  { 0, 2, -12, 58, 94, -16, 2, 0 },  { 0, 2, -12, 48, 102, -14, 2, 0 },
  { 0, 2, -10, 38, 110, -14, 2, 0 }, { 0, 2, -8, 28, 116, -12, 2, 0 },
  { 0, 0, -4, 18, 122, -10, 2, 0 },  { 0, 0, -2, 8, 126, -6, 2, 0 }
};

#if USE_12TAP_FILTER
DECLARE_ALIGNED(256, static const InterpKernel,
                sub_pel_filters_8sharp[SUBPEL_SHIFTS]) = {
  // intfilt 0.8
  { 0, 0, 0, 128, 0, 0, 0, 0 },         { -1, 2, -6, 127, 9, -4, 2, -1 },
  { -2, 5, -12, 124, 18, -7, 4, -2 },   { -2, 7, -16, 119, 28, -11, 5, -2 },
  { -3, 8, -19, 114, 38, -14, 7, -3 },  { -3, 9, -22, 107, 49, -17, 8, -3 },
  { -4, 10, -23, 99, 60, -20, 10, -4 }, { -4, 11, -23, 90, 70, -22, 10, -4 },
  { -4, 11, -23, 80, 80, -23, 11, -4 }, { -4, 10, -22, 70, 90, -23, 11, -4 },
  { -4, 10, -20, 60, 99, -23, 10, -4 }, { -3, 8, -17, 49, 107, -22, 9, -3 },
  { -3, 7, -14, 38, 114, -19, 8, -3 },  { -2, 5, -11, 28, 119, -16, 7, -2 },
  { -2, 4, -7, 18, 124, -12, 5, -2 },   { -1, 2, -4, 9, 127, -6, 2, -1 },
};

DECLARE_ALIGNED(256, static const int16_t,
                sub_pel_filters_10sharp[SUBPEL_SHIFTS][12]) = {
  // intfilt 0.85
  { 0, 0, 0, 0, 0, 128, 0, 0, 0, 0, 0, 0 },
  { 0, 1, -2, 3, -7, 127, 8, -4, 2, -1, 1, 0 },
  { 0, 1, -3, 6, -13, 124, 18, -8, 4, -2, 1, 0 },
  { 0, 2, -4, 8, -18, 120, 28, -12, 6, -4, 2, 0 },
  { 0, 2, -5, 10, -21, 114, 38, -15, 8, -5, 2, 0 },
  { 0, 3, -6, 11, -24, 107, 49, -19, 10, -6, 3, 0 },
  { 0, 3, -7, 12, -25, 99, 59, -21, 11, -6, 3, 0 },
  { 0, 3, -7, 12, -25, 90, 70, -23, 12, -7, 3, 0 },
  { 0, 3, -7, 12, -25, 81, 81, -25, 12, -7, 3, 0 },
  { 0, 3, -7, 12, -23, 70, 90, -25, 12, -7, 3, 0 },
  { 0, 3, -6, 11, -21, 59, 99, -25, 12, -7, 3, 0 },
  { 0, 3, -6, 10, -19, 49, 107, -24, 11, -6, 3, 0 },
  { 0, 2, -5, 8, -15, 38, 114, -21, 10, -5, 2, 0 },
  { 0, 2, -4, 6, -12, 28, 120, -18, 8, -4, 2, 0 },
  { 0, 1, -2, 4, -8, 18, 124, -13, 6, -3, 1, 0 },
  { 0, 1, -1, 2, -4, 8, 127, -7, 3, -2, 1, 0 },
};
#else
DECLARE_ALIGNED(256, static const InterpKernel,
                sub_pel_filters_8sharp[SUBPEL_SHIFTS]) = {
  { 0, 0, 0, 128, 0, 0, 0, 0 },         { -2, 2, -6, 126, 8, -2, 2, 0 },
  { -2, 6, -12, 124, 16, -6, 4, -2 },   { -2, 8, -18, 120, 26, -10, 6, -2 },
  { -4, 10, -22, 116, 38, -14, 6, -2 }, { -4, 10, -22, 108, 48, -18, 8, -2 },
  { -4, 10, -24, 100, 60, -20, 8, -2 }, { -4, 10, -24, 90, 70, -22, 10, -2 },
  { -4, 12, -24, 80, 80, -24, 12, -4 }, { -2, 10, -22, 70, 90, -24, 10, -4 },
  { -2, 8, -20, 60, 100, -24, 10, -4 }, { -2, 8, -18, 48, 108, -22, 10, -4 },
  { -2, 6, -14, 38, 116, -22, 10, -4 }, { -2, 6, -10, 26, 120, -18, 8, -2 },
  { -2, 4, -6, 16, 124, -12, 6, -2 },   { 0, 2, -2, 8, 126, -6, 2, -2 }
};
#endif

DECLARE_ALIGNED(256, static const InterpKernel,
                sub_pel_filters_8smooth2[SUBPEL_SHIFTS]) = {
  // freqmultiplier = 0.2
  { 0, 0, 0, 128, 0, 0, 0, 0 },   { 0, 9, 30, 44, 32, 11, 2, 0 },
  { 0, 8, 28, 44, 34, 12, 2, 0 }, { 0, 7, 27, 44, 35, 13, 2, 0 },
  { 0, 6, 26, 43, 37, 14, 2, 0 }, { 0, 5, 24, 43, 38, 16, 2, 0 },
  { 0, 5, 23, 42, 38, 17, 3, 0 }, { 0, 4, 21, 41, 40, 19, 3, 0 },
  { 0, 4, 20, 40, 40, 20, 4, 0 }, { 0, 3, 19, 40, 41, 21, 4, 0 },
  { 0, 3, 17, 38, 42, 23, 5, 0 }, { 0, 2, 16, 38, 43, 24, 5, 0 },
  { 0, 2, 14, 37, 43, 26, 6, 0 }, { 0, 2, 13, 35, 44, 27, 7, 0 },
  { 0, 2, 12, 34, 44, 28, 8, 0 }, { 0, 2, 11, 32, 44, 30, 9, 0 },
};

DECLARE_ALIGNED(256, static const InterpKernel,
                sub_pel_filters_smooth2_uv[SUBPEL_SHIFTS]) = {
  // freqmultiplier = 0.2
  { 0, 0, 0, 128, 0, 0, 0, 0 },   { 0, 9, 30, 44, 32, 11, 2, 0 },
  { 0, 8, 28, 44, 34, 12, 2, 0 }, { 0, 7, 27, 44, 35, 13, 2, 0 },
  { 0, 6, 26, 43, 37, 14, 2, 0 }, { 0, 5, 24, 43, 38, 16, 2, 0 },
  { 0, 5, 23, 42, 38, 17, 3, 0 }, { 0, 4, 21, 41, 40, 19, 3, 0 },
  { 0, 4, 20, 40, 40, 20, 4, 0 }, { 0, 3, 19, 40, 41, 21, 4, 0 },
  { 0, 3, 17, 38, 42, 23, 5, 0 }, { 0, 2, 16, 38, 43, 24, 5, 0 },
  { 0, 2, 14, 37, 43, 26, 6, 0 }, { 0, 2, 13, 35, 44, 27, 7, 0 },
  { 0, 2, 12, 34, 44, 28, 8, 0 }, { 0, 2, 11, 32, 44, 30, 9, 0 },
};

DECLARE_ALIGNED(256, static const InterpKernel,
                sub_pel_filters_8smooth[SUBPEL_SHIFTS]) = {
  { 0, 0, 0, 128, 0, 0, 0, 0 },     { 0, 2, 28, 62, 34, 2, 0, 0 },
  { 0, 0, 26, 62, 36, 4, 0, 0 },    { 0, 0, 22, 62, 40, 4, 0, 0 },
  { 0, 0, 20, 60, 42, 6, 0, 0 },    { 0, 0, 18, 58, 44, 8, 0, 0 },
  { 0, 0, 16, 56, 46, 10, 0, 0 },   { 0, -2, 16, 54, 48, 12, 0, 0 },
  { 0, -2, 14, 52, 52, 14, -2, 0 }, { 0, 0, 12, 48, 54, 16, -2, 0 },
  { 0, 0, 10, 46, 56, 16, 0, 0 },   { 0, 0, 8, 44, 58, 18, 0, 0 },
  { 0, 0, 6, 42, 60, 20, 0, 0 },    { 0, 0, 4, 40, 62, 22, 0, 0 },
  { 0, 0, 4, 36, 62, 26, 0, 0 },    { 0, 0, 2, 34, 62, 28, 2, 0 }
};

DECLARE_ALIGNED(256, static const InterpKernel,
                sub_pel_filters_smooth_uv[SUBPEL_SHIFTS]) = {
  { 0, 0, 0, 128, 0, 0, 0, 0 },     { 0, 2, 28, 62, 34, 2, 0, 0 },
  { 0, 0, 26, 62, 36, 4, 0, 0 },    { 0, 0, 22, 62, 40, 4, 0, 0 },
  { 0, 0, 20, 60, 42, 6, 0, 0 },    { 0, 0, 18, 58, 44, 8, 0, 0 },
  { 0, 0, 16, 56, 46, 10, 0, 0 },   { 0, -2, 16, 54, 48, 12, 0, 0 },
  { 0, -2, 14, 52, 52, 14, -2, 0 }, { 0, 0, 12, 48, 54, 16, -2, 0 },
  { 0, 0, 10, 46, 56, 16, 0, 0 },   { 0, 0, 8, 44, 58, 18, 0, 0 },
  { 0, 0, 6, 42, 60, 20, 0, 0 },    { 0, 0, 4, 40, 62, 22, 0, 0 },
  { 0, 0, 4, 36, 62, 26, 0, 0 },    { 0, 0, 2, 34, 62, 28, 2, 0 }
};
#else   // USE_EXTRA_FILTER

DECLARE_ALIGNED(256, static const InterpKernel,
                sub_pel_filters_8[SUBPEL_SHIFTS]) = {
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
                sub_pel_filters_8sharp[SUBPEL_SHIFTS]) = {
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
                sub_pel_filters_8smooth[SUBPEL_SHIFTS]) = {
  { 0, 0, 0, 128, 0, 0, 0, 0 },     { 0, 2, 28, 62, 34, 2, 0, 0 },
  { 0, 0, 26, 62, 36, 4, 0, 0 },    { 0, 0, 22, 62, 40, 4, 0, 0 },
  { 0, 0, 20, 60, 42, 6, 0, 0 },    { 0, 0, 18, 58, 44, 8, 0, 0 },
  { 0, 0, 16, 56, 46, 10, 0, 0 },   { 0, -2, 16, 54, 48, 12, 0, 0 },
  { 0, -2, 14, 52, 52, 14, -2, 0 }, { 0, 0, 12, 48, 54, 16, -2, 0 },
  { 0, 0, 10, 46, 56, 16, 0, 0 },   { 0, 0, 8, 44, 58, 18, 0, 0 },
  { 0, 0, 6, 42, 60, 20, 0, 0 },    { 0, 0, 4, 40, 62, 22, 0, 0 },
  { 0, 0, 4, 36, 62, 26, 0, 0 },    { 0, 0, 2, 34, 62, 28, 2, 0 }
};
#endif  // USE_EXTRA_FILTER

#if CONFIG_EXT_INTRA
#if CONFIG_INTRA_INTERP
const InterpKernel *av1_intra_filter_kernels[INTRA_FILTERS] = {
  bilinear_filters,         // INTRA_FILTER_LINEAR
  sub_pel_filters_8,        // INTRA_FILTER_8TAP
  sub_pel_filters_8sharp,   // INTRA_FILTER_8TAP_SHARP
  sub_pel_filters_8smooth,  // INTRA_FILTER_8TAP_SMOOTH
};
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA

#if USE_EXTRA_FILTER
static const InterpFilterParams
    av1_interp_filter_params_list[SWITCHABLE_FILTERS + EXTRA_FILTERS] = {
      { (const int16_t *)sub_pel_filters_8, SUBPEL_TAPS, SUBPEL_SHIFTS,
        EIGHTTAP_REGULAR },
      { (const int16_t *)sub_pel_filters_8smooth, SUBPEL_TAPS, SUBPEL_SHIFTS,
        EIGHTTAP_SMOOTH },
#if USE_12TAP_FILTER
      { (const int16_t *)sub_pel_filters_10sharp, 12, SUBPEL_SHIFTS,
        MULTITAP_SHARP },
#else
      { (const int16_t *)sub_pel_filters_8sharp, SUBPEL_TAPS, SUBPEL_SHIFTS,
        EIGHTTAP_SHARP },
#endif
      { (const int16_t *)sub_pel_filters_8smooth2, SUBPEL_TAPS, SUBPEL_SHIFTS,
        EIGHTTAP_SMOOTH2 },
      { (const int16_t *)bilinear_filters, SUBPEL_TAPS, SUBPEL_SHIFTS,
        BILINEAR },
      { (const int16_t *)sub_pel_filters_8sharp, SUBPEL_TAPS, SUBPEL_SHIFTS,
        EIGHTTAP_SHARP },
      { (const int16_t *)sub_pel_filters_regular_uv, SUBPEL_TAPS, SUBPEL_SHIFTS,
        FILTER_REGULAR_UV },
      { (const int16_t *)sub_pel_filters_smooth_uv, SUBPEL_TAPS, SUBPEL_SHIFTS,
        FILTER_SMOOTH_UV },
      { (const int16_t *)sub_pel_filters_8sharp, SUBPEL_TAPS, SUBPEL_SHIFTS,
        FILTER_SHARP_UV },
      { (const int16_t *)sub_pel_filters_smooth2_uv, SUBPEL_TAPS, SUBPEL_SHIFTS,
        FILTER_SMOOTH2_UV },
    };
#else
static const InterpFilterParams
    av1_interp_filter_params_list[SWITCHABLE_FILTERS + 1] = {
      { (const int16_t *)sub_pel_filters_8, SUBPEL_TAPS, SUBPEL_SHIFTS,
        EIGHTTAP_REGULAR },
      { (const int16_t *)sub_pel_filters_8smooth, SUBPEL_TAPS, SUBPEL_SHIFTS,
        EIGHTTAP_SMOOTH },
      { (const int16_t *)sub_pel_filters_8sharp, SUBPEL_TAPS, SUBPEL_SHIFTS,
        MULTITAP_SHARP },
      { (const int16_t *)bilinear_filters, SUBPEL_TAPS, SUBPEL_SHIFTS,
        BILINEAR }
    };
#endif  // USE_EXTRA_FILTER

#if USE_TEMPORALFILTER_12TAP
static const InterpFilterParams av1_interp_temporalfilter_12tap = {
  (const int16_t *)sub_pel_filters_temporalfilter_12, 12, SUBPEL_SHIFTS,
  TEMPORALFILTER_12TAP
};
#endif  // USE_TEMPORALFILTER_12TAP

InterpFilterParams av1_get_interp_filter_params(
    const InterpFilter interp_filter) {
#if USE_TEMPORALFILTER_12TAP
  if (interp_filter == TEMPORALFILTER_12TAP)
    return av1_interp_temporalfilter_12tap;
#endif  // USE_TEMPORALFILTER_12TAP
  return av1_interp_filter_params_list[interp_filter];
}

const int16_t *av1_get_interp_filter_kernel(const InterpFilter interp_filter) {
#if USE_TEMPORALFILTER_12TAP
  if (interp_filter == TEMPORALFILTER_12TAP)
    return av1_interp_temporalfilter_12tap.filter_ptr;
#endif  // USE_TEMPORALFILTER_12TAP
  return (const int16_t *)av1_interp_filter_params_list[interp_filter]
      .filter_ptr;
}

#if CONFIG_DUAL_FILTER
InterpFilter av1_get_plane_interp_filter(InterpFilter interp_filter,
                                         int plane) {
#if USE_TEMPORALFILTER_12TAP
#if USE_EXTRA_FILTER
  assert(interp_filter <= EIGHTTAP_SHARP ||
         interp_filter == TEMPORALFILTER_12TAP);
#else   // USE_EXTRA_FILTER
  assert(interp_filter <= SWITCHABLE_FILTERS ||
         interp_filter == TEMPORALFILTER_12TAP);
#endif  // USE_EXTRA_FILTER
#else
  assert(interp_filter <= EIGHTTAP_SHARP);
#endif
#if USE_EXTRA_FILTER
  if (plane == 0) {
    return interp_filter;
  } else {
    switch (interp_filter) {
      case EIGHTTAP_REGULAR: return FILTER_REGULAR_UV;
      case EIGHTTAP_SMOOTH: return FILTER_SMOOTH_UV;
      case MULTITAP_SHARP: return FILTER_SHARP_UV;
      case EIGHTTAP_SMOOTH2: return FILTER_SMOOTH2_UV;
      default: return interp_filter;
    }
  }
#else   // USE_EXTRA_FILTER
  (void)plane;
  return interp_filter;
#endif  // USE_EXTRA_FILTER
}
#endif
