/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vpx_config.h"
#include "vp8_rtcd.h"
#include "vpx_ports/x86.h"
#include "vp8/encoder/block.h"

int vp8_fast_quantize_b_impl_mmx(short *coeff_ptr, short *zbin_ptr,
                                 short *qcoeff_ptr, short *dequant_ptr,
                                 const short *scan_mask, short *round_ptr,
                                 short *quant_ptr, short *dqcoeff_ptr);
void vp8_fast_quantize_b_mmx(BLOCK *b, BLOCKD *d) {
  const short *scan_mask = vp8_default_zig_zag_mask;
  short *coeff_ptr = b->coeff;
  short *zbin_ptr = b->zbin;
  short *round_ptr = b->round;
  short *quant_ptr = b->quant_fast;
  short *qcoeff_ptr = d->qcoeff;
  short *dqcoeff_ptr = d->dqcoeff;
  short *dequant_ptr = d->dequant;

  *d->eob = (char)vp8_fast_quantize_b_impl_mmx(
      coeff_ptr, zbin_ptr, qcoeff_ptr, dequant_ptr, scan_mask,

      round_ptr, quant_ptr, dqcoeff_ptr);
}
