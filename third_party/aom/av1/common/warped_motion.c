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

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <assert.h>

#include "./av1_rtcd.h"
#include "av1/common/warped_motion.h"
#include "av1/common/scale.h"

#define WARP_ERROR_BLOCK 32

/* clang-format off */
static const int error_measure_lut[512] = {
  // pow 0.7
  16384, 16339, 16294, 16249, 16204, 16158, 16113, 16068,
  16022, 15977, 15932, 15886, 15840, 15795, 15749, 15703,
  15657, 15612, 15566, 15520, 15474, 15427, 15381, 15335,
  15289, 15242, 15196, 15149, 15103, 15056, 15010, 14963,
  14916, 14869, 14822, 14775, 14728, 14681, 14634, 14587,
  14539, 14492, 14445, 14397, 14350, 14302, 14254, 14206,
  14159, 14111, 14063, 14015, 13967, 13918, 13870, 13822,
  13773, 13725, 13676, 13628, 13579, 13530, 13481, 13432,
  13383, 13334, 13285, 13236, 13187, 13137, 13088, 13038,
  12988, 12939, 12889, 12839, 12789, 12739, 12689, 12639,
  12588, 12538, 12487, 12437, 12386, 12335, 12285, 12234,
  12183, 12132, 12080, 12029, 11978, 11926, 11875, 11823,
  11771, 11719, 11667, 11615, 11563, 11511, 11458, 11406,
  11353, 11301, 11248, 11195, 11142, 11089, 11036, 10982,
  10929, 10875, 10822, 10768, 10714, 10660, 10606, 10552,
  10497, 10443, 10388, 10333, 10279, 10224, 10168, 10113,
  10058, 10002,  9947,  9891,  9835,  9779,  9723,  9666,
  9610, 9553, 9497, 9440, 9383, 9326, 9268, 9211,
  9153, 9095, 9037, 8979, 8921, 8862, 8804, 8745,
  8686, 8627, 8568, 8508, 8449, 8389, 8329, 8269,
  8208, 8148, 8087, 8026, 7965, 7903, 7842, 7780,
  7718, 7656, 7593, 7531, 7468, 7405, 7341, 7278,
  7214, 7150, 7086, 7021, 6956, 6891, 6826, 6760,
  6695, 6628, 6562, 6495, 6428, 6361, 6293, 6225,
  6157, 6089, 6020, 5950, 5881, 5811, 5741, 5670,
  5599, 5527, 5456, 5383, 5311, 5237, 5164, 5090,
  5015, 4941, 4865, 4789, 4713, 4636, 4558, 4480,
  4401, 4322, 4242, 4162, 4080, 3998, 3916, 3832,
  3748, 3663, 3577, 3490, 3402, 3314, 3224, 3133,
  3041, 2948, 2854, 2758, 2661, 2562, 2461, 2359,
  2255, 2148, 2040, 1929, 1815, 1698, 1577, 1452,
  1323, 1187, 1045,  894,  731,  550,  339,    0,
  339,  550,  731,  894, 1045, 1187, 1323, 1452,
  1577, 1698, 1815, 1929, 2040, 2148, 2255, 2359,
  2461, 2562, 2661, 2758, 2854, 2948, 3041, 3133,
  3224, 3314, 3402, 3490, 3577, 3663, 3748, 3832,
  3916, 3998, 4080, 4162, 4242, 4322, 4401, 4480,
  4558, 4636, 4713, 4789, 4865, 4941, 5015, 5090,
  5164, 5237, 5311, 5383, 5456, 5527, 5599, 5670,
  5741, 5811, 5881, 5950, 6020, 6089, 6157, 6225,
  6293, 6361, 6428, 6495, 6562, 6628, 6695, 6760,
  6826, 6891, 6956, 7021, 7086, 7150, 7214, 7278,
  7341, 7405, 7468, 7531, 7593, 7656, 7718, 7780,
  7842, 7903, 7965, 8026, 8087, 8148, 8208, 8269,
  8329, 8389, 8449, 8508, 8568, 8627, 8686, 8745,
  8804, 8862, 8921, 8979, 9037, 9095, 9153, 9211,
  9268, 9326, 9383, 9440, 9497, 9553, 9610, 9666,
  9723,  9779,  9835,  9891,  9947, 10002, 10058, 10113,
  10168, 10224, 10279, 10333, 10388, 10443, 10497, 10552,
  10606, 10660, 10714, 10768, 10822, 10875, 10929, 10982,
  11036, 11089, 11142, 11195, 11248, 11301, 11353, 11406,
  11458, 11511, 11563, 11615, 11667, 11719, 11771, 11823,
  11875, 11926, 11978, 12029, 12080, 12132, 12183, 12234,
  12285, 12335, 12386, 12437, 12487, 12538, 12588, 12639,
  12689, 12739, 12789, 12839, 12889, 12939, 12988, 13038,
  13088, 13137, 13187, 13236, 13285, 13334, 13383, 13432,
  13481, 13530, 13579, 13628, 13676, 13725, 13773, 13822,
  13870, 13918, 13967, 14015, 14063, 14111, 14159, 14206,
  14254, 14302, 14350, 14397, 14445, 14492, 14539, 14587,
  14634, 14681, 14728, 14775, 14822, 14869, 14916, 14963,
  15010, 15056, 15103, 15149, 15196, 15242, 15289, 15335,
  15381, 15427, 15474, 15520, 15566, 15612, 15657, 15703,
  15749, 15795, 15840, 15886, 15932, 15977, 16022, 16068,
  16113, 16158, 16204, 16249, 16294, 16339, 16384, 16384,
};
/* clang-format on */

static ProjectPointsFunc get_project_points_type(TransformationType type) {
  switch (type) {
    case VERTRAPEZOID: return project_points_vertrapezoid;
    case HORTRAPEZOID: return project_points_hortrapezoid;
    case HOMOGRAPHY: return project_points_homography;
    case AFFINE: return project_points_affine;
    case ROTZOOM: return project_points_rotzoom;
    case TRANSLATION: return project_points_translation;
    default: assert(0); return NULL;
  }
}

void project_points_translation(const int32_t *mat, int *points, int *proj,
                                const int n, const int stride_points,
                                const int stride_proj, const int subsampling_x,
                                const int subsampling_y) {
  int i;
  for (i = 0; i < n; ++i) {
    const int x = *(points++), y = *(points++);
    if (subsampling_x)
      *(proj++) = ROUND_POWER_OF_TWO_SIGNED(
          ((x * (1 << (WARPEDMODEL_PREC_BITS + 1))) + mat[0]),
          WARPEDDIFF_PREC_BITS + 1);
    else
      *(proj++) = ROUND_POWER_OF_TWO_SIGNED(
          ((x * (1 << WARPEDMODEL_PREC_BITS)) + mat[0]), WARPEDDIFF_PREC_BITS);
    if (subsampling_y)
      *(proj++) = ROUND_POWER_OF_TWO_SIGNED(
          ((y * (1 << (WARPEDMODEL_PREC_BITS + 1))) + mat[1]),
          WARPEDDIFF_PREC_BITS + 1);
    else
      *(proj++) = ROUND_POWER_OF_TWO_SIGNED(
          ((y * (1 << WARPEDMODEL_PREC_BITS))) + mat[1], WARPEDDIFF_PREC_BITS);
    points += stride_points - 2;
    proj += stride_proj - 2;
  }
}

void project_points_rotzoom(const int32_t *mat, int *points, int *proj,
                            const int n, const int stride_points,
                            const int stride_proj, const int subsampling_x,
                            const int subsampling_y) {
  int i;
  for (i = 0; i < n; ++i) {
    const int x = *(points++), y = *(points++);
    if (subsampling_x)
      *(proj++) = ROUND_POWER_OF_TWO_SIGNED(
          mat[2] * 2 * x + mat[3] * 2 * y + mat[0] +
              (mat[2] + mat[3] - (1 << WARPEDMODEL_PREC_BITS)) / 2,
          WARPEDDIFF_PREC_BITS + 1);
    else
      *(proj++) = ROUND_POWER_OF_TWO_SIGNED(mat[2] * x + mat[3] * y + mat[0],
                                            WARPEDDIFF_PREC_BITS);
    if (subsampling_y)
      *(proj++) = ROUND_POWER_OF_TWO_SIGNED(
          -mat[3] * 2 * x + mat[2] * 2 * y + mat[1] +
              (-mat[3] + mat[2] - (1 << WARPEDMODEL_PREC_BITS)) / 2,
          WARPEDDIFF_PREC_BITS + 1);
    else
      *(proj++) = ROUND_POWER_OF_TWO_SIGNED(-mat[3] * x + mat[2] * y + mat[1],
                                            WARPEDDIFF_PREC_BITS);
    points += stride_points - 2;
    proj += stride_proj - 2;
  }
}

void project_points_affine(const int32_t *mat, int *points, int *proj,
                           const int n, const int stride_points,
                           const int stride_proj, const int subsampling_x,
                           const int subsampling_y) {
  int i;
  for (i = 0; i < n; ++i) {
    const int x = *(points++), y = *(points++);
    if (subsampling_x)
      *(proj++) = ROUND_POWER_OF_TWO_SIGNED(
          mat[2] * 2 * x + mat[3] * 2 * y + mat[0] +
              (mat[2] + mat[3] - (1 << WARPEDMODEL_PREC_BITS)) / 2,
          WARPEDDIFF_PREC_BITS + 1);
    else
      *(proj++) = ROUND_POWER_OF_TWO_SIGNED(mat[2] * x + mat[3] * y + mat[0],
                                            WARPEDDIFF_PREC_BITS);
    if (subsampling_y)
      *(proj++) = ROUND_POWER_OF_TWO_SIGNED(
          mat[4] * 2 * x + mat[5] * 2 * y + mat[1] +
              (mat[4] + mat[5] - (1 << WARPEDMODEL_PREC_BITS)) / 2,
          WARPEDDIFF_PREC_BITS + 1);
    else
      *(proj++) = ROUND_POWER_OF_TWO_SIGNED(mat[4] * x + mat[5] * y + mat[1],
                                            WARPEDDIFF_PREC_BITS);
    points += stride_points - 2;
    proj += stride_proj - 2;
  }
}

void project_points_hortrapezoid(const int32_t *mat, int *points, int *proj,
                                 const int n, const int stride_points,
                                 const int stride_proj, const int subsampling_x,
                                 const int subsampling_y) {
  int i;
  int64_t x, y, Z;
  int64_t xp, yp;
  for (i = 0; i < n; ++i) {
    x = *(points++), y = *(points++);
    x = (subsampling_x ? 4 * x + 1 : 2 * x);
    y = (subsampling_y ? 4 * y + 1 : 2 * y);

    Z = (mat[7] * y + (1 << (WARPEDMODEL_ROW3HOMO_PREC_BITS + 1)));
    xp = (mat[2] * x + mat[3] * y + 2 * mat[0]) *
         (1 << (WARPEDPIXEL_PREC_BITS + WARPEDMODEL_ROW3HOMO_PREC_BITS -
                WARPEDMODEL_PREC_BITS));
    yp = (mat[5] * y + 2 * mat[1]) *
         (1 << (WARPEDPIXEL_PREC_BITS + WARPEDMODEL_ROW3HOMO_PREC_BITS -
                WARPEDMODEL_PREC_BITS));

    xp = xp > 0 ? (xp + Z / 2) / Z : (xp - Z / 2) / Z;
    yp = yp > 0 ? (yp + Z / 2) / Z : (yp - Z / 2) / Z;

    if (subsampling_x) xp = (xp - (1 << (WARPEDPIXEL_PREC_BITS - 1))) / 2;
    if (subsampling_y) yp = (yp - (1 << (WARPEDPIXEL_PREC_BITS - 1))) / 2;
    *(proj++) = (int)xp;
    *(proj++) = (int)yp;

    points += stride_points - 2;
    proj += stride_proj - 2;
  }
}

void project_points_vertrapezoid(const int32_t *mat, int *points, int *proj,
                                 const int n, const int stride_points,
                                 const int stride_proj, const int subsampling_x,
                                 const int subsampling_y) {
  int i;
  int64_t x, y, Z;
  int64_t xp, yp;
  for (i = 0; i < n; ++i) {
    x = *(points++), y = *(points++);
    x = (subsampling_x ? 4 * x + 1 : 2 * x);
    y = (subsampling_y ? 4 * y + 1 : 2 * y);

    Z = (mat[6] * x + (1 << (WARPEDMODEL_ROW3HOMO_PREC_BITS + 1)));
    xp = (mat[2] * x + 2 * mat[0]) *
         (1 << (WARPEDPIXEL_PREC_BITS + WARPEDMODEL_ROW3HOMO_PREC_BITS -
                WARPEDMODEL_PREC_BITS));
    yp = (mat[4] * x + mat[5] * y + 2 * mat[1]) *
         (1 << (WARPEDPIXEL_PREC_BITS + WARPEDMODEL_ROW3HOMO_PREC_BITS -
                WARPEDMODEL_PREC_BITS));

    xp = xp > 0 ? (xp + Z / 2) / Z : (xp - Z / 2) / Z;
    yp = yp > 0 ? (yp + Z / 2) / Z : (yp - Z / 2) / Z;

    if (subsampling_x) xp = (xp - (1 << (WARPEDPIXEL_PREC_BITS - 1))) / 2;
    if (subsampling_y) yp = (yp - (1 << (WARPEDPIXEL_PREC_BITS - 1))) / 2;
    *(proj++) = (int)xp;
    *(proj++) = (int)yp;

    points += stride_points - 2;
    proj += stride_proj - 2;
  }
}

