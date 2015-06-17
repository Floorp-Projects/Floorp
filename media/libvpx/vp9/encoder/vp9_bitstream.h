/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP9_ENCODER_VP9_BITSTREAM_H_
#define VP9_ENCODER_VP9_BITSTREAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "vp9/encoder/vp9_encoder.h"

void vp9_pack_bitstream(VP9_COMP *cpi, uint8_t *dest, size_t *size);

static INLINE int vp9_preserve_existing_gf(VP9_COMP *cpi) {
  return !cpi->multi_arf_allowed && cpi->refresh_golden_frame &&
         cpi->rc.is_src_frame_alt_ref &&
         (!cpi->use_svc ||      // Add spatial svc base layer case here
          (is_two_pass_svc(cpi) &&
           cpi->svc.spatial_layer_id == 0 &&
           cpi->svc.layer_context[0].gold_ref_idx >=0 &&
           cpi->oxcf.ss_enable_auto_arf[0]));
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_BITSTREAM_H_
