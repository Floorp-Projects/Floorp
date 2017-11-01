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

#include <math.h>
#include <stdlib.h>

#include "av1/encoder/cost.h"
#include "av1/encoder/palette.h"
#include "av1/encoder/random.h"

#define AV1_K_MEANS_DIM 1
#include "av1/encoder/k_means_template.h"
#undef AV1_K_MEANS_DIM
#define AV1_K_MEANS_DIM 2
#include "av1/encoder/k_means_template.h"
#undef AV1_K_MEANS_DIM

static int float_comparer(const void *a, const void *b) {
  const float fa = *(const float *)a;
  const float fb = *(const float *)b;
  return (fa > fb) - (fa < fb);
}

int av1_remove_duplicates(float *centroids, int num_centroids) {
  int num_unique;  // number of unique centroids
  int i;
  qsort(centroids, num_centroids, sizeof(*centroids), float_comparer);
  // Remove duplicates.
  num_unique = 1;
  for (i = 1; i < num_centroids; ++i) {
    if (centroids[i] != centroids[i - 1]) {  // found a new unique centroid
      centroids[num_unique++] = centroids[i];
    }
  }
  return num_unique;
}

#if CONFIG_PALETTE_DELTA_ENCODING
static int delta_encode_cost(const int *colors, int num, int bit_depth,
                             int min_val) {
  if (num <= 0) return 0;
  int bits_cost = bit_depth;
  if (num == 1) return bits_cost;
  bits_cost += 2;
  int max_delta = 0;
  int deltas[PALETTE_MAX_SIZE];
  const int min_bits = bit_depth - 3;
  for (int i = 1; i < num; ++i) {
    const int delta = colors[i] - colors[i - 1];
    deltas[i - 1] = delta;
    assert(delta >= min_val);
    if (delta > max_delta) max_delta = delta;
  }
  int bits_per_delta = AOMMAX(av1_ceil_log2(max_delta + 1 - min_val), min_bits);
  assert(bits_per_delta <= bit_depth);
  int range = (1 << bit_depth) - colors[0] - min_val;
  for (int i = 0; i < num - 1; ++i) {
    bits_cost += bits_per_delta;
    range -= deltas[i];
    bits_per_delta = AOMMIN(bits_per_delta, av1_ceil_log2(range));
  }
  return bits_cost;
}

int av1_index_color_cache(const uint16_t *color_cache, int n_cache,
                          const uint16_t *colors, int n_colors,
                          uint8_t *cache_color_found, int *out_cache_colors) {
  if (n_cache <= 0) {
    for (int i = 0; i < n_colors; ++i) out_cache_colors[i] = colors[i];
    return n_colors;
  }
  memset(cache_color_found, 0, n_cache * sizeof(*cache_color_found));
  int n_in_cache = 0;
  int in_cache_flags[PALETTE_MAX_SIZE];
  memset(in_cache_flags, 0, sizeof(in_cache_flags));
  for (int i = 0; i < n_cache && n_in_cache < n_colors; ++i) {
    for (int j = 0; j < n_colors; ++j) {
      if (colors[j] == color_cache[i]) {
        in_cache_flags[j] = 1;
        cache_color_found[i] = 1;
        ++n_in_cache;
        break;
      }
    }
  }
  int j = 0;
  for (int i = 0; i < n_colors; ++i)
    if (!in_cache_flags[i]) out_cache_colors[j++] = colors[i];
  assert(j == n_colors - n_in_cache);
  return j;
}

int av1_get_palette_delta_bits_v(const PALETTE_MODE_INFO *const pmi,
                                 int bit_depth, int *zero_count,
                                 int *min_bits) {
  const int n = pmi->palette_size[1];
  const int max_val = 1 << bit_depth;
  int max_d = 0;
  *min_bits = bit_depth - 4;
  *zero_count = 0;
  for (int i = 1; i < n; ++i) {
    const int delta = pmi->palette_colors[2 * PALETTE_MAX_SIZE + i] -
                      pmi->palette_colors[2 * PALETTE_MAX_SIZE + i - 1];
    const int v = abs(delta);
    const int d = AOMMIN(v, max_val - v);
    if (d > max_d) max_d = d;
    if (d == 0) ++(*zero_count);
  }
  return AOMMAX(av1_ceil_log2(max_d + 1), *min_bits);
}
#endif  // CONFIG_PALETTE_DELTA_ENCODING

int av1_palette_color_cost_y(const PALETTE_MODE_INFO *const pmi,
#if CONFIG_PALETTE_DELTA_ENCODING
                             uint16_t *color_cache, int n_cache,
#endif  // CONFIG_PALETTE_DELTA_ENCODING
                             int bit_depth) {
  const int n = pmi->palette_size[0];
#if CONFIG_PALETTE_DELTA_ENCODING
  int out_cache_colors[PALETTE_MAX_SIZE];
  uint8_t cache_color_found[2 * PALETTE_MAX_SIZE];
  const int n_out_cache =
      av1_index_color_cache(color_cache, n_cache, pmi->palette_colors, n,
                            cache_color_found, out_cache_colors);
  const int total_bits =
      n_cache + delta_encode_cost(out_cache_colors, n_out_cache, bit_depth, 1);
  return total_bits * av1_cost_bit(128, 0);
#else
  return bit_depth * n * av1_cost_bit(128, 0);
#endif  // CONFIG_PALETTE_DELTA_ENCODING
}

int av1_palette_color_cost_uv(const PALETTE_MODE_INFO *const pmi,
#if CONFIG_PALETTE_DELTA_ENCODING
                              uint16_t *color_cache, int n_cache,
#endif  // CONFIG_PALETTE_DELTA_ENCODING
                              int bit_depth) {
  const int n = pmi->palette_size[1];
#if CONFIG_PALETTE_DELTA_ENCODING
  int total_bits = 0;
  // U channel palette color cost.
  int out_cache_colors[PALETTE_MAX_SIZE];
  uint8_t cache_color_found[2 * PALETTE_MAX_SIZE];
  const int n_out_cache = av1_index_color_cache(
      color_cache, n_cache, pmi->palette_colors + PALETTE_MAX_SIZE, n,
      cache_color_found, out_cache_colors);
  total_bits +=
      n_cache + delta_encode_cost(out_cache_colors, n_out_cache, bit_depth, 0);

  // V channel palette color cost.
  int zero_count = 0, min_bits_v = 0;
  const int bits_v =
      av1_get_palette_delta_bits_v(pmi, bit_depth, &zero_count, &min_bits_v);
  const int bits_using_delta =
      2 + bit_depth + (bits_v + 1) * (n - 1) - zero_count;
  const int bits_using_raw = bit_depth * n;
  total_bits += 1 + AOMMIN(bits_using_delta, bits_using_raw);
  return total_bits * av1_cost_bit(128, 0);
#else
  return 2 * bit_depth * n * av1_cost_bit(128, 0);
#endif  // CONFIG_PALETTE_DELTA_ENCODING
}
