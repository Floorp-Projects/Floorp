/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_RECONINTRA_MT_H
#define __INC_RECONINTRA_MT_H

/* reconintra functions used in multi-threaded decoder */
#if CONFIG_MULTITHREAD
extern void vp8mt_build_intra_predictors_mby(VP8D_COMP *pbi, MACROBLOCKD *x, int mb_row, int mb_col);
extern void vp8mt_build_intra_predictors_mby_s(VP8D_COMP *pbi, MACROBLOCKD *x, int mb_row, int mb_col);
extern void vp8mt_build_intra_predictors_mbuv(VP8D_COMP *pbi, MACROBLOCKD *x, int mb_row, int mb_col);
extern void vp8mt_build_intra_predictors_mbuv_s(VP8D_COMP *pbi, MACROBLOCKD *x, int mb_row, int mb_col);

extern void vp8mt_predict_intra4x4(VP8D_COMP *pbi, MACROBLOCKD *x, int b_mode, unsigned char *predictor, int stride, int mb_row, int mb_col, int num);
extern void vp8mt_intra_prediction_down_copy(VP8D_COMP *pbi, MACROBLOCKD *x, int mb_row, int mb_col);
#endif

#endif
