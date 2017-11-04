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
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./aom_config.h"
#if CONFIG_HIGHBITDEPTH
#include "aom_dsp/aom_dsp_common.h"
#endif  // CONFIG_HIGHBITDEPTH
#include "aom_ports/mem.h"
#include "aom_scale/aom_scale.h"
#include "av1/common/common.h"
#include "av1/common/resize.h"

#include "./aom_scale_rtcd.h"

#define FILTER_BITS 7

#define INTERP_TAPS 8
#define SUBPEL_BITS_RS 6
#define SUBPEL_MASK_RS ((1 << SUBPEL_BITS_RS) - 1)
#define INTERP_PRECISION_BITS 16
#define SUBPEL_INTERP_EXTRA_BITS (INTERP_PRECISION_BITS - SUBPEL_BITS_RS)
#define SUBPEL_INTERP_EXTRA_OFF (1 << (SUBPEL_INTERP_EXTRA_BITS - 1))

typedef int16_t interp_kernel[INTERP_TAPS];

// Filters for interpolation (0.5-band) - note this also filters integer pels.
static const interp_kernel filteredinterp_filters500[(1 << SUBPEL_BITS_RS)] = {
  { -3, 0, 35, 64, 35, 0, -3, 0 },    { -3, 0, 34, 64, 36, 0, -3, 0 },
  { -3, -1, 34, 64, 36, 1, -3, 0 },   { -3, -1, 33, 64, 37, 1, -3, 0 },
  { -3, -1, 32, 64, 38, 1, -3, 0 },   { -3, -1, 31, 64, 39, 1, -3, 0 },
  { -3, -1, 31, 63, 39, 2, -3, 0 },   { -2, -2, 30, 63, 40, 2, -3, 0 },
  { -2, -2, 29, 63, 41, 2, -3, 0 },   { -2, -2, 29, 63, 41, 3, -4, 0 },
  { -2, -2, 28, 63, 42, 3, -4, 0 },   { -2, -2, 27, 63, 43, 3, -4, 0 },
  { -2, -3, 27, 63, 43, 4, -4, 0 },   { -2, -3, 26, 62, 44, 5, -4, 0 },
  { -2, -3, 25, 62, 45, 5, -4, 0 },   { -2, -3, 25, 62, 45, 5, -4, 0 },
  { -2, -3, 24, 62, 46, 5, -4, 0 },   { -2, -3, 23, 61, 47, 6, -4, 0 },
  { -2, -3, 23, 61, 47, 6, -4, 0 },   { -2, -3, 22, 61, 48, 7, -4, -1 },
  { -2, -3, 21, 60, 49, 7, -4, 0 },   { -1, -4, 20, 60, 49, 8, -4, 0 },
  { -1, -4, 20, 60, 50, 8, -4, -1 },  { -1, -4, 19, 59, 51, 9, -4, -1 },
  { -1, -4, 19, 59, 51, 9, -4, -1 },  { -1, -4, 18, 58, 52, 10, -4, -1 },
  { -1, -4, 17, 58, 52, 11, -4, -1 }, { -1, -4, 16, 58, 53, 11, -4, -1 },
  { -1, -4, 16, 57, 53, 12, -4, -1 }, { -1, -4, 15, 57, 54, 12, -4, -1 },
  { -1, -4, 15, 56, 54, 13, -4, -1 }, { -1, -4, 14, 56, 55, 13, -4, -1 },
  { -1, -4, 14, 55, 55, 14, -4, -1 }, { -1, -4, 13, 55, 56, 14, -4, -1 },
  { -1, -4, 13, 54, 56, 15, -4, -1 }, { -1, -4, 12, 54, 57, 15, -4, -1 },
  { -1, -4, 12, 53, 57, 16, -4, -1 }, { -1, -4, 11, 53, 58, 16, -4, -1 },
  { -1, -4, 11, 52, 58, 17, -4, -1 }, { -1, -4, 10, 52, 58, 18, -4, -1 },
  { -1, -4, 9, 51, 59, 19, -4, -1 },  { -1, -4, 9, 51, 59, 19, -4, -1 },
  { -1, -4, 8, 50, 60, 20, -4, -1 },  { 0, -4, 8, 49, 60, 20, -4, -1 },
  { 0, -4, 7, 49, 60, 21, -3, -2 },   { -1, -4, 7, 48, 61, 22, -3, -2 },
  { 0, -4, 6, 47, 61, 23, -3, -2 },   { 0, -4, 6, 47, 61, 23, -3, -2 },
  { 0, -4, 5, 46, 62, 24, -3, -2 },   { 0, -4, 5, 45, 62, 25, -3, -2 },
  { 0, -4, 5, 45, 62, 25, -3, -2 },   { 0, -4, 5, 44, 62, 26, -3, -2 },
  { 0, -4, 4, 43, 63, 27, -3, -2 },   { 0, -4, 3, 43, 63, 27, -2, -2 },
  { 0, -4, 3, 42, 63, 28, -2, -2 },   { 0, -4, 3, 41, 63, 29, -2, -2 },
  { 0, -3, 2, 41, 63, 29, -2, -2 },   { 0, -3, 2, 40, 63, 30, -2, -2 },
  { 0, -3, 2, 39, 63, 31, -1, -3 },   { 0, -3, 1, 39, 64, 31, -1, -3 },
  { 0, -3, 1, 38, 64, 32, -1, -3 },   { 0, -3, 1, 37, 64, 33, -1, -3 },
  { 0, -3, 1, 36, 64, 34, -1, -3 },   { 0, -3, 0, 36, 64, 34, 0, -3 },
};

// Filters for interpolation (0.625-band) - note this also filters integer pels.
static const interp_kernel filteredinterp_filters625[(1 << SUBPEL_BITS_RS)] = {
  { -1, -8, 33, 80, 33, -8, -1, 0 }, { -1, -8, 31, 80, 34, -8, -1, 1 },
  { -1, -8, 30, 80, 35, -8, -1, 1 }, { -1, -8, 29, 80, 36, -7, -2, 1 },
  { -1, -8, 28, 80, 37, -7, -2, 1 }, { -1, -8, 27, 80, 38, -7, -2, 1 },
  { 0, -8, 26, 79, 39, -7, -2, 1 },  { 0, -8, 25, 79, 40, -7, -2, 1 },
  { 0, -8, 24, 79, 41, -7, -2, 1 },  { 0, -8, 23, 78, 42, -6, -2, 1 },
  { 0, -8, 22, 78, 43, -6, -2, 1 },  { 0, -8, 21, 78, 44, -6, -2, 1 },
  { 0, -8, 20, 78, 45, -5, -3, 1 },  { 0, -8, 19, 77, 47, -5, -3, 1 },
  { 0, -8, 18, 77, 48, -5, -3, 1 },  { 0, -8, 17, 77, 49, -5, -3, 1 },
  { 0, -8, 16, 76, 50, -4, -3, 1 },  { 0, -8, 15, 76, 51, -4, -3, 1 },
  { 0, -8, 15, 75, 52, -3, -4, 1 },  { 0, -7, 14, 74, 53, -3, -4, 1 },
  { 0, -7, 13, 74, 54, -3, -4, 1 },  { 0, -7, 12, 73, 55, -2, -4, 1 },
  { 0, -7, 11, 73, 56, -2, -4, 1 },  { 0, -7, 10, 72, 57, -1, -4, 1 },
  { 1, -7, 10, 71, 58, -1, -5, 1 },  { 0, -7, 9, 71, 59, 0, -5, 1 },
  { 1, -7, 8, 70, 60, 0, -5, 1 },    { 1, -7, 7, 69, 61, 1, -5, 1 },
  { 1, -6, 6, 68, 62, 1, -5, 1 },    { 0, -6, 6, 68, 62, 2, -5, 1 },
  { 1, -6, 5, 67, 63, 2, -5, 1 },    { 1, -6, 5, 66, 64, 3, -6, 1 },
  { 1, -6, 4, 65, 65, 4, -6, 1 },    { 1, -6, 3, 64, 66, 5, -6, 1 },
  { 1, -5, 2, 63, 67, 5, -6, 1 },    { 1, -5, 2, 62, 68, 6, -6, 0 },
  { 1, -5, 1, 62, 68, 6, -6, 1 },    { 1, -5, 1, 61, 69, 7, -7, 1 },
  { 1, -5, 0, 60, 70, 8, -7, 1 },    { 1, -5, 0, 59, 71, 9, -7, 0 },
  { 1, -5, -1, 58, 71, 10, -7, 1 },  { 1, -4, -1, 57, 72, 10, -7, 0 },
  { 1, -4, -2, 56, 73, 11, -7, 0 },  { 1, -4, -2, 55, 73, 12, -7, 0 },
  { 1, -4, -3, 54, 74, 13, -7, 0 },  { 1, -4, -3, 53, 74, 14, -7, 0 },
  { 1, -4, -3, 52, 75, 15, -8, 0 },  { 1, -3, -4, 51, 76, 15, -8, 0 },
  { 1, -3, -4, 50, 76, 16, -8, 0 },  { 1, -3, -5, 49, 77, 17, -8, 0 },
  { 1, -3, -5, 48, 77, 18, -8, 0 },  { 1, -3, -5, 47, 77, 19, -8, 0 },
  { 1, -3, -5, 45, 78, 20, -8, 0 },  { 1, -2, -6, 44, 78, 21, -8, 0 },
  { 1, -2, -6, 43, 78, 22, -8, 0 },  { 1, -2, -6, 42, 78, 23, -8, 0 },
  { 1, -2, -7, 41, 79, 24, -8, 0 },  { 1, -2, -7, 40, 79, 25, -8, 0 },
  { 1, -2, -7, 39, 79, 26, -8, 0 },  { 1, -2, -7, 38, 80, 27, -8, -1 },
  { 1, -2, -7, 37, 80, 28, -8, -1 }, { 1, -2, -7, 36, 80, 29, -8, -1 },
  { 1, -1, -8, 35, 80, 30, -8, -1 }, { 1, -1, -8, 34, 80, 31, -8, -1 },
};

