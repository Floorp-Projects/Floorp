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

extern unsigned int vp9_get4x4var_mmx
(
  const unsigned char *src_ptr,
  int  source_stride,
  const unsigned char *ref_ptr,
  int  recon_stride,
  unsigned int *SSE,
  int *Sum
);

unsigned int vp9_get_mb_ss_sse2
(
  const int16_t *src_ptr
);
unsigned int vp9_get16x16var_sse2
(
  const unsigned char *src_ptr,
  int source_stride,
  const unsigned char *ref_ptr,
  int recon_stride,
  unsigned int *SSE,
  int *Sum
);
unsigned int vp9_get8x8var_sse2
(
  const unsigned char *src_ptr,
  int source_stride,
  const unsigned char *ref_ptr,
  int recon_stride,
  unsigned int *SSE,
  int *Sum
);
void vp9_half_horiz_vert_variance8x_h_sse2
(
  const unsigned char *ref_ptr,
  int ref_pixels_per_line,
  const unsigned char *src_ptr,
  int src_pixels_per_line,
  unsigned int Height,
  int *sum,
  unsigned int *sumsquared
);
void vp9_half_horiz_vert_variance16x_h_sse2
(
  const unsigned char *ref_ptr,
  int ref_pixels_per_line,
  const unsigned char *src_ptr,
  int src_pixels_per_line,
  unsigned int Height,
  int *sum,
  unsigned int *sumsquared
);
void vp9_half_horiz_variance8x_h_sse2
(
  const unsigned char *ref_ptr,
  int ref_pixels_per_line,
  const unsigned char *src_ptr,
  int src_pixels_per_line,
  unsigned int Height,
  int *sum,
  unsigned int *sumsquared
);
void vp9_half_horiz_variance16x_h_sse2
(
  const unsigned char *ref_ptr,
  int ref_pixels_per_line,
  const unsigned char *src_ptr,
  int src_pixels_per_line,
  unsigned int Height,
  int *sum,
  unsigned int *sumsquared
);
void vp9_half_vert_variance8x_h_sse2
(
  const unsigned char *ref_ptr,
  int ref_pixels_per_line,
  const unsigned char *src_ptr,
  int src_pixels_per_line,
  unsigned int Height,
  int *sum,
  unsigned int *sumsquared
);
void vp9_half_vert_variance16x_h_sse2
(
  const unsigned char *ref_ptr,
  int ref_pixels_per_line,
  const unsigned char *src_ptr,
  int src_pixels_per_line,
  unsigned int Height,
  int *sum,
  unsigned int *sumsquared
);

typedef unsigned int (*get_var_sse2) (
  const unsigned char *src_ptr,
  int source_stride,
  const unsigned char *ref_ptr,
  int recon_stride,
  unsigned int *SSE,
  int *Sum
);

static void variance_sse2(const unsigned char *src_ptr, int  source_stride,
                        const unsigned char *ref_ptr, int  recon_stride,
                        int  w, int  h, unsigned int *sse, int *sum,
                        get_var_sse2 var_fn, int block_size) {
  unsigned int sse0;
  int sum0;
  int i, j;

  *sse = 0;
  *sum = 0;

  for (i = 0; i < h; i += block_size) {
    for (j = 0; j < w; j += block_size) {
      var_fn(src_ptr + source_stride * i + j, source_stride,
             ref_ptr + recon_stride * i + j, recon_stride, &sse0, &sum0);
      *sse += sse0;
      *sum += sum0;
    }
  }
}

unsigned int vp9_variance4x4_sse2(
  const unsigned char *src_ptr,
  int  source_stride,
  const unsigned char *ref_ptr,
  int  recon_stride,
  unsigned int *sse) {
  unsigned int var;
  int avg;

  variance_sse2(src_ptr, source_stride, ref_ptr, recon_stride, 4, 4,
                  &var, &avg, vp9_get4x4var_mmx, 4);
  *sse = var;
  return (var - (((unsigned int)avg * avg) >> 4));
}

unsigned int vp9_variance8x4_sse2(const uint8_t *src_ptr,
                                  int  source_stride,
                                  const uint8_t *ref_ptr,
                                  int  recon_stride,
                                  unsigned int *sse) {
  unsigned int var;
  int avg;

  variance_sse2(src_ptr, source_stride, ref_ptr, recon_stride, 8, 4,
                  &var, &avg, vp9_get4x4var_mmx, 4);
  *sse = var;
  return (var - (((unsigned int)avg * avg) >> 5));
}

unsigned int vp9_variance4x8_sse2(const uint8_t *src_ptr,
                                  int  source_stride,
                                  const uint8_t *ref_ptr,
                                  int  recon_stride,
                                  unsigned int *sse) {
  unsigned int var;
  int avg;

  variance_sse2(src_ptr, source_stride, ref_ptr, recon_stride, 4, 8,
                  &var, &avg, vp9_get4x4var_mmx, 4);
  *sse = var;
  return (var - (((unsigned int)avg * avg) >> 5));
}

