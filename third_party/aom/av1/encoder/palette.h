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

#ifndef AV1_ENCODER_PALETTE_H_
#define AV1_ENCODER_PALETTE_H_

#include "av1/common/blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

// Given 'n' 'data' points and 'k' 'centroids' each of dimension 'dim',
// calculate the centroid 'indices' for the data points.
void av1_calc_indices(const float *data, const float *centroids,
                      uint8_t *indices, int n, int k, int dim);

// Given 'n' 'data' points and an initial guess of 'k' 'centroids' each of
// dimension 'dim', runs up to 'max_itr' iterations of k-means algorithm to get
// updated 'centroids' and the centroid 'indices' for elements in 'data'.
// Note: the output centroids are rounded off to nearest integers.
void av1_k_means(const float *data, float *centroids, uint8_t *indices, int n,
                 int k, int dim, int max_itr);

// Given a list of centroids, returns the unique number of centroids 'k', and
// puts these unique centroids in first 'k' indices of 'centroids' array.
// Ideally, the centroids should be rounded to integers before calling this
// method.
int av1_remove_duplicates(float *centroids, int num_centroids);

// Returns the number of colors in 'src'.
int av1_count_colors(const uint8_t *src, int stride, int rows, int cols);
#if CONFIG_HIGHBITDEPTH
// Same as av1_count_colors(), but for high-bitdepth mode.
int av1_count_colors_highbd(const uint8_t *src8, int stride, int rows, int cols,
                            int bit_depth);
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_PALETTE_DELTA_ENCODING
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
#endif  // CONFIG_PALETTE_DELTA_ENCODING

// Return the rate cost for transmitting luma palette color values.
int av1_palette_color_cost_y(const PALETTE_MODE_INFO *const pmi,
#if CONFIG_PALETTE_DELTA_ENCODING
                             uint16_t *color_cache, int n_cache,
#endif  // CONFIG_PALETTE_DELTA_ENCODING
                             int bit_depth);

// Return the rate cost for transmitting chroma palette color values.
int av1_palette_color_cost_uv(const PALETTE_MODE_INFO *const pmi,
#if CONFIG_PALETTE_DELTA_ENCODING
                              uint16_t *color_cache, int n_cache,
#endif  // CONFIG_PALETTE_DELTA_ENCODING
                              int bit_depth);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* AV1_ENCODER_PALETTE_H_ */