void project_points_homography(const int32_t *mat, int *points, int *proj,
                               const int n, const int stride_points,
                               const int stride_proj, const int subsampling_x,
                               const int subsampling_y) {
  int i;
  int64_t x, y, Z;
  int64_t xp, yp;
  for (i = 0; i < n; ++i) {
    x = *(points++), y = *(points++);
    x = (subsampling_x ? 4 * x + 1 : 2 * x);
    y = (subsampling_y ? 4 * y + 1 : 2 * y);

    Z = (mat[6] * x + mat[7] * y + (1 << (WARPEDMODEL_ROW3HOMO_PREC_BITS + 1)));
    xp = (mat[2] * x + mat[3] * y + 2 * mat[0]) *
         (1 << (WARPEDPIXEL_PREC_BITS + WARPEDMODEL_ROW3HOMO_PREC_BITS -
                WARPEDMODEL_PREC_BITS));
    yp = (mat[4] * x + mat[5] * y + 2 * mat[1]) *
         (1 << (WARPEDPIXEL_PREC_BITS + WARPEDMODEL_ROW3HOMO_PREC_BITS -
                WARPEDMODEL_PREC_BITS));

    xp = xp > 0 ? (xp + Z / 2) / Z : (xp - Z / 2) / Z;
    yp = yp > 0 ? (yp + Z / 2) / Z : (yp - Z / 2) / Z;

    if (subsampling_x) xp = (xp - (1 << (WARPEDPIXEL_PREC_BITS - 1))) / 2;
    if (subsampling_y) yp = (yp - (1 << (WARPEDPIXEL_PREC_BITS - 1))) / 2;
    *(proj++) = (int)xp;
    *(proj++) = (int)yp;

    points += stride_points - 2;
    proj += stride_proj - 2;
  }
}

static const int16_t
    filter_ntap[WARPEDPIXEL_PREC_SHIFTS][WARPEDPIXEL_FILTER_TAPS] = {
#if WARPEDPIXEL_PREC_BITS == 6
      { 0, 0, 128, 0, 0, 0 },      { 0, -1, 128, 2, -1, 0 },
      { 1, -3, 127, 4, -1, 0 },    { 1, -4, 126, 6, -2, 1 },
      { 1, -5, 126, 8, -3, 1 },    { 1, -6, 125, 11, -4, 1 },
      { 1, -7, 124, 13, -4, 1 },   { 2, -8, 123, 15, -5, 1 },
      { 2, -9, 122, 18, -6, 1 },   { 2, -10, 121, 20, -6, 1 },
      { 2, -11, 120, 22, -7, 2 },  { 2, -12, 119, 25, -8, 2 },
      { 3, -13, 117, 27, -8, 2 },  { 3, -13, 116, 29, -9, 2 },
      { 3, -14, 114, 32, -10, 3 }, { 3, -15, 113, 35, -10, 2 },
      { 3, -15, 111, 37, -11, 3 }, { 3, -16, 109, 40, -11, 3 },
      { 3, -16, 108, 42, -12, 3 }, { 4, -17, 106, 45, -13, 3 },
      { 4, -17, 104, 47, -13, 3 }, { 4, -17, 102, 50, -14, 3 },
      { 4, -17, 100, 52, -14, 3 }, { 4, -18, 98, 55, -15, 4 },
      { 4, -18, 96, 58, -15, 3 },  { 4, -18, 94, 60, -16, 4 },
      { 4, -18, 91, 63, -16, 4 },  { 4, -18, 89, 65, -16, 4 },
      { 4, -18, 87, 68, -17, 4 },  { 4, -18, 85, 70, -17, 4 },
      { 4, -18, 82, 73, -17, 4 },  { 4, -18, 80, 75, -17, 4 },
      { 4, -18, 78, 78, -18, 4 },  { 4, -17, 75, 80, -18, 4 },
      { 4, -17, 73, 82, -18, 4 },  { 4, -17, 70, 85, -18, 4 },
      { 4, -17, 68, 87, -18, 4 },  { 4, -16, 65, 89, -18, 4 },
      { 4, -16, 63, 91, -18, 4 },  { 4, -16, 60, 94, -18, 4 },
      { 3, -15, 58, 96, -18, 4 },  { 4, -15, 55, 98, -18, 4 },
      { 3, -14, 52, 100, -17, 4 }, { 3, -14, 50, 102, -17, 4 },
      { 3, -13, 47, 104, -17, 4 }, { 3, -13, 45, 106, -17, 4 },
      { 3, -12, 42, 108, -16, 3 }, { 3, -11, 40, 109, -16, 3 },
      { 3, -11, 37, 111, -15, 3 }, { 2, -10, 35, 113, -15, 3 },
      { 3, -10, 32, 114, -14, 3 }, { 2, -9, 29, 116, -13, 3 },
      { 2, -8, 27, 117, -13, 3 },  { 2, -8, 25, 119, -12, 2 },
      { 2, -7, 22, 120, -11, 2 },  { 1, -6, 20, 121, -10, 2 },
      { 1, -6, 18, 122, -9, 2 },   { 1, -5, 15, 123, -8, 2 },
      { 1, -4, 13, 124, -7, 1 },   { 1, -4, 11, 125, -6, 1 },
      { 1, -3, 8, 126, -5, 1 },    { 1, -2, 6, 126, -4, 1 },
      { 0, -1, 4, 127, -3, 1 },    { 0, -1, 2, 128, -1, 0 },
#elif WARPEDPIXEL_PREC_BITS == 5
      { 0, 0, 128, 0, 0, 0 },      { 1, -3, 127, 4, -1, 0 },
      { 1, -5, 126, 8, -3, 1 },    { 1, -7, 124, 13, -4, 1 },
      { 2, -9, 122, 18, -6, 1 },   { 2, -11, 120, 22, -7, 2 },
      { 3, -13, 117, 27, -8, 2 },  { 3, -14, 114, 32, -10, 3 },
      { 3, -15, 111, 37, -11, 3 }, { 3, -16, 108, 42, -12, 3 },
      { 4, -17, 104, 47, -13, 3 }, { 4, -17, 100, 52, -14, 3 },
      { 4, -18, 96, 58, -15, 3 },  { 4, -18, 91, 63, -16, 4 },
      { 4, -18, 87, 68, -17, 4 },  { 4, -18, 82, 73, -17, 4 },
      { 4, -18, 78, 78, -18, 4 },  { 4, -17, 73, 82, -18, 4 },
      { 4, -17, 68, 87, -18, 4 },  { 4, -16, 63, 91, -18, 4 },
      { 3, -15, 58, 96, -18, 4 },  { 3, -14, 52, 100, -17, 4 },
      { 3, -13, 47, 104, -17, 4 }, { 3, -12, 42, 108, -16, 3 },
      { 3, -11, 37, 111, -15, 3 }, { 3, -10, 32, 114, -14, 3 },
      { 2, -8, 27, 117, -13, 3 },  { 2, -7, 22, 120, -11, 2 },
      { 1, -6, 18, 122, -9, 2 },   { 1, -4, 13, 124, -7, 1 },
      { 1, -3, 8, 126, -5, 1 },    { 0, -1, 4, 127, -3, 1 },
#endif  // WARPEDPIXEL_PREC_BITS == 6
    };

static int32_t do_ntap_filter(const int32_t *const p, int x) {
  int i;
  int32_t sum = 0;
  for (i = 0; i < WARPEDPIXEL_FILTER_TAPS; ++i) {
    sum += p[i - WARPEDPIXEL_FILTER_TAPS / 2 + 1] * filter_ntap[x][i];
  }
  return sum;
}

static int32_t do_cubic_filter(const int32_t *const p, int x) {
  if (x == 0) {
    return p[0] * (1 << WARPEDPIXEL_FILTER_BITS);
  } else if (x == (1 << WARPEDPIXEL_PREC_BITS)) {
    return p[1] * (1 << WARPEDPIXEL_FILTER_BITS);
  } else {
    const int64_t v1 = (int64_t)x * x * x * (3 * (p[0] - p[1]) + p[2] - p[-1]);
    const int64_t v2 =
        (int64_t)x * x * (2 * p[-1] - 5 * p[0] + 4 * p[1] - p[2]);
    const int64_t v3 = x * (p[1] - p[-1]);
    const int64_t v4 = 2 * p[0];
    return (int32_t)ROUND_POWER_OF_TWO_SIGNED(
        (v4 * (1 << (3 * WARPEDPIXEL_PREC_BITS))) +
            (v3 * (1 << (2 * WARPEDPIXEL_PREC_BITS))) +
            (v2 * (1 << WARPEDPIXEL_PREC_BITS)) + v1,
        3 * WARPEDPIXEL_PREC_BITS + 1 - WARPEDPIXEL_FILTER_BITS);
  }
}

static INLINE void get_subcolumn(int taps, const uint8_t *const ref,
                                 int32_t *col, int stride, int x, int y_start) {
  int i;
  for (i = 0; i < taps; ++i) {
    col[i] = ref[(i + y_start) * stride + x];
  }
}

static uint8_t bi_ntap_filter(const uint8_t *const ref, int x, int y,
                              int stride) {
  int32_t val, arr[WARPEDPIXEL_FILTER_TAPS];
  int k;
  const int i = (int)x >> WARPEDPIXEL_PREC_BITS;
  const int j = (int)y >> WARPEDPIXEL_PREC_BITS;
  for (k = 0; k < WARPEDPIXEL_FILTER_TAPS; ++k) {
    int32_t arr_temp[WARPEDPIXEL_FILTER_TAPS];
    get_subcolumn(WARPEDPIXEL_FILTER_TAPS, ref, arr_temp, stride,
                  i + k + 1 - WARPEDPIXEL_FILTER_TAPS / 2,
                  j + 1 - WARPEDPIXEL_FILTER_TAPS / 2);
    arr[k] = do_ntap_filter(arr_temp + WARPEDPIXEL_FILTER_TAPS / 2 - 1,
                            y - (j * (1 << WARPEDPIXEL_PREC_BITS)));
  }
  val = do_ntap_filter(arr + WARPEDPIXEL_FILTER_TAPS / 2 - 1,
                       x - (i * (1 << WARPEDPIXEL_PREC_BITS)));
  val = ROUND_POWER_OF_TWO_SIGNED(val, WARPEDPIXEL_FILTER_BITS * 2);
  return (uint8_t)clip_pixel(val);
}

static uint8_t bi_cubic_filter(const uint8_t *const ref, int x, int y,
                               int stride) {
  int32_t val, arr[4];
  int k;
  const int i = (int)x >> WARPEDPIXEL_PREC_BITS;
  const int j = (int)y >> WARPEDPIXEL_PREC_BITS;
  for (k = 0; k < 4; ++k) {
    int32_t arr_temp[4];
    get_subcolumn(4, ref, arr_temp, stride, i + k - 1, j - 1);
    arr[k] =
        do_cubic_filter(arr_temp + 1, y - (j * (1 << WARPEDPIXEL_PREC_BITS)));
  }
  val = do_cubic_filter(arr + 1, x - (i * (1 << WARPEDPIXEL_PREC_BITS)));
  val = ROUND_POWER_OF_TWO_SIGNED(val, WARPEDPIXEL_FILTER_BITS * 2);
  return (uint8_t)clip_pixel(val);
}

static uint8_t bi_linear_filter(const uint8_t *const ref, int x, int y,
                                int stride) {
  const int ix = x >> WARPEDPIXEL_PREC_BITS;
  const int iy = y >> WARPEDPIXEL_PREC_BITS;
  const int sx = x - (ix * (1 << WARPEDPIXEL_PREC_BITS));
  const int sy = y - (iy * (1 << WARPEDPIXEL_PREC_BITS));
  int32_t val;
  val = ROUND_POWER_OF_TWO_SIGNED(
      ref[iy * stride + ix] * (WARPEDPIXEL_PREC_SHIFTS - sy) *
              (WARPEDPIXEL_PREC_SHIFTS - sx) +
          ref[iy * stride + ix + 1] * (WARPEDPIXEL_PREC_SHIFTS - sy) * sx +
          ref[(iy + 1) * stride + ix] * sy * (WARPEDPIXEL_PREC_SHIFTS - sx) +
          ref[(iy + 1) * stride + ix + 1] * sy * sx,
      WARPEDPIXEL_PREC_BITS * 2);
  return (uint8_t)clip_pixel(val);
}

