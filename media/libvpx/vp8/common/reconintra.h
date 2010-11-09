/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_RECONINTRA_H
#define __INC_RECONINTRA_H

extern void init_intra_left_above_pixels(MACROBLOCKD *x);

extern void (*vp8_build_intra_predictors_mby_ptr)(MACROBLOCKD *x);
extern void vp8_build_intra_predictors_mby(MACROBLOCKD *x);
extern void vp8_build_intra_predictors_mby_neon(MACROBLOCKD *x);
extern void (*vp8_build_intra_predictors_mby_s_ptr)(MACROBLOCKD *x);
extern void vp8_build_intra_predictors_mby_s(MACROBLOCKD *x);
extern void vp8_build_intra_predictors_mby_s_neon(MACROBLOCKD *x);

extern void vp8_build_intra_predictors_mbuv(MACROBLOCKD *x);
extern void vp8_build_intra_predictors_mbuv_s(MACROBLOCKD *x);

extern void vp8_predict_intra4x4(BLOCKD *x, int b_mode, unsigned char *Predictor);

#endif
