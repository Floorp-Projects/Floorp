/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#define _POSIX_C_SOURCE 200112L  // rand_r()
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "av1/encoder/bgsprite.h"

#include "aom_mem/aom_mem.h"
#include "./aom_scale_rtcd.h"
#include "av1/common/mv.h"
#include "av1/common/warped_motion.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/global_motion.h"
#include "av1/encoder/mathutils.h"
#include "av1/encoder/temporal_filter.h"

/* Blending Modes:
 * 0 = Median
 * 1 = Mean
 */
#define BGSPRITE_BLENDING_MODE 1

// Enable removal of outliers from mean blending mode.
#if BGSPRITE_BLENDING_MODE == 1
#define BGSPRITE_MEAN_REMOVE_OUTLIERS 0
#endif  // BGSPRITE_BLENDING_MODE == 1

/* Interpolation for panorama alignment sampling:
 * 0 = Nearest neighbor
 * 1 = Bilinear
 */
#define BGSPRITE_INTERPOLATION 0

// Enable turning off bgsprite from firstpass metrics in define_gf_group.
#define BGSPRITE_ENABLE_METRICS 1

// Enable foreground/backgrond segmentation and combine with temporal filter.
#define BGSPRITE_ENABLE_SEGMENTATION 1

// Enable alignment using global motion.
#define BGSPRITE_ENABLE_GME 0

// Block size for foreground mask.
#define BGSPRITE_MASK_BLOCK_SIZE 4

typedef struct {
#if CONFIG_HIGHBITDEPTH
  uint16_t y;
  uint16_t u;
  uint16_t v;
#else
  uint8_t y;
  uint8_t u;
  uint8_t v;
#endif  // CONFIG_HIGHBITDEPTH
  uint8_t exists;
} YuvPixel;

typedef struct {
  int curr_model;
  double mean[2];
  double var[2];
  int age[2];
  double u_mean[2];
  double v_mean[2];

#if CONFIG_HIGHBITDEPTH
  uint16_t y;
  uint16_t u;
  uint16_t v;
#else
  uint8_t y;
  uint8_t u;
  uint8_t v;
#endif  // CONFIG_HIGHBITDEPTH
  double final_var;
} YuvPixelGaussian;

// Maps to convert from matrix form to param vector form.
static const int params_to_matrix_map[] = { 2, 3, 0, 4, 5, 1, 6, 7 };
static const int matrix_to_params_map[] = { 2, 5, 0, 1, 3, 4, 6, 7 };

// Convert the parameter array to a 3x3 matrix form.
static void params_to_matrix(const double *const params, double *target) {
  for (int i = 0; i < MAX_PARAMDIM - 1; i++) {
    assert(params_to_matrix_map[i] < MAX_PARAMDIM - 1);
    target[i] = params[params_to_matrix_map[i]];
  }
  target[8] = 1;
}

// Convert a 3x3 matrix to a parameter array form.
static void matrix_to_params(const double *const matrix, double *target) {
  for (int i = 0; i < MAX_PARAMDIM - 1; i++) {
    assert(matrix_to_params_map[i] < MAX_PARAMDIM - 1);
    target[i] = matrix[matrix_to_params_map[i]];
  }
}

#define TRANSFORM_MAT_DIM 3

// Do matrix multiplication on params.
static void multiply_params(double *const m1, double *const m2,
                            double *target) {
  double m1_matrix[MAX_PARAMDIM];
  double m2_matrix[MAX_PARAMDIM];
  double result[MAX_PARAMDIM];

  params_to_matrix(m1, m1_matrix);
  params_to_matrix(m2, m2_matrix);
  multiply_mat(m2_matrix, m1_matrix, result, TRANSFORM_MAT_DIM,
               TRANSFORM_MAT_DIM, TRANSFORM_MAT_DIM);
  matrix_to_params(result, target);
}

// Finds x and y limits of a single transformed image.
// Width and height are the size of the input video.
static void find_frame_limit(int width, int height,
                             const double *const transform, int *x_min,
                             int *x_max, int *y_min, int *y_max) {
  double transform_matrix[MAX_PARAMDIM];
  double xy_matrix[3] = { 0, 0, 1 };
  double uv_matrix[3] = { 0 };
// Macro used to update frame limits based on transformed coordinates.
#define UPDATELIMITS(u, v, x_min, x_max, y_min, y_max) \
  {                                                    \
    if ((int)ceil(u) > *x_max) {                       \
      *x_max = (int)ceil(u);                           \
    }                                                  \
    if ((int)floor(u) < *x_min) {                      \
      *x_min = (int)floor(u);                          \
    }                                                  \
    if ((int)ceil(v) > *y_max) {                       \
      *y_max = (int)ceil(v);                           \
    }                                                  \
    if ((int)floor(v) < *y_min) {                      \
      *y_min = (int)floor(v);                          \
    }                                                  \
  }

  params_to_matrix(transform, transform_matrix);
  xy_matrix[0] = 0;
  xy_matrix[1] = 0;
  multiply_mat(transform_matrix, xy_matrix, uv_matrix, TRANSFORM_MAT_DIM,
               TRANSFORM_MAT_DIM, 1);
  *x_max = (int)ceil(uv_matrix[0]);
  *x_min = (int)floor(uv_matrix[0]);
  *y_max = (int)ceil(uv_matrix[1]);
  *y_min = (int)floor(uv_matrix[1]);

  xy_matrix[0] = width - 1;
  xy_matrix[1] = 0;
  multiply_mat(transform_matrix, xy_matrix, uv_matrix, TRANSFORM_MAT_DIM,
               TRANSFORM_MAT_DIM, 1);
  UPDATELIMITS(uv_matrix[0], uv_matrix[1], x_min, x_max, y_min, y_max);

  xy_matrix[0] = width - 1;
  xy_matrix[1] = height - 1;
  multiply_mat(transform_matrix, xy_matrix, uv_matrix, TRANSFORM_MAT_DIM,
               TRANSFORM_MAT_DIM, 1);
  UPDATELIMITS(uv_matrix[0], uv_matrix[1], x_min, x_max, y_min, y_max);

  xy_matrix[0] = 0;
  xy_matrix[1] = height - 1;
  multiply_mat(transform_matrix, xy_matrix, uv_matrix, TRANSFORM_MAT_DIM,
               TRANSFORM_MAT_DIM, 1);
  UPDATELIMITS(uv_matrix[0], uv_matrix[1], x_min, x_max, y_min, y_max);

#undef UPDATELIMITS
}