unsigned int vp9_variance8x8_sse2
(
  const unsigned char *src_ptr,
  int  source_stride,
  const unsigned char *ref_ptr,
  int  recon_stride,
  unsigned int *sse) {
  unsigned int var;
  int avg;

  variance_sse2(src_ptr, source_stride, ref_ptr, recon_stride, 8, 8,
                  &var, &avg, vp9_get8x8var_sse2, 8);
  *sse = var;
  return (var - (((unsigned int)avg * avg) >> 6));
}

unsigned int vp9_variance16x8_sse2
(
  const unsigned char *src_ptr,
  int  source_stride,
  const unsigned char *ref_ptr,
  int  recon_stride,
  unsigned int *sse) {
  unsigned int var;
  int avg;

  variance_sse2(src_ptr, source_stride, ref_ptr, recon_stride, 16, 8,
                  &var, &avg, vp9_get8x8var_sse2, 8);
  *sse = var;
  return (var - (((unsigned int)avg * avg) >> 7));
}

unsigned int vp9_variance8x16_sse2
(
  const unsigned char *src_ptr,
  int  source_stride,
  const unsigned char *ref_ptr,
  int  recon_stride,
  unsigned int *sse) {
  unsigned int var;
  int avg;

  variance_sse2(src_ptr, source_stride, ref_ptr, recon_stride, 8, 16,
                &var, &avg, vp9_get8x8var_sse2, 8);
  *sse = var;
  return (var - (((unsigned int)avg * avg) >> 7));
}

unsigned int vp9_variance16x16_sse2
(
  const unsigned char *src_ptr,
  int  source_stride,
  const unsigned char *ref_ptr,
  int  recon_stride,
  unsigned int *sse) {
  unsigned int var;
  int avg;

  variance_sse2(src_ptr, source_stride, ref_ptr, recon_stride, 16, 16,
                &var, &avg, vp9_get16x16var_sse2, 16);
  *sse = var;
  return (var - (((unsigned int)avg * avg) >> 8));
}

unsigned int vp9_mse16x16_sse2(
  const unsigned char *src_ptr,
  int  source_stride,
  const unsigned char *ref_ptr,
  int  recon_stride,
  unsigned int *sse) {
  unsigned int sse0;
  int sum0;
  vp9_get16x16var_sse2(src_ptr, source_stride, ref_ptr, recon_stride, &sse0,
                       &sum0);
  *sse = sse0;
  return sse0;
}

unsigned int vp9_variance32x32_sse2(const uint8_t *src_ptr,
                                    int  source_stride,
                                    const uint8_t *ref_ptr,
                                    int  recon_stride,
                                    unsigned int *sse) {
  unsigned int var;
  int avg;

  variance_sse2(src_ptr, source_stride, ref_ptr, recon_stride, 32, 32,
                &var, &avg, vp9_get16x16var_sse2, 16);
  *sse = var;
  return (var - (((int64_t)avg * avg) >> 10));
}

unsigned int vp9_variance32x16_sse2(const uint8_t *src_ptr,
                                    int  source_stride,
                                    const uint8_t *ref_ptr,
                                    int  recon_stride,
                                    unsigned int *sse) {
  unsigned int var;
  int avg;

  variance_sse2(src_ptr, source_stride, ref_ptr, recon_stride, 32, 16,
                &var, &avg, vp9_get16x16var_sse2, 16);
  *sse = var;
  return (var - (((int64_t)avg * avg) >> 9));
}

unsigned int vp9_variance16x32_sse2(const uint8_t *src_ptr,
                                    int  source_stride,
                                    const uint8_t *ref_ptr,
                                    int  recon_stride,
                                    unsigned int *sse) {
  unsigned int var;
  int avg;

  variance_sse2(src_ptr, source_stride, ref_ptr, recon_stride, 16, 32,
                &var, &avg, vp9_get16x16var_sse2, 16);
  *sse = var;
  return (var - (((int64_t)avg * avg) >> 9));
}

unsigned int vp9_variance64x64_sse2(const uint8_t *src_ptr,
                                    int  source_stride,
                                    const uint8_t *ref_ptr,
                                    int  recon_stride,
                                    unsigned int *sse) {
  unsigned int var;
  int avg;

  variance_sse2(src_ptr, source_stride, ref_ptr, recon_stride, 64, 64,
                &var, &avg, vp9_get16x16var_sse2, 16);
  *sse = var;
  return (var - (((int64_t)avg * avg) >> 12));
}

