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

#ifndef AOM_AV1_ENCODER_PALETTE_H_
#define AOM_AV1_ENCODER_PALETTE_H_

#include "av1/common/blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AV1_K_MEANS_RENAME(func, dim) func##_dim##dim

void AV1_K_MEANS_RENAME(av1_calc_indices, 1)(const int *data,
                                             const int *centroids,
                                             uint8_t *indices, int n, int k);
void AV1_K_MEANS_RENAME(av1_calc_indices, 2)(const int *data,
                                             const int *centroids,
                                             uint8_t *indices, int n, int k);
void AV1_K_MEANS_RENAME(av1_k_means, 1)(const int *data, int *centroids,
                                        uint8_t *indices, int n, int k,
                                        int max_itr);
void AV1_K_MEANS_RENAME(av1_k_means, 2)(const int *data, int *centroids,
                                        uint8_t *indices, int n, int k,
                                        int max_itr);

// Given 'n' 'data' points and 'k' 'centroids' each of dimension 'dim',
// calculate the centroid 'indices' for the data points.
static INLINE void av1_calc_indices(const int *data, const int *centroids,
                                    uint8_t *indices, int n, int k, int dim) {
  if (dim == 1) {
    AV1_K_MEANS_RENAME(av1_calc_indices, 1)(data, centroids, indices, n, k);
  } else if (dim == 2) {
    AV1_K_MEANS_RENAME(av1_calc_indices, 2)(data, centroids, indices, n, k);
  } else {
    assert(0 && "Untemplated k means dimension");
  }
}

// Given 'n' 'data' points and an initial guess of 'k' 'centroids' each of
// dimension 'dim', runs up to 'max_itr' iterations of k-means algorithm to get
// updated 'centroids' and the centroid 'indices' for elements in 'data'.
// Note: the output centroids are rounded off to nearest integers.
static INLINE void av1_k_means(const int *data, int *centroids,
                               uint8_t *indices, int n, int k, int dim,
                               int max_itr) {
  if (dim == 1) {
    AV1_K_MEANS_RENAME(av1_k_means, 1)(data, centroids, indices, n, k, max_itr);
  } else if (dim == 2) {
    AV1_K_MEANS_RENAME(av1_k_means, 2)(data, centroids, indices, n, k, max_itr);
  } else {
    assert(0 && "Untemplated k means dimension");
  }
}

// Given a list of centroids, returns the unique number of centroids 'k', and
// puts these unique centroids in first 'k' indices of 'centroids' array.
// Ideally, the centroids should be rounded to integers before calling this
// method.
int av1_remove_duplicates(int *centroids, int num_centroids);

// Given a color cache and a set of base colors, find if each cache color is
// present in the base colors, record the binary results in "cache_color_found".
// Record the colors that are not in the color cache in "out_cache_colors".
int av1_index_color_cache(const uint16_t *color_cache, int n_cache,
                          const uint16_t *colors, int n_colors,
                          uint8_t *cache_color_found, int *out_cache_colors);

// Return the number of bits used to transmit each v palette color delta;
// assign zero_count with the number of deltas being 0.
int av1_get_palette_delta_bits_v(const PALETTE_MODE_INFO *const pmi,
                                 int bit_depth, int *zero_count, int *min_bits);

// Return the rate cost for transmitting luma palette color values.
int av1_palette_color_cost_y(const PALETTE_MODE_INFO *const pmi,
                             uint16_t *color_cache, int n_cache, int bit_depth);

// Return the rate cost for transmitting chroma palette color values.
int av1_palette_color_cost_uv(const PALETTE_MODE_INFO *const pmi,
                              uint16_t *color_cache, int n_cache,
                              int bit_depth);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_PALETTE_H_