// Finds x and y limits for arrays. Also finds the overall max and minimums
static void find_limits(int width, int height, const double **const params,
                        int num_frames, int *x_min, int *x_max, int *y_min,
                        int *y_max, int *pano_x_min, int *pano_x_max,
                        int *pano_y_min, int *pano_y_max) {
  *pano_x_max = INT_MIN;
  *pano_x_min = INT_MAX;
  *pano_y_max = INT_MIN;
  *pano_y_min = INT_MAX;
  for (int i = 0; i < num_frames; ++i) {
    find_frame_limit(width, height, (const double *const)params[i], &x_min[i],
                     &x_max[i], &y_min[i], &y_max[i]);
    if (x_max[i] > *pano_x_max) {
      *pano_x_max = x_max[i];
    }
    if (x_min[i] < *pano_x_min) {
      *pano_x_min = x_min[i];
    }
    if (y_max[i] > *pano_y_max) {
      *pano_y_max = y_max[i];
    }
    if (y_min[i] < *pano_y_min) {
      *pano_y_min = y_min[i];
    }
  }
}

// Inverts a 3x3 matrix that is in the parameter form.
static void invert_params(const double *const params, double *target) {
  double temp[MAX_PARAMDIM] = { 0 };
  params_to_matrix(params, temp);

  // Find determinant of matrix (expansion by minors).
  const double det = temp[0] * ((temp[4] * temp[8]) - (temp[5] * temp[7])) -
                     temp[1] * ((temp[3] * temp[8]) - (temp[5] * temp[6])) +
                     temp[2] * ((temp[3] * temp[7]) - (temp[4] * temp[6]));
  assert(det != 0);

  // inverse is transpose of cofactor * 1/det.
  double inverse[MAX_PARAMDIM] = { 0 };
  inverse[0] = (temp[4] * temp[8] - temp[7] * temp[5]) / det;
  inverse[1] = (temp[2] * temp[7] - temp[1] * temp[8]) / det;
  inverse[2] = (temp[1] * temp[5] - temp[2] * temp[4]) / det;
  inverse[3] = (temp[5] * temp[6] - temp[3] * temp[8]) / det;
  inverse[4] = (temp[0] * temp[8] - temp[2] * temp[6]) / det;
  inverse[5] = (temp[3] * temp[2] - temp[0] * temp[5]) / det;
  inverse[6] = (temp[3] * temp[7] - temp[6] * temp[4]) / det;
  inverse[7] = (temp[6] * temp[1] - temp[0] * temp[7]) / det;
  inverse[8] = (temp[0] * temp[4] - temp[3] * temp[1]) / det;

  matrix_to_params(inverse, target);
}

static void build_image_stack(YV12_BUFFER_CONFIG **const frames,
                              const int num_frames, const double **const params,
                              const int *const x_min, const int *const x_max,
                              const int *const y_min, const int *const y_max,
                              int pano_x_min, int pano_y_min,
                              YuvPixel ***img_stack) {
  // Re-sample images onto panorama (pre-filtering).
  const int x_offset = -pano_x_min;
  const int y_offset = -pano_y_min;
  const int frame_width = frames[0]->y_width;
  const int frame_height = frames[0]->y_height;
  for (int i = 0; i < num_frames; ++i) {
    // Find transforms from panorama coordinate system back to single image
    // coordinate system for sampling.
    int transformed_width = x_max[i] - x_min[i] + 1;
    int transformed_height = y_max[i] - y_min[i] + 1;

    double transform_matrix[MAX_PARAMDIM];
    double transform_params[MAX_PARAMDIM - 1];
    invert_params(params[i], transform_params);
    params_to_matrix(transform_params, transform_matrix);

#if CONFIG_HIGHBITDEPTH
    const uint16_t *y_buffer16 = CONVERT_TO_SHORTPTR(frames[i]->y_buffer);
    const uint16_t *u_buffer16 = CONVERT_TO_SHORTPTR(frames[i]->u_buffer);
    const uint16_t *v_buffer16 = CONVERT_TO_SHORTPTR(frames[i]->v_buffer);
#endif  // CONFIG_HIGHBITDEPTH

    for (int y = 0; y < transformed_height; ++y) {
      for (int x = 0; x < transformed_width; ++x) {
        // Do transform.
        double xy_matrix[3] = { x + x_min[i], y + y_min[i], 1 };
        double uv_matrix[3] = { 0 };
        multiply_mat(transform_matrix, xy_matrix, uv_matrix, TRANSFORM_MAT_DIM,
                     TRANSFORM_MAT_DIM, 1);

        // Coordinates used for nearest neighbor interpolation.
        int image_x = (int)round(uv_matrix[0]);
        int image_y = (int)round(uv_matrix[1]);

        // Temporary values for bilinear interpolation
        double interpolated_yvalue = 0.0;
        double interpolated_uvalue = 0.0;
        double interpolated_vvalue = 0.0;
        double interpolated_fraction = 0.0;
        int interpolation_count = 0;

#if BGSPRITE_INTERPOLATION == 1
        // Coordintes used for bilinear interpolation.
        double x_base;
        double y_base;
        double x_decimal = modf(uv_matrix[0], &x_base);
        double y_decimal = modf(uv_matrix[1], &y_base);

        if ((x_decimal > 0.2 && x_decimal < 0.8) ||
            (y_decimal > 0.2 && y_decimal < 0.8)) {
          for (int u = 0; u < 2; ++u) {
            for (int v = 0; v < 2; ++v) {
              int interp_x = (int)x_base + u;
              int interp_y = (int)y_base + v;
              if (interp_x >= 0 && interp_x < frame_width && interp_y >= 0 &&
                  interp_y < frame_height) {
                interpolation_count++;

                interpolated_fraction +=
                    fabs(u - x_decimal) * fabs(v - y_decimal);
                int ychannel_idx = interp_y * frames[i]->y_stride + interp_x;
                int uvchannel_idx = (interp_y >> frames[i]->subsampling_y) *
                                        frames[i]->uv_stride +
                                    (interp_x >> frames[i]->subsampling_x);
#if CONFIG_HIGHBITDEPTH
                if (frames[i]->flags & YV12_FLAG_HIGHBITDEPTH) {
                  interpolated_yvalue += (1 - fabs(u - x_decimal)) *
                                         (1 - fabs(v - y_decimal)) *
                                         y_buffer16[ychannel_idx];
                  interpolated_uvalue += (1 - fabs(u - x_decimal)) *
                                         (1 - fabs(v - y_decimal)) *
                                         u_buffer16[uvchannel_idx];
                  interpolated_vvalue += (1 - fabs(u - x_decimal)) *
                                         (1 - fabs(v - y_decimal)) *
                                         v_buffer16[uvchannel_idx];
                } else {
#endif  // CONFIG_HIGHBITDEPTH
                  interpolated_yvalue += (1 - fabs(u - x_decimal)) *
                                         (1 - fabs(v - y_decimal)) *
                                         frames[i]->y_buffer[ychannel_idx];
                  interpolated_uvalue += (1 - fabs(u - x_decimal)) *
                                         (1 - fabs(v - y_decimal)) *
                                         frames[i]->u_buffer[uvchannel_idx];
                  interpolated_vvalue += (1 - fabs(u - x_decimal)) *
                                         (1 - fabs(v - y_decimal)) *
                                         frames[i]->v_buffer[uvchannel_idx];
#if CONFIG_HIGHBITDEPTH
                }
#endif  // CONFIG_HIGHBITDEPTH
              }
            }
          }
        }
#endif  // BGSPRITE_INTERPOLATION == 1

        if (BGSPRITE_INTERPOLATION && interpolation_count > 2) {
          if (interpolation_count != 4) {
            interpolated_yvalue /= interpolated_fraction;
            interpolated_uvalue /= interpolated_fraction;
            interpolated_vvalue /= interpolated_fraction;
          }
          int pano_x = x + x_min[i] + x_offset;
          int pano_y = y + y_min[i] + y_offset;

#if CONFIG_HIGHBITDEPTH
          if (frames[i]->flags & YV12_FLAG_HIGHBITDEPTH) {
            img_stack[pano_y][pano_x][i].y = (uint16_t)interpolated_yvalue;
            img_stack[pano_y][pano_x][i].u = (uint16_t)interpolated_uvalue;
            img_stack[pano_y][pano_x][i].v = (uint16_t)interpolated_vvalue;
            img_stack[pano_y][pano_x][i].exists = 1;
          } else {
#endif  // CONFIG_HIGHBITDEPTH
            img_stack[pano_y][pano_x][i].y = (uint8_t)interpolated_yvalue;
            img_stack[pano_y][pano_x][i].u = (uint8_t)interpolated_uvalue;
            img_stack[pano_y][pano_x][i].v = (uint8_t)interpolated_vvalue;
            img_stack[pano_y][pano_x][i].exists = 1;
#if CONFIG_HIGHBITDEPTH
          }
#endif  // CONFIG_HIGHBITDEPTH
        } else if (image_x >= 0 && image_x < frame_width && image_y >= 0 &&
                   image_y < frame_height) {
          // Place in panorama stack.
          int pano_x = x + x_min[i] + x_offset;
          int pano_y = y + y_min[i] + y_offset;

          int ychannel_idx = image_y * frames[i]->y_stride + image_x;
          int uvchannel_idx =
              (image_y >> frames[i]->subsampling_y) * frames[i]->uv_stride +
              (image_x >> frames[i]->subsampling_x);
#if CONFIG_HIGHBITDEPTH
          if (frames[i]->flags & YV12_FLAG_HIGHBITDEPTH) {
            img_stack[pano_y][pano_x][i].y = y_buffer16[ychannel_idx];
            img_stack[pano_y][pano_x][i].u = u_buffer16[uvchannel_idx];
            img_stack[pano_y][pano_x][i].v = v_buffer16[uvchannel_idx];
            img_stack[pano_y][pano_x][i].exists = 1;
          } else {
#endif  // CONFIG_HIGHBITDEPTH
            img_stack[pano_y][pano_x][i].y = frames[i]->y_buffer[ychannel_idx];
            img_stack[pano_y][pano_x][i].u = frames[i]->u_buffer[uvchannel_idx];
            img_stack[pano_y][pano_x][i].v = frames[i]->v_buffer[uvchannel_idx];
            img_stack[pano_y][pano_x][i].exists = 1;
#if CONFIG_HIGHBITDEPTH
          }
#endif  // CONFIG_HIGHBITDEPTH
        }
      }
    }
  }
}

