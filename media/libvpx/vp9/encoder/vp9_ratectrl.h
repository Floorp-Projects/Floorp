/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP9_ENCODER_VP9_RATECTRL_H_
#define VP9_ENCODER_VP9_RATECTRL_H_

#include "vp9/encoder/vp9_onyx_int.h"

#define FRAME_OVERHEAD_BITS 200

void vp9_save_coding_context(VP9_COMP *cpi);
void vp9_restore_coding_context(VP9_COMP *cpi);

void vp9_setup_key_frame(VP9_COMP *cpi);
void vp9_update_rate_correction_factors(VP9_COMP *cpi, int damp_var);
int vp9_regulate_q(VP9_COMP *cpi, int target_bits_per_frame);
void vp9_adjust_key_frame_context(VP9_COMP *cpi);
void vp9_compute_frame_size_bounds(VP9_COMP *cpi,
                                   int *frame_under_shoot_limit,
                                   int *frame_over_shoot_limit);

// return of 0 means drop frame
int vp9_pick_frame_size(VP9_COMP *cpi);

double vp9_convert_qindex_to_q(int qindex);
int vp9_gfboost_qadjust(int qindex);
int vp9_bits_per_mb(FRAME_TYPE frame_type, int qindex,
                    double correction_factor);
void vp9_setup_inter_frame(VP9_COMP *cpi);

#endif  // VP9_ENCODER_VP9_RATECTRL_H_
