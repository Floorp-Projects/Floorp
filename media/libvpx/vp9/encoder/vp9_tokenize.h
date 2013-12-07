/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_TOKENIZE_H_
#define VP9_ENCODER_VP9_TOKENIZE_H_

#include "vp9/common/vp9_entropy.h"
#include "vp9/encoder/vp9_block.h"

void vp9_tokenize_initialize();

typedef struct {
  int16_t token;
  int16_t extra;
} TOKENVALUE;

typedef struct {
  const vp9_prob *context_tree;
  int16_t         extra;
  uint8_t         token;
  uint8_t         skip_eob_node;
} TOKENEXTRA;

int vp9_sb_is_skippable(MACROBLOCKD *xd, BLOCK_SIZE bsize);
int vp9_is_skippable_in_plane(MACROBLOCKD *xd, BLOCK_SIZE bsize,
                              int plane);
struct VP9_COMP;

void vp9_tokenize_sb(struct VP9_COMP *cpi, TOKENEXTRA **t, int dry_run,
                     BLOCK_SIZE bsize);

extern const int *vp9_dct_value_cost_ptr;
/* TODO: The Token field should be broken out into a separate char array to
 *  improve cache locality, since it's needed for costing when the rest of the
 *  fields are not.
 */
extern const TOKENVALUE *vp9_dct_value_tokens_ptr;

#endif  // VP9_ENCODER_VP9_TOKENIZE_H_