#if BGSPRITE_BLENDING_MODE == 0
// swaps two YuvPixels.
static void swap_yuv(YuvPixel *a, YuvPixel *b) {
  const YuvPixel temp = *b;
  *b = *a;
  *a = temp;
}

// Partitions array to find pivot index in qselect.
static int partition(YuvPixel arr[], int left, int right, int pivot_idx) {
  YuvPixel pivot = arr[pivot_idx];

  // Move pivot to the end.
  swap_yuv(&arr[pivot_idx], &arr[right]);

  int p_idx = left;
  for (int i = left; i < right; ++i) {
    if (arr[i].y <= pivot.y) {
      swap_yuv(&arr[i], &arr[p_idx]);
      p_idx++;
    }
  }

  swap_yuv(&arr[p_idx], &arr[right]);

  return p_idx;
}

// Returns the kth element in array, partially sorted in place (quickselect).
static YuvPixel qselect(YuvPixel arr[], int left, int right, int k) {
  if (left >= right) {
    return arr[left];
  }
  unsigned int seed = (int)time(NULL);
  int pivot_idx = left + rand_r(&seed) % (right - left + 1);
  pivot_idx = partition(arr, left, right, pivot_idx);

  if (k == pivot_idx) {
    return arr[k];
  } else if (k < pivot_idx) {
    return qselect(arr, left, pivot_idx - 1, k);
  } else {
    return qselect(arr, pivot_idx + 1, right, k);
  }
}

// Blends image stack together using a temporal median.
static void blend_median(const int width, const int height,
                         const int num_frames, const YuvPixel ***image_stack,
                         YuvPixel **blended_img) {
  // Allocate stack of pixels
  YuvPixel *pixel_stack = aom_calloc(num_frames, sizeof(*pixel_stack));

  // Apply median filtering using quickselect.
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int count = 0;
      for (int i = 0; i < num_frames; ++i) {
        if (image_stack[y][x][i].exists) {
          pixel_stack[count] = image_stack[y][x][i];
          ++count;
        }
      }
      if (count == 0) {
        // Just make the pixel black.
        // TODO(toddnguyen): Color the pixel with nearest neighbor
        blended_img[y][x].exists = 0;
      } else {
        const int median_idx = (int)floor(count / 2);
        YuvPixel median = qselect(pixel_stack, 0, count - 1, median_idx);

        // Make the median value the 0th index for UV subsampling later
        blended_img[y][x] = median;
        blended_img[y][x].exists = 1;
      }
    }
  }

  aom_free(pixel_stack);
}
#endif  // BGSPRITE_BLENDING_MODE == 0

#if BGSPRITE_BLENDING_MODE == 1
// Blends image stack together using a temporal mean.
static void blend_mean(const int width, const int height, const int num_frames,
                       const YuvPixel ***image_stack, YuvPixel **blended_img,
                       int highbitdepth) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      // Find
      uint32_t y_sum = 0;
      uint32_t u_sum = 0;
      uint32_t v_sum = 0;
      uint32_t count = 0;
      for (int i = 0; i < num_frames; ++i) {
        if (image_stack[y][x][i].exists) {
          y_sum += image_stack[y][x][i].y;
          u_sum += image_stack[y][x][i].u;
          v_sum += image_stack[y][x][i].v;
          ++count;
        }
      }