unsigned int vp9_variance64x32_sse2(const uint8_t *src_ptr,
                                    int  source_stride,
                                    const uint8_t *ref_ptr,
                                    int  recon_stride,
                                    unsigned int *sse) {
  unsigned int var;
  int avg;

  variance_sse2(src_ptr, source_stride, ref_ptr, recon_stride, 64, 32,
                &var, &avg, vp9_get16x16var_sse2, 16);
  *sse = var;
  return (var - (((int64_t)avg * avg) >> 11));
}

unsigned int vp9_variance32x64_sse2(const uint8_t *src_ptr,
                                    int  source_stride,
                                    const uint8_t *ref_ptr,
                                    int  recon_stride,
                                    unsigned int *sse) {
  unsigned int var;
  int avg;

  variance_sse2(src_ptr, source_stride, ref_ptr, recon_stride, 32, 64,
                &var, &avg, vp9_get16x16var_sse2, 16);
  *sse = var;
  return (var - (((int64_t)avg * avg) >> 11));
}

#define DECL(w, opt) \
int vp9_sub_pixel_variance##w##xh_##opt(const uint8_t *src, \
                                        ptrdiff_t src_stride, \
                                        int x_offset, int y_offset, \
                                        const uint8_t *dst, \
                                        ptrdiff_t dst_stride, \
                                        int height, unsigned int *sse)
#define DECLS(opt1, opt2) \
DECL(4, opt2); \
DECL(8, opt1); \
DECL(16, opt1)

DECLS(sse2, sse);
DECLS(ssse3, ssse3);
#undef DECLS
#undef DECL

#define FN(w, h, wf, wlog2, hlog2, opt, cast) \
unsigned int vp9_sub_pixel_variance##w##x##h##_##opt(const uint8_t *src, \
                                                     int src_stride, \
                                                     int x_offset, \
                                                     int y_offset, \
                                                     const uint8_t *dst, \
                                                     int dst_stride, \
                                                     unsigned int *sse_ptr) { \
  unsigned int sse; \
  int se = vp9_sub_pixel_variance##wf##xh_##opt(src, src_stride, x_offset, \
                                                y_offset, dst, dst_stride, \
                                                h, &sse); \
  if (w > wf) { \
    unsigned int sse2; \
    int se2 = vp9_sub_pixel_variance##wf##xh_##opt(src + 16, src_stride, \
                                                   x_offset, y_offset, \
                                                   dst + 16, dst_stride, \
                                                   h, &sse2); \
    se += se2; \
    sse += sse2; \
    if (w > wf * 2) { \
      se2 = vp9_sub_pixel_variance##wf##xh_##opt(src + 32, src_stride, \
                                                 x_offset, y_offset, \
                                                 dst + 32, dst_stride, \
                                                 h, &sse2); \
      se += se2; \
      sse += sse2; \
      se2 = vp9_sub_pixel_variance##wf##xh_##opt(src + 48, src_stride, \
                                                 x_offset, y_offset, \
                                                 dst + 48, dst_stride, \
                                                 h, &sse2); \
      se += se2; \
      sse += sse2; \
    } \
  } \
  *sse_ptr = sse; \
  return sse - ((cast se * se) >> (wlog2 + hlog2)); \
}

#define FNS(opt1, opt2) \
FN(64, 64, 16, 6, 6, opt1, (int64_t)); \
FN(64, 32, 16, 6, 5, opt1, (int64_t)); \
FN(32, 64, 16, 5, 6, opt1, (int64_t)); \
FN(32, 32, 16, 5, 5, opt1, (int64_t)); \
FN(32, 16, 16, 5, 4, opt1, (int64_t)); \
FN(16, 32, 16, 4, 5, opt1, (int64_t)); \
FN(16, 16, 16, 4, 4, opt1, (unsigned int)); \
FN(16,  8, 16, 4, 3, opt1, (unsigned int)); \
FN(8,  16,  8, 3, 4, opt1, (unsigned int)); \
FN(8,   8,  8, 3, 3, opt1, (unsigned int)); \
FN(8,   4,  8, 3, 2, opt1, (unsigned int)); \
FN(4,   8,  4, 2, 3, opt2, (unsigned int)); \
FN(4,   4,  4, 2, 2, opt2, (unsigned int))

FNS(sse2, sse);
FNS(ssse3, ssse3);

#undef FNS
#undef FN

#define DECL(w, opt) \
int vp9_sub_pixel_avg_variance##w##xh_##opt(const uint8_t *src, \
                                            ptrdiff_t src_stride, \
                                            int x_offset, int y_offset, \
                                            const uint8_t *dst, \
                                            ptrdiff_t dst_stride, \
                                            const uint8_t *sec, \
                                            ptrdiff_t sec_stride, \
                                            int height, unsigned int *sse)
#define DECLS(opt1, opt2) \
DECL(4, opt2); \
DECL(8, opt1); \
DECL(16, opt1)

DECLS(sse2, sse);
DECLS(ssse3, ssse3);
#undef DECL
#undef DECLS