static uint8_t warp_interpolate(const uint8_t *const ref, int x, int y,
                                int width, int height, int stride) {
  const int ix = x >> WARPEDPIXEL_PREC_BITS;
  const int iy = y >> WARPEDPIXEL_PREC_BITS;
  const int sx = x - (ix * (1 << WARPEDPIXEL_PREC_BITS));
  const int sy = y - (iy * (1 << WARPEDPIXEL_PREC_BITS));
  int32_t v;

  if (ix < 0 && iy < 0)
    return ref[0];
  else if (ix < 0 && iy >= height - 1)
    return ref[(height - 1) * stride];
  else if (ix >= width - 1 && iy < 0)
    return ref[width - 1];
  else if (ix >= width - 1 && iy >= height - 1)
    return ref[(height - 1) * stride + (width - 1)];
  else if (ix < 0) {
    v = ROUND_POWER_OF_TWO_SIGNED(
        ref[iy * stride] * (WARPEDPIXEL_PREC_SHIFTS - sy) +
            ref[(iy + 1) * stride] * sy,
        WARPEDPIXEL_PREC_BITS);
    return clip_pixel(v);
  } else if (iy < 0) {
    v = ROUND_POWER_OF_TWO_SIGNED(
        ref[ix] * (WARPEDPIXEL_PREC_SHIFTS - sx) + ref[ix + 1] * sx,
        WARPEDPIXEL_PREC_BITS);
    return clip_pixel(v);
  } else if (ix >= width - 1) {
    v = ROUND_POWER_OF_TWO_SIGNED(
        ref[iy * stride + width - 1] * (WARPEDPIXEL_PREC_SHIFTS - sy) +
            ref[(iy + 1) * stride + width - 1] * sy,
        WARPEDPIXEL_PREC_BITS);
    return clip_pixel(v);
  } else if (iy >= height - 1) {
    v = ROUND_POWER_OF_TWO_SIGNED(
        ref[(height - 1) * stride + ix] * (WARPEDPIXEL_PREC_SHIFTS - sx) +
            ref[(height - 1) * stride + ix + 1] * sx,
        WARPEDPIXEL_PREC_BITS);
    return clip_pixel(v);
  } else if (ix >= WARPEDPIXEL_FILTER_TAPS / 2 - 1 &&
             iy >= WARPEDPIXEL_FILTER_TAPS / 2 - 1 &&
             ix < width - WARPEDPIXEL_FILTER_TAPS / 2 &&
             iy < height - WARPEDPIXEL_FILTER_TAPS / 2) {
    return bi_ntap_filter(ref, x, y, stride);
  } else if (ix >= 1 && iy >= 1 && ix < width - 2 && iy < height - 2) {
    return bi_cubic_filter(ref, x, y, stride);
  } else {
    return bi_linear_filter(ref, x, y, stride);
  }
}

// For warping, we really use a 6-tap filter, but we do blocks of 8 pixels
// at a time. The zoom/rotation/shear in the model are applied to the
// "fractional" position of each pixel, which therefore varies within
// [-1, 2) * WARPEDPIXEL_PREC_SHIFTS.
// We need an extra 2 taps to fit this in, for a total of 8 taps.
/* clang-format off */
const int16_t warped_filter[WARPEDPIXEL_PREC_SHIFTS * 3 + 1][8] = {
#if WARPEDPIXEL_PREC_BITS == 6
  // [-1, 0)
  { 0,   0, 127,   1,   0, 0, 0, 0 }, { 0, - 1, 127,   2,   0, 0, 0, 0 },
  { 1, - 3, 127,   4, - 1, 0, 0, 0 }, { 1, - 4, 126,   6, - 2, 1, 0, 0 },
  { 1, - 5, 126,   8, - 3, 1, 0, 0 }, { 1, - 6, 125,  11, - 4, 1, 0, 0 },
  { 1, - 7, 124,  13, - 4, 1, 0, 0 }, { 2, - 8, 123,  15, - 5, 1, 0, 0 },
  { 2, - 9, 122,  18, - 6, 1, 0, 0 }, { 2, -10, 121,  20, - 6, 1, 0, 0 },
  { 2, -11, 120,  22, - 7, 2, 0, 0 }, { 2, -12, 119,  25, - 8, 2, 0, 0 },
  { 3, -13, 117,  27, - 8, 2, 0, 0 }, { 3, -13, 116,  29, - 9, 2, 0, 0 },
  { 3, -14, 114,  32, -10, 3, 0, 0 }, { 3, -15, 113,  35, -10, 2, 0, 0 },
  { 3, -15, 111,  37, -11, 3, 0, 0 }, { 3, -16, 109,  40, -11, 3, 0, 0 },
  { 3, -16, 108,  42, -12, 3, 0, 0 }, { 4, -17, 106,  45, -13, 3, 0, 0 },
  { 4, -17, 104,  47, -13, 3, 0, 0 }, { 4, -17, 102,  50, -14, 3, 0, 0 },
  { 4, -17, 100,  52, -14, 3, 0, 0 }, { 4, -18,  98,  55, -15, 4, 0, 0 },
  { 4, -18,  96,  58, -15, 3, 0, 0 }, { 4, -18,  94,  60, -16, 4, 0, 0 },
  { 4, -18,  91,  63, -16, 4, 0, 0 }, { 4, -18,  89,  65, -16, 4, 0, 0 },
  { 4, -18,  87,  68, -17, 4, 0, 0 }, { 4, -18,  85,  70, -17, 4, 0, 0 },
  { 4, -18,  82,  73, -17, 4, 0, 0 }, { 4, -18,  80,  75, -17, 4, 0, 0 },
  { 4, -18,  78,  78, -18, 4, 0, 0 }, { 4, -17,  75,  80, -18, 4, 0, 0 },
  { 4, -17,  73,  82, -18, 4, 0, 0 }, { 4, -17,  70,  85, -18, 4, 0, 0 },
  { 4, -17,  68,  87, -18, 4, 0, 0 }, { 4, -16,  65,  89, -18, 4, 0, 0 },
  { 4, -16,  63,  91, -18, 4, 0, 0 }, { 4, -16,  60,  94, -18, 4, 0, 0 },
  { 3, -15,  58,  96, -18, 4, 0, 0 }, { 4, -15,  55,  98, -18, 4, 0, 0 },
  { 3, -14,  52, 100, -17, 4, 0, 0 }, { 3, -14,  50, 102, -17, 4, 0, 0 },
  { 3, -13,  47, 104, -17, 4, 0, 0 }, { 3, -13,  45, 106, -17, 4, 0, 0 },
  { 3, -12,  42, 108, -16, 3, 0, 0 }, { 3, -11,  40, 109, -16, 3, 0, 0 },
  { 3, -11,  37, 111, -15, 3, 0, 0 }, { 2, -10,  35, 113, -15, 3, 0, 0 },
  { 3, -10,  32, 114, -14, 3, 0, 0 }, { 2, - 9,  29, 116, -13, 3, 0, 0 },
  { 2, - 8,  27, 117, -13, 3, 0, 0 }, { 2, - 8,  25, 119, -12, 2, 0, 0 },
  { 2, - 7,  22, 120, -11, 2, 0, 0 }, { 1, - 6,  20, 121, -10, 2, 0, 0 },
  { 1, - 6,  18, 122, - 9, 2, 0, 0 }, { 1, - 5,  15, 123, - 8, 2, 0, 0 },
  { 1, - 4,  13, 124, - 7, 1, 0, 0 }, { 1, - 4,  11, 125, - 6, 1, 0, 0 },
  { 1, - 3,   8, 126, - 5, 1, 0, 0 }, { 1, - 2,   6, 126, - 4, 1, 0, 0 },
  { 0, - 1,   4, 127, - 3, 1, 0, 0 }, { 0,   0,   2, 127, - 1, 0, 0, 0 },

  // [0, 1)
  { 0,  0,   0, 127,   1,   0,  0,  0}, { 0,  0,  -1, 127,   2,   0,  0,  0},
  { 0,  1,  -3, 127,   4,  -2,  1,  0}, { 0,  1,  -5, 127,   6,  -2,  1,  0},
  { 0,  2,  -6, 126,   8,  -3,  1,  0}, {-1,  2,  -7, 126,  11,  -4,  2, -1},
  {-1,  3,  -8, 125,  13,  -5,  2, -1}, {-1,  3, -10, 124,  16,  -6,  3, -1},
  {-1,  4, -11, 123,  18,  -7,  3, -1}, {-1,  4, -12, 122,  20,  -7,  3, -1},
  {-1,  4, -13, 121,  23,  -8,  3, -1}, {-2,  5, -14, 120,  25,  -9,  4, -1},
  {-1,  5, -15, 119,  27, -10,  4, -1}, {-1,  5, -16, 118,  30, -11,  4, -1},
  {-2,  6, -17, 116,  33, -12,  5, -1}, {-2,  6, -17, 114,  35, -12,  5, -1},
  {-2,  6, -18, 113,  38, -13,  5, -1}, {-2,  7, -19, 111,  41, -14,  6, -2},
  {-2,  7, -19, 110,  43, -15,  6, -2}, {-2,  7, -20, 108,  46, -15,  6, -2},
  {-2,  7, -20, 106,  49, -16,  6, -2}, {-2,  7, -21, 104,  51, -16,  7, -2},
  {-2,  7, -21, 102,  54, -17,  7, -2}, {-2,  8, -21, 100,  56, -18,  7, -2},
  {-2,  8, -22,  98,  59, -18,  7, -2}, {-2,  8, -22,  96,  62, -19,  7, -2},
  {-2,  8, -22,  94,  64, -19,  7, -2}, {-2,  8, -22,  91,  67, -20,  8, -2},
  {-2,  8, -22,  89,  69, -20,  8, -2}, {-2,  8, -22,  87,  72, -21,  8, -2},
  {-2,  8, -21,  84,  74, -21,  8, -2}, {-2,  8, -22,  82,  77, -21,  8, -2},
  {-2,  8, -21,  79,  79, -21,  8, -2}, {-2,  8, -21,  77,  82, -22,  8, -2},
  {-2,  8, -21,  74,  84, -21,  8, -2}, {-2,  8, -21,  72,  87, -22,  8, -2},
  {-2,  8, -20,  69,  89, -22,  8, -2}, {-2,  8, -20,  67,  91, -22,  8, -2},
  {-2,  7, -19,  64,  94, -22,  8, -2}, {-2,  7, -19,  62,  96, -22,  8, -2},
  {-2,  7, -18,  59,  98, -22,  8, -2}, {-2,  7, -18,  56, 100, -21,  8, -2},
  {-2,  7, -17,  54, 102, -21,  7, -2}, {-2,  7, -16,  51, 104, -21,  7, -2},
  {-2,  6, -16,  49, 106, -20,  7, -2}, {-2,  6, -15,  46, 108, -20,  7, -2},
  {-2,  6, -15,  43, 110, -19,  7, -2}, {-2,  6, -14,  41, 111, -19,  7, -2},
  {-1,  5, -13,  38, 113, -18,  6, -2}, {-1,  5, -12,  35, 114, -17,  6, -2},
  {-1,  5, -12,  33, 116, -17,  6, -2}, {-1,  4, -11,  30, 118, -16,  5, -1},
  {-1,  4, -10,  27, 119, -15,  5, -1}, {-1,  4,  -9,  25, 120, -14,  5, -2},
  {-1,  3,  -8,  23, 121, -13,  4, -1}, {-1,  3,  -7,  20, 122, -12,  4, -1},
  {-1,  3,  -7,  18, 123, -11,  4, -1}, {-1,  3,  -6,  16, 124, -10,  3, -1},
  {-1,  2,  -5,  13, 125,  -8,  3, -1}, {-1,  2,  -4,  11, 126,  -7,  2, -1},
  { 0,  1,  -3,   8, 126,  -6,  2,  0}, { 0,  1,  -2,   6, 127,  -5,  1,  0},
  { 0,  1,  -2,   4, 127,  -3,  1,  0}, { 0,  0,   0,   2, 127,  -1,  0,  0},

  // [1, 2)
  { 0, 0, 0,   1, 127,   0,   0, 0 }, { 0, 0, 0, - 1, 127,   2,   0, 0 },
  { 0, 0, 1, - 3, 127,   4, - 1, 0 }, { 0, 0, 1, - 4, 126,   6, - 2, 1 },
  { 0, 0, 1, - 5, 126,   8, - 3, 1 }, { 0, 0, 1, - 6, 125,  11, - 4, 1 },
  { 0, 0, 1, - 7, 124,  13, - 4, 1 }, { 0, 0, 2, - 8, 123,  15, - 5, 1 },
  { 0, 0, 2, - 9, 122,  18, - 6, 1 }, { 0, 0, 2, -10, 121,  20, - 6, 1 },
  { 0, 0, 2, -11, 120,  22, - 7, 2 }, { 0, 0, 2, -12, 119,  25, - 8, 2 },
  { 0, 0, 3, -13, 117,  27, - 8, 2 }, { 0, 0, 3, -13, 116,  29, - 9, 2 },
  { 0, 0, 3, -14, 114,  32, -10, 3 }, { 0, 0, 3, -15, 113,  35, -10, 2 },
  { 0, 0, 3, -15, 111,  37, -11, 3 }, { 0, 0, 3, -16, 109,  40, -11, 3 },
  { 0, 0, 3, -16, 108,  42, -12, 3 }, { 0, 0, 4, -17, 106,  45, -13, 3 },
  { 0, 0, 4, -17, 104,  47, -13, 3 }, { 0, 0, 4, -17, 102,  50, -14, 3 },
  { 0, 0, 4, -17, 100,  52, -14, 3 }, { 0, 0, 4, -18,  98,  55, -15, 4 },
  { 0, 0, 4, -18,  96,  58, -15, 3 }, { 0, 0, 4, -18,  94,  60, -16, 4 },
  { 0, 0, 4, -18,  91,  63, -16, 4 }, { 0, 0, 4, -18,  89,  65, -16, 4 },
  { 0, 0, 4, -18,  87,  68, -17, 4 }, { 0, 0, 4, -18,  85,  70, -17, 4 },
  { 0, 0, 4, -18,  82,  73, -17, 4 }, { 0, 0, 4, -18,  80,  75, -17, 4 },
  { 0, 0, 4, -18,  78,  78, -18, 4 }, { 0, 0, 4, -17,  75,  80, -18, 4 },
  { 0, 0, 4, -17,  73,  82, -18, 4 }, { 0, 0, 4, -17,  70,  85, -18, 4 },
  { 0, 0, 4, -17,  68,  87, -18, 4 }, { 0, 0, 4, -16,  65,  89, -18, 4 },
  { 0, 0, 4, -16,  63,  91, -18, 4 }, { 0, 0, 4, -16,  60,  94, -18, 4 },
  { 0, 0, 3, -15,  58,  96, -18, 4 }, { 0, 0, 4, -15,  55,  98, -18, 4 },
  { 0, 0, 3, -14,  52, 100, -17, 4 }, { 0, 0, 3, -14,  50, 102, -17, 4 },
  { 0, 0, 3, -13,  47, 104, -17, 4 }, { 0, 0, 3, -13,  45, 106, -17, 4 },
  { 0, 0, 3, -12,  42, 108, -16, 3 }, { 0, 0, 3, -11,  40, 109, -16, 3 },
  { 0, 0, 3, -11,  37, 111, -15, 3 }, { 0, 0, 2, -10,  35, 113, -15, 3 },
  { 0, 0, 3, -10,  32, 114, -14, 3 }, { 0, 0, 2, - 9,  29, 116, -13, 3 },
  { 0, 0, 2, - 8,  27, 117, -13, 3 }, { 0, 0, 2, - 8,  25, 119, -12, 2 },
  { 0, 0, 2, - 7,  22, 120, -11, 2 }, { 0, 0, 1, - 6,  20, 121, -10, 2 },
  { 0, 0, 1, - 6,  18, 122, - 9, 2 }, { 0, 0, 1, - 5,  15, 123, - 8, 2 },
  { 0, 0, 1, - 4,  13, 124, - 7, 1 }, { 0, 0, 1, - 4,  11, 125, - 6, 1 },
  { 0, 0, 1, - 3,   8, 126, - 5, 1 }, { 0, 0, 1, - 2,   6, 126, - 4, 1 },
  { 0, 0, 0, - 1,   4, 127, - 3, 1 }, { 0, 0, 0,   0,   2, 127, - 1, 0 },
  // dummy (replicate row index 191)
  { 0, 0, 0,   0,   2, 127, - 1, 0 },

#elif WARPEDPIXEL_PREC_BITS == 5
  // [-1, 0)
  {0,   0, 127,   1,   0, 0, 0, 0}, {1,  -3, 127,   4,  -1, 0, 0, 0},
  {1,  -5, 126,   8,  -3, 1, 0, 0}, {1,  -7, 124,  13,  -4, 1, 0, 0},
  {2,  -9, 122,  18,  -6, 1, 0, 0}, {2, -11, 120,  22,  -7, 2, 0, 0},
  {3, -13, 117,  27,  -8, 2, 0, 0}, {3, -14, 114,  32, -10, 3, 0, 0},
  {3, -15, 111,  37, -11, 3, 0, 0}, {3, -16, 108,  42, -12, 3, 0, 0},
  {4, -17, 104,  47, -13, 3, 0, 0}, {4, -17, 100,  52, -14, 3, 0, 0},
  {4, -18,  96,  58, -15, 3, 0, 0}, {4, -18,  91,  63, -16, 4, 0, 0},
  {4, -18,  87,  68, -17, 4, 0, 0}, {4, -18,  82,  73, -17, 4, 0, 0},
  {4, -18,  78,  78, -18, 4, 0, 0}, {4, -17,  73,  82, -18, 4, 0, 0},
  {4, -17,  68,  87, -18, 4, 0, 0}, {4, -16,  63,  91, -18, 4, 0, 0},
  {3, -15,  58,  96, -18, 4, 0, 0}, {3, -14,  52, 100, -17, 4, 0, 0},
  {3, -13,  47, 104, -17, 4, 0, 0}, {3, -12,  42, 108, -16, 3, 0, 0},
  {3, -11,  37, 111, -15, 3, 0, 0}, {3, -10,  32, 114, -14, 3, 0, 0},
  {2,  -8,  27, 117, -13, 3, 0, 0}, {2,  -7,  22, 120, -11, 2, 0, 0},
  {1,  -6,  18, 122,  -9, 2, 0, 0}, {1,  -4,  13, 124,  -7, 1, 0, 0},
  {1,  -3,   8, 126,  -5, 1, 0, 0}, {0,  -1,   4, 127,  -3, 1, 0, 0},
  // [0, 1)
  { 0,  0,   0, 127,   1,   0,   0,  0}, { 0,  1,  -3, 127,   4,  -2,   1,  0},
  { 0,  2,  -6, 126,   8,  -3,   1,  0}, {-1,  3,  -8, 125,  13,  -5,   2, -1},
  {-1,  4, -11, 123,  18,  -7,   3, -1}, {-1,  4, -13, 121,  23,  -8,   3, -1},
  {-1,  5, -15, 119,  27, -10,   4, -1}, {-2,  6, -17, 116,  33, -12,   5, -1},
  {-2,  6, -18, 113,  38, -13,   5, -1}, {-2,  7, -19, 110,  43, -15,   6, -2},
  {-2,  7, -20, 106,  49, -16,   6, -2}, {-2,  7, -21, 102,  54, -17,   7, -2},
  {-2,  8, -22,  98,  59, -18,   7, -2}, {-2,  8, -22,  94,  64, -19,   7, -2},
  {-2,  8, -22,  89,  69, -20,   8, -2}, {-2,  8, -21,  84,  74, -21,   8, -2},
  {-2,  8, -21,  79,  79, -21,   8, -2}, {-2,  8, -21,  74,  84, -21,   8, -2},
  {-2,  8, -20,  69,  89, -22,   8, -2}, {-2,  7, -19,  64,  94, -22,   8, -2},
  {-2,  7, -18,  59,  98, -22,   8, -2}, {-2,  7, -17,  54, 102, -21,   7, -2},
  {-2,  6, -16,  49, 106, -20,   7, -2}, {-2,  6, -15,  43, 110, -19,   7, -2},
  {-1,  5, -13,  38, 113, -18,   6, -2}, {-1,  5, -12,  33, 116, -17,   6, -2},
  {-1,  4, -10,  27, 119, -15,   5, -1}, {-1,  3,  -8,  23, 121, -13,   4, -1},
  {-1,  3,  -7,  18, 123, -11,   4, -1}, {-1,  2,  -5,  13, 125,  -8,   3, -1},
  { 0,  1,  -3,   8, 126,  -6,   2,  0}, { 0,  1,  -2,   4, 127,  -3,   1,  0},
  // [1, 2)
  {0, 0, 0,   1, 127,   0,   0, 0}, {0, 0, 1,  -3, 127,   4,  -1, 0},
  {0, 0, 1,  -5, 126,   8,  -3, 1}, {0, 0, 1,  -7, 124,  13,  -4, 1},
  {0, 0, 2,  -9, 122,  18,  -6, 1}, {0, 0, 2, -11, 120,  22,  -7, 2},
  {0, 0, 3, -13, 117,  27,  -8, 2}, {0, 0, 3, -14, 114,  32, -10, 3},
  {0, 0, 3, -15, 111,  37, -11, 3}, {0, 0, 3, -16, 108,  42, -12, 3},
  {0, 0, 4, -17, 104,  47, -13, 3}, {0, 0, 4, -17, 100,  52, -14, 3},
  {0, 0, 4, -18,  96,  58, -15, 3}, {0, 0, 4, -18,  91,  63, -16, 4},
  {0, 0, 4, -18,  87,  68, -17, 4}, {0, 0, 4, -18,  82,  73, -17, 4},
  {0, 0, 4, -18,  78,  78, -18, 4}, {0, 0, 4, -17,  73,  82, -18, 4},
  {0, 0, 4, -17,  68,  87, -18, 4}, {0, 0, 4, -16,  63,  91, -18, 4},
  {0, 0, 3, -15,  58,  96, -18, 4}, {0, 0, 3, -14,  52, 100, -17, 4},
  {0, 0, 3, -13,  47, 104, -17, 4}, {0, 0, 3, -12,  42, 108, -16, 3},
  {0, 0, 3, -11,  37, 111, -15, 3}, {0, 0, 3, -10,  32, 114, -14, 3},
  {0, 0, 2,  -8,  27, 117, -13, 3}, {0, 0, 2,  -7,  22, 120, -11, 2},
  {0, 0, 1,  -6,  18, 122,  -9, 2}, {0, 0, 1,  -4,  13, 124,  -7, 1},
  {0, 0, 1,  -3,   8, 126,  -5, 1}, {0, 0, 0,  -1,   4, 127,  -3, 1},
  // dummy (replicate row index 95)
  {0, 0, 0,  -1,   4, 127,  -3, 1},

#endif  // WARPEDPIXEL_PREC_BITS == 6
};