#if BGSPRITE_MEAN_REMOVE_OUTLIERS
      if (count > 1) {
        double stdev = 0;
        double y_mean = (double)y_sum / count;
        for (int i = 0; i < num_frames; ++i) {
          if (image_stack[y][x][i].exists) {
            stdev += pow(y_mean - image_stack[y][x][i].y, 2);
          }
        }
        stdev = sqrt(stdev / count);

        uint32_t inlier_y_sum = 0;
        uint32_t inlier_u_sum = 0;
        uint32_t inlier_v_sum = 0;
        uint32_t inlier_count = 0;
        for (int i = 0; i < num_frames; ++i) {
          if (image_stack[y][x][i].exists &&
              fabs(image_stack[y][x][i].y - y_mean) <= 1.5 * stdev) {
            inlier_y_sum += image_stack[y][x][i].y;
            inlier_u_sum += image_stack[y][x][i].u;
            inlier_v_sum += image_stack[y][x][i].v;
            ++inlier_count;
          }
        }
        count = inlier_count;
        y_sum = inlier_y_sum;
        u_sum = inlier_u_sum;
        v_sum = inlier_v_sum;
      }
#endif  // BGSPRITE_MEAN_REMOVE_OUTLIERS

      if (count != 0) {
        blended_img[y][x].exists = 1;
#if CONFIG_HIGHBITDEPTH
        if (highbitdepth) {
          blended_img[y][x].y = (uint16_t)OD_DIVU(y_sum, count);
          blended_img[y][x].u = (uint16_t)OD_DIVU(u_sum, count);
          blended_img[y][x].v = (uint16_t)OD_DIVU(v_sum, count);
        } else {
#endif  // CONFIG_HIGHBITDEPTH
          (void)highbitdepth;
          blended_img[y][x].y = (uint8_t)OD_DIVU(y_sum, count);
          blended_img[y][x].u = (uint8_t)OD_DIVU(u_sum, count);
          blended_img[y][x].v = (uint8_t)OD_DIVU(v_sum, count);
#if CONFIG_HIGHBITDEPTH
        }
#endif  // CONFIG_HIGHBITDEPTH
      } else {
        blended_img[y][x].exists = 0;
      }
    }
  }
}
#endif  // BGSPRITE_BLENDING_MODE == 1

#if BGSPRITE_ENABLE_SEGMENTATION
// Builds dual-mode single gaussian model from image stack.
static void build_gaussian(const YuvPixel ***image_stack, const int num_frames,
                           const int width, const int height,
                           const int x_block_width, const int y_block_height,
                           const int block_size, YuvPixelGaussian **gauss) {
  const double initial_variance = 10.0;
  const double s_theta = 2.0;

  // Add images to dual-mode single gaussian model
  for (int y_block = 0; y_block < y_block_height; ++y_block) {
    for (int x_block = 0; x_block < x_block_width; ++x_block) {
      // Process all blocks.
      YuvPixelGaussian *model = &gauss[y_block][x_block];

      // Process all frames.
      for (int i = 0; i < num_frames; ++i) {
        // Add block to the Gaussian model.
        double max_variance[2] = { 0.0, 0.0 };
        double temp_y_mean = 0.0;
        double temp_u_mean = 0.0;
        double temp_v_mean = 0.0;

        // Find mean/variance of a block of pixels.
        int temp_count = 0;
        for (int sub_y = 0; sub_y < block_size; ++sub_y) {
          for (int sub_x = 0; sub_x < block_size; ++sub_x) {
            const int y = y_block * block_size + sub_y;
            const int x = x_block * block_size + sub_x;
            if (y < height && x < width && image_stack[y][x][i].exists) {
              ++temp_count;
              temp_y_mean += (double)image_stack[y][x][i].y;
              temp_u_mean += (double)image_stack[y][x][i].u;
              temp_v_mean += (double)image_stack[y][x][i].v;

              const double variance_0 =
                  pow((double)image_stack[y][x][i].y - model->mean[0], 2);
              const double variance_1 =
                  pow((double)image_stack[y][x][i].y - model->mean[1], 2);

              if (variance_0 > max_variance[0]) {
                max_variance[0] = variance_0;
              }
              if (variance_1 > max_variance[1]) {
                max_variance[1] = variance_1;
              }
            }
          }
        }

        // If pixels exist in the block, add to the model.
        if (temp_count > 0) {
          assert(temp_count <= block_size * block_size);
          temp_y_mean /= temp_count;
          temp_u_mean /= temp_count;
          temp_v_mean /= temp_count;

          // Switch the background model to the oldest model.
          if (model->age[0] > model->age[1]) {
            model->curr_model = 0;
          } else if (model->age[1] > model->age[0]) {
            model->curr_model = 1;
          }

          // If model is empty, initialize model.
          if (model->age[model->curr_model] == 0) {
            model->mean[model->curr_model] = temp_y_mean;
            model->u_mean[model->curr_model] = temp_u_mean;
            model->v_mean[model->curr_model] = temp_v_mean;
            model->var[model->curr_model] = initial_variance;
            model->age[model->curr_model] = 1;
          } else {
            // Constants for current model and foreground model (0 or 1).
            const int opposite = 1 - model->curr_model;
            const int current = model->curr_model;
            const double j = i;

            // Put block into the appropriate model.
            if (pow(temp_y_mean - model->mean[current], 2) <
                s_theta * model->var[current]) {
              // Add block to the current background model
              model->age[current] += 1;
              const double prev_weight = 1 / j;
              const double curr_weight = (j - 1) / j;
              model->mean[current] = prev_weight * model->mean[current] +
                                     curr_weight * temp_y_mean;
              model->u_mean[current] = prev_weight * model->u_mean[current] +
                                       curr_weight * temp_u_mean;
              model->v_mean[current] = prev_weight * model->v_mean[current] +
                                       curr_weight * temp_v_mean;
              model->var[current] = prev_weight * model->var[current] +
                                    curr_weight * max_variance[current];
            } else {
              // Block does not fit into current background candidate. Add to
              // foreground candidate and reinitialize if necessary.
              const double var_fg = pow(temp_y_mean - model->mean[opposite], 2);

              if (var_fg <= s_theta * model->var[opposite]) {
                model->age[opposite] += 1;
                const double prev_weight = 1 / j;
                const double curr_weight = (j - 1) / j;
                model->mean[opposite] = prev_weight * model->mean[opposite] +
                                        curr_weight * temp_y_mean;
                model->u_mean[opposite] =
                    prev_weight * model->u_mean[opposite] +
                    curr_weight * temp_u_mean;
                model->v_mean[opposite] =
                    prev_weight * model->v_mean[opposite] +
                    curr_weight * temp_v_mean;
                model->var[opposite] = prev_weight * model->var[opposite] +
                                       curr_weight * max_variance[opposite];
              } else if (model->age[opposite] == 0 ||
                         var_fg > s_theta * model->var[opposite]) {
                model->mean[opposite] = temp_y_mean;
                model->u_mean[opposite] = temp_u_mean;
                model->v_mean[opposite] = temp_v_mean;
                model->var[opposite] = initial_variance;
                model->age[opposite] = 1;
              } else {
                // This case should never happen.
                assert(0);
              }
            }
          }
        }
      }

      // Select the oldest candidate as the background model.
      if (model->age[0] == 0 && model->age[1] == 0) {
        model->y = 0;
        model->u = 0;
        model->v = 0;
        model->final_var = 0;
      } else if (model->age[0] > model->age[1]) {
        model->y = (uint8_t)model->mean[0];
        model->u = (uint8_t)model->u_mean[0];
        model->v = (uint8_t)model->v_mean[0];
        model->final_var = model->var[0];
      } else {
        model->y = (uint8_t)model->mean[1];
        model->u = (uint8_t)model->u_mean[1];
        model->v = (uint8_t)model->v_mean[1];
        model->final_var = model->var[1];
      }
    }
  }
}

