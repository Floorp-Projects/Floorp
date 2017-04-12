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

#ifndef AV1_ENCODER_VARIANCE_TREE_H_
#define AV1_ENCODER_VARIANCE_TREE_H_

#include <assert.h>

#include "./aom_config.h"

#include "aom/aom_integer.h"

#include "av1/common/enums.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AV1Common;
struct ThreadData;

typedef struct {
  int64_t sum_square_error;
  int64_t sum_error;
  int log2_count;
  int variance;
} VAR;

typedef struct {
  VAR none;
  VAR horz[2];
  VAR vert[2];
} partition_variance;

typedef struct VAR_TREE {
  int force_split;
  partition_variance variances;
  struct VAR_TREE *split[4];
  BLOCK_SIZE bsize;
  const uint8_t *src;
  const uint8_t *ref;
  int src_stride;
  int ref_stride;
  int width;
  int height;
#if CONFIG_HIGHBITDEPTH
  int highbd;
#endif  // CONFIG_HIGHBITDEPTH
} VAR_TREE;

void av1_setup_var_tree(struct AV1Common *cm, struct ThreadData *td);
void av1_free_var_tree(struct ThreadData *td);

// Set variance values given sum square error, sum error, count.
static INLINE void fill_variance(int64_t s2, int64_t s, int c, VAR *v) {
  v->sum_square_error = s2;
  v->sum_error = s;
  v->log2_count = c;
  v->variance =
      (int)(256 * (v->sum_square_error -
                   ((v->sum_error * v->sum_error) >> v->log2_count)) >>
            v->log2_count);
}

static INLINE void sum_2_variances(const VAR *a, const VAR *b, VAR *r) {
  assert(a->log2_count == b->log2_count);
  fill_variance(a->sum_square_error + b->sum_square_error,
                a->sum_error + b->sum_error, a->log2_count + 1, r);
}

static INLINE void fill_variance_node(VAR_TREE *vt) {
  sum_2_variances(&vt->split[0]->variances.none, &vt->split[1]->variances.none,
                  &vt->variances.horz[0]);
  sum_2_variances(&vt->split[2]->variances.none, &vt->split[3]->variances.none,
                  &vt->variances.horz[1]);
  sum_2_variances(&vt->split[0]->variances.none, &vt->split[2]->variances.none,
                  &vt->variances.vert[0]);
  sum_2_variances(&vt->split[1]->variances.none, &vt->split[3]->variances.none,
                  &vt->variances.vert[1]);
  sum_2_variances(&vt->variances.vert[0], &vt->variances.vert[1],
                  &vt->variances.none);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* AV1_ENCODER_VARIANCE_TREE_H_ */