/* clang-format on */

#define DIV_LUT_PREC_BITS 14
#define DIV_LUT_BITS 8
#define DIV_LUT_NUM (1 << DIV_LUT_BITS)

static const uint16_t div_lut[DIV_LUT_NUM + 1] = {
  16384, 16320, 16257, 16194, 16132, 16070, 16009, 15948, 15888, 15828, 15768,
  15709, 15650, 15592, 15534, 15477, 15420, 15364, 15308, 15252, 15197, 15142,
  15087, 15033, 14980, 14926, 14873, 14821, 14769, 14717, 14665, 14614, 14564,
  14513, 14463, 14413, 14364, 14315, 14266, 14218, 14170, 14122, 14075, 14028,
  13981, 13935, 13888, 13843, 13797, 13752, 13707, 13662, 13618, 13574, 13530,
  13487, 13443, 13400, 13358, 13315, 13273, 13231, 13190, 13148, 13107, 13066,
  13026, 12985, 12945, 12906, 12866, 12827, 12788, 12749, 12710, 12672, 12633,
  12596, 12558, 12520, 12483, 12446, 12409, 12373, 12336, 12300, 12264, 12228,
  12193, 12157, 12122, 12087, 12053, 12018, 11984, 11950, 11916, 11882, 11848,
  11815, 11782, 11749, 11716, 11683, 11651, 11619, 11586, 11555, 11523, 11491,
  11460, 11429, 11398, 11367, 11336, 11305, 11275, 11245, 11215, 11185, 11155,
  11125, 11096, 11067, 11038, 11009, 10980, 10951, 10923, 10894, 10866, 10838,
  10810, 10782, 10755, 10727, 10700, 10673, 10645, 10618, 10592, 10565, 10538,
  10512, 10486, 10460, 10434, 10408, 10382, 10356, 10331, 10305, 10280, 10255,
  10230, 10205, 10180, 10156, 10131, 10107, 10082, 10058, 10034, 10010, 9986,
  9963,  9939,  9916,  9892,  9869,  9846,  9823,  9800,  9777,  9754,  9732,
  9709,  9687,  9664,  9642,  9620,  9598,  9576,  9554,  9533,  9511,  9489,
  9468,  9447,  9425,  9404,  9383,  9362,  9341,  9321,  9300,  9279,  9259,
  9239,  9218,  9198,  9178,  9158,  9138,  9118,  9098,  9079,  9059,  9039,
  9020,  9001,  8981,  8962,  8943,  8924,  8905,  8886,  8867,  8849,  8830,
  8812,  8793,  8775,  8756,  8738,  8720,  8702,  8684,  8666,  8648,  8630,
  8613,  8595,  8577,  8560,  8542,  8525,  8508,  8490,  8473,  8456,  8439,
  8422,  8405,  8389,  8372,  8355,  8339,  8322,  8306,  8289,  8273,  8257,
  8240,  8224,  8208,  8192,
};

#if CONFIG_WARPED_MOTION
// Decomposes a divisor D such that 1/D = y/2^shift, where y is returned
// at precision of DIV_LUT_PREC_BITS along with the shift.
static int16_t resolve_divisor_64(uint64_t D, int16_t *shift) {
  int64_t e, f;
  *shift = (int16_t)((D >> 32) ? get_msb((unsigned int)(D >> 32)) + 32
                               : get_msb((unsigned int)D));
  // e is obtained from D after resetting the most significant 1 bit.
  e = D - ((uint64_t)1 << *shift);
  // Get the most significant DIV_LUT_BITS (8) bits of e into f
  if (*shift > DIV_LUT_BITS)
    f = ROUND_POWER_OF_TWO_64(e, *shift - DIV_LUT_BITS);
  else
    f = e << (DIV_LUT_BITS - *shift);
  assert(f <= DIV_LUT_NUM);
  *shift += DIV_LUT_PREC_BITS;
  // Use f as lookup into the precomputed table of multipliers
  return div_lut[f];
}
#endif  // CONFIG_WARPED_MOTION

static int16_t resolve_divisor_32(uint32_t D, int16_t *shift) {
  int32_t e, f;
  *shift = get_msb(D);
  // e is obtained from D after resetting the most significant 1 bit.
  e = D - ((uint32_t)1 << *shift);
  // Get the most significant DIV_LUT_BITS (8) bits of e into f
  if (*shift > DIV_LUT_BITS)
    f = ROUND_POWER_OF_TWO(e, *shift - DIV_LUT_BITS);
  else
    f = e << (DIV_LUT_BITS - *shift);
  assert(f <= DIV_LUT_NUM);
  *shift += DIV_LUT_PREC_BITS;
  // Use f as lookup into the precomputed table of multipliers
  return div_lut[f];
}

static int is_affine_valid(const WarpedMotionParams *const wm) {
  const int32_t *mat = wm->wmmat;
  return (mat[2] > 0);
}