// Builds foreground mask based on reference image and gaussian model.
// In mask[][], 1 is foreground and 0 is background.
static void build_mask(const int x_min, const int y_min, const int x_offset,
                       const int y_offset, const int x_block_width,
                       const int y_block_height, const int block_size,
                       const YuvPixelGaussian **gauss,
                       YV12_BUFFER_CONFIG *const reference,
                       YV12_BUFFER_CONFIG *const panorama, uint8_t **mask) {
  const int crop_x_offset = x_min + x_offset;
  const int crop_y_offset = y_min + y_offset;
  const double d_theta = 4.0;

  for (int y_block = 0; y_block < y_block_height; ++y_block) {
    for (int x_block = 0; x_block < x_block_width; ++x_block) {
      // Create mask to determine if ARF is background for foreground.
      const YuvPixelGaussian *model = &gauss[y_block][x_block];
      double temp_y_mean = 0.0;
      int temp_count = 0;

      for (int sub_y = 0; sub_y < block_size; ++sub_y) {
        for (int sub_x = 0; sub_x < block_size; ++sub_x) {
          // x and y are panorama coordinates.
          const int y = y_block * block_size + sub_y;
          const int x = x_block * block_size + sub_x;

          const int arf_y = y - crop_y_offset;
          const int arf_x = x - crop_x_offset;

          if (arf_y >= 0 && arf_y < panorama->y_height && arf_x >= 0 &&
              arf_x < panorama->y_width) {
            ++temp_count;
            const int ychannel_idx = arf_y * panorama->y_stride + arf_x;
            temp_y_mean += (double)reference->y_buffer[ychannel_idx];
          }
        }
      }
      if (temp_count > 0) {
        assert(temp_count <= block_size * block_size);
        temp_y_mean /= temp_count;

        if (pow(temp_y_mean - model->y, 2) > model->final_var * d_theta) {
          // Mark block as foreground.
          mask[y_block][x_block] = 1;
        }
      }
    }
  }
}
#endif  // BGSPRITE_ENABLE_SEGMENTATION

// Resamples blended_img into panorama, including UV subsampling.
static void resample_panorama(YuvPixel **blended_img, const int center_idx,
                              const int *const x_min, const int *const y_min,
                              int pano_x_min, int pano_x_max, int pano_y_min,
                              int pano_y_max, YV12_BUFFER_CONFIG *panorama) {
  const int width = pano_x_max - pano_x_min + 1;
  const int height = pano_y_max - pano_y_min + 1;
  const int x_offset = -pano_x_min;
  const int y_offset = -pano_y_min;
  const int crop_x_offset = x_min[center_idx] + x_offset;
  const int crop_y_offset = y_min[center_idx] + y_offset;
#if CONFIG_HIGHBITDEPTH
  if (panorama->flags & YV12_FLAG_HIGHBITDEPTH) {
    // Use median Y value.
    uint16_t *pano_y_buffer16 = CONVERT_TO_SHORTPTR(panorama->y_buffer);
    uint16_t *pano_u_buffer16 = CONVERT_TO_SHORTPTR(panorama->u_buffer);
    uint16_t *pano_v_buffer16 = CONVERT_TO_SHORTPTR(panorama->v_buffer);

    for (int y = 0; y < panorama->y_height; ++y) {
      for (int x = 0; x < panorama->y_width; ++x) {
        const int ychannel_idx = y * panorama->y_stride + x;
        if (blended_img[y + crop_y_offset][x + crop_x_offset].exists) {
          pano_y_buffer16[ychannel_idx] =
              blended_img[y + crop_y_offset][x + crop_x_offset].y;
        } else {
          pano_y_buffer16[ychannel_idx] = 0;
        }
      }
    }

    // UV subsampling with median UV values
    for (int y = 0; y < panorama->uv_height; ++y) {
      for (int x = 0; x < panorama->uv_width; ++x) {
        uint32_t avg_count = 0;
        uint32_t u_sum = 0;
        uint32_t v_sum = 0;

        // Look at surrounding pixels for subsampling
        for (int s_x = 0; s_x < panorama->subsampling_x + 1; ++s_x) {
          for (int s_y = 0; s_y < panorama->subsampling_y + 1; ++s_y) {
            int y_sample = crop_y_offset + (y << panorama->subsampling_y) + s_y;
            int x_sample = crop_x_offset + (x << panorama->subsampling_x) + s_x;
            if (y_sample > 0 && y_sample < height && x_sample > 0 &&
                x_sample < width && blended_img[y_sample][x_sample].exists) {
              u_sum += blended_img[y_sample][x_sample].u;
              v_sum += blended_img[y_sample][x_sample].v;
              avg_count++;
            }
          }
        }

        const int uvchannel_idx = y * panorama->uv_stride + x;
        if (avg_count != 0) {
          pano_u_buffer16[uvchannel_idx] = (uint16_t)OD_DIVU(u_sum, avg_count);
          pano_v_buffer16[uvchannel_idx] = (uint16_t)OD_DIVU(v_sum, avg_count);
        } else {
          pano_u_buffer16[uvchannel_idx] = 0;
          pano_v_buffer16[uvchannel_idx] = 0;
        }
      }
    }
  } else {
#endif  // CONFIG_HIGHBITDEPTH
    // Use blended Y value.
    for (int y = 0; y < panorama->y_height; ++y) {
      for (int x = 0; x < panorama->y_width; ++x) {
        const int ychannel_idx = y * panorama->y_stride + x;
        // Use filtered background.
        if (blended_img[y + crop_y_offset][x + crop_x_offset].exists) {
          panorama->y_buffer[ychannel_idx] =
              blended_img[y + crop_y_offset][x + crop_x_offset].y;
        } else {
          panorama->y_buffer[ychannel_idx] = 0;
        }
      }
    }

    // UV subsampling with blended UV values.
    for (int y = 0; y < panorama->uv_height; ++y) {
      for (int x = 0; x < panorama->uv_width; ++x) {
        uint16_t avg_count = 0;
        uint16_t u_sum = 0;
        uint16_t v_sum = 0;

        // Look at surrounding pixels for subsampling.
        for (int s_x = 0; s_x < panorama->subsampling_x + 1; ++s_x) {
          for (int s_y = 0; s_y < panorama->subsampling_y + 1; ++s_y) {
            int y_sample = crop_y_offset + (y << panorama->subsampling_y) + s_y;
            int x_sample = crop_x_offset + (x << panorama->subsampling_x) + s_x;
            if (y_sample > 0 && y_sample < height && x_sample > 0 &&
                x_sample < width && blended_img[y_sample][x_sample].exists) {
              u_sum += blended_img[y_sample][x_sample].u;
              v_sum += blended_img[y_sample][x_sample].v;
              avg_count++;
            }
          }
        }

        const int uvchannel_idx = y * panorama->uv_stride + x;
        if (avg_count != 0) {
          panorama->u_buffer[uvchannel_idx] =
              (uint8_t)OD_DIVU(u_sum, avg_count);
          panorama->v_buffer[uvchannel_idx] =
              (uint8_t)OD_DIVU(v_sum, avg_count);
        } else {
          panorama->u_buffer[uvchannel_idx] = 0;
          panorama->v_buffer[uvchannel_idx] = 0;
        }
      }
    }
#if CONFIG_HIGHBITDEPTH
  }
#endif  // CONFIG_HIGHBITDEPTH
}