#define FN(w, h, wf, wlog2, hlog2, opt, cast) \
unsigned int vp9_sub_pixel_avg_variance##w##x##h##_##opt(const uint8_t *src, \
                                                         int src_stride, \
                                                         int x_offset, \
                                                         int y_offset, \
                                                         const uint8_t *dst, \
                                                         int dst_stride, \
                                                         unsigned int *sseptr, \
                                                         const uint8_t *sec) { \
  unsigned int sse; \
  int se = vp9_sub_pixel_avg_variance##wf##xh_##opt(src, src_stride, x_offset, \
                                                    y_offset, dst, dst_stride, \
                                                    sec, w, h, &sse); \
  if (w > wf) { \
    unsigned int sse2; \
    int se2 = vp9_sub_pixel_avg_variance##wf##xh_##opt(src + 16, src_stride, \
                                                       x_offset, y_offset, \
                                                       dst + 16, dst_stride, \
                                                       sec + 16, w, h, &sse2); \
    se += se2; \
    sse += sse2; \
    if (w > wf * 2) { \
      se2 = vp9_sub_pixel_avg_variance##wf##xh_##opt(src + 32, src_stride, \
                                                     x_offset, y_offset, \
                                                     dst + 32, dst_stride, \
                                                     sec + 32, w, h, &sse2); \
      se += se2; \
      sse += sse2; \
      se2 = vp9_sub_pixel_avg_variance##wf##xh_##opt(src + 48, src_stride, \
                                                     x_offset, y_offset, \
                                                     dst + 48, dst_stride, \
                                                     sec + 48, w, h, &sse2); \
      se += se2; \
      sse += sse2; \
    } \
  } \
  *sseptr = sse; \
  return sse - ((cast se * se) >> (wlog2 + hlog2)); \
}

#define FNS(opt1, opt2) \
FN(64, 64, 16, 6, 6, opt1, (int64_t)); \
FN(64, 32, 16, 6, 5, opt1, (int64_t)); \
FN(32, 64, 16, 5, 6, opt1, (int64_t)); \
FN(32, 32, 16, 5, 5, opt1, (int64_t)); \
FN(32, 16, 16, 5, 4, opt1, (int64_t)); \
FN(16, 32, 16, 4, 5, opt1, (int64_t)); \
FN(16, 16, 16, 4, 4, opt1, (unsigned int)); \
FN(16,  8, 16, 4, 3, opt1, (unsigned int)); \
FN(8,  16,  8, 3, 4, opt1, (unsigned int)); \
FN(8,   8,  8, 3, 3, opt1, (unsigned int)); \
FN(8,   4,  8, 3, 2, opt1, (unsigned int)); \
FN(4,   8,  4, 2, 3, opt2, (unsigned int)); \
FN(4,   4,  4, 2, 2, opt2, (unsigned int))

FNS(sse2, sse);
FNS(ssse3, ssse3);

#undef FNS
#undef FN

unsigned int vp9_variance_halfpixvar16x16_h_sse2(
  const unsigned char *src_ptr,
  int  src_pixels_per_line,
  const unsigned char *dst_ptr,
  int  dst_pixels_per_line,
  unsigned int *sse) {
  int xsum0;
  unsigned int xxsum0;

  vp9_half_horiz_variance16x_h_sse2(
    src_ptr, src_pixels_per_line,
    dst_ptr, dst_pixels_per_line, 16,
    &xsum0, &xxsum0);

  *sse = xxsum0;
  return (xxsum0 - (((unsigned int)xsum0 * xsum0) >> 8));
}


unsigned int vp9_variance_halfpixvar16x16_v_sse2(
  const unsigned char *src_ptr,
  int  src_pixels_per_line,
  const unsigned char *dst_ptr,
  int  dst_pixels_per_line,
  unsigned int *sse) {
  int xsum0;
  unsigned int xxsum0;
  vp9_half_vert_variance16x_h_sse2(
    src_ptr, src_pixels_per_line,
    dst_ptr, dst_pixels_per_line, 16,
    &xsum0, &xxsum0);

  *sse = xxsum0;
  return (xxsum0 - (((unsigned int)xsum0 * xsum0) >> 8));
}


unsigned int vp9_variance_halfpixvar16x16_hv_sse2(
  const unsigned char *src_ptr,
  int  src_pixels_per_line,
  const unsigned char *dst_ptr,
  int  dst_pixels_per_line,
  unsigned int *sse) {
  int xsum0;
  unsigned int xxsum0;

  vp9_half_horiz_vert_variance16x_h_sse2(
    src_ptr, src_pixels_per_line,
    dst_ptr, dst_pixels_per_line, 16,
    &xsum0, &xxsum0);

  *sse = xxsum0;
  return (xxsum0 - (((unsigned int)xsum0 * xsum0) >> 8));
}
