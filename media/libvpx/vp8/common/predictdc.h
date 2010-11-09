/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __PREDICTDC_H
#define __PREDICTDC_H

void uvvp8_predict_dc(short *lastdc, short *thisdc, short quant, short *cons);
void vp8_predict_dc(short *lastdc, short *thisdc, short quant, short *cons);

#endif