#if BGSPRITE_ENABLE_SEGMENTATION
// Combines temporal filter output and bgsprite output to make final ARF output
static void combine_arf(YV12_BUFFER_CONFIG *const temporal_arf,
                        YV12_BUFFER_CONFIG *const bgsprite,
                        uint8_t **const mask, const int block_size,
                        const int x_offset, const int y_offset,
                        YV12_BUFFER_CONFIG *target) {
  const int height = temporal_arf->y_height;
  const int width = temporal_arf->y_width;

  YuvPixel **blended_img = aom_malloc(height * sizeof(*blended_img));
  for (int i = 0; i < height; ++i) {
    blended_img[i] = aom_malloc(width * sizeof(**blended_img));
  }

  const int block_2_height = (height / BGSPRITE_MASK_BLOCK_SIZE) +
                             (height % BGSPRITE_MASK_BLOCK_SIZE != 0 ? 1 : 0);
  const int block_2_width = (width / BGSPRITE_MASK_BLOCK_SIZE) +
                            (width % BGSPRITE_MASK_BLOCK_SIZE != 0 ? 1 : 0);

  for (int block_y = 0; block_y < block_2_height; ++block_y) {
    for (int block_x = 0; block_x < block_2_width; ++block_x) {
      int count = 0;
      int total = 0;
      for (int sub_y = 0; sub_y < BGSPRITE_MASK_BLOCK_SIZE; ++sub_y) {
        for (int sub_x = 0; sub_x < BGSPRITE_MASK_BLOCK_SIZE; ++sub_x) {
          const int img_y = block_y * BGSPRITE_MASK_BLOCK_SIZE + sub_y;
          const int img_x = block_x * BGSPRITE_MASK_BLOCK_SIZE + sub_x;
          const int mask_y = (y_offset + img_y) / block_size;
          const int mask_x = (x_offset + img_x) / block_size;

          if (img_y < height && img_x < width) {
            if (mask[mask_y][mask_x]) {
              ++count;
            }
            ++total;
          }
        }
      }

      const double threshold = 0.30;
      const int amount = (int)(threshold * total);
      for (int sub_y = 0; sub_y < BGSPRITE_MASK_BLOCK_SIZE; ++sub_y) {
        for (int sub_x = 0; sub_x < BGSPRITE_MASK_BLOCK_SIZE; ++sub_x) {
          const int y = block_y * BGSPRITE_MASK_BLOCK_SIZE + sub_y;
          const int x = block_x * BGSPRITE_MASK_BLOCK_SIZE + sub_x;
          if (y < height && x < width) {
            blended_img[y][x].exists = 1;
            const int ychannel_idx = y * temporal_arf->y_stride + x;
            const int uvchannel_idx =
                (y >> temporal_arf->subsampling_y) * temporal_arf->uv_stride +
                (x >> temporal_arf->subsampling_x);

            if (count > amount) {
// Foreground; use temporal arf.
#if CONFIG_HIGHBITDEPTH
              if (temporal_arf->flags & YV12_FLAG_HIGHBITDEPTH) {
                uint16_t *pano_y_buffer16 =
                    CONVERT_TO_SHORTPTR(temporal_arf->y_buffer);
                uint16_t *pano_u_buffer16 =
                    CONVERT_TO_SHORTPTR(temporal_arf->u_buffer);
                uint16_t *pano_v_buffer16 =
                    CONVERT_TO_SHORTPTR(temporal_arf->v_buffer);
                blended_img[y][x].y = pano_y_buffer16[ychannel_idx];
                blended_img[y][x].u = pano_u_buffer16[uvchannel_idx];
                blended_img[y][x].v = pano_v_buffer16[uvchannel_idx];
              } else {
#endif  // CONFIG_HIGHBITDEPTH
                blended_img[y][x].y = temporal_arf->y_buffer[ychannel_idx];
                blended_img[y][x].u = temporal_arf->u_buffer[uvchannel_idx];
                blended_img[y][x].v = temporal_arf->v_buffer[uvchannel_idx];
#if CONFIG_HIGHBITDEPTH
              }
#endif  // CONFIG_HIGHBITDEPTH
            } else {
// Background; use bgsprite arf.
#if CONFIG_HIGHBITDEPTH
              if (bgsprite->flags & YV12_FLAG_HIGHBITDEPTH) {
                uint16_t *pano_y_buffer16 =
                    CONVERT_TO_SHORTPTR(bgsprite->y_buffer);
                uint16_t *pano_u_buffer16 =
                    CONVERT_TO_SHORTPTR(bgsprite->u_buffer);
                uint16_t *pano_v_buffer16 =
                    CONVERT_TO_SHORTPTR(bgsprite->v_buffer);
                blended_img[y][x].y = pano_y_buffer16[ychannel_idx];
                blended_img[y][x].u = pano_u_buffer16[uvchannel_idx];
                blended_img[y][x].v = pano_v_buffer16[uvchannel_idx];
              } else {
#endif  // CONFIG_HIGHBITDEPTH
                blended_img[y][x].y = bgsprite->y_buffer[ychannel_idx];
                blended_img[y][x].u = bgsprite->u_buffer[uvchannel_idx];
                blended_img[y][x].v = bgsprite->v_buffer[uvchannel_idx];
#if CONFIG_HIGHBITDEPTH
              }
#endif  // CONFIG_HIGHBITDEPTH
            }
          }
        }
      }
    }
  }

  const int x_min = 0;
  const int y_min = 0;
  resample_panorama(blended_img, 0, &x_min, &y_min, 0, width - 1, 0, height - 1,
                    target);

  for (int i = 0; i < height; ++i) {
    aom_free(blended_img[i]);
  }
  aom_free(blended_img);
}
#endif  // BGSPRITE_ENABLE_SEGMENTATION

