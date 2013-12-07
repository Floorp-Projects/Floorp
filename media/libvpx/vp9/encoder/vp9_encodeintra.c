/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "./vpx_config.h"
#include "./vp9_rtcd.h"
#include "vp9/encoder/vp9_quantize.h"
#include "vp9/common/vp9_reconintra.h"
#include "vp9/encoder/vp9_encodemb.h"
#include "vp9/encoder/vp9_encodeintra.h"

int vp9_encode_intra(MACROBLOCK *x, int use_16x16_pred) {
  MB_MODE_INFO * mbmi = &x->e_mbd.mi_8x8[0]->mbmi;
  x->skip_encode = 0;
  mbmi->mode = DC_PRED;
  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->tx_size = use_16x16_pred ? (mbmi->sb_type >= BLOCK_16X16 ? TX_16X16
                                                                 : TX_8X8)
                                   : TX_4X4;
  vp9_encode_intra_block_y(x, mbmi->sb_type);
  return vp9_get_mb_ss(x->plane[0].src_diff);
}