static int is_affine_shear_allowed(int16_t alpha, int16_t beta, int16_t gamma,
                                   int16_t delta) {
  if ((4 * abs(alpha) + 7 * abs(beta) >= (1 << WARPEDMODEL_PREC_BITS)) ||
      (4 * abs(gamma) + 4 * abs(delta) >= (1 << WARPEDMODEL_PREC_BITS)))
    return 0;
  else
    return 1;
}

// Returns 1 on success or 0 on an invalid affine set
int get_shear_params(WarpedMotionParams *wm) {
  const int32_t *mat = wm->wmmat;
  if (!is_affine_valid(wm)) return 0;
  wm->alpha =
      clamp(mat[2] - (1 << WARPEDMODEL_PREC_BITS), INT16_MIN, INT16_MAX);
  wm->beta = clamp(mat[3], INT16_MIN, INT16_MAX);
  int16_t shift;
  int16_t y = resolve_divisor_32(abs(mat[2]), &shift) * (mat[2] < 0 ? -1 : 1);
  int64_t v;
  v = ((int64_t)mat[4] * (1 << WARPEDMODEL_PREC_BITS)) * y;
  wm->gamma =
      clamp((int)ROUND_POWER_OF_TWO_SIGNED_64(v, shift), INT16_MIN, INT16_MAX);
  v = ((int64_t)mat[3] * mat[4]) * y;
  wm->delta = clamp(mat[5] - (int)ROUND_POWER_OF_TWO_SIGNED_64(v, shift) -
                        (1 << WARPEDMODEL_PREC_BITS),
                    INT16_MIN, INT16_MAX);
  if (!is_affine_shear_allowed(wm->alpha, wm->beta, wm->gamma, wm->delta))
    return 0;

  wm->alpha = ROUND_POWER_OF_TWO_SIGNED(wm->alpha, WARP_PARAM_REDUCE_BITS) *
              (1 << WARP_PARAM_REDUCE_BITS);
  wm->beta = ROUND_POWER_OF_TWO_SIGNED(wm->beta, WARP_PARAM_REDUCE_BITS) *
             (1 << WARP_PARAM_REDUCE_BITS);
  wm->gamma = ROUND_POWER_OF_TWO_SIGNED(wm->gamma, WARP_PARAM_REDUCE_BITS) *
              (1 << WARP_PARAM_REDUCE_BITS);
  wm->delta = ROUND_POWER_OF_TWO_SIGNED(wm->delta, WARP_PARAM_REDUCE_BITS) *
              (1 << WARP_PARAM_REDUCE_BITS);
  return 1;
}

#if CONFIG_HIGHBITDEPTH
static INLINE void highbd_get_subcolumn(int taps, const uint16_t *const ref,
                                        int32_t *col, int stride, int x,
                                        int y_start) {
  int i;
  for (i = 0; i < taps; ++i) {
    col[i] = ref[(i + y_start) * stride + x];
  }
}

static uint16_t highbd_bi_ntap_filter(const uint16_t *const ref, int x, int y,
                                      int stride, int bd) {
  int32_t val, arr[WARPEDPIXEL_FILTER_TAPS];
  int k;
  const int i = (int)x >> WARPEDPIXEL_PREC_BITS;
  const int j = (int)y >> WARPEDPIXEL_PREC_BITS;
  for (k = 0; k < WARPEDPIXEL_FILTER_TAPS; ++k) {
    int32_t arr_temp[WARPEDPIXEL_FILTER_TAPS];
    highbd_get_subcolumn(WARPEDPIXEL_FILTER_TAPS, ref, arr_temp, stride,
                         i + k + 1 - WARPEDPIXEL_FILTER_TAPS / 2,
                         j + 1 - WARPEDPIXEL_FILTER_TAPS / 2);
    arr[k] = do_ntap_filter(arr_temp + WARPEDPIXEL_FILTER_TAPS / 2 - 1,
                            y - (j * (1 << WARPEDPIXEL_PREC_BITS)));
  }
  val = do_ntap_filter(arr + WARPEDPIXEL_FILTER_TAPS / 2 - 1,
                       x - (i * (1 << WARPEDPIXEL_PREC_BITS)));
  val = ROUND_POWER_OF_TWO_SIGNED(val, WARPEDPIXEL_FILTER_BITS * 2);
  return (uint16_t)clip_pixel_highbd(val, bd);
}

static uint16_t highbd_bi_cubic_filter(const uint16_t *const ref, int x, int y,
                                       int stride, int bd) {
  int32_t val, arr[4];
  int k;
  const int i = (int)x >> WARPEDPIXEL_PREC_BITS;
  const int j = (int)y >> WARPEDPIXEL_PREC_BITS;
  for (k = 0; k < 4; ++k) {
    int32_t arr_temp[4];
    highbd_get_subcolumn(4, ref, arr_temp, stride, i + k - 1, j - 1);
    arr[k] =
        do_cubic_filter(arr_temp + 1, y - (j * (1 << WARPEDPIXEL_PREC_BITS)));
  }
  val = do_cubic_filter(arr + 1, x - (i * (1 << WARPEDPIXEL_PREC_BITS)));
  val = ROUND_POWER_OF_TWO_SIGNED(val, WARPEDPIXEL_FILTER_BITS * 2);
  return (uint16_t)clip_pixel_highbd(val, bd);
}

static uint16_t highbd_bi_linear_filter(const uint16_t *const ref, int x, int y,
                                        int stride, int bd) {
  const int ix = x >> WARPEDPIXEL_PREC_BITS;
  const int iy = y >> WARPEDPIXEL_PREC_BITS;
  const int sx = x - (ix * (1 << WARPEDPIXEL_PREC_BITS));
  const int sy = y - (iy * (1 << WARPEDPIXEL_PREC_BITS));
  int32_t val;
  val = ROUND_POWER_OF_TWO_SIGNED(
      ref[iy * stride + ix] * (WARPEDPIXEL_PREC_SHIFTS - sy) *
              (WARPEDPIXEL_PREC_SHIFTS - sx) +
          ref[iy * stride + ix + 1] * (WARPEDPIXEL_PREC_SHIFTS - sy) * sx +
          ref[(iy + 1) * stride + ix] * sy * (WARPEDPIXEL_PREC_SHIFTS - sx) +
          ref[(iy + 1) * stride + ix + 1] * sy * sx,
      WARPEDPIXEL_PREC_BITS * 2);
  return (uint16_t)clip_pixel_highbd(val, bd);
}

static uint16_t highbd_warp_interpolate(const uint16_t *const ref, int x, int y,
                                        int width, int height, int stride,
                                        int bd) {
  const int ix = x >> WARPEDPIXEL_PREC_BITS;
  const int iy = y >> WARPEDPIXEL_PREC_BITS;
  const int sx = x - (ix * (1 << WARPEDPIXEL_PREC_BITS));
  const int sy = y - (iy * (1 << WARPEDPIXEL_PREC_BITS));
  int32_t v;

  if (ix < 0 && iy < 0)
    return ref[0];
  else if (ix < 0 && iy > height - 1)
    return ref[(height - 1) * stride];
  else if (ix > width - 1 && iy < 0)
    return ref[width - 1];
  else if (ix > width - 1 && iy > height - 1)
    return ref[(height - 1) * stride + (width - 1)];
  else if (ix < 0) {
    v = ROUND_POWER_OF_TWO_SIGNED(
        ref[iy * stride] * (WARPEDPIXEL_PREC_SHIFTS - sy) +
            ref[(iy + 1) * stride] * sy,
        WARPEDPIXEL_PREC_BITS);
    return clip_pixel_highbd(v, bd);
  } else if (iy < 0) {
    v = ROUND_POWER_OF_TWO_SIGNED(
        ref[ix] * (WARPEDPIXEL_PREC_SHIFTS - sx) + ref[ix + 1] * sx,
        WARPEDPIXEL_PREC_BITS);
    return clip_pixel_highbd(v, bd);
  } else if (ix > width - 1) {
    v = ROUND_POWER_OF_TWO_SIGNED(
        ref[iy * stride + width - 1] * (WARPEDPIXEL_PREC_SHIFTS - sy) +
            ref[(iy + 1) * stride + width - 1] * sy,
        WARPEDPIXEL_PREC_BITS);
    return clip_pixel_highbd(v, bd);
  } else if (iy > height - 1) {
    v = ROUND_POWER_OF_TWO_SIGNED(
        ref[(height - 1) * stride + ix] * (WARPEDPIXEL_PREC_SHIFTS - sx) +
            ref[(height - 1) * stride + ix + 1] * sx,
        WARPEDPIXEL_PREC_BITS);
    return clip_pixel_highbd(v, bd);
  } else if (ix >= WARPEDPIXEL_FILTER_TAPS / 2 - 1 &&
             iy >= WARPEDPIXEL_FILTER_TAPS / 2 - 1 &&
             ix < width - WARPEDPIXEL_FILTER_TAPS / 2 &&
             iy < height - WARPEDPIXEL_FILTER_TAPS / 2) {
    return highbd_bi_ntap_filter(ref, x, y, stride, bd);
  } else if (ix >= 1 && iy >= 1 && ix < width - 2 && iy < height - 2) {
    return highbd_bi_cubic_filter(ref, x, y, stride, bd);
  } else {
    return highbd_bi_linear_filter(ref, x, y, stride, bd);
  }
}

static INLINE int highbd_error_measure(int err, int bd) {
  const int b = bd - 8;
  const int bmask = (1 << b) - 1;
  const int v = (1 << b);
  int e1, e2;
  err = abs(err);
  e1 = err >> b;
  e2 = err & bmask;
  return error_measure_lut[255 + e1] * (v - e2) +
         error_measure_lut[256 + e1] * e2;
}

static void highbd_warp_plane_old(const WarpedMotionParams *const wm,
                                  const uint8_t *const ref8, int width,
                                  int height, int stride,
                                  const uint8_t *const pred8, int p_col,
                                  int p_row, int p_width, int p_height,
                                  int p_stride, int subsampling_x,
                                  int subsampling_y, int x_scale, int y_scale,
                                  int bd, ConvolveParams *conv_params) {
  int i, j;
  ProjectPointsFunc projectpoints = get_project_points_type(wm->wmtype);
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  const uint16_t *const ref = CONVERT_TO_SHORTPTR(ref8);
  if (projectpoints == NULL) return;
  for (i = p_row; i < p_row + p_height; ++i) {
    for (j = p_col; j < p_col + p_width; ++j) {
      int in[2], out[2];
      in[0] = j;
      in[1] = i;
      projectpoints(wm->wmmat, in, out, 1, 2, 2, subsampling_x, subsampling_y);
      out[0] = ROUND_POWER_OF_TWO_SIGNED(out[0] * x_scale, SCALE_SUBPEL_BITS);
      out[1] = ROUND_POWER_OF_TWO_SIGNED(out[1] * y_scale, SCALE_SUBPEL_BITS);
      if (conv_params->do_average)
        pred[(j - p_col) + (i - p_row) * p_stride] = ROUND_POWER_OF_TWO(
            pred[(j - p_col) + (i - p_row) * p_stride] +
                highbd_warp_interpolate(ref, out[0], out[1], width, height,
                                        stride, bd),
            1);
      else
        pred[(j - p_col) + (i - p_row) * p_stride] = highbd_warp_interpolate(
            ref, out[0], out[1], width, height, stride, bd);
    }
  }
}

