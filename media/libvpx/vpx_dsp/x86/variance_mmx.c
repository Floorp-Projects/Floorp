/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "./vpx_dsp_rtcd.h"

extern void vpx_get4x4var_mmx(const uint8_t *a, int a_stride,
                              const uint8_t *b, int b_stride,
                              unsigned int *sse, int *sum);

unsigned int vpx_variance4x4_mmx(const unsigned char *a, int  a_stride,
                                 const unsigned char *b, int  b_stride,
                                 unsigned int *sse) {
    unsigned int var;
    int avg;

    vpx_get4x4var_mmx(a, a_stride, b, b_stride, &var, &avg);
    *sse = var;
    return (var - (((unsigned int)avg * avg) >> 4));
}

unsigned int vpx_variance8x8_mmx(const unsigned char *a, int  a_stride,
                                 const unsigned char *b, int  b_stride,
                                 unsigned int *sse) {
    unsigned int var;
    int avg;

    vpx_get8x8var_mmx(a, a_stride, b, b_stride, &var, &avg);
    *sse = var;

    return (var - (((unsigned int)avg * avg) >> 6));
}

unsigned int vpx_mse16x16_mmx(const unsigned char *a, int  a_stride,
                              const unsigned char *b, int  b_stride,
                              unsigned int *sse) {
    unsigned int sse0, sse1, sse2, sse3, var;
    int sum0, sum1, sum2, sum3;

    vpx_get8x8var_mmx(a, a_stride, b, b_stride, &sse0, &sum0);
    vpx_get8x8var_mmx(a + 8, a_stride, b + 8, b_stride, &sse1, &sum1);
    vpx_get8x8var_mmx(a + 8 * a_stride, a_stride,
                      b + 8 * b_stride, b_stride, &sse2, &sum2);
    vpx_get8x8var_mmx(a + 8 * a_stride + 8, a_stride,
                      b + 8 * b_stride + 8, b_stride, &sse3, &sum3);

    var = sse0 + sse1 + sse2 + sse3;
    *sse = var;
    return var;
}

unsigned int vpx_variance16x16_mmx(const unsigned char *a, int  a_stride,
                                   const unsigned char *b, int  b_stride,
                                   unsigned int *sse) {
    unsigned int sse0, sse1, sse2, sse3, var;
    int sum0, sum1, sum2, sum3, avg;

    vpx_get8x8var_mmx(a, a_stride, b, b_stride, &sse0, &sum0);
    vpx_get8x8var_mmx(a + 8, a_stride, b + 8, b_stride, &sse1, &sum1);
    vpx_get8x8var_mmx(a + 8 * a_stride, a_stride,
                      b + 8 * b_stride, b_stride, &sse2, &sum2);
    vpx_get8x8var_mmx(a + 8 * a_stride + 8, a_stride,
                      b + 8 * b_stride + 8, b_stride, &sse3, &sum3);

    var = sse0 + sse1 + sse2 + sse3;
    avg = sum0 + sum1 + sum2 + sum3;
    *sse = var;
    return (var - (((unsigned int)avg * avg) >> 8));
}

unsigned int vpx_variance16x8_mmx(const unsigned char *a, int  a_stride,
                                  const unsigned char *b, int  b_stride,
                                  unsigned int *sse) {
    unsigned int sse0, sse1, var;
    int sum0, sum1, avg;

    vpx_get8x8var_mmx(a, a_stride, b, b_stride, &sse0, &sum0);
    vpx_get8x8var_mmx(a + 8, a_stride, b + 8, b_stride, &sse1, &sum1);

    var = sse0 + sse1;
    avg = sum0 + sum1;
    *sse = var;
    return (var - (((unsigned int)avg * avg) >> 7));
}

unsigned int vpx_variance8x16_mmx(const unsigned char *a, int  a_stride,
                                  const unsigned char *b, int  b_stride,
                                  unsigned int *sse) {
    unsigned int sse0, sse1, var;
    int sum0, sum1, avg;

    vpx_get8x8var_mmx(a, a_stride, b, b_stride, &sse0, &sum0);
    vpx_get8x8var_mmx(a + 8 * a_stride, a_stride,
                      b + 8 * b_stride, b_stride, &sse1, &sum1);

    var = sse0 + sse1;
    avg = sum0 + sum1;
    *sse = var;

    return (var - (((unsigned int)avg * avg) >> 7));
}
