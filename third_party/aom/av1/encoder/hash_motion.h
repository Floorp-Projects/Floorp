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

#ifndef AV1_ENCODER_HASH_MOTION_H_
#define AV1_ENCODER_HASH_MOTION_H_

#include "config/aom_config.h"

#include "aom/aom_integer.h"
#include "aom_scale/yv12config.h"
#include "third_party/vector/vector.h"
#ifdef __cplusplus
extern "C" {
#endif

// store a block's hash info.
// x and y are the position from the top left of the picture
// hash_value2 is used to store the second hash value
typedef struct _block_hash {
  int16_t x;
  int16_t y;
  uint32_t hash_value2;
} block_hash;

typedef struct _hash_table {
  Vector **p_lookup_table;
} hash_table;

void av1_hash_table_init(hash_table *p_hash_table);
void av1_hash_table_destroy(hash_table *p_hash_table);
void av1_hash_table_create(hash_table *p_hash_table);
int32_t av1_hash_table_count(hash_table *p_hash_table, uint32_t hash_value);
Iterator av1_hash_get_first_iterator(hash_table *p_hash_table,
                                     uint32_t hash_value);
int32_t av1_has_exact_match(hash_table *p_hash_table, uint32_t hash_value1,
                            uint32_t hash_value2);
void av1_generate_block_2x2_hash_value(const YV12_BUFFER_CONFIG *picture,
                                       uint32_t *pic_block_hash[2],
                                       int8_t *pic_block_same_info[3]);
void av1_generate_block_hash_value(const YV12_BUFFER_CONFIG *picture,
                                   int block_size,
                                   uint32_t *src_pic_block_hash[2],
                                   uint32_t *dst_pic_block_hash[2],
                                   int8_t *src_pic_block_same_info[3],
                                   int8_t *dst_pic_block_same_info[3]);
void av1_add_to_hash_map_by_row_with_precal_data(hash_table *p_hash_table,
                                                 uint32_t *pic_hash[2],
                                                 int8_t *pic_is_same,
                                                 int pic_width, int pic_height,
                                                 int block_size);

// check whether the block starts from (x_start, y_start) with the size of
// block_size x block_size has the same color in all rows
int av1_hash_is_horizontal_perfect(const YV12_BUFFER_CONFIG *picture,
                                   int block_size, int x_start, int y_start);
// check whether the block starts from (x_start, y_start) with the size of
// block_size x block_size has the same color in all columns
int av1_hash_is_vertical_perfect(const YV12_BUFFER_CONFIG *picture,
                                 int block_size, int x_start, int y_start);
void av1_get_block_hash_value(uint8_t *y_src, int stride, int block_size,
                              uint32_t *hash_value1, uint32_t *hash_value2,
                              int use_highbitdepth);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_HASH_MOTION_H_