/* Note: For an explanation of the warp algorithm, and some notes on bit widths
    for hardware implementations, see the comments above av1_warp_affine_c
*/
void av1_highbd_warp_affine_c(const int32_t *mat, const uint16_t *ref,
                              int width, int height, int stride, uint16_t *pred,
                              int p_col, int p_row, int p_width, int p_height,
                              int p_stride, int subsampling_x,
                              int subsampling_y, int bd,
                              ConvolveParams *conv_params, int16_t alpha,
                              int16_t beta, int16_t gamma, int16_t delta) {
  int32_t tmp[15 * 8];
  int i, j, k, l, m;
#if CONFIG_CONVOLVE_ROUND
  const int use_conv_params = conv_params->round == CONVOLVE_OPT_NO_ROUND;
  const int reduce_bits_horiz =
      use_conv_params ? conv_params->round_0 : HORSHEAR_REDUCE_PREC_BITS;
  const int max_bits_horiz =
      use_conv_params
          ? bd + FILTER_BITS + 1 - conv_params->round_0
          : bd + WARPEDPIXEL_FILTER_BITS + 1 - HORSHEAR_REDUCE_PREC_BITS;
  const int offset_bits_horiz =
      use_conv_params ? bd + FILTER_BITS - 1 : bd + WARPEDPIXEL_FILTER_BITS - 1;
  const int offset_bits_vert =
      use_conv_params
          ? bd + 2 * FILTER_BITS - conv_params->round_0
          : bd + 2 * WARPEDPIXEL_FILTER_BITS - HORSHEAR_REDUCE_PREC_BITS;
  if (use_conv_params) {
    conv_params->do_post_rounding = 1;
  }
  assert(FILTER_BITS == WARPEDPIXEL_FILTER_BITS);
#else
  const int reduce_bits_horiz = HORSHEAR_REDUCE_PREC_BITS;
  const int max_bits_horiz =
      bd + WARPEDPIXEL_FILTER_BITS + 1 - HORSHEAR_REDUCE_PREC_BITS;
  const int offset_bits_horiz = bd + WARPEDPIXEL_FILTER_BITS - 1;
  const int offset_bits_vert =
      bd + 2 * WARPEDPIXEL_FILTER_BITS - HORSHEAR_REDUCE_PREC_BITS;
#endif
  (void)max_bits_horiz;

  for (i = p_row; i < p_row + p_height; i += 8) {
    for (j = p_col; j < p_col + p_width; j += 8) {
      // Calculate the center of this 8x8 block,
      // project to luma coordinates (if in a subsampled chroma plane),
      // apply the affine transformation,
      // then convert back to the original coordinates (if necessary)
      const int32_t src_x = (j + 4) << subsampling_x;
      const int32_t src_y = (i + 4) << subsampling_y;
      const int32_t dst_x = mat[2] * src_x + mat[3] * src_y + mat[0];
      const int32_t dst_y = mat[4] * src_x + mat[5] * src_y + mat[1];
      const int32_t x4 = dst_x >> subsampling_x;
      const int32_t y4 = dst_y >> subsampling_y;

      int32_t ix4 = x4 >> WARPEDMODEL_PREC_BITS;
      int32_t sx4 = x4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);
      int32_t iy4 = y4 >> WARPEDMODEL_PREC_BITS;
      int32_t sy4 = y4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);

      sx4 += alpha * (-4) + beta * (-4);
      sy4 += gamma * (-4) + delta * (-4);

      sx4 &= ~((1 << WARP_PARAM_REDUCE_BITS) - 1);
      sy4 &= ~((1 << WARP_PARAM_REDUCE_BITS) - 1);

      // Horizontal filter
      for (k = -7; k < 8; ++k) {
        int iy = iy4 + k;
        if (iy < 0)
          iy = 0;
        else if (iy > height - 1)
          iy = height - 1;

        int sx = sx4 + beta * (k + 4);
        for (l = -4; l < 4; ++l) {
          int ix = ix4 + l - 3;
          const int offs = ROUND_POWER_OF_TWO(sx, WARPEDDIFF_PREC_BITS) +
                           WARPEDPIXEL_PREC_SHIFTS;
          assert(offs >= 0 && offs <= WARPEDPIXEL_PREC_SHIFTS * 3);
          const int16_t *coeffs = warped_filter[offs];

          int32_t sum = 1 << offset_bits_horiz;
          for (m = 0; m < 8; ++m) {
            int sample_x = ix + m;
            if (sample_x < 0)
              sample_x = 0;
            else if (sample_x > width - 1)
              sample_x = width - 1;
            sum += ref[iy * stride + sample_x] * coeffs[m];
          }
          sum = ROUND_POWER_OF_TWO(sum, reduce_bits_horiz);
          assert(0 <= sum && sum < (1 << max_bits_horiz));
          tmp[(k + 7) * 8 + (l + 4)] = sum;
          sx += alpha;
        }
      }

      // Vertical filter
      for (k = -4; k < AOMMIN(4, p_row + p_height - i - 4); ++k) {
        int sy = sy4 + delta * (k + 4);
        for (l = -4; l < AOMMIN(4, p_col + p_width - j - 4); ++l) {
          const int offs = ROUND_POWER_OF_TWO(sy, WARPEDDIFF_PREC_BITS) +
                           WARPEDPIXEL_PREC_SHIFTS;
          assert(offs >= 0 && offs <= WARPEDPIXEL_PREC_SHIFTS * 3);
          const int16_t *coeffs = warped_filter[offs];

          int32_t sum = 1 << offset_bits_vert;
          for (m = 0; m < 8; ++m) {
            sum += tmp[(k + m + 4) * 8 + (l + 4)] * coeffs[m];
          }
#if CONFIG_CONVOLVE_ROUND
          if (use_conv_params) {
            CONV_BUF_TYPE *p =
                &conv_params
                     ->dst[(i - p_row + k + 4) * conv_params->dst_stride +
                           (j - p_col + l + 4)];
            sum = ROUND_POWER_OF_TWO(sum, conv_params->round_1) -
                  (1 << (offset_bits_horiz + FILTER_BITS -
                         conv_params->round_0 - conv_params->round_1)) -
                  (1 << (offset_bits_vert - conv_params->round_1));
            if (conv_params->do_average)
              *p += sum;
            else
              *p = sum;
          } else {
#else
          {
#endif
            uint16_t *p =
                &pred[(i - p_row + k + 4) * p_stride + (j - p_col + l + 4)];
            sum = ROUND_POWER_OF_TWO(sum, VERSHEAR_REDUCE_PREC_BITS);
            assert(0 <= sum && sum < (1 << (bd + 2)));
            uint16_t px =
                clip_pixel_highbd(sum - (1 << (bd - 1)) - (1 << bd), bd);
            if (conv_params->do_average)
              *p = ROUND_POWER_OF_TWO(*p + px, 1);
            else
              *p = px;
          }
          sy += gamma;
        }
      }
    }
  }
}

static void highbd_warp_plane(WarpedMotionParams *wm, const uint8_t *const ref8,
                              int width, int height, int stride,
                              const uint8_t *const pred8, int p_col, int p_row,
                              int p_width, int p_height, int p_stride,
                              int subsampling_x, int subsampling_y, int x_scale,
                              int y_scale, int bd,
                              ConvolveParams *conv_params) {
  if (wm->wmtype == ROTZOOM) {
    wm->wmmat[5] = wm->wmmat[2];
    wm->wmmat[4] = -wm->wmmat[3];
  }
  if ((wm->wmtype == ROTZOOM || wm->wmtype == AFFINE) &&
      x_scale == SCALE_SUBPEL_SHIFTS && y_scale == SCALE_SUBPEL_SHIFTS) {
    const int32_t *const mat = wm->wmmat;
    const int16_t alpha = wm->alpha;
    const int16_t beta = wm->beta;
    const int16_t gamma = wm->gamma;
    const int16_t delta = wm->delta;

    const uint16_t *const ref = CONVERT_TO_SHORTPTR(ref8);
    uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
    av1_highbd_warp_affine(mat, ref, width, height, stride, pred, p_col, p_row,
                           p_width, p_height, p_stride, subsampling_x,
                           subsampling_y, bd, conv_params, alpha, beta, gamma,
                           delta);
  } else {
    highbd_warp_plane_old(wm, ref8, width, height, stride, pred8, p_col, p_row,
                          p_width, p_height, p_stride, subsampling_x,
                          subsampling_y, x_scale, y_scale, bd, conv_params);
  }
}

static int64_t highbd_frame_error(const uint16_t *const ref, int stride,
                                  const uint16_t *const dst, int p_width,
                                  int p_height, int p_stride, int bd) {
  int64_t sum_error = 0;
  for (int i = 0; i < p_height; ++i) {
    for (int j = 0; j < p_width; ++j) {
      sum_error +=
          highbd_error_measure(dst[j + i * p_stride] - ref[j + i * stride], bd);
    }
  }
  return sum_error;
}

static int64_t highbd_warp_error(
    WarpedMotionParams *wm, const uint8_t *const ref8, int width, int height,
    int stride, const uint8_t *const dst8, int p_col, int p_row, int p_width,
    int p_height, int p_stride, int subsampling_x, int subsampling_y,
    int x_scale, int y_scale, int bd, int64_t best_error) {
  int64_t gm_sumerr = 0;
  int warp_w, warp_h;
  int error_bsize_w = AOMMIN(p_width, WARP_ERROR_BLOCK);
  int error_bsize_h = AOMMIN(p_height, WARP_ERROR_BLOCK);
  uint16_t tmp[WARP_ERROR_BLOCK * WARP_ERROR_BLOCK];

  ConvolveParams conv_params = get_conv_params(0, 0, 0);
  for (int i = p_row; i < p_row + p_height; i += WARP_ERROR_BLOCK) {
    for (int j = p_col; j < p_col + p_width; j += WARP_ERROR_BLOCK) {
      // avoid warping extra 8x8 blocks in the padded region of the frame
      // when p_width and p_height are not multiples of WARP_ERROR_BLOCK
      warp_w = AOMMIN(error_bsize_w, p_col + p_width - j);
      warp_h = AOMMIN(error_bsize_h, p_row + p_height - i);
      highbd_warp_plane(wm, ref8, width, height, stride,
                        CONVERT_TO_BYTEPTR(tmp), j, i, warp_w, warp_h,
                        WARP_ERROR_BLOCK, subsampling_x, subsampling_y, x_scale,
                        y_scale, bd, &conv_params);

      gm_sumerr += highbd_frame_error(
          tmp, WARP_ERROR_BLOCK, CONVERT_TO_SHORTPTR(dst8) + j + i * p_stride,
          warp_w, warp_h, p_stride, bd);
      if (gm_sumerr > best_error) return gm_sumerr;
    }
  }
  return gm_sumerr;
}
#endif  // CONFIG_HIGHBITDEPTH

static INLINE int error_measure(int err) {
  return error_measure_lut[255 + err];
}

static void warp_plane_old(const WarpedMotionParams *const wm,
                           const uint8_t *const ref, int width, int height,
                           int stride, uint8_t *pred, int p_col, int p_row,
                           int p_width, int p_height, int p_stride,
                           int subsampling_x, int subsampling_y, int x_scale,
                           int y_scale, ConvolveParams *conv_params) {
  int i, j;
  ProjectPointsFunc projectpoints = get_project_points_type(wm->wmtype);
  if (projectpoints == NULL) return;
  for (i = p_row; i < p_row + p_height; ++i) {
    for (j = p_col; j < p_col + p_width; ++j) {
      int in[2], out[2];
      in[0] = j;
      in[1] = i;
      projectpoints(wm->wmmat, in, out, 1, 2, 2, subsampling_x, subsampling_y);
      out[0] = ROUND_POWER_OF_TWO_SIGNED(out[0] * x_scale, SCALE_SUBPEL_BITS);
      out[1] = ROUND_POWER_OF_TWO_SIGNED(out[1] * y_scale, SCALE_SUBPEL_BITS);
      if (conv_params->do_average)
        pred[(j - p_col) + (i - p_row) * p_stride] = ROUND_POWER_OF_TWO(
            pred[(j - p_col) + (i - p_row) * p_stride] +
                warp_interpolate(ref, out[0], out[1], width, height, stride),
            1);
      else
        pred[(j - p_col) + (i - p_row) * p_stride] =
            warp_interpolate(ref, out[0], out[1], width, height, stride);
    }
  }
}

