/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#if !defined __INC_RATECTRL_H

#include "onyx_int.h"

extern void vp8_save_coding_context(VP8_COMP *cpi);
extern void vp8_restore_coding_context(VP8_COMP *cpi);

extern void vp8_setup_key_frame(VP8_COMP *cpi);
extern void vp8_update_rate_correction_factors(VP8_COMP *cpi, int damp_var);
extern int vp8_regulate_q(VP8_COMP *cpi, int target_bits_per_frame);
extern void vp8_adjust_key_frame_context(VP8_COMP *cpi);
extern void vp8_compute_frame_size_bounds(VP8_COMP *cpi, int *frame_under_shoot_limit, int *frame_over_shoot_limit);

/* return of 0 means drop frame */
extern int vp8_pick_frame_size(VP8_COMP *cpi);

#endif