// Filters for interpolation (0.75-band) - note this also filters integer pels.
static const interp_kernel filteredinterp_filters750[(1 << SUBPEL_BITS_RS)] = {
  { 2, -11, 25, 96, 25, -11, 2, 0 }, { 2, -11, 24, 96, 26, -11, 2, 0 },
  { 2, -11, 22, 96, 28, -11, 2, 0 }, { 2, -10, 21, 96, 29, -12, 2, 0 },
  { 2, -10, 19, 96, 31, -12, 2, 0 }, { 2, -10, 18, 95, 32, -11, 2, 0 },
  { 2, -10, 17, 95, 34, -12, 2, 0 }, { 2, -9, 15, 95, 35, -12, 2, 0 },
  { 2, -9, 14, 94, 37, -12, 2, 0 },  { 2, -9, 13, 94, 38, -12, 2, 0 },
  { 2, -8, 12, 93, 40, -12, 1, 0 },  { 2, -8, 11, 93, 41, -12, 1, 0 },
  { 2, -8, 9, 92, 43, -12, 1, 1 },   { 2, -8, 8, 92, 44, -12, 1, 1 },
  { 2, -7, 7, 91, 46, -12, 1, 0 },   { 2, -7, 6, 90, 47, -12, 1, 1 },
  { 2, -7, 5, 90, 49, -12, 1, 0 },   { 2, -6, 4, 89, 50, -12, 1, 0 },
  { 2, -6, 3, 88, 52, -12, 0, 1 },   { 2, -6, 2, 87, 54, -12, 0, 1 },
  { 2, -5, 1, 86, 55, -12, 0, 1 },   { 2, -5, 0, 85, 57, -12, 0, 1 },
  { 2, -5, -1, 84, 58, -11, 0, 1 },  { 2, -5, -2, 83, 60, -11, 0, 1 },
  { 2, -4, -2, 82, 61, -11, -1, 1 }, { 1, -4, -3, 81, 63, -10, -1, 1 },
  { 2, -4, -4, 80, 64, -10, -1, 1 }, { 1, -4, -4, 79, 66, -10, -1, 1 },
  { 1, -3, -5, 77, 67, -9, -1, 1 },  { 1, -3, -6, 76, 69, -9, -1, 1 },
  { 1, -3, -6, 75, 70, -8, -2, 1 },  { 1, -2, -7, 74, 71, -8, -2, 1 },
  { 1, -2, -7, 72, 72, -7, -2, 1 },  { 1, -2, -8, 71, 74, -7, -2, 1 },
  { 1, -2, -8, 70, 75, -6, -3, 1 },  { 1, -1, -9, 69, 76, -6, -3, 1 },
  { 1, -1, -9, 67, 77, -5, -3, 1 },  { 1, -1, -10, 66, 79, -4, -4, 1 },
  { 1, -1, -10, 64, 80, -4, -4, 2 }, { 1, -1, -10, 63, 81, -3, -4, 1 },
  { 1, -1, -11, 61, 82, -2, -4, 2 }, { 1, 0, -11, 60, 83, -2, -5, 2 },
  { 1, 0, -11, 58, 84, -1, -5, 2 },  { 1, 0, -12, 57, 85, 0, -5, 2 },
  { 1, 0, -12, 55, 86, 1, -5, 2 },   { 1, 0, -12, 54, 87, 2, -6, 2 },
  { 1, 0, -12, 52, 88, 3, -6, 2 },   { 0, 1, -12, 50, 89, 4, -6, 2 },
  { 0, 1, -12, 49, 90, 5, -7, 2 },   { 1, 1, -12, 47, 90, 6, -7, 2 },
  { 0, 1, -12, 46, 91, 7, -7, 2 },   { 1, 1, -12, 44, 92, 8, -8, 2 },
  { 1, 1, -12, 43, 92, 9, -8, 2 },   { 0, 1, -12, 41, 93, 11, -8, 2 },
  { 0, 1, -12, 40, 93, 12, -8, 2 },  { 0, 2, -12, 38, 94, 13, -9, 2 },
  { 0, 2, -12, 37, 94, 14, -9, 2 },  { 0, 2, -12, 35, 95, 15, -9, 2 },
  { 0, 2, -12, 34, 95, 17, -10, 2 }, { 0, 2, -11, 32, 95, 18, -10, 2 },
  { 0, 2, -12, 31, 96, 19, -10, 2 }, { 0, 2, -12, 29, 96, 21, -10, 2 },
  { 0, 2, -11, 28, 96, 22, -11, 2 }, { 0, 2, -11, 26, 96, 24, -11, 2 },
};

// Filters for interpolation (0.875-band) - note this also filters integer pels.
static const interp_kernel filteredinterp_filters875[(1 << SUBPEL_BITS_RS)] = {
  { 3, -8, 13, 112, 13, -8, 3, 0 },   { 2, -7, 12, 112, 15, -8, 3, -1 },
  { 3, -7, 10, 112, 17, -9, 3, -1 },  { 2, -6, 8, 112, 19, -9, 3, -1 },
  { 2, -6, 7, 112, 21, -10, 3, -1 },  { 2, -5, 6, 111, 22, -10, 3, -1 },
  { 2, -5, 4, 111, 24, -10, 3, -1 },  { 2, -4, 3, 110, 26, -11, 3, -1 },
  { 2, -4, 1, 110, 28, -11, 3, -1 },  { 2, -4, 0, 109, 30, -12, 4, -1 },
  { 1, -3, -1, 108, 32, -12, 4, -1 }, { 1, -3, -2, 108, 34, -13, 4, -1 },
  { 1, -2, -4, 107, 36, -13, 4, -1 }, { 1, -2, -5, 106, 38, -13, 4, -1 },
  { 1, -1, -6, 105, 40, -14, 4, -1 }, { 1, -1, -7, 104, 42, -14, 4, -1 },
  { 1, -1, -7, 103, 44, -15, 4, -1 }, { 1, 0, -8, 101, 46, -15, 4, -1 },
  { 1, 0, -9, 100, 48, -15, 4, -1 },  { 1, 0, -10, 99, 50, -15, 4, -1 },
  { 1, 1, -11, 97, 53, -16, 4, -1 },  { 0, 1, -11, 96, 55, -16, 4, -1 },
  { 0, 1, -12, 95, 57, -16, 4, -1 },  { 0, 2, -13, 93, 59, -16, 4, -1 },
  { 0, 2, -13, 91, 61, -16, 4, -1 },  { 0, 2, -14, 90, 63, -16, 4, -1 },
  { 0, 2, -14, 88, 65, -16, 4, -1 },  { 0, 2, -15, 86, 67, -16, 4, 0 },
  { 0, 3, -15, 84, 69, -17, 4, 0 },   { 0, 3, -16, 83, 71, -17, 4, 0 },
  { 0, 3, -16, 81, 73, -16, 3, 0 },   { 0, 3, -16, 79, 75, -16, 3, 0 },
  { 0, 3, -16, 77, 77, -16, 3, 0 },   { 0, 3, -16, 75, 79, -16, 3, 0 },
  { 0, 3, -16, 73, 81, -16, 3, 0 },   { 0, 4, -17, 71, 83, -16, 3, 0 },
  { 0, 4, -17, 69, 84, -15, 3, 0 },   { 0, 4, -16, 67, 86, -15, 2, 0 },
  { -1, 4, -16, 65, 88, -14, 2, 0 },  { -1, 4, -16, 63, 90, -14, 2, 0 },
  { -1, 4, -16, 61, 91, -13, 2, 0 },  { -1, 4, -16, 59, 93, -13, 2, 0 },
  { -1, 4, -16, 57, 95, -12, 1, 0 },  { -1, 4, -16, 55, 96, -11, 1, 0 },
  { -1, 4, -16, 53, 97, -11, 1, 1 },  { -1, 4, -15, 50, 99, -10, 0, 1 },
  { -1, 4, -15, 48, 100, -9, 0, 1 },  { -1, 4, -15, 46, 101, -8, 0, 1 },
  { -1, 4, -15, 44, 103, -7, -1, 1 }, { -1, 4, -14, 42, 104, -7, -1, 1 },
  { -1, 4, -14, 40, 105, -6, -1, 1 }, { -1, 4, -13, 38, 106, -5, -2, 1 },
  { -1, 4, -13, 36, 107, -4, -2, 1 }, { -1, 4, -13, 34, 108, -2, -3, 1 },
  { -1, 4, -12, 32, 108, -1, -3, 1 }, { -1, 4, -12, 30, 109, 0, -4, 2 },
  { -1, 3, -11, 28, 110, 1, -4, 2 },  { -1, 3, -11, 26, 110, 3, -4, 2 },
  { -1, 3, -10, 24, 111, 4, -5, 2 },  { -1, 3, -10, 22, 111, 6, -5, 2 },
  { -1, 3, -10, 21, 112, 7, -6, 2 },  { -1, 3, -9, 19, 112, 8, -6, 2 },
  { -1, 3, -9, 17, 112, 10, -7, 3 },  { -1, 3, -8, 15, 112, 12, -7, 2 },
};