/* The warp filter for ROTZOOM and AFFINE models works as follows:
   * Split the input into 8x8 blocks
   * For each block, project the point (4, 4) within the block, to get the
     overall block position. Split into integer and fractional coordinates,
     maintaining full WARPEDMODEL precision
   * Filter horizontally: Generate 15 rows of 8 pixels each. Each pixel gets a
     variable horizontal offset. This means that, while the rows of the
     intermediate buffer align with the rows of the *reference* image, the
     columns align with the columns of the *destination* image.
   * Filter vertically: Generate the output block (up to 8x8 pixels, but if the
     destination is too small we crop the output at this stage). Each pixel has
     a variable vertical offset, so that the resulting rows are aligned with
     the rows of the destination image.

   To accomplish these alignments, we factor the warp matrix as a
   product of two shear / asymmetric zoom matrices:
   / a b \  = /   1       0    \ * / 1+alpha  beta \
   \ c d /    \ gamma  1+delta /   \    0      1   /
   where a, b, c, d are wmmat[2], wmmat[3], wmmat[4], wmmat[5] respectively.
   The horizontal shear (with alpha and beta) is applied first,
   then the vertical shear (with gamma and delta) is applied second.

   The only limitation is that, to fit this in a fixed 8-tap filter size,
   the fractional pixel offsets must be at most +-1. Since the horizontal filter
   generates 15 rows of 8 columns, and the initial point we project is at (4, 4)
   within the block, the parameters must satisfy
   4 * |alpha| + 7 * |beta| <= 1   and   4 * |gamma| + 4 * |delta| <= 1
   for this filter to be applicable.

   Note: This function assumes that the caller has done all of the relevant
   checks, ie. that we have a ROTZOOM or AFFINE model, that wm[4] and wm[5]
   are set appropriately (if using a ROTZOOM model), and that alpha, beta,
   gamma, delta are all in range.

   TODO(david.barker): Maybe support scaled references?
*/
/* A note on hardware implementation:
    The warp filter is intended to be implementable using the same hardware as
    the high-precision convolve filters from the loop-restoration and
    convolve-round experiments.

    For a single filter stage, considering all of the coefficient sets for the
    warp filter and the regular convolution filter, an input in the range
    [0, 2^k - 1] is mapped into the range [-56 * (2^k - 1), 184 * (2^k - 1)]
    before rounding.

    Allowing for some changes to the filter coefficient sets, call the range
    [-64 * 2^k, 192 * 2^k]. Then, if we initialize the accumulator to 64 * 2^k,
    we can replace this by the range [0, 256 * 2^k], which can be stored in an
    unsigned value with 8 + k bits.

    This allows the derivation of the appropriate bit widths and offsets for
    the various intermediate values: If

    F := WARPEDPIXEL_FILTER_BITS = 7 (or else the above ranges need adjusting)
         So a *single* filter stage maps a k-bit input to a (k + F + 1)-bit
         intermediate value.
    H := HORSHEAR_REDUCE_PREC_BITS
    V := VERSHEAR_REDUCE_PREC_BITS
    (and note that we must have H + V = 2*F for the output to have the same
     scale as the input)

    then we end up with the following offsets and ranges:
    Horizontal filter: Apply an offset of 1 << (bd + F - 1), sum fits into a
                       uint{bd + F + 1}
    After rounding: The values stored in 'tmp' fit into a uint{bd + F + 1 - H}.
    Vertical filter: Apply an offset of 1 << (bd + 2*F - H), sum fits into a
                     uint{bd + 2*F + 2 - H}
    After rounding: The final value, before undoing the offset, fits into a
                    uint{bd + 2}.

    Then we need to undo the offsets before clamping to a pixel. Note that,
    if we do this at the end, the amount to subtract is actually independent
    of H and V:

    offset to subtract = (1 << ((bd + F - 1) - H + F - V)) +
                         (1 << ((bd + 2*F - H) - V))
                      == (1 << (bd - 1)) + (1 << bd)

    This allows us to entirely avoid clamping in both the warp filter and
    the convolve-round experiment. As of the time of writing, the Wiener filter
    from loop-restoration can encode a central coefficient up to 216, which
    leads to a maximum value of about 282 * 2^k after applying the offset.
    So in that case we still need to clamp.
*/
void av1_warp_affine_c(const int32_t *mat, const uint8_t *ref, int width,
                       int height, int stride, uint8_t *pred, int p_col,
                       int p_row, int p_width, int p_height, int p_stride,
                       int subsampling_x, int subsampling_y,
                       ConvolveParams *conv_params, int16_t alpha, int16_t beta,
                       int16_t gamma, int16_t delta) {
  int32_t tmp[15 * 8];
  int i, j, k, l, m;
  const int bd = 8;
#if CONFIG_CONVOLVE_ROUND
  const int use_conv_params = conv_params->round == CONVOLVE_OPT_NO_ROUND;
  const int reduce_bits_horiz =
      use_conv_params ? conv_params->round_0 : HORSHEAR_REDUCE_PREC_BITS;
  const int max_bits_horiz =
      use_conv_params
          ? bd + FILTER_BITS + 1 - conv_params->round_0
          : bd + WARPEDPIXEL_FILTER_BITS + 1 - HORSHEAR_REDUCE_PREC_BITS;
  const int offset_bits_horiz =
      use_conv_params ? bd + FILTER_BITS - 1 : bd + WARPEDPIXEL_FILTER_BITS - 1;
  const int offset_bits_vert =
      use_conv_params
          ? bd + 2 * FILTER_BITS - conv_params->round_0
          : bd + 2 * WARPEDPIXEL_FILTER_BITS - HORSHEAR_REDUCE_PREC_BITS;
  if (use_conv_params) {
    conv_params->do_post_rounding = 1;
  }
  assert(FILTER_BITS == WARPEDPIXEL_FILTER_BITS);
#else
  const int reduce_bits_horiz = HORSHEAR_REDUCE_PREC_BITS;
  const int max_bits_horiz =
      bd + WARPEDPIXEL_FILTER_BITS + 1 - HORSHEAR_REDUCE_PREC_BITS;
  const int offset_bits_horiz = bd + WARPEDPIXEL_FILTER_BITS - 1;
  const int offset_bits_vert =
      bd + 2 * WARPEDPIXEL_FILTER_BITS - HORSHEAR_REDUCE_PREC_BITS;
#endif
  (void)max_bits_horiz;

  for (i = p_row; i < p_row + p_height; i += 8) {
    for (j = p_col; j < p_col + p_width; j += 8) {
      // Calculate the center of this 8x8 block,
      // project to luma coordinates (if in a subsampled chroma plane),
      // apply the affine transformation,
      // then convert back to the original coordinates (if necessary)
      const int32_t src_x = (j + 4) << subsampling_x;
      const int32_t src_y = (i + 4) << subsampling_y;
      const int32_t dst_x = mat[2] * src_x + mat[3] * src_y + mat[0];
      const int32_t dst_y = mat[4] * src_x + mat[5] * src_y + mat[1];
      const int32_t x4 = dst_x >> subsampling_x;
      const int32_t y4 = dst_y >> subsampling_y;

      int32_t ix4 = x4 >> WARPEDMODEL_PREC_BITS;
      int32_t sx4 = x4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);
      int32_t iy4 = y4 >> WARPEDMODEL_PREC_BITS;
      int32_t sy4 = y4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);

      sx4 += alpha * (-4) + beta * (-4);
      sy4 += gamma * (-4) + delta * (-4);

      sx4 &= ~((1 << WARP_PARAM_REDUCE_BITS) - 1);
      sy4 &= ~((1 << WARP_PARAM_REDUCE_BITS) - 1);

      // Horizontal filter
      for (k = -7; k < 8; ++k) {
        // Clamp to top/bottom edge of the frame
        int iy = iy4 + k;
        if (iy < 0)
          iy = 0;
        else if (iy > height - 1)
          iy = height - 1;

        int sx = sx4 + beta * (k + 4);

        for (l = -4; l < 4; ++l) {
          int ix = ix4 + l - 3;
          // At this point, sx = sx4 + alpha * l + beta * k
          const int offs = ROUND_POWER_OF_TWO(sx, WARPEDDIFF_PREC_BITS) +
                           WARPEDPIXEL_PREC_SHIFTS;
          assert(offs >= 0 && offs <= WARPEDPIXEL_PREC_SHIFTS * 3);
          const int16_t *coeffs = warped_filter[offs];

          int32_t sum = 1 << offset_bits_horiz;
          for (m = 0; m < 8; ++m) {
            // Clamp to left/right edge of the frame
            int sample_x = ix + m;
            if (sample_x < 0)
              sample_x = 0;
            else if (sample_x > width - 1)
              sample_x = width - 1;

            sum += ref[iy * stride + sample_x] * coeffs[m];
          }
          sum = ROUND_POWER_OF_TWO(sum, reduce_bits_horiz);
          assert(0 <= sum && sum < (1 << max_bits_horiz));
          tmp[(k + 7) * 8 + (l + 4)] = sum;
          sx += alpha;
        }
      }

      // Vertical filter
      for (k = -4; k < AOMMIN(4, p_row + p_height - i - 4); ++k) {
        int sy = sy4 + delta * (k + 4);
        for (l = -4; l < AOMMIN(4, p_col + p_width - j - 4); ++l) {
          // At this point, sy = sy4 + gamma * l + delta * k
          const int offs = ROUND_POWER_OF_TWO(sy, WARPEDDIFF_PREC_BITS) +
                           WARPEDPIXEL_PREC_SHIFTS;
          assert(offs >= 0 && offs <= WARPEDPIXEL_PREC_SHIFTS * 3);
          const int16_t *coeffs = warped_filter[offs];

          int32_t sum = 1 << offset_bits_vert;
          for (m = 0; m < 8; ++m) {
            sum += tmp[(k + m + 4) * 8 + (l + 4)] * coeffs[m];
          }
#if CONFIG_CONVOLVE_ROUND
          if (use_conv_params) {
            CONV_BUF_TYPE *p =
                &conv_params
                     ->dst[(i - p_row + k + 4) * conv_params->dst_stride +
                           (j - p_col + l + 4)];
            sum = ROUND_POWER_OF_TWO(sum, conv_params->round_1) -
                  (1 << (offset_bits_horiz + FILTER_BITS -
                         conv_params->round_0 - conv_params->round_1)) -
                  (1 << (offset_bits_vert - conv_params->round_1));
            if (conv_params->do_average)
              *p += sum;
            else
              *p = sum;
          } else {
#else
          {
#endif
            uint8_t *p =
                &pred[(i - p_row + k + 4) * p_stride + (j - p_col + l + 4)];
            sum = ROUND_POWER_OF_TWO(sum, VERSHEAR_REDUCE_PREC_BITS);
            assert(0 <= sum && sum < (1 << (bd + 2)));
            uint8_t px = clip_pixel(sum - (1 << (bd - 1)) - (1 << bd));
            if (conv_params->do_average)
              *p = ROUND_POWER_OF_TWO(*p + px, 1);
            else
              *p = px;
          }
          sy += gamma;
        }
      }
    }
  }
}

static void warp_plane(WarpedMotionParams *wm, const uint8_t *const ref,
                       int width, int height, int stride, uint8_t *pred,
                       int p_col, int p_row, int p_width, int p_height,
                       int p_stride, int subsampling_x, int subsampling_y,
                       int x_scale, int y_scale, ConvolveParams *conv_params) {
  if (wm->wmtype == ROTZOOM) {
    wm->wmmat[5] = wm->wmmat[2];
    wm->wmmat[4] = -wm->wmmat[3];
  }
  if ((wm->wmtype == ROTZOOM || wm->wmtype == AFFINE) &&
      x_scale == SCALE_SUBPEL_SHIFTS && y_scale == SCALE_SUBPEL_SHIFTS) {
    const int32_t *const mat = wm->wmmat;
    const int16_t alpha = wm->alpha;
    const int16_t beta = wm->beta;
    const int16_t gamma = wm->gamma;
    const int16_t delta = wm->delta;

    av1_warp_affine(mat, ref, width, height, stride, pred, p_col, p_row,
                    p_width, p_height, p_stride, subsampling_x, subsampling_y,
                    conv_params, alpha, beta, gamma, delta);
  } else {
    warp_plane_old(wm, ref, width, height, stride, pred, p_col, p_row, p_width,
                   p_height, p_stride, subsampling_x, subsampling_y, x_scale,
                   y_scale, conv_params);
  }
}

static int64_t frame_error(const uint8_t *const ref, int stride,
                           const uint8_t *const dst, int p_width, int p_height,
                           int p_stride) {
  int64_t sum_error = 0;
  for (int i = 0; i < p_height; ++i) {
    for (int j = 0; j < p_width; ++j) {
      sum_error +=
          (int64_t)error_measure(dst[j + i * p_stride] - ref[j + i * stride]);
    }
  }
  return sum_error;
}

static int64_t warp_error(WarpedMotionParams *wm, const uint8_t *const ref,
                          int width, int height, int stride,
                          const uint8_t *const dst, int p_col, int p_row,
                          int p_width, int p_height, int p_stride,
                          int subsampling_x, int subsampling_y, int x_scale,
                          int y_scale, int64_t best_error) {
  int64_t gm_sumerr = 0;
  int warp_w, warp_h;
  int error_bsize_w = AOMMIN(p_width, WARP_ERROR_BLOCK);
  int error_bsize_h = AOMMIN(p_height, WARP_ERROR_BLOCK);
  uint8_t tmp[WARP_ERROR_BLOCK * WARP_ERROR_BLOCK];
  ConvolveParams conv_params = get_conv_params(0, 0, 0);

  for (int i = p_row; i < p_row + p_height; i += WARP_ERROR_BLOCK) {
    for (int j = p_col; j < p_col + p_width; j += WARP_ERROR_BLOCK) {
      // avoid warping extra 8x8 blocks in the padded region of the frame
      // when p_width and p_height are not multiples of WARP_ERROR_BLOCK
      warp_w = AOMMIN(error_bsize_w, p_col + p_width - j);
      warp_h = AOMMIN(error_bsize_h, p_row + p_height - i);
      warp_plane(wm, ref, width, height, stride, tmp, j, i, warp_w, warp_h,
                 WARP_ERROR_BLOCK, subsampling_x, subsampling_y, x_scale,
                 y_scale, &conv_params);

      gm_sumerr += frame_error(tmp, WARP_ERROR_BLOCK, dst + j + i * p_stride,
                               warp_w, warp_h, p_stride);
      if (gm_sumerr > best_error) return gm_sumerr;
    }
  }
  return gm_sumerr;
}

int64_t av1_frame_error(
#if CONFIG_HIGHBITDEPTH
    int use_hbd, int bd,
#endif  // CONFIG_HIGHBITDEPTH
    const uint8_t *ref, int stride, uint8_t *dst, int p_width, int p_height,
    int p_stride) {
#if CONFIG_HIGHBITDEPTH
  if (use_hbd) {
    return highbd_frame_error(CONVERT_TO_SHORTPTR(ref), stride,
                              CONVERT_TO_SHORTPTR(dst), p_width, p_height,
                              p_stride, bd);
  }
#endif  // CONFIG_HIGHBITDEPTH
  return frame_error(ref, stride, dst, p_width, p_height, p_stride);
}

int64_t av1_warp_error(WarpedMotionParams *wm,
#if CONFIG_HIGHBITDEPTH
                       int use_hbd, int bd,
#endif  // CONFIG_HIGHBITDEPTH
                       const uint8_t *ref, int width, int height, int stride,
                       uint8_t *dst, int p_col, int p_row, int p_width,
                       int p_height, int p_stride, int subsampling_x,
                       int subsampling_y, int x_scale, int y_scale,
                       int64_t best_error) {
  if (wm->wmtype <= AFFINE)
    if (!get_shear_params(wm)) return 1;
#if CONFIG_HIGHBITDEPTH
  if (use_hbd)
    return highbd_warp_error(wm, ref, width, height, stride, dst, p_col, p_row,
                             p_width, p_height, p_stride, subsampling_x,
                             subsampling_y, x_scale, y_scale, bd, best_error);
#endif  // CONFIG_HIGHBITDEPTH
  return warp_error(wm, ref, width, height, stride, dst, p_col, p_row, p_width,
                    p_height, p_stride, subsampling_x, subsampling_y, x_scale,
                    y_scale, best_error);
}

