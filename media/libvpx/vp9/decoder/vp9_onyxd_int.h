/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_DECODER_VP9_ONYXD_INT_H_
#define VP9_DECODER_VP9_ONYXD_INT_H_

#include "./vpx_config.h"

#include "vp9/common/vp9_onyxc_int.h"
#include "vp9/decoder/vp9_dthread.h"
#include "vp9/decoder/vp9_onyxd.h"
#include "vp9/decoder/vp9_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VP9Decompressor {
  DECLARE_ALIGNED(16, MACROBLOCKD, mb);

  DECLARE_ALIGNED(16, VP9_COMMON, common);

  DECLARE_ALIGNED(16, int16_t,  dqcoeff[MAX_MB_PLANE][64 * 64]);

  VP9D_CONFIG oxcf;

  const uint8_t *source;
  size_t source_sz;

  int64_t last_time_stamp;
  int ready_for_new_data;

  int refresh_frame_flags;

  int decoded_key_frame;

  int initial_width;
  int initial_height;

  int do_loopfilter_inline;  // apply loopfilter to available rows immediately
  VP9Worker lf_worker;

  VP9Worker *tile_workers;
  int num_tile_workers;

  VP9LfSync lf_row_sync;

  /* Each tile column has its own MODE_INFO stream. This array indexes them by
     tile column index. */
  MODE_INFO **mi_streams;

  ENTROPY_CONTEXT *above_context[MAX_MB_PLANE];
  PARTITION_CONTEXT *above_seg_context;
} VP9D_COMP;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_DECODER_VP9_ONYXD_INT_H_