// Filters for interpolation (full-band) - no filtering for integer pixels
static const interp_kernel filteredinterp_filters1000[(1 << SUBPEL_BITS_RS)] = {
  { 0, 0, 0, 128, 0, 0, 0, 0 },        { 0, 0, -1, 128, 2, -1, 0, 0 },
  { 0, 1, -3, 127, 4, -2, 1, 0 },      { 0, 1, -4, 127, 6, -3, 1, 0 },
  { 0, 2, -6, 126, 8, -3, 1, 0 },      { 0, 2, -7, 125, 11, -4, 1, 0 },
  { -1, 2, -8, 125, 13, -5, 2, 0 },    { -1, 3, -9, 124, 15, -6, 2, 0 },
  { -1, 3, -10, 123, 18, -6, 2, -1 },  { -1, 3, -11, 122, 20, -7, 3, -1 },
  { -1, 4, -12, 121, 22, -8, 3, -1 },  { -1, 4, -13, 120, 25, -9, 3, -1 },
  { -1, 4, -14, 118, 28, -9, 3, -1 },  { -1, 4, -15, 117, 30, -10, 4, -1 },
  { -1, 5, -16, 116, 32, -11, 4, -1 }, { -1, 5, -16, 114, 35, -12, 4, -1 },
  { -1, 5, -17, 112, 38, -12, 4, -1 }, { -1, 5, -18, 111, 40, -13, 5, -1 },
  { -1, 5, -18, 109, 43, -14, 5, -1 }, { -1, 6, -19, 107, 45, -14, 5, -1 },
  { -1, 6, -19, 105, 48, -15, 5, -1 }, { -1, 6, -19, 103, 51, -16, 5, -1 },
  { -1, 6, -20, 101, 53, -16, 6, -1 }, { -1, 6, -20, 99, 56, -17, 6, -1 },
  { -1, 6, -20, 97, 58, -17, 6, -1 },  { -1, 6, -20, 95, 61, -18, 6, -1 },
  { -2, 7, -20, 93, 64, -18, 6, -2 },  { -2, 7, -20, 91, 66, -19, 6, -1 },
  { -2, 7, -20, 88, 69, -19, 6, -1 },  { -2, 7, -20, 86, 71, -19, 6, -1 },
  { -2, 7, -20, 84, 74, -20, 7, -2 },  { -2, 7, -20, 81, 76, -20, 7, -1 },
  { -2, 7, -20, 79, 79, -20, 7, -2 },  { -1, 7, -20, 76, 81, -20, 7, -2 },
  { -2, 7, -20, 74, 84, -20, 7, -2 },  { -1, 6, -19, 71, 86, -20, 7, -2 },
  { -1, 6, -19, 69, 88, -20, 7, -2 },  { -1, 6, -19, 66, 91, -20, 7, -2 },
  { -2, 6, -18, 64, 93, -20, 7, -2 },  { -1, 6, -18, 61, 95, -20, 6, -1 },
  { -1, 6, -17, 58, 97, -20, 6, -1 },  { -1, 6, -17, 56, 99, -20, 6, -1 },
  { -1, 6, -16, 53, 101, -20, 6, -1 }, { -1, 5, -16, 51, 103, -19, 6, -1 },
  { -1, 5, -15, 48, 105, -19, 6, -1 }, { -1, 5, -14, 45, 107, -19, 6, -1 },
  { -1, 5, -14, 43, 109, -18, 5, -1 }, { -1, 5, -13, 40, 111, -18, 5, -1 },
  { -1, 4, -12, 38, 112, -17, 5, -1 }, { -1, 4, -12, 35, 114, -16, 5, -1 },
  { -1, 4, -11, 32, 116, -16, 5, -1 }, { -1, 4, -10, 30, 117, -15, 4, -1 },
  { -1, 3, -9, 28, 118, -14, 4, -1 },  { -1, 3, -9, 25, 120, -13, 4, -1 },
  { -1, 3, -8, 22, 121, -12, 4, -1 },  { -1, 3, -7, 20, 122, -11, 3, -1 },
  { -1, 2, -6, 18, 123, -10, 3, -1 },  { 0, 2, -6, 15, 124, -9, 3, -1 },
  { 0, 2, -5, 13, 125, -8, 2, -1 },    { 0, 1, -4, 11, 125, -7, 2, 0 },
  { 0, 1, -3, 8, 126, -6, 2, 0 },      { 0, 1, -3, 6, 127, -4, 1, 0 },
  { 0, 1, -2, 4, 127, -3, 1, 0 },      { 0, 0, -1, 2, 128, -1, 0, 0 },
};