// Stitches images together to create ARF and stores it in 'panorama'.
static void stitch_images(AV1_COMP *cpi, YV12_BUFFER_CONFIG **const frames,
                          const int num_frames, const int distance,
                          const int center_idx, const double **const params,
                          const int *const x_min, const int *const x_max,
                          const int *const y_min, const int *const y_max,
                          int pano_x_min, int pano_x_max, int pano_y_min,
                          int pano_y_max, YV12_BUFFER_CONFIG *panorama) {
  const int width = pano_x_max - pano_x_min + 1;
  const int height = pano_y_max - pano_y_min + 1;

  // Create pano_stack[y][x][num_frames] stack of pixel values
  YuvPixel ***pano_stack = aom_malloc(height * sizeof(*pano_stack));
  for (int i = 0; i < height; ++i) {
    pano_stack[i] = aom_malloc(width * sizeof(**pano_stack));
    for (int j = 0; j < width; ++j) {
      pano_stack[i][j] = aom_calloc(num_frames, sizeof(***pano_stack));
    }
  }

  build_image_stack(frames, num_frames, params, x_min, x_max, y_min, y_max,
                    pano_x_min, pano_y_min, pano_stack);

  // Create blended_img[y][x] of combined panorama pixel values.
  YuvPixel **blended_img = aom_malloc(height * sizeof(*blended_img));
  for (int i = 0; i < height; ++i) {
    blended_img[i] = aom_malloc(width * sizeof(**blended_img));
  }

// Blending and saving result in blended_img.
#if BGSPRITE_BLENDING_MODE == 1
  blend_mean(width, height, num_frames, (const YuvPixel ***)pano_stack,
             blended_img, panorama->flags & YV12_FLAG_HIGHBITDEPTH);
#else   // BGSPRITE_BLENDING_MODE != 1
  blend_median(width, height, num_frames, (const YuvPixel ***)pano_stack,
               blended_img);
#endif  // BGSPRITE_BLENDING_MODE == 1

  // NOTE(toddnguyen): Right now the ARF in the cpi struct is fixed size at
  // the same size as the frames. For now, we crop the generated panorama.
  assert(panorama->y_width <= width && panorama->y_height <= height);

  // Resamples the blended_img into the panorama buffer.
  YV12_BUFFER_CONFIG bgsprite;
  memset(&bgsprite, 0, sizeof(bgsprite));
  aom_alloc_frame_buffer(&bgsprite, frames[0]->y_width, frames[0]->y_height,
                         frames[0]->subsampling_x, frames[0]->subsampling_y,
#if CONFIG_HIGHBITDEPTH
                         frames[0]->flags & YV12_FLAG_HIGHBITDEPTH,
#endif
                         frames[0]->border, 0);
  aom_yv12_copy_frame(frames[0], &bgsprite);
  bgsprite.bit_depth = frames[0]->bit_depth;
  resample_panorama(blended_img, center_idx, x_min, y_min, pano_x_min,
                    pano_x_max, pano_y_min, pano_y_max, &bgsprite);

#if BGSPRITE_ENABLE_SEGMENTATION
  YV12_BUFFER_CONFIG temporal_bgsprite;
  memset(&temporal_bgsprite, 0, sizeof(temporal_bgsprite));
  aom_alloc_frame_buffer(&temporal_bgsprite, frames[0]->y_width,
                         frames[0]->y_height, frames[0]->subsampling_x,
                         frames[0]->subsampling_y,
#if CONFIG_HIGHBITDEPTH
                         frames[0]->flags & YV12_FLAG_HIGHBITDEPTH,
#endif
                         frames[0]->border, 0);
  aom_yv12_copy_frame(frames[0], &temporal_bgsprite);
  temporal_bgsprite.bit_depth = frames[0]->bit_depth;

  av1_temporal_filter(cpi, &bgsprite, &temporal_bgsprite, distance);

  // Block size constants for gaussian model.
  const int N_1 = 2;
  const int y_block_height = (height / N_1) + (height % N_1 != 0 ? 1 : 0);
  const int x_block_width = (width / N_1) + (height % N_1 != 0 ? 1 : 0);
  YuvPixelGaussian **gauss = aom_malloc(y_block_height * sizeof(*gauss));
  for (int i = 0; i < y_block_height; ++i) {
    gauss[i] = aom_calloc(x_block_width, sizeof(**gauss));
  }

  // Build Gaussian model.
  build_gaussian((const YuvPixel ***)pano_stack, num_frames, width, height,
                 x_block_width, y_block_height, N_1, gauss);

  // Select background model and build foreground mask.
  uint8_t **mask = aom_malloc(y_block_height * sizeof(*mask));
  for (int i = 0; i < y_block_height; ++i) {
    mask[i] = aom_calloc(x_block_width, sizeof(**mask));
  }

  const int x_offset = -pano_x_min;
  const int y_offset = -pano_y_min;
  build_mask(x_min[center_idx], y_min[center_idx], x_offset, y_offset,
             x_block_width, y_block_height, N_1,
             (const YuvPixelGaussian **)gauss,
             (YV12_BUFFER_CONFIG * const) frames[center_idx], panorama, mask);

  YV12_BUFFER_CONFIG temporal_arf;
  memset(&temporal_arf, 0, sizeof(temporal_arf));
  aom_alloc_frame_buffer(&temporal_arf, frames[0]->y_width, frames[0]->y_height,
                         frames[0]->subsampling_x, frames[0]->subsampling_y,
#if CONFIG_HIGHBITDEPTH
                         frames[0]->flags & YV12_FLAG_HIGHBITDEPTH,
#endif
                         frames[0]->border, 0);
  aom_yv12_copy_frame(frames[0], &temporal_arf);
  temporal_arf.bit_depth = frames[0]->bit_depth;
  av1_temporal_filter(cpi, NULL, &temporal_arf, distance);

  combine_arf(&temporal_arf, &temporal_bgsprite, mask, N_1, x_offset, y_offset,
              panorama);

  aom_free_frame_buffer(&temporal_arf);
  aom_free_frame_buffer(&temporal_bgsprite);
  for (int i = 0; i < y_block_height; ++i) {
    aom_free(gauss[i]);
    aom_free(mask[i]);
  }
  aom_free(gauss);
  aom_free(mask);
#else   // !BGSPRITE_ENABLE_SEGMENTATION
  av1_temporal_filter(cpi, &bgsprite, panorama, distance);
#endif  // BGSPRITE_ENABLE_SEGMENTATION

  aom_free_frame_buffer(&bgsprite);
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      aom_free(pano_stack[i][j]);
    }
    aom_free(pano_stack[i]);
    aom_free(blended_img[i]);
  }
  aom_free(pano_stack);
  aom_free(blended_img);
}

