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
#include <stdint.h>
#include <string.h>

#include "av1/encoder/palette.h"
#include "av1/encoder/random.h"

#ifndef AV1_K_MEANS_DIM
#error "This template requires AV1_K_MEANS_DIM to be defined"
#endif

#define RENAME_(x, y) AV1_K_MEANS_RENAME(x, y)
#define RENAME(x) RENAME_(x, AV1_K_MEANS_DIM)

static float RENAME(calc_dist)(const float *p1, const float *p2) {
  float dist = 0;
  int i;
  for (i = 0; i < AV1_K_MEANS_DIM; ++i) {
    const float diff = p1[i] - p2[i];
    dist += diff * diff;
  }
  return dist;
}

void RENAME(av1_calc_indices)(const float *data, const float *centroids,
                              uint8_t *indices, int n, int k) {
  int i, j;
  for (i = 0; i < n; ++i) {
    float min_dist = RENAME(calc_dist)(data + i * AV1_K_MEANS_DIM, centroids);
    indices[i] = 0;
    for (j = 1; j < k; ++j) {
      const float this_dist = RENAME(calc_dist)(
          data + i * AV1_K_MEANS_DIM, centroids + j * AV1_K_MEANS_DIM);
      if (this_dist < min_dist) {
        min_dist = this_dist;
        indices[i] = j;
      }
    }
  }
}

static void RENAME(calc_centroids)(const float *data, float *centroids,
                                   const uint8_t *indices, int n, int k) {
  int i, j, index;
  int count[PALETTE_MAX_SIZE];
  unsigned int rand_state = (unsigned int)data[0];

  assert(n <= 32768);

  memset(count, 0, sizeof(count[0]) * k);
  memset(centroids, 0, sizeof(centroids[0]) * k * AV1_K_MEANS_DIM);

  for (i = 0; i < n; ++i) {
    index = indices[i];
    assert(index < k);
    ++count[index];
    for (j = 0; j < AV1_K_MEANS_DIM; ++j) {
      centroids[index * AV1_K_MEANS_DIM + j] += data[i * AV1_K_MEANS_DIM + j];
    }
  }

  for (i = 0; i < k; ++i) {
    if (count[i] == 0) {
      memcpy(centroids + i * AV1_K_MEANS_DIM,
             data + (lcg_rand16(&rand_state) % n) * AV1_K_MEANS_DIM,
             sizeof(centroids[0]) * AV1_K_MEANS_DIM);
    } else {
      const float norm = 1.0f / count[i];
      for (j = 0; j < AV1_K_MEANS_DIM; ++j)
        centroids[i * AV1_K_MEANS_DIM + j] *= norm;
    }
  }

  // Round to nearest integers.
  for (i = 0; i < k * AV1_K_MEANS_DIM; ++i) {
    centroids[i] = roundf(centroids[i]);
  }
}

static float RENAME(calc_total_dist)(const float *data, const float *centroids,
                                     const uint8_t *indices, int n, int k) {
  float dist = 0;
  int i;
  (void)k;

  for (i = 0; i < n; ++i)
    dist += RENAME(calc_dist)(data + i * AV1_K_MEANS_DIM,
                              centroids + indices[i] * AV1_K_MEANS_DIM);

  return dist;
}

void RENAME(av1_k_means)(const float *data, float *centroids, uint8_t *indices,
                         int n, int k, int max_itr) {
  int i;
  float this_dist;
  float pre_centroids[2 * PALETTE_MAX_SIZE];
  uint8_t pre_indices[MAX_SB_SQUARE];

  RENAME(av1_calc_indices)(data, centroids, indices, n, k);
  this_dist = RENAME(calc_total_dist)(data, centroids, indices, n, k);

  for (i = 0; i < max_itr; ++i) {
    const float pre_dist = this_dist;
    memcpy(pre_centroids, centroids,
           sizeof(pre_centroids[0]) * k * AV1_K_MEANS_DIM);
    memcpy(pre_indices, indices, sizeof(pre_indices[0]) * n);

    RENAME(calc_centroids)(data, centroids, indices, n, k);
    RENAME(av1_calc_indices)(data, centroids, indices, n, k);
    this_dist = RENAME(calc_total_dist)(data, centroids, indices, n, k);

    if (this_dist > pre_dist) {
      memcpy(centroids, pre_centroids,
             sizeof(pre_centroids[0]) * k * AV1_K_MEANS_DIM);
      memcpy(indices, pre_indices, sizeof(pre_indices[0]) * n);
      break;
    }
    if (!memcmp(centroids, pre_centroids,
                sizeof(pre_centroids[0]) * k * AV1_K_MEANS_DIM))
      break;
  }
}

#undef RENAME_
#undef RENAME