#if CONFIG_FRAME_SUPERRES && CONFIG_LOOP_RESTORATION
#define INTERP_SIMPLE_TAPS 4
static const int16_t filter_simple[(1
                                    << SUBPEL_BITS_RS)][INTERP_SIMPLE_TAPS] = {
#if INTERP_SIMPLE_TAPS == 2
  { 128, 0 },  { 126, 2 },  { 124, 4 },  { 122, 6 },  { 120, 8 },  { 118, 10 },
  { 116, 12 }, { 114, 14 }, { 112, 16 }, { 110, 18 }, { 108, 20 }, { 106, 22 },
  { 104, 24 }, { 102, 26 }, { 100, 28 }, { 98, 30 },  { 96, 32 },  { 94, 34 },
  { 92, 36 },  { 90, 38 },  { 88, 40 },  { 86, 42 },  { 84, 44 },  { 82, 46 },
  { 80, 48 },  { 78, 50 },  { 76, 52 },  { 74, 54 },  { 72, 56 },  { 70, 58 },
  { 68, 60 },  { 66, 62 },  { 64, 64 },  { 62, 66 },  { 60, 68 },  { 58, 70 },
  { 56, 72 },  { 54, 74 },  { 52, 76 },  { 50, 78 },  { 48, 80 },  { 46, 82 },
  { 44, 84 },  { 42, 86 },  { 40, 88 },  { 38, 90 },  { 36, 92 },  { 34, 94 },
  { 32, 96 },  { 30, 98 },  { 28, 100 }, { 26, 102 }, { 24, 104 }, { 22, 106 },
  { 20, 108 }, { 18, 110 }, { 16, 112 }, { 14, 114 }, { 12, 116 }, { 10, 118 },
  { 8, 120 },  { 6, 122 },  { 4, 124 },  { 2, 126 },
#elif INTERP_SIMPLE_TAPS == 4
  { 0, 128, 0, 0 },      { -1, 128, 2, -1 },    { -2, 127, 4, -1 },
  { -3, 126, 7, -2 },    { -4, 125, 9, -2 },    { -5, 125, 11, -3 },
  { -6, 124, 13, -3 },   { -7, 123, 16, -4 },   { -7, 122, 18, -5 },
  { -8, 121, 20, -5 },   { -9, 120, 23, -6 },   { -9, 118, 25, -6 },
  { -10, 117, 28, -7 },  { -11, 116, 30, -7 },  { -11, 114, 33, -8 },
  { -12, 113, 35, -8 },  { -12, 111, 38, -9 },  { -13, 109, 41, -9 },
  { -13, 108, 43, -10 }, { -13, 106, 45, -10 }, { -13, 104, 48, -11 },
  { -14, 102, 51, -11 }, { -14, 100, 53, -11 }, { -14, 98, 56, -12 },
  { -14, 96, 58, -12 },  { -14, 94, 61, -13 },  { -15, 92, 64, -13 },
  { -15, 90, 66, -13 },  { -15, 87, 69, -13 },  { -14, 85, 71, -14 },
  { -14, 83, 73, -14 },  { -14, 80, 76, -14 },  { -14, 78, 78, -14 },
  { -14, 76, 80, -14 },  { -14, 73, 83, -14 },  { -14, 71, 85, -14 },
  { -13, 69, 87, -15 },  { -13, 66, 90, -15 },  { -13, 64, 92, -15 },
  { -13, 61, 94, -14 },  { -12, 58, 96, -14 },  { -12, 56, 98, -14 },
  { -11, 53, 100, -14 }, { -11, 51, 102, -14 }, { -11, 48, 104, -13 },
  { -10, 45, 106, -13 }, { -10, 43, 108, -13 }, { -9, 41, 109, -13 },
  { -9, 38, 111, -12 },  { -8, 35, 113, -12 },  { -8, 33, 114, -11 },
  { -7, 30, 116, -11 },  { -7, 28, 117, -10 },  { -6, 25, 118, -9 },
  { -6, 23, 120, -9 },   { -5, 20, 121, -8 },   { -5, 18, 122, -7 },
  { -4, 16, 123, -7 },   { -3, 13, 124, -6 },   { -3, 11, 125, -5 },
  { -2, 9, 125, -4 },    { -2, 7, 126, -3 },    { -1, 4, 127, -2 },
  { -1, 2, 128, -1 },
#elif INTERP_SIMPLE_TAPS == 6
  { 0, 0, 128, 0, 0, 0 },      { 0, -1, 128, 2, -1, 0 },
  { 1, -3, 127, 4, -2, 1 },    { 1, -4, 127, 6, -3, 1 },
  { 2, -6, 126, 8, -3, 1 },    { 2, -7, 125, 11, -4, 1 },
  { 2, -9, 125, 13, -5, 2 },   { 3, -10, 124, 15, -6, 2 },
  { 3, -11, 123, 18, -7, 2 },  { 3, -12, 122, 20, -8, 3 },
  { 4, -13, 121, 22, -9, 3 },  { 4, -14, 119, 25, -9, 3 },
  { 4, -15, 118, 27, -10, 4 }, { 4, -16, 117, 30, -11, 4 },
  { 5, -17, 116, 32, -12, 4 }, { 5, -17, 114, 35, -13, 4 },
  { 5, -18, 112, 37, -13, 5 }, { 5, -19, 111, 40, -14, 5 },
  { 6, -19, 109, 42, -15, 5 }, { 6, -20, 107, 45, -15, 5 },
  { 6, -20, 105, 48, -16, 5 }, { 6, -21, 103, 51, -17, 6 },
  { 6, -21, 101, 53, -17, 6 }, { 6, -21, 99, 56, -18, 6 },
  { 7, -22, 97, 58, -18, 6 },  { 7, -22, 95, 61, -19, 6 },
  { 7, -22, 93, 63, -19, 6 },  { 7, -22, 91, 66, -20, 6 },
  { 7, -22, 88, 69, -20, 6 },  { 7, -22, 86, 71, -21, 7 },
  { 7, -22, 83, 74, -21, 7 },  { 7, -22, 81, 76, -21, 7 },
  { 7, -22, 79, 79, -22, 7 },  { 7, -21, 76, 81, -22, 7 },
  { 7, -21, 74, 83, -22, 7 },  { 7, -21, 71, 86, -22, 7 },
  { 6, -20, 69, 88, -22, 7 },  { 6, -20, 66, 91, -22, 7 },
  { 6, -19, 63, 93, -22, 7 },  { 6, -19, 61, 95, -22, 7 },
  { 6, -18, 58, 97, -22, 7 },  { 6, -18, 56, 99, -21, 6 },
  { 6, -17, 53, 101, -21, 6 }, { 6, -17, 51, 103, -21, 6 },
  { 5, -16, 48, 105, -20, 6 }, { 5, -15, 45, 107, -20, 6 },
  { 5, -15, 42, 109, -19, 6 }, { 5, -14, 40, 111, -19, 5 },
  { 5, -13, 37, 112, -18, 5 }, { 4, -13, 35, 114, -17, 5 },
  { 4, -12, 32, 116, -17, 5 }, { 4, -11, 30, 117, -16, 4 },
  { 4, -10, 27, 118, -15, 4 }, { 3, -9, 25, 119, -14, 4 },
  { 3, -9, 22, 121, -13, 4 },  { 3, -8, 20, 122, -12, 3 },
  { 2, -7, 18, 123, -11, 3 },  { 2, -6, 15, 124, -10, 3 },
  { 2, -5, 13, 125, -9, 2 },   { 1, -4, 11, 125, -7, 2 },
  { 1, -3, 8, 126, -6, 2 },    { 1, -3, 6, 127, -4, 1 },
  { 1, -2, 4, 127, -3, 1 },    { 0, -1, 2, 128, -1, 0 },
#else
#error "Invalid value of INTERP_SIMPLE_TAPS"
#endif  // INTERP_SIMPLE_TAPS == 2
};
#endif  // CONFIG_FRAME_SUPERRES && CONFIG_LOOP_RESTORATION

// Filters for factor of 2 downsampling.
static const int16_t av1_down2_symeven_half_filter[] = { 56, 12, -3, -1 };
static const int16_t av1_down2_symodd_half_filter[] = { 64, 35, 0, -3 };

static const interp_kernel *choose_interp_filter(int inlength, int outlength) {
  int outlength16 = outlength * 16;
  if (outlength16 >= inlength * 16)
    return filteredinterp_filters1000;
  else if (outlength16 >= inlength * 13)
    return filteredinterp_filters875;
  else if (outlength16 >= inlength * 11)
    return filteredinterp_filters750;
  else if (outlength16 >= inlength * 9)
    return filteredinterp_filters625;
  else
    return filteredinterp_filters500;
}

static void interpolate_core(const uint8_t *const input, int inlength,
                             uint8_t *output, int outlength,
                             const int16_t *interp_filters, int interp_taps) {
  const int32_t delta =
      (((uint32_t)inlength << INTERP_PRECISION_BITS) + outlength / 2) /
      outlength;
  const int32_t offset =
      inlength > outlength
          ? (((int32_t)(inlength - outlength) << (INTERP_PRECISION_BITS - 1)) +
             outlength / 2) /
                outlength
          : -(((int32_t)(outlength - inlength) << (INTERP_PRECISION_BITS - 1)) +
              outlength / 2) /
                outlength;
  uint8_t *optr = output;
  int x, x1, x2, sum, k, int_pel, sub_pel;
  int32_t y;

  x = 0;
  y = offset + SUBPEL_INTERP_EXTRA_OFF;
  while ((y >> INTERP_PRECISION_BITS) < (interp_taps / 2 - 1)) {
    x++;
    y += delta;
  }
  x1 = x;
  x = outlength - 1;
  y = delta * x + offset + SUBPEL_INTERP_EXTRA_OFF;
  while ((y >> INTERP_PRECISION_BITS) + (int32_t)(interp_taps / 2) >=
         inlength) {
    x--;
    y -= delta;
  }
  x2 = x;
  if (x1 > x2) {
    for (x = 0, y = offset + SUBPEL_INTERP_EXTRA_OFF; x < outlength;
         ++x, y += delta) {
      int_pel = y >> INTERP_PRECISION_BITS;
      sub_pel = (y >> SUBPEL_INTERP_EXTRA_BITS) & SUBPEL_MASK_RS;
      const int16_t *filter = &interp_filters[sub_pel * interp_taps];
      sum = 0;
      for (k = 0; k < interp_taps; ++k) {
        const int pk = int_pel - interp_taps / 2 + 1 + k;
        sum += filter[k] * input[AOMMAX(AOMMIN(pk, inlength - 1), 0)];
      }
      *optr++ = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
    }
  } else {
    // Initial part.
    for (x = 0, y = offset + SUBPEL_INTERP_EXTRA_OFF; x < x1; ++x, y += delta) {
      int_pel = y >> INTERP_PRECISION_BITS;
      sub_pel = (y >> SUBPEL_INTERP_EXTRA_BITS) & SUBPEL_MASK_RS;
      const int16_t *filter = &interp_filters[sub_pel * interp_taps];
      sum = 0;
      for (k = 0; k < interp_taps; ++k)
        sum += filter[k] * input[AOMMAX(int_pel - interp_taps / 2 + 1 + k, 0)];
      *optr++ = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
    }
    // Middle part.
    for (; x <= x2; ++x, y += delta) {
      int_pel = y >> INTERP_PRECISION_BITS;
      sub_pel = (y >> SUBPEL_INTERP_EXTRA_BITS) & SUBPEL_MASK_RS;
      const int16_t *filter = &interp_filters[sub_pel * interp_taps];
      sum = 0;
      for (k = 0; k < interp_taps; ++k)
        sum += filter[k] * input[int_pel - interp_taps / 2 + 1 + k];
      *optr++ = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
    }
    // End part.
    for (; x < outlength; ++x, y += delta) {
      int_pel = y >> INTERP_PRECISION_BITS;
      sub_pel = (y >> SUBPEL_INTERP_EXTRA_BITS) & SUBPEL_MASK_RS;
      const int16_t *filter = &interp_filters[sub_pel * interp_taps];
      sum = 0;
      for (k = 0; k < interp_taps; ++k)
        sum += filter[k] *
               input[AOMMIN(int_pel - interp_taps / 2 + 1 + k, inlength - 1)];
      *optr++ = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
    }
  }
}

static void interpolate(const uint8_t *const input, int inlength,
                        uint8_t *output, int outlength) {
  const interp_kernel *interp_filters =
      choose_interp_filter(inlength, outlength);

  interpolate_core(input, inlength, output, outlength, &interp_filters[0][0],
                   INTERP_TAPS);
}

#if CONFIG_FRAME_SUPERRES && CONFIG_LOOP_RESTORATION
static void interpolate_simple(const uint8_t *const input, int inlength,
                               uint8_t *output, int outlength) {
  interpolate_core(input, inlength, output, outlength, &filter_simple[0][0],
                   INTERP_SIMPLE_TAPS);
}
#endif  // CONFIG_FRAME_SUPERRES && CONFIG_LOOP_RESTORATION

