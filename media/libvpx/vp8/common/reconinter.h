/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_RECONINTER_H
#define __INC_RECONINTER_H

extern void vp8_build_inter_predictors_mb(MACROBLOCKD *x);
extern void vp8_build_inter_predictors_mb_s(MACROBLOCKD *x);

extern void vp8_build_inter_predictors_mby(MACROBLOCKD *x);
extern void vp8_build_uvmvs(MACROBLOCKD *x, int fullpixel);
extern void vp8_build_inter_predictors_b(BLOCKD *d, int pitch, vp8_subpix_fn_t sppf);
extern void vp8_build_inter_predictors_mbuv(MACROBLOCKD *x);

#endif