int av1_background_sprite(AV1_COMP *cpi, int distance) {
#if BGSPRITE_ENABLE_METRICS
  // Do temporal filter if firstpass stats disable bgsprite.
  if (!cpi->bgsprite_allowed) {
    return 1;
  }
#endif  // BGSPRITE_ENABLE_METRICS

  YV12_BUFFER_CONFIG *frames[MAX_LAG_BUFFERS] = { NULL };
  static const double identity_params[MAX_PARAMDIM - 1] = {
    0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0
  };

  const int frames_after_arf =
      av1_lookahead_depth(cpi->lookahead) - distance - 1;
  int frames_fwd = (cpi->oxcf.arnr_max_frames - 1) >> 1;
  int frames_bwd;

  // Define the forward and backwards filter limits for this arnr group.
  if (frames_fwd > frames_after_arf) frames_fwd = frames_after_arf;
  if (frames_fwd > distance) frames_fwd = distance;
  frames_bwd = frames_fwd;

#if CONFIG_EXT_REFS
  const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
  if (gf_group->rf_level[gf_group->index] == GF_ARF_LOW) {
    cpi->is_arf_filter_off[gf_group->arf_update_idx[gf_group->index]] = 1;
    frames_fwd = 0;
    frames_bwd = 0;
  } else {
    cpi->is_arf_filter_off[gf_group->arf_update_idx[gf_group->index]] = 0;
  }
#endif  // CONFIG_EXT_REFS

  const int start_frame = distance + frames_fwd;
  const int frames_to_stitch = frames_bwd + 1 + frames_fwd;

  // Get frames to be included in background sprite.
  for (int frame = 0; frame < frames_to_stitch; ++frame) {
    const int which_buffer = start_frame - frame;
    struct lookahead_entry *buf =
        av1_lookahead_peek(cpi->lookahead, which_buffer);
    frames[frames_to_stitch - 1 - frame] = &buf->img;
  }

  // Allocate empty arrays for parameters between frames.
  double **params = aom_malloc(frames_to_stitch * sizeof(*params));
  for (int i = 0; i < frames_to_stitch; ++i) {
    params[i] = aom_malloc(sizeof(identity_params));
    memcpy(params[i], identity_params, sizeof(identity_params));
  }

// Use global motion to find affine transformations between frames.
// params[i] will have the transform from frame[i] to frame[i-1].
// params[0] will have the identity matrix (has no previous frame).
#if BGSPRITE_ENABLE_GME
  TransformationType model = AFFINE;
  int inliers_by_motion[RANSAC_NUM_MOTIONS];
  for (int frame = 0; frame < frames_to_stitch - 1; ++frame) {
    const int global_motion_ret = compute_global_motion_feature_based(
        model, frames[frame + 1], frames[frame],
#if CONFIG_HIGHBITDEPTH
        cpi->common.bit_depth,
#endif  // CONFIG_HIGHBITDEPTH
        inliers_by_motion, params[frame + 1], RANSAC_NUM_MOTIONS);

    // Quit if global motion had an error.
    if (global_motion_ret == 0) {
      for (int i = 0; i < frames_to_stitch; ++i) {
        aom_free(params[i]);
      }
      aom_free(params);
      return 1;
    }
  }
#endif  // BGSPRITE_ENABLE_GME

  // Compound the transformation parameters.
  for (int i = 1; i < frames_to_stitch; ++i) {
    multiply_params(params[i - 1], params[i], params[i]);
  }

  // Compute frame limits for final stitched images.
  int pano_x_max = INT_MIN;
  int pano_x_min = INT_MAX;
  int pano_y_max = INT_MIN;
  int pano_y_min = INT_MAX;
  int *x_max = aom_malloc(frames_to_stitch * sizeof(*x_max));
  int *x_min = aom_malloc(frames_to_stitch * sizeof(*x_min));
  int *y_max = aom_malloc(frames_to_stitch * sizeof(*y_max));
  int *y_min = aom_malloc(frames_to_stitch * sizeof(*y_min));

  find_limits(frames[0]->y_width, frames[0]->y_height,
              (const double **const)params, frames_to_stitch, x_min, x_max,
              y_min, y_max, &pano_x_min, &pano_x_max, &pano_y_min, &pano_y_max);

  // Center panorama on the ARF.
  const int center_idx = frames_bwd;
  assert(center_idx >= 0 && center_idx < frames_to_stitch);

  // Recompute transformations to adjust to center image.
  // Invert center image's transform.
  double inverse[MAX_PARAMDIM - 1] = { 0 };
  invert_params(params[center_idx], inverse);

  // Multiply the inverse to all transformation parameters.
  for (int i = 0; i < frames_to_stitch; ++i) {
    multiply_params(inverse, params[i], params[i]);
  }

  // Recompute frame limits for new adjusted center.
  find_limits(frames[0]->y_width, frames[0]->y_height,
              (const double **const)params, frames_to_stitch, x_min, x_max,
              y_min, y_max, &pano_x_min, &pano_x_max, &pano_y_min, &pano_y_max);

  // Stitch Images and apply bgsprite filter.
  stitch_images(cpi, frames, frames_to_stitch, distance, center_idx,
                (const double **const)params, x_min, x_max, y_min, y_max,
                pano_x_min, pano_x_max, pano_y_min, pano_y_max,
                &cpi->alt_ref_buffer);

  // Free memory.
  for (int i = 0; i < frames_to_stitch; ++i) {
    aom_free(params[i]);
  }
  aom_free(params);
  aom_free(x_max);
  aom_free(x_min);
  aom_free(y_max);
  aom_free(y_min);

  return 0;
}

#undef _POSIX_C_SOURCE
#undef BGSPRITE_BLENDING_MODE
#undef BGSPRITE_INTERPOLATION
#undef BGSPRITE_ENABLE_METRICS
#undef BGSPRITE_ENABLE_SEGMENTATION
#undef BGSPRITE_ENABLE_GME
#undef BGSPRITE_MASK_BLOCK_SIZE
#undef TRANSFORM_MAT_DIM