#ifndef __clang_analyzer__
static void down2_symeven(const uint8_t *const input, int length,
                          uint8_t *output) {
  // Actual filter len = 2 * filter_len_half.
  const int16_t *filter = av1_down2_symeven_half_filter;
  const int filter_len_half = sizeof(av1_down2_symeven_half_filter) / 2;
  int i, j;
  uint8_t *optr = output;
  int l1 = filter_len_half;
  int l2 = (length - filter_len_half);
  l1 += (l1 & 1);
  l2 += (l2 & 1);
  if (l1 > l2) {
    // Short input length.
    for (i = 0; i < length; i += 2) {
      int sum = (1 << (FILTER_BITS - 1));
      for (j = 0; j < filter_len_half; ++j) {
        sum +=
            (input[AOMMAX(i - j, 0)] + input[AOMMIN(i + 1 + j, length - 1)]) *
            filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel(sum);
    }
  } else {
    // Initial part.
    for (i = 0; i < l1; i += 2) {
      int sum = (1 << (FILTER_BITS - 1));
      for (j = 0; j < filter_len_half; ++j) {
        sum += (input[AOMMAX(i - j, 0)] + input[i + 1 + j]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel(sum);
    }
    // Middle part.
    for (; i < l2; i += 2) {
      int sum = (1 << (FILTER_BITS - 1));
      for (j = 0; j < filter_len_half; ++j) {
        sum += (input[i - j] + input[i + 1 + j]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel(sum);
    }
    // End part.
    for (; i < length; i += 2) {
      int sum = (1 << (FILTER_BITS - 1));
      for (j = 0; j < filter_len_half; ++j) {
        sum +=
            (input[i - j] + input[AOMMIN(i + 1 + j, length - 1)]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel(sum);
    }
  }
}
#endif

static void down2_symodd(const uint8_t *const input, int length,
                         uint8_t *output) {
  // Actual filter len = 2 * filter_len_half - 1.
  const int16_t *filter = av1_down2_symodd_half_filter;
  const int filter_len_half = sizeof(av1_down2_symodd_half_filter) / 2;
  int i, j;
  uint8_t *optr = output;
  int l1 = filter_len_half - 1;
  int l2 = (length - filter_len_half + 1);
  l1 += (l1 & 1);
  l2 += (l2 & 1);
  if (l1 > l2) {
    // Short input length.
    for (i = 0; i < length; i += 2) {
      int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
      for (j = 1; j < filter_len_half; ++j) {
        sum += (input[(i - j < 0 ? 0 : i - j)] +
                input[(i + j >= length ? length - 1 : i + j)]) *
               filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel(sum);
    }
  } else {
    // Initial part.
    for (i = 0; i < l1; i += 2) {
      int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
      for (j = 1; j < filter_len_half; ++j) {
        sum += (input[(i - j < 0 ? 0 : i - j)] + input[i + j]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel(sum);
    }
    // Middle part.
    for (; i < l2; i += 2) {
      int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
      for (j = 1; j < filter_len_half; ++j) {
        sum += (input[i - j] + input[i + j]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel(sum);
    }
    // End part.
    for (; i < length; i += 2) {
      int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
      for (j = 1; j < filter_len_half; ++j) {
        sum += (input[i - j] + input[(i + j >= length ? length - 1 : i + j)]) *
               filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel(sum);
    }
  }
}

static int get_down2_length(int length, int steps) {
  int s;
  for (s = 0; s < steps; ++s) length = (length + 1) >> 1;
  return length;
}

static int get_down2_steps(int in_length, int out_length) {
  int steps = 0;
  int proj_in_length;
  while ((proj_in_length = get_down2_length(in_length, 1)) >= out_length) {
    ++steps;
    in_length = proj_in_length;
  }
  return steps;
}

static void resize_multistep(const uint8_t *const input, int length,
                             uint8_t *output, int olength, uint8_t *otmp) {
  if (length == olength) {
    memcpy(output, input, sizeof(output[0]) * length);
    return;
  }
  const int steps = get_down2_steps(length, olength);

  if (steps > 0) {
    uint8_t *out = NULL;
    int filteredlength = length;

    assert(otmp != NULL);
    uint8_t *otmp2 = otmp + get_down2_length(length, 1);
    for (int s = 0; s < steps; ++s) {
      const int proj_filteredlength = get_down2_length(filteredlength, 1);
      const uint8_t *const in = (s == 0 ? input : out);
      if (s == steps - 1 && proj_filteredlength == olength)
        out = output;
      else
        out = (s & 1 ? otmp2 : otmp);
      if (filteredlength & 1)
        down2_symodd(in, filteredlength, out);
      else
        down2_symeven(in, filteredlength, out);
      filteredlength = proj_filteredlength;
    }
    if (filteredlength != olength) {
      interpolate(out, filteredlength, output, olength);
    }
  } else {
    interpolate(input, length, output, olength);
  }
}

static void fill_col_to_arr(uint8_t *img, int stride, int len, uint8_t *arr) {
  int i;
  uint8_t *iptr = img;
  uint8_t *aptr = arr;
  for (i = 0; i < len; ++i, iptr += stride) {
    *aptr++ = *iptr;
  }
}

static void fill_arr_to_col(uint8_t *img, int stride, int len, uint8_t *arr) {
  int i;
  uint8_t *iptr = img;
  uint8_t *aptr = arr;
  for (i = 0; i < len; ++i, iptr += stride) {
    *iptr = *aptr++;
  }
}

static void resize_plane(const uint8_t *const input, int height, int width,
                         int in_stride, uint8_t *output, int height2,
                         int width2, int out_stride) {
  int i;
  uint8_t *intbuf = (uint8_t *)aom_malloc(sizeof(uint8_t) * width2 * height);
  uint8_t *tmpbuf =
      (uint8_t *)aom_malloc(sizeof(uint8_t) * AOMMAX(width, height));
  uint8_t *arrbuf = (uint8_t *)aom_malloc(sizeof(uint8_t) * height);
  uint8_t *arrbuf2 = (uint8_t *)aom_malloc(sizeof(uint8_t) * height2);
  if (intbuf == NULL || tmpbuf == NULL || arrbuf == NULL || arrbuf2 == NULL)
    goto Error;
  assert(width > 0);
  assert(height > 0);
  assert(width2 > 0);
  assert(height2 > 0);
  for (i = 0; i < height; ++i)
    resize_multistep(input + in_stride * i, width, intbuf + width2 * i, width2,
                     tmpbuf);
  for (i = 0; i < width2; ++i) {
    fill_col_to_arr(intbuf + i, width2, height, arrbuf);
    resize_multistep(arrbuf, height, arrbuf2, height2, tmpbuf);
    fill_arr_to_col(output + i, out_stride, height2, arrbuf2);
  }

Error:
  aom_free(intbuf);
  aom_free(tmpbuf);
  aom_free(arrbuf);
  aom_free(arrbuf2);
}

#if CONFIG_FRAME_SUPERRES
static void upscale_normative(const uint8_t *const input, int length,
                              uint8_t *output, int olength) {
#if CONFIG_LOOP_RESTORATION
  interpolate_simple(input, length, output, olength);
#else
  interpolate(input, length, output, olength);
#endif  // CONFIG_LOOP_RESTORATION
}

static void upscale_normative_plane(const uint8_t *const input, int height,
                                    int width, int in_stride, uint8_t *output,
                                    int height2, int width2, int out_stride) {
  int i;
  uint8_t *intbuf = (uint8_t *)aom_malloc(sizeof(uint8_t) * width2 * height);
  uint8_t *arrbuf = (uint8_t *)aom_malloc(sizeof(uint8_t) * height);
  uint8_t *arrbuf2 = (uint8_t *)aom_malloc(sizeof(uint8_t) * height2);
  if (intbuf == NULL || arrbuf == NULL || arrbuf2 == NULL) goto Error;
  assert(width > 0);
  assert(height > 0);
  assert(width2 > 0);
  assert(height2 > 0);
  for (i = 0; i < height; ++i)
    upscale_normative(input + in_stride * i, width, intbuf + width2 * i,
                      width2);
  for (i = 0; i < width2; ++i) {
    fill_col_to_arr(intbuf + i, width2, height, arrbuf);
    upscale_normative(arrbuf, height, arrbuf2, height2);
    fill_arr_to_col(output + i, out_stride, height2, arrbuf2);
  }

Error:
  aom_free(intbuf);
  aom_free(arrbuf);
  aom_free(arrbuf2);
}
#endif  // CONFIG_FRAME_SUPERRES

#if CONFIG_HIGHBITDEPTH
static void highbd_interpolate_core(const uint16_t *const input, int inlength,
                                    uint16_t *output, int outlength, int bd,
                                    const int16_t *interp_filters,
                                    int interp_taps) {
  const int32_t delta =
      (((uint32_t)inlength << INTERP_PRECISION_BITS) + outlength / 2) /
      outlength;
  const int32_t offset =
      inlength > outlength
          ? (((int32_t)(inlength - outlength) << (INTERP_PRECISION_BITS - 1)) +
             outlength / 2) /
                outlength
          : -(((int32_t)(outlength - inlength) << (INTERP_PRECISION_BITS - 1)) +
              outlength / 2) /
                outlength;
  uint16_t *optr = output;
  int x, x1, x2, sum, k, int_pel, sub_pel;
  int32_t y;

  x = 0;
  y = offset + SUBPEL_INTERP_EXTRA_OFF;
  while ((y >> INTERP_PRECISION_BITS) < (interp_taps / 2 - 1)) {
    x++;
    y += delta;
  }
  x1 = x;
  x = outlength - 1;
  y = delta * x + offset + SUBPEL_INTERP_EXTRA_OFF;
  while ((y >> INTERP_PRECISION_BITS) + (int32_t)(interp_taps / 2) >=
         inlength) {
    x--;
    y -= delta;
  }
  x2 = x;
  if (x1 > x2) {
    for (x = 0, y = offset + SUBPEL_INTERP_EXTRA_OFF; x < outlength;
         ++x, y += delta) {
      int_pel = y >> INTERP_PRECISION_BITS;
      sub_pel = (y >> SUBPEL_INTERP_EXTRA_BITS) & SUBPEL_MASK_RS;
      const int16_t *filter = &interp_filters[sub_pel * interp_taps];
      sum = 0;
      for (k = 0; k < interp_taps; ++k) {
        const int pk = int_pel - interp_taps / 2 + 1 + k;
        sum += filter[k] * input[AOMMAX(AOMMIN(pk, inlength - 1), 0)];
      }
      *optr++ = clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
    }
  } else {
    // Initial part.
    for (x = 0, y = offset + SUBPEL_INTERP_EXTRA_OFF; x < x1; ++x, y += delta) {
      int_pel = y >> INTERP_PRECISION_BITS;
      sub_pel = (y >> SUBPEL_INTERP_EXTRA_BITS) & SUBPEL_MASK_RS;
      const int16_t *filter = &interp_filters[sub_pel * interp_taps];
      sum = 0;
      for (k = 0; k < interp_taps; ++k)
        sum += filter[k] * input[AOMMAX(int_pel - interp_taps / 2 + 1 + k, 0)];
      *optr++ = clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
    }
    // Middle part.
    for (; x <= x2; ++x, y += delta) {
      int_pel = y >> INTERP_PRECISION_BITS;
      sub_pel = (y >> SUBPEL_INTERP_EXTRA_BITS) & SUBPEL_MASK_RS;
      const int16_t *filter = &interp_filters[sub_pel * interp_taps];
      sum = 0;
      for (k = 0; k < interp_taps; ++k)
        sum += filter[k] * input[int_pel - interp_taps / 2 + 1 + k];
      *optr++ = clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
    }
    // End part.
    for (; x < outlength; ++x, y += delta) {
      int_pel = y >> INTERP_PRECISION_BITS;
      sub_pel = (y >> SUBPEL_INTERP_EXTRA_BITS) & SUBPEL_MASK_RS;
      const int16_t *filter = &interp_filters[sub_pel * interp_taps];
      sum = 0;
      for (k = 0; k < interp_taps; ++k)
        sum += filter[k] *
               input[AOMMIN(int_pel - interp_taps / 2 + 1 + k, inlength - 1)];
      *optr++ = clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
    }
  }
}

static void highbd_interpolate(const uint16_t *const input, int inlength,
                               uint16_t *output, int outlength, int bd) {
  const interp_kernel *interp_filters =
      choose_interp_filter(inlength, outlength);

  highbd_interpolate_core(input, inlength, output, outlength, bd,
                          &interp_filters[0][0], INTERP_TAPS);
}

#if CONFIG_FRAME_SUPERRES && CONFIG_LOOP_RESTORATION
static void highbd_interpolate_simple(const uint16_t *const input, int inlength,
                                      uint16_t *output, int outlength, int bd) {
  highbd_interpolate_core(input, inlength, output, outlength, bd,
                          &filter_simple[0][0], INTERP_SIMPLE_TAPS);
}
#endif  // CONFIG_FRAME_SUPERRES && CONFIG_LOOP_RESTORATION

#ifndef __clang_analyzer__
static void highbd_down2_symeven(const uint16_t *const input, int length,
                                 uint16_t *output, int bd) {
  // Actual filter len = 2 * filter_len_half.
  static const int16_t *filter = av1_down2_symeven_half_filter;
  const int filter_len_half = sizeof(av1_down2_symeven_half_filter) / 2;
  int i, j;
  uint16_t *optr = output;
  int l1 = filter_len_half;
  int l2 = (length - filter_len_half);
  l1 += (l1 & 1);
  l2 += (l2 & 1);
  if (l1 > l2) {
    // Short input length.
    for (i = 0; i < length; i += 2) {
      int sum = (1 << (FILTER_BITS - 1));
      for (j = 0; j < filter_len_half; ++j) {
        sum +=
            (input[AOMMAX(0, i - j)] + input[AOMMIN(i + 1 + j, length - 1)]) *
            filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel_highbd(sum, bd);
    }
  } else {
    // Initial part.
    for (i = 0; i < l1; i += 2) {
      int sum = (1 << (FILTER_BITS - 1));
      for (j = 0; j < filter_len_half; ++j) {
        sum += (input[AOMMAX(0, i - j)] + input[i + 1 + j]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel_highbd(sum, bd);
    }
    // Middle part.
    for (; i < l2; i += 2) {
      int sum = (1 << (FILTER_BITS - 1));
      for (j = 0; j < filter_len_half; ++j) {
        sum += (input[i - j] + input[i + 1 + j]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel_highbd(sum, bd);
    }
    // End part.
    for (; i < length; i += 2) {
      int sum = (1 << (FILTER_BITS - 1));
      for (j = 0; j < filter_len_half; ++j) {
        sum +=
            (input[i - j] + input[AOMMIN(i + 1 + j, length - 1)]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel_highbd(sum, bd);
    }
  }
}

static void highbd_down2_symodd(const uint16_t *const input, int length,
                                uint16_t *output, int bd) {
  // Actual filter len = 2 * filter_len_half - 1.
  static const int16_t *filter = av1_down2_symodd_half_filter;
  const int filter_len_half = sizeof(av1_down2_symodd_half_filter) / 2;
  int i, j;
  uint16_t *optr = output;
  int l1 = filter_len_half - 1;
  int l2 = (length - filter_len_half + 1);
  l1 += (l1 & 1);
  l2 += (l2 & 1);
  if (l1 > l2) {
    // Short input length.
    for (i = 0; i < length; i += 2) {
      int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
      for (j = 1; j < filter_len_half; ++j) {
        sum += (input[AOMMAX(i - j, 0)] + input[AOMMIN(i + j, length - 1)]) *
               filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel_highbd(sum, bd);
    }
  } else {
    // Initial part.
    for (i = 0; i < l1; i += 2) {
      int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
      for (j = 1; j < filter_len_half; ++j) {
        sum += (input[AOMMAX(i - j, 0)] + input[i + j]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel_highbd(sum, bd);
    }
    // Middle part.
    for (; i < l2; i += 2) {
      int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
      for (j = 1; j < filter_len_half; ++j) {
        sum += (input[i - j] + input[i + j]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel_highbd(sum, bd);
    }
    // End part.
    for (; i < length; i += 2) {
      int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
      for (j = 1; j < filter_len_half; ++j) {
        sum += (input[i - j] + input[AOMMIN(i + j, length - 1)]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel_highbd(sum, bd);
    }
  }
}
#endif

static void highbd_resize_multistep(const uint16_t *const input, int length,
                                    uint16_t *output, int olength,
                                    uint16_t *otmp, int bd) {
  if (length == olength) {
    memcpy(output, input, sizeof(output[0]) * length);
    return;
  }
  const int steps = get_down2_steps(length, olength);

  if (steps > 0) {
    uint16_t *out = NULL;
    int filteredlength = length;

    assert(otmp != NULL);
    uint16_t *otmp2 = otmp + get_down2_length(length, 1);
    for (int s = 0; s < steps; ++s) {
      const int proj_filteredlength = get_down2_length(filteredlength, 1);
      const uint16_t *const in = (s == 0 ? input : out);
      if (s == steps - 1 && proj_filteredlength == olength)
        out = output;
      else
        out = (s & 1 ? otmp2 : otmp);
      if (filteredlength & 1)
        highbd_down2_symodd(in, filteredlength, out, bd);
      else
        highbd_down2_symeven(in, filteredlength, out, bd);
      filteredlength = proj_filteredlength;
    }
    if (filteredlength != olength) {
      highbd_interpolate(out, filteredlength, output, olength, bd);
    }
  } else {
    highbd_interpolate(input, length, output, olength, bd);
  }
}

static void highbd_fill_col_to_arr(uint16_t *img, int stride, int len,
                                   uint16_t *arr) {
  int i;
  uint16_t *iptr = img;
  uint16_t *aptr = arr;
  for (i = 0; i < len; ++i, iptr += stride) {
    *aptr++ = *iptr;
  }
}

static void highbd_fill_arr_to_col(uint16_t *img, int stride, int len,
                                   uint16_t *arr) {
  int i;
  uint16_t *iptr = img;
  uint16_t *aptr = arr;
  for (i = 0; i < len; ++i, iptr += stride) {
    *iptr = *aptr++;
  }
}

static void highbd_resize_plane(const uint8_t *const input, int height,
                                int width, int in_stride, uint8_t *output,
                                int height2, int width2, int out_stride,
                                int bd) {
  int i;
  uint16_t *intbuf = (uint16_t *)aom_malloc(sizeof(uint16_t) * width2 * height);
  uint16_t *tmpbuf =
      (uint16_t *)aom_malloc(sizeof(uint16_t) * AOMMAX(width, height));
  uint16_t *arrbuf = (uint16_t *)aom_malloc(sizeof(uint16_t) * height);
  uint16_t *arrbuf2 = (uint16_t *)aom_malloc(sizeof(uint16_t) * height2);
  if (intbuf == NULL || tmpbuf == NULL || arrbuf == NULL || arrbuf2 == NULL)
    goto Error;
  for (i = 0; i < height; ++i) {
    highbd_resize_multistep(CONVERT_TO_SHORTPTR(input + in_stride * i), width,
                            intbuf + width2 * i, width2, tmpbuf, bd);
  }
  for (i = 0; i < width2; ++i) {
    highbd_fill_col_to_arr(intbuf + i, width2, height, arrbuf);
    highbd_resize_multistep(arrbuf, height, arrbuf2, height2, tmpbuf, bd);
    highbd_fill_arr_to_col(CONVERT_TO_SHORTPTR(output + i), out_stride, height2,
                           arrbuf2);
  }

Error:
  aom_free(intbuf);
  aom_free(tmpbuf);
  aom_free(arrbuf);
  aom_free(arrbuf2);
}

#if CONFIG_FRAME_SUPERRES
static void highbd_upscale_normative(const uint16_t *const input, int length,
                                     uint16_t *output, int olength, int bd) {
#if CONFIG_LOOP_RESTORATION
  highbd_interpolate_simple(input, length, output, olength, bd);
#else
  highbd_interpolate(input, length, output, olength, bd);
#endif  // CONFIG_LOOP_RESTORATION
}

static void highbd_upscale_normative_plane(const uint8_t *const input,
                                           int height, int width, int in_stride,
                                           uint8_t *output, int height2,
                                           int width2, int out_stride, int bd) {
  int i;
  uint16_t *intbuf = (uint16_t *)aom_malloc(sizeof(uint16_t) * width2 * height);
  uint16_t *arrbuf = (uint16_t *)aom_malloc(sizeof(uint16_t) * height);
  uint16_t *arrbuf2 = (uint16_t *)aom_malloc(sizeof(uint16_t) * height2);
  if (intbuf == NULL || arrbuf == NULL || arrbuf2 == NULL) goto Error;
  for (i = 0; i < height; ++i) {
    highbd_upscale_normative(CONVERT_TO_SHORTPTR(input + in_stride * i), width,
                             intbuf + width2 * i, width2, bd);
  }
  for (i = 0; i < width2; ++i) {
    highbd_fill_col_to_arr(intbuf + i, width2, height, arrbuf);
    highbd_upscale_normative(arrbuf, height, arrbuf2, height2, bd);
    highbd_fill_arr_to_col(CONVERT_TO_SHORTPTR(output + i), out_stride, height2,
                           arrbuf2);
  }

Error:
  aom_free(intbuf);
  aom_free(arrbuf);
  aom_free(arrbuf2);
}
#endif  // CONFIG_FRAME_SUPERRES

#endif  // CONFIG_HIGHBITDEPTH

void av1_resize_frame420(const uint8_t *const y, int y_stride,
                         const uint8_t *const u, const uint8_t *const v,
                         int uv_stride, int height, int width, uint8_t *oy,
                         int oy_stride, uint8_t *ou, uint8_t *ov,
                         int ouv_stride, int oheight, int owidth) {
  resize_plane(y, height, width, y_stride, oy, oheight, owidth, oy_stride);
  resize_plane(u, height / 2, width / 2, uv_stride, ou, oheight / 2, owidth / 2,
               ouv_stride);
  resize_plane(v, height / 2, width / 2, uv_stride, ov, oheight / 2, owidth / 2,
               ouv_stride);
}

void av1_resize_frame422(const uint8_t *const y, int y_stride,
                         const uint8_t *const u, const uint8_t *const v,
                         int uv_stride, int height, int width, uint8_t *oy,
                         int oy_stride, uint8_t *ou, uint8_t *ov,
                         int ouv_stride, int oheight, int owidth) {
  resize_plane(y, height, width, y_stride, oy, oheight, owidth, oy_stride);
  resize_plane(u, height, width / 2, uv_stride, ou, oheight, owidth / 2,
               ouv_stride);
  resize_plane(v, height, width / 2, uv_stride, ov, oheight, owidth / 2,
               ouv_stride);
}

void av1_resize_frame444(const uint8_t *const y, int y_stride,
                         const uint8_t *const u, const uint8_t *const v,
                         int uv_stride, int height, int width, uint8_t *oy,
                         int oy_stride, uint8_t *ou, uint8_t *ov,
                         int ouv_stride, int oheight, int owidth) {
  resize_plane(y, height, width, y_stride, oy, oheight, owidth, oy_stride);
  resize_plane(u, height, width, uv_stride, ou, oheight, owidth, ouv_stride);
  resize_plane(v, height, width, uv_stride, ov, oheight, owidth, ouv_stride);
}

#if CONFIG_HIGHBITDEPTH
void av1_highbd_resize_frame420(const uint8_t *const y, int y_stride,
                                const uint8_t *const u, const uint8_t *const v,
                                int uv_stride, int height, int width,
                                uint8_t *oy, int oy_stride, uint8_t *ou,
                                uint8_t *ov, int ouv_stride, int oheight,
                                int owidth, int bd) {
  highbd_resize_plane(y, height, width, y_stride, oy, oheight, owidth,
                      oy_stride, bd);
  highbd_resize_plane(u, height / 2, width / 2, uv_stride, ou, oheight / 2,
                      owidth / 2, ouv_stride, bd);
  highbd_resize_plane(v, height / 2, width / 2, uv_stride, ov, oheight / 2,
                      owidth / 2, ouv_stride, bd);
}

void av1_highbd_resize_frame422(const uint8_t *const y, int y_stride,
                                const uint8_t *const u, const uint8_t *const v,
                                int uv_stride, int height, int width,
                                uint8_t *oy, int oy_stride, uint8_t *ou,
                                uint8_t *ov, int ouv_stride, int oheight,
                                int owidth, int bd) {
  highbd_resize_plane(y, height, width, y_stride, oy, oheight, owidth,
                      oy_stride, bd);
  highbd_resize_plane(u, height, width / 2, uv_stride, ou, oheight, owidth / 2,
                      ouv_stride, bd);
  highbd_resize_plane(v, height, width / 2, uv_stride, ov, oheight, owidth / 2,
                      ouv_stride, bd);
}

void av1_highbd_resize_frame444(const uint8_t *const y, int y_stride,
                                const uint8_t *const u, const uint8_t *const v,
                                int uv_stride, int height, int width,
                                uint8_t *oy, int oy_stride, uint8_t *ou,
                                uint8_t *ov, int ouv_stride, int oheight,
                                int owidth, int bd) {
  highbd_resize_plane(y, height, width, y_stride, oy, oheight, owidth,
                      oy_stride, bd);
  highbd_resize_plane(u, height, width, uv_stride, ou, oheight, owidth,
                      ouv_stride, bd);
  highbd_resize_plane(v, height, width, uv_stride, ov, oheight, owidth,
                      ouv_stride, bd);
}
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_HIGHBITDEPTH
void av1_resize_and_extend_frame(const YV12_BUFFER_CONFIG *src,
                                 YV12_BUFFER_CONFIG *dst, int bd) {
#else
void av1_resize_and_extend_frame(const YV12_BUFFER_CONFIG *src,
                                 YV12_BUFFER_CONFIG *dst) {
#endif  // CONFIG_HIGHBITDEPTH
  // TODO(dkovalev): replace YV12_BUFFER_CONFIG with aom_image_t
  int i;
  const uint8_t *const srcs[3] = { src->y_buffer, src->u_buffer,
                                   src->v_buffer };
  const int src_strides[3] = { src->y_stride, src->uv_stride, src->uv_stride };
  const int src_widths[3] = { src->y_crop_width, src->uv_crop_width,
                              src->uv_crop_width };
  const int src_heights[3] = { src->y_crop_height, src->uv_crop_height,
                               src->uv_crop_height };
  uint8_t *const dsts[3] = { dst->y_buffer, dst->u_buffer, dst->v_buffer };
  const int dst_strides[3] = { dst->y_stride, dst->uv_stride, dst->uv_stride };
  const int dst_widths[3] = { dst->y_crop_width, dst->uv_crop_width,
                              dst->uv_crop_width };
  const int dst_heights[3] = { dst->y_crop_height, dst->uv_crop_height,
                               dst->uv_crop_height };

  for (i = 0; i < MAX_MB_PLANE; ++i) {
#if CONFIG_HIGHBITDEPTH
    if (src->flags & YV12_FLAG_HIGHBITDEPTH)
      highbd_resize_plane(srcs[i], src_heights[i], src_widths[i],
                          src_strides[i], dsts[i], dst_heights[i],
                          dst_widths[i], dst_strides[i], bd);
    else
#endif  // CONFIG_HIGHBITDEPTH
      resize_plane(srcs[i], src_heights[i], src_widths[i], src_strides[i],
                   dsts[i], dst_heights[i], dst_widths[i], dst_strides[i]);
  }
  aom_extend_frame_borders(dst);
}

#if CONFIG_FRAME_SUPERRES
#if CONFIG_HIGHBITDEPTH
void av1_upscale_normative_and_extend_frame(const YV12_BUFFER_CONFIG *src,
                                            YV12_BUFFER_CONFIG *dst, int bd) {
#else
void av1_upscale_normative_and_extend_frame(const YV12_BUFFER_CONFIG *src,
                                            YV12_BUFFER_CONFIG *dst) {
#endif  // CONFIG_HIGHBITDEPTH
  // TODO(dkovalev): replace YV12_BUFFER_CONFIG with aom_image_t
  int i;
  const uint8_t *const srcs[3] = { src->y_buffer, src->u_buffer,
                                   src->v_buffer };
  const int src_strides[3] = { src->y_stride, src->uv_stride, src->uv_stride };
  const int src_widths[3] = { src->y_crop_width, src->uv_crop_width,
                              src->uv_crop_width };
  const int src_heights[3] = { src->y_crop_height, src->uv_crop_height,
                               src->uv_crop_height };
  uint8_t *const dsts[3] = { dst->y_buffer, dst->u_buffer, dst->v_buffer };
  const int dst_strides[3] = { dst->y_stride, dst->uv_stride, dst->uv_stride };
  const int dst_widths[3] = { dst->y_crop_width, dst->uv_crop_width,
                              dst->uv_crop_width };
  const int dst_heights[3] = { dst->y_crop_height, dst->uv_crop_height,
                               dst->uv_crop_height };

  for (i = 0; i < MAX_MB_PLANE; ++i) {
#if CONFIG_HIGHBITDEPTH
    if (src->flags & YV12_FLAG_HIGHBITDEPTH)
      highbd_upscale_normative_plane(srcs[i], src_heights[i], src_widths[i],
                                     src_strides[i], dsts[i], dst_heights[i],
                                     dst_widths[i], dst_strides[i], bd);
    else
#endif  // CONFIG_HIGHBITDEPTH
      upscale_normative_plane(srcs[i], src_heights[i], src_widths[i],
                              src_strides[i], dsts[i], dst_heights[i],
                              dst_widths[i], dst_strides[i]);
  }
  aom_extend_frame_borders(dst);
}
#endif  // CONFIG_FRAME_SUPERRES

YV12_BUFFER_CONFIG *av1_scale_if_required(AV1_COMMON *cm,
                                          YV12_BUFFER_CONFIG *unscaled,
                                          YV12_BUFFER_CONFIG *scaled) {
  if (cm->width != unscaled->y_crop_width ||
      cm->height != unscaled->y_crop_height) {
#if CONFIG_HIGHBITDEPTH
    av1_resize_and_extend_frame(unscaled, scaled, (int)cm->bit_depth);
#else
    av1_resize_and_extend_frame(unscaled, scaled);
#endif  // CONFIG_HIGHBITDEPTH
    return scaled;
  } else {
    return unscaled;
  }
}

// Calculates scaled dimensions given original dimensions and the scale
// denominator. If 'scale_height' is 1, both width and height are scaled;
// otherwise, only the width is scaled.
static void calculate_scaled_size_helper(int *width, int *height, int denom,
                                         int scale_height) {
  if (denom != SCALE_NUMERATOR) {
    *width = *width * SCALE_NUMERATOR / denom;
    *width += *width & 1;  // Make it even.
    if (scale_height) {
      *height = *height * SCALE_NUMERATOR / denom;
      *height += *height & 1;  // Make it even.
    }
  }
}

void av1_calculate_scaled_size(int *width, int *height, int resize_denom) {
  calculate_scaled_size_helper(width, height, resize_denom, 1);
}

#if CONFIG_FRAME_SUPERRES
void av1_calculate_scaled_superres_size(int *width, int *height,
                                        int superres_denom) {
  calculate_scaled_size_helper(width, height, superres_denom,
                               !CONFIG_HORZONLY_FRAME_SUPERRES);
}

void av1_calculate_unscaled_superres_size(int *width, int *height, int denom) {
  if (denom != SCALE_NUMERATOR) {
    // Note: av1_calculate_scaled_superres_size() rounds *up* after division
    // when the resulting dimensions are odd. So here, we round *down*.
    *width = *width * denom / SCALE_NUMERATOR;
#if CONFIG_HORZONLY_FRAME_SUPERRES
    (void)height;
#else
    *height = *height * denom / SCALE_NUMERATOR;
#endif  // CONFIG_HORZONLY_FRAME_SUPERRES
  }
}

// TODO(afergs): Look for in-place upscaling
// TODO(afergs): aom_ vs av1_ functions? Which can I use?
// Upscale decoded image.
void av1_superres_upscale(AV1_COMMON *cm, BufferPool *const pool) {
  if (av1_superres_unscaled(cm)) return;

  YV12_BUFFER_CONFIG copy_buffer;
  memset(&copy_buffer, 0, sizeof(copy_buffer));

  YV12_BUFFER_CONFIG *const frame_to_show = get_frame_new_buffer(cm);

  if (aom_alloc_frame_buffer(&copy_buffer, cm->width, cm->height,
                             cm->subsampling_x, cm->subsampling_y,
#if CONFIG_HIGHBITDEPTH
                             cm->use_highbitdepth,
#endif  // CONFIG_HIGHBITDEPTH
                             AOM_BORDER_IN_PIXELS, cm->byte_alignment))
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate copy buffer for superres upscaling");

  // Copy function assumes the frames are the same size, doesn't copy bit_depth.
  aom_yv12_copy_frame(frame_to_show, &copy_buffer);
  copy_buffer.bit_depth = frame_to_show->bit_depth;
  assert(copy_buffer.y_crop_width == cm->width);
  assert(copy_buffer.y_crop_height == cm->height);

  // Realloc the current frame buffer at a higher resolution in place.
  if (pool != NULL) {
    // Use callbacks if on the decoder.
    aom_codec_frame_buffer_t *fb =
        &pool->frame_bufs[cm->new_fb_idx].raw_frame_buffer;
    aom_release_frame_buffer_cb_fn_t release_fb_cb = pool->release_fb_cb;
    aom_get_frame_buffer_cb_fn_t cb = pool->get_fb_cb;
    void *cb_priv = pool->cb_priv;

    // Realloc with callback does not release the frame buffer - release first.
    if (release_fb_cb(cb_priv, fb))
      aom_internal_error(
          &cm->error, AOM_CODEC_MEM_ERROR,
          "Failed to free current frame buffer before superres upscaling");

    if (aom_realloc_frame_buffer(
            frame_to_show, cm->superres_upscaled_width,
            cm->superres_upscaled_height, cm->subsampling_x, cm->subsampling_y,
#if CONFIG_HIGHBITDEPTH
            cm->use_highbitdepth,
#endif  // CONFIG_HIGHBITDEPTH
            AOM_BORDER_IN_PIXELS, cm->byte_alignment, fb, cb, cb_priv))
      aom_internal_error(
          &cm->error, AOM_CODEC_MEM_ERROR,
          "Failed to allocate current frame buffer for superres upscaling");
  } else {
    // Don't use callbacks on the encoder.
    if (aom_alloc_frame_buffer(frame_to_show, cm->superres_upscaled_width,
                               cm->superres_upscaled_height, cm->subsampling_x,
                               cm->subsampling_y,
#if CONFIG_HIGHBITDEPTH
                               cm->use_highbitdepth,
#endif  // CONFIG_HIGHBITDEPTH
                               AOM_BORDER_IN_PIXELS, cm->byte_alignment))
      aom_internal_error(
          &cm->error, AOM_CODEC_MEM_ERROR,
          "Failed to reallocate current frame buffer for superres upscaling");
  }
  // TODO(afergs): verify frame_to_show is correct after realloc
  //               encoder:
  //               decoder:
  frame_to_show->bit_depth = copy_buffer.bit_depth;
  assert(frame_to_show->y_crop_width == cm->superres_upscaled_width);
  assert(frame_to_show->y_crop_height == cm->superres_upscaled_height);

  // Scale up and back into frame_to_show.
  assert(frame_to_show->y_crop_width != cm->width);
  assert(IMPLIES(!CONFIG_HORZONLY_FRAME_SUPERRES,
                 frame_to_show->y_crop_height != cm->height));
#if CONFIG_HIGHBITDEPTH
  av1_upscale_normative_and_extend_frame(&copy_buffer, frame_to_show,
                                         (int)cm->bit_depth);
#else
  av1_upscale_normative_and_extend_frame(&copy_buffer, frame_to_show);
#endif  // CONFIG_HIGHBITDEPTH

  // Free the copy buffer
  aom_free_frame_buffer(&copy_buffer);
}
#endif  // CONFIG_FRAME_SUPERRES