void av1_warp_plane(WarpedMotionParams *wm,
#if CONFIG_HIGHBITDEPTH
                    int use_hbd, int bd,
#endif  // CONFIG_HIGHBITDEPTH
                    const uint8_t *ref, int width, int height, int stride,
                    uint8_t *pred, int p_col, int p_row, int p_width,
                    int p_height, int p_stride, int subsampling_x,
                    int subsampling_y, int x_scale, int y_scale,
                    ConvolveParams *conv_params) {
#if CONFIG_HIGHBITDEPTH
  if (use_hbd)
    highbd_warp_plane(wm, ref, width, height, stride, pred, p_col, p_row,
                      p_width, p_height, p_stride, subsampling_x, subsampling_y,
                      x_scale, y_scale, bd, conv_params);
  else
#endif  // CONFIG_HIGHBITDEPTH
    warp_plane(wm, ref, width, height, stride, pred, p_col, p_row, p_width,
               p_height, p_stride, subsampling_x, subsampling_y, x_scale,
               y_scale, conv_params);
}

#if CONFIG_WARPED_MOTION
#define LEAST_SQUARES_ORDER 2

#define LS_MV_MAX 256  // max mv in 1/8-pel
#define LS_STEP 2

// Assuming LS_MV_MAX is < MAX_SB_SIZE * 8,
// the precision needed is:
//   (MAX_SB_SIZE_LOG2 + 3) [for sx * sx magnitude] +
//   (MAX_SB_SIZE_LOG2 + 4) [for sx * dx magnitude] +
//   1 [for sign] +
//   LEAST_SQUARES_SAMPLES_MAX_BITS
//        [for adding up to LEAST_SQUARES_SAMPLES_MAX samples]
// The value is 23
#define LS_MAT_RANGE_BITS \
  ((MAX_SB_SIZE_LOG2 + 4) * 2 + LEAST_SQUARES_SAMPLES_MAX_BITS)

// Bit-depth reduction from the full-range
#define LS_MAT_DOWN_BITS 2

// bits range of A, Bx and By after downshifting
#define LS_MAT_BITS (LS_MAT_RANGE_BITS - LS_MAT_DOWN_BITS)
#define LS_MAT_MIN (-(1 << (LS_MAT_BITS - 1)))
#define LS_MAT_MAX ((1 << (LS_MAT_BITS - 1)) - 1)

#define LS_SUM(a) ((a)*4 + LS_STEP * 2)
#define LS_SQUARE(a) \
  (((a) * (a)*4 + (a)*4 * LS_STEP + LS_STEP * LS_STEP * 2) >> 2)
#define LS_PRODUCT1(a, b) \
  (((a) * (b)*4 + ((a) + (b)) * 2 * LS_STEP + LS_STEP * LS_STEP) >> 2)
#define LS_PRODUCT2(a, b) \
  (((a) * (b)*4 + ((a) + (b)) * 2 * LS_STEP + LS_STEP * LS_STEP * 2) >> 2)

#define USE_LIMITED_PREC_MULT 0

#if USE_LIMITED_PREC_MULT

#define MUL_PREC_BITS 16
static uint16_t resolve_multiplier_64(uint64_t D, int16_t *shift) {
  int msb = 0;
  uint16_t mult = 0;
  *shift = 0;
  if (D != 0) {
    msb = (int16_t)((D >> 32) ? get_msb((unsigned int)(D >> 32)) + 32
                              : get_msb((unsigned int)D));
    if (msb >= MUL_PREC_BITS) {
      mult = (uint16_t)ROUND_POWER_OF_TWO_64(D, msb + 1 - MUL_PREC_BITS);
      *shift = msb + 1 - MUL_PREC_BITS;
    } else {
      mult = (uint16_t)D;
      *shift = 0;
    }
  }
  return mult;
}

static int32_t get_mult_shift_ndiag(int64_t Px, int16_t iDet, int shift) {
  int32_t ret;
  int16_t mshift;
  uint16_t Mul = resolve_multiplier_64(llabs(Px), &mshift);
  int32_t v = (int32_t)Mul * (int32_t)iDet * (Px < 0 ? -1 : 1);
  shift -= mshift;
  if (shift > 0) {
    return (int32_t)clamp(ROUND_POWER_OF_TWO_SIGNED(v, shift),
                          -WARPEDMODEL_NONDIAGAFFINE_CLAMP + 1,
                          WARPEDMODEL_NONDIAGAFFINE_CLAMP - 1);
  } else {
    return (int32_t)clamp(v * (1 << (-shift)),
                          -WARPEDMODEL_NONDIAGAFFINE_CLAMP + 1,
                          WARPEDMODEL_NONDIAGAFFINE_CLAMP - 1);
  }
  return ret;
}

static int32_t get_mult_shift_diag(int64_t Px, int16_t iDet, int shift) {
  int16_t mshift;
  uint16_t Mul = resolve_multiplier_64(llabs(Px), &mshift);
  int32_t v = (int32_t)Mul * (int32_t)iDet * (Px < 0 ? -1 : 1);
  shift -= mshift;
  if (shift > 0) {
    return (int32_t)clamp(
        ROUND_POWER_OF_TWO_SIGNED(v, shift),
        (1 << WARPEDMODEL_PREC_BITS) - WARPEDMODEL_NONDIAGAFFINE_CLAMP + 1,
        (1 << WARPEDMODEL_PREC_BITS) + WARPEDMODEL_NONDIAGAFFINE_CLAMP - 1);
  } else {
    return (int32_t)clamp(
        v * (1 << (-shift)),
        (1 << WARPEDMODEL_PREC_BITS) - WARPEDMODEL_NONDIAGAFFINE_CLAMP + 1,
        (1 << WARPEDMODEL_PREC_BITS) + WARPEDMODEL_NONDIAGAFFINE_CLAMP - 1);
  }
}

#else

static int32_t get_mult_shift_ndiag(int64_t Px, int16_t iDet, int shift) {
  int64_t v = Px * (int64_t)iDet;
  return (int32_t)clamp64(ROUND_POWER_OF_TWO_SIGNED_64(v, shift),
                          -WARPEDMODEL_NONDIAGAFFINE_CLAMP + 1,
                          WARPEDMODEL_NONDIAGAFFINE_CLAMP - 1);
}

static int32_t get_mult_shift_diag(int64_t Px, int16_t iDet, int shift) {
  int64_t v = Px * (int64_t)iDet;
  return (int32_t)clamp64(
      ROUND_POWER_OF_TWO_SIGNED_64(v, shift),
      (1 << WARPEDMODEL_PREC_BITS) - WARPEDMODEL_NONDIAGAFFINE_CLAMP + 1,
      (1 << WARPEDMODEL_PREC_BITS) + WARPEDMODEL_NONDIAGAFFINE_CLAMP - 1);
}
#endif  // USE_LIMITED_PREC_MULT

static int find_affine_int(int np, int *pts1, int *pts2, BLOCK_SIZE bsize,
                           int mvy, int mvx, WarpedMotionParams *wm, int mi_row,
                           int mi_col) {
  int32_t A[2][2] = { { 0, 0 }, { 0, 0 } };
  int32_t Bx[2] = { 0, 0 };
  int32_t By[2] = { 0, 0 };
  int i, n = 0;

  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const int isuy = (mi_row * MI_SIZE + AOMMAX(bh, MI_SIZE) / 2 - 1);
  const int isux = (mi_col * MI_SIZE + AOMMAX(bw, MI_SIZE) / 2 - 1);
  const int suy = isuy * 8;
  const int sux = isux * 8;
  const int duy = suy + mvy;
  const int dux = sux + mvx;

  // Assume the center pixel of the block has exactly the same motion vector
  // as transmitted for the block. First shift the origin of the source
  // points to the block center, and the origin of the destination points to
  // the block center added to the motion vector transmitted.
  // Let (xi, yi) denote the source points and (xi', yi') denote destination
  // points after origin shfifting, for i = 0, 1, 2, .... n-1.
  // Then if  P = [x0, y0,
  //               x1, y1
  //               x2, y1,
  //                ....
  //              ]
  //          q = [x0', x1', x2', ... ]'
  //          r = [y0', y1', y2', ... ]'
  // the least squares problems that need to be solved are:
  //          [h1, h2]' = inv(P'P)P'q and
  //          [h3, h4]' = inv(P'P)P'r
  // where the affine transformation is given by:
  //          x' = h1.x + h2.y
  //          y' = h3.x + h4.y
  //
  // The loop below computes: A = P'P, Bx = P'q, By = P'r
  // We need to just compute inv(A).Bx and inv(A).By for the solutions.
  int sx, sy, dx, dy;
  // Contribution from neighbor block
  for (i = 0; i < np && n < LEAST_SQUARES_SAMPLES_MAX; i++) {
    dx = pts2[i * 2] - dux;
    dy = pts2[i * 2 + 1] - duy;
    sx = pts1[i * 2] - sux;
    sy = pts1[i * 2 + 1] - suy;
    if (abs(sx - dx) < LS_MV_MAX && abs(sy - dy) < LS_MV_MAX) {
      A[0][0] += LS_SQUARE(sx);
      A[0][1] += LS_PRODUCT1(sx, sy);
      A[1][1] += LS_SQUARE(sy);
      Bx[0] += LS_PRODUCT2(sx, dx);
      Bx[1] += LS_PRODUCT1(sy, dx);
      By[0] += LS_PRODUCT1(sx, dy);
      By[1] += LS_PRODUCT2(sy, dy);
      n++;
    }
  }
  int downshift;
  if (n >= 4)
    downshift = LS_MAT_DOWN_BITS;
  else if (n >= 2)
    downshift = LS_MAT_DOWN_BITS - 1;
  else
    downshift = LS_MAT_DOWN_BITS - 2;

  // Reduce precision by downshift bits
  A[0][0] = clamp(ROUND_POWER_OF_TWO_SIGNED(A[0][0], downshift), LS_MAT_MIN,
                  LS_MAT_MAX);
  A[0][1] = clamp(ROUND_POWER_OF_TWO_SIGNED(A[0][1], downshift), LS_MAT_MIN,
                  LS_MAT_MAX);
  A[1][1] = clamp(ROUND_POWER_OF_TWO_SIGNED(A[1][1], downshift), LS_MAT_MIN,
                  LS_MAT_MAX);
  Bx[0] = clamp(ROUND_POWER_OF_TWO_SIGNED(Bx[0], downshift), LS_MAT_MIN,
                LS_MAT_MAX);
  Bx[1] = clamp(ROUND_POWER_OF_TWO_SIGNED(Bx[1], downshift), LS_MAT_MIN,
                LS_MAT_MAX);
  By[0] = clamp(ROUND_POWER_OF_TWO_SIGNED(By[0], downshift), LS_MAT_MIN,
                LS_MAT_MAX);
  By[1] = clamp(ROUND_POWER_OF_TWO_SIGNED(By[1], downshift), LS_MAT_MIN,
                LS_MAT_MAX);

  int64_t Px[2], Py[2], Det;
  int16_t iDet, shift;

  // These divided by the Det, are the least squares solutions
  Px[0] = (int64_t)A[1][1] * Bx[0] - (int64_t)A[0][1] * Bx[1];
  Px[1] = -(int64_t)A[0][1] * Bx[0] + (int64_t)A[0][0] * Bx[1];
  Py[0] = (int64_t)A[1][1] * By[0] - (int64_t)A[0][1] * By[1];
  Py[1] = -(int64_t)A[0][1] * By[0] + (int64_t)A[0][0] * By[1];

  // Compute Determinant of A
  Det = (int64_t)A[0][0] * A[1][1] - (int64_t)A[0][1] * A[0][1];
  if (Det == 0) return 1;
  iDet = resolve_divisor_64(llabs(Det), &shift) * (Det < 0 ? -1 : 1);
  shift -= WARPEDMODEL_PREC_BITS;
  if (shift < 0) {
    iDet <<= (-shift);
    shift = 0;
  }

  wm->wmmat[2] = get_mult_shift_diag(Px[0], iDet, shift);
  wm->wmmat[3] = get_mult_shift_ndiag(Px[1], iDet, shift);
  wm->wmmat[4] = get_mult_shift_ndiag(Py[0], iDet, shift);
  wm->wmmat[5] = get_mult_shift_diag(Py[1], iDet, shift);

  // Note: In the vx, vy expressions below, the max value of each of the
  // 2nd and 3rd terms are (2^16 - 1) * (2^13 - 1). That leaves enough room
  // for the first term so that the overall sum in the worst case fits
  // within 32 bits overall.
  int32_t vx = mvx * (1 << (WARPEDMODEL_PREC_BITS - 3)) -
               (isux * (wm->wmmat[2] - (1 << WARPEDMODEL_PREC_BITS)) +
                isuy * wm->wmmat[3]);
  int32_t vy = mvy * (1 << (WARPEDMODEL_PREC_BITS - 3)) -
               (isux * wm->wmmat[4] +
                isuy * (wm->wmmat[5] - (1 << WARPEDMODEL_PREC_BITS)));
  wm->wmmat[0] =
      clamp(vx, -WARPEDMODEL_TRANS_CLAMP, WARPEDMODEL_TRANS_CLAMP - 1);
  wm->wmmat[1] =
      clamp(vy, -WARPEDMODEL_TRANS_CLAMP, WARPEDMODEL_TRANS_CLAMP - 1);

  wm->wmmat[6] = wm->wmmat[7] = 0;
  return 0;
}

int find_projection(int np, int *pts1, int *pts2, BLOCK_SIZE bsize, int mvy,
                    int mvx, WarpedMotionParams *wm_params, int mi_row,
                    int mi_col) {
  assert(wm_params->wmtype == AFFINE);
  const int result = find_affine_int(np, pts1, pts2, bsize, mvy, mvx, wm_params,
                                     mi_row, mi_col);
  if (result == 0) {
    // check compatibility with the fast warp filter
    if (!get_shear_params(wm_params)) return 1;
  }

  return result;
}
#endif  // CONFIG_WARPED_MOTION
