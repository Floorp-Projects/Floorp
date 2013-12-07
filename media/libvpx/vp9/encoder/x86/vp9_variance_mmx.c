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
#include "vp9/encoder/vp9_variance.h"
#include "vp9/common/vp9_pragmas.h"
#include "vpx_ports/mem.h"

extern unsigned int vp9_get_mb_ss_mmx(const int16_t *src_ptr);
extern unsigned int vp9_get8x8var_mmx
(
  const unsigned char *src_ptr,
  int  source_stride,
  const unsigned char *ref_ptr,
  int  recon_stride,
  unsigned int *SSE,
  int *Sum
);
extern unsigned int vp9_get4x4var_mmx
(
  const unsigned char *src_ptr,
  int  source_stride,
  const unsigned char *ref_ptr,
  int  recon_stride,
  unsigned int *SSE,
  int *Sum
);

unsigned int vp9_variance4x4_mmx(
  const unsigned char *src_ptr,
  int  source_stride,
  const unsigned char *ref_ptr,
  int  recon_stride,
  unsigned int *sse) {
  unsigned int var;
  int avg;

  vp9_get4x4var_mmx(src_ptr, source_stride, ref_ptr, recon_stride, &var, &avg);
  *sse = var;
  return (var - (((unsigned int)avg * avg) >> 4));
}

unsigned int vp9_variance8x8_mmx(
  const unsigned char *src_ptr,
  int  source_stride,
  const unsigned char *ref_ptr,
  int  recon_stride,
  unsigned int *sse) {
  unsigned int var;
  int avg;

  vp9_get8x8var_mmx(src_ptr, source_stride, ref_ptr, recon_stride, &var, &avg);
  *sse = var;

  return (var - (((unsigned int)avg * avg) >> 6));
}

unsigned int vp9_mse16x16_mmx(
  const unsigned char *src_ptr,
  int  source_stride,
  const unsigned char *ref_ptr,
  int  recon_stride,
  unsigned int *sse) {
  unsigned int sse0, sse1, sse2, sse3, var;
  int sum0, sum1, sum2, sum3;


  vp9_get8x8var_mmx(src_ptr, source_stride, ref_ptr, recon_stride, &sse0,
                    &sum0);
  vp9_get8x8var_mmx(src_ptr + 8, source_stride, ref_ptr + 8, recon_stride,
                    &sse1, &sum1);
  vp9_get8x8var_mmx(src_ptr + 8 * source_stride, source_stride,
                    ref_ptr + 8 * recon_stride, recon_stride, &sse2, &sum2);
  vp9_get8x8var_mmx(src_ptr + 8 * source_stride + 8, source_stride,
                    ref_ptr + 8 * recon_stride + 8, recon_stride, &sse3, &sum3);

  var = sse0 + sse1 + sse2 + sse3;
  *sse = var;
  return var;
}


unsigned int vp9_variance16x16_mmx(
  const unsigned char *src_ptr,
  int  source_stride,
  const unsigned char *ref_ptr,
  int  recon_stride,
  unsigned int *sse) {
  unsigned int sse0, sse1, sse2, sse3, var;
  int sum0, sum1, sum2, sum3, avg;

  vp9_get8x8var_mmx(src_ptr, source_stride, ref_ptr, recon_stride, &sse0,
                    &sum0);
  vp9_get8x8var_mmx(src_ptr + 8, source_stride, ref_ptr + 8, recon_stride,
                    &sse1, &sum1);
  vp9_get8x8var_mmx(src_ptr + 8 * source_stride, source_stride,
                    ref_ptr + 8 * recon_stride, recon_stride, &sse2, &sum2);
  vp9_get8x8var_mmx(src_ptr + 8 * source_stride + 8, source_stride,
                    ref_ptr + 8 * recon_stride + 8, recon_stride, &sse3, &sum3);

  var = sse0 + sse1 + sse2 + sse3;
  avg = sum0 + sum1 + sum2 + sum3;
  *sse = var;
  return (var - (((unsigned int)avg * avg) >> 8));
}

unsigned int vp9_variance16x8_mmx(
  const unsigned char *src_ptr,
  int  source_stride,
  const unsigned char *ref_ptr,
  int  recon_stride,
  unsigned int *sse) {
  unsigned int sse0, sse1, var;
  int sum0, sum1, avg;

  vp9_get8x8var_mmx(src_ptr, source_stride, ref_ptr, recon_stride, &sse0,
                    &sum0);
  vp9_get8x8var_mmx(src_ptr + 8, source_stride, ref_ptr + 8, recon_stride,
                    &sse1, &sum1);

  var = sse0 + sse1;
  avg = sum0 + sum1;
  *sse = var;
  return (var - (((unsigned int)avg * avg) >> 7));
}


unsigned int vp9_variance8x16_mmx(
  const unsigned char *src_ptr,
  int  source_stride,
  const unsigned char *ref_ptr,
  int  recon_stride,
  unsigned int *sse) {
  unsigned int sse0, sse1, var;
  int sum0, sum1, avg;

  vp9_get8x8var_mmx(src_ptr, source_stride, ref_ptr, recon_stride, &sse0,
                    &sum0);
  vp9_get8x8var_mmx(src_ptr + 8 * source_stride, source_stride,
                    ref_ptr + 8 * recon_stride, recon_stride, &sse1, &sum1);

  var = sse0 + sse1;
  avg = sum0 + sum1;
  *sse = var;

  return (var - (((unsigned int)avg * avg) >> 7));
}
