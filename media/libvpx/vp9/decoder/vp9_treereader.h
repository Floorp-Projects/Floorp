/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP9_DECODER_VP9_TREEREADER_H_
#define VP9_DECODER_VP9_TREEREADER_H_

#include "vp9/common/vp9_treecoder.h"
#include "vp9/decoder/vp9_dboolhuff.h"

// Intent of tree data structure is to make decoding trivial.
static int treed_read(vp9_reader *const r, /* !!! must return a 0 or 1 !!! */
                      vp9_tree t,
                      const vp9_prob *const p) {
  register vp9_tree_index i = 0;

  while ((i = t[ i + vp9_read(r, p[i >> 1])]) > 0)
    continue;

  return -i;
}

#endif  // VP9_DECODER_VP9_TREEREADER_H_
