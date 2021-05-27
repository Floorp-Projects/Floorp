/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "src/cpu.h"
#include "src/looprestoration.h"

#include "common/intops.h"

#define decl_wiener_filter_fns(ext) \
decl_lr_filter_fn(BF(dav1d_wiener_filter7, ext)); \
decl_lr_filter_fn(BF(dav1d_wiener_filter5, ext))

#define decl_sgr_filter_fns(ext) \
decl_lr_filter_fn(BF(dav1d_sgr_filter_5x5, ext)); \
decl_lr_filter_fn(BF(dav1d_sgr_filter_3x3, ext)); \
decl_lr_filter_fn(BF(dav1d_sgr_filter_mix, ext))

/* FIXME: Replace with a port of the AVX2 code */
#define SGR_FILTER_OLD(ext) \
void BF(dav1d_sgr_box3_h, ext)(int32_t *sumsq, int16_t *sum, \
                               const pixel (*left)[4], \
                               const pixel *src, const ptrdiff_t stride, \
                               const int w, const int h, \
                               const enum LrEdgeFlags edges); \
void BF(dav1d_sgr_box3_v, ext)(int32_t *sumsq, int16_t *sum, \
                               const int w, const int h, \
                               const enum LrEdgeFlags edges); \
void BF(dav1d_sgr_calc_ab1, ext)(int32_t *a, int16_t *b, \
                                 const int w, const int h, const unsigned s); \
void BF(dav1d_sgr_finish_filter1, ext)(int16_t *tmp, \
                                       const pixel *src, const ptrdiff_t stride, \
                                       const int32_t *a, const int16_t *b, \
                                       const int w, const int h); \
\
/* filter with a 3x3 box (radius=1) */ \
static void BF(dav1d_sgr_filter1, ext)(int16_t *tmp, \
                                       const pixel *src, const ptrdiff_t stride, \
                                       const pixel (*left)[4], \
                                       const pixel *lpf, const ptrdiff_t lpf_stride, \
                                       const int w, const int h, const int strength, \
                                       const enum LrEdgeFlags edges) \
{ \
    ALIGN_STK_32(int32_t, sumsq_mem, (384 + 16) * 68 + 8,); \
    int32_t *const sumsq = &sumsq_mem[(384 + 16) * 2 + 8], *const a = sumsq; \
    ALIGN_STK_32(int16_t, sum_mem, (384 + 16) * 68 + 16,); \
    int16_t *const sum = &sum_mem[(384 + 16) * 2 + 16], *const b = sum; \
\
    BF(dav1d_sgr_box3_h, ext)(sumsq, sum, left, src, stride, w, h, edges); \
    if (edges & LR_HAVE_TOP) \
        BF(dav1d_sgr_box3_h, ext)(&sumsq[-2 * (384 + 16)], &sum[-2 * (384 + 16)], \
                                  NULL, lpf, lpf_stride, w, 2, edges); \
\
    if (edges & LR_HAVE_BOTTOM) \
        BF(dav1d_sgr_box3_h, ext)(&sumsq[h * (384 + 16)], &sum[h * (384 + 16)], \
                                  NULL, lpf + 6 * PXSTRIDE(lpf_stride), \
                                  lpf_stride, w, 2, edges); \
\
    BF(dav1d_sgr_box3_v, ext)(sumsq, sum, w, h, edges); \
    BF(dav1d_sgr_calc_ab1, ext)(a, b, w, h, strength); \
    BF(dav1d_sgr_finish_filter1, ext)(tmp, src, stride, a, b, w, h); \
} \
\
void BF(dav1d_sgr_box5_h, ext)(int32_t *sumsq, int16_t *sum, \
                               const pixel (*left)[4], \
                               const pixel *src, const ptrdiff_t stride, \
                               const int w, const int h, \
                               const enum LrEdgeFlags edges); \
void BF(dav1d_sgr_box5_v, ext)(int32_t *sumsq, int16_t *sum, \
                               const int w, const int h, \
                               const enum LrEdgeFlags edges); \
void BF(dav1d_sgr_calc_ab2, ext)(int32_t *a, int16_t *b, \
                                 const int w, const int h, const int strength); \
void BF(dav1d_sgr_finish_filter2, ext)(int16_t *tmp, \
                                       const pixel *src, const ptrdiff_t stride, \
                                       const int32_t *a, const int16_t *b, \
                                       const int w, const int h); \
\
/* filter with a 5x5 box (radius=2) */ \
static void BF(dav1d_sgr_filter2, ext)(int16_t *tmp, \
                                       const pixel *src, const ptrdiff_t stride, \
                                       const pixel (*left)[4], \
                                       const pixel *lpf, const ptrdiff_t lpf_stride, \
                                       const int w, const int h, const int strength, \
                                       const enum LrEdgeFlags edges) \
{ \
    ALIGN_STK_32(int32_t, sumsq_mem, (384 + 16) * 68 + 8,); \
    int32_t *const sumsq = &sumsq_mem[(384 + 16) * 2 + 8], *const a = sumsq; \
    ALIGN_STK_32(int16_t, sum_mem, (384 + 16) * 68 + 16,); \
    int16_t *const sum = &sum_mem[(384 + 16) * 2 + 16], *const b = sum; \
\
    BF(dav1d_sgr_box5_h, ext)(sumsq, sum, left, src, stride, w, h, edges); \
    if (edges & LR_HAVE_TOP) \
        BF(dav1d_sgr_box5_h, ext)(&sumsq[-2 * (384 + 16)], &sum[-2 * (384 + 16)], \
                                  NULL, lpf, lpf_stride, w, 2, edges); \
\
    if (edges & LR_HAVE_BOTTOM) \
        BF(dav1d_sgr_box5_h, ext)(&sumsq[h * (384 + 16)], &sum[h * (384 + 16)], \
                                  NULL, lpf + 6 * PXSTRIDE(lpf_stride), \
                                  lpf_stride, w, 2, edges); \
\
    BF(dav1d_sgr_box5_v, ext)(sumsq, sum, w, h, edges); \
    BF(dav1d_sgr_calc_ab2, ext)(a, b, w, h, strength); \
    BF(dav1d_sgr_finish_filter2, ext)(tmp, src, stride, a, b, w, h); \
} \
\
void BF(dav1d_sgr_weighted1, ext)(pixel *dst, const ptrdiff_t stride, \
                                  const int16_t *t1, const int w, const int h, \
                                  const int wt); \
void BF(dav1d_sgr_weighted2, ext)(pixel *dst, const ptrdiff_t stride, \
                                  const int16_t *t1, const int16_t *t2, \
                                  const int w, const int h, \
                                  const uint32_t wt); \
\
static void BF(sgr_filter_5x5, ext)(pixel *const dst, const ptrdiff_t dst_stride, \
                                    const pixel (*const left)[4], \
                                    const pixel *lpf, const ptrdiff_t lpf_stride, \
                                    const int w, const int h, \
                                    const LooprestorationParams *const params, \
                                    const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX) \
{ \
    ALIGN_STK_32(int16_t, tmp, 64 * 384,); \
    BF(dav1d_sgr_filter2, ext)(tmp, dst, dst_stride, left, lpf, lpf_stride, \
                               w, h, params->sgr.s0, edges); \
    BF(dav1d_sgr_weighted1, ext)(dst, dst_stride, tmp, w, h, params->sgr.w0); \
} \
static void BF(sgr_filter_3x3, ext)(pixel *const dst, const ptrdiff_t dst_stride, \
                                    const pixel (*const left)[4], \
                                    const pixel *lpf, const ptrdiff_t lpf_stride, \
                                    const int w, const int h, \
                                    const LooprestorationParams *const params, \
                                    const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX) \
{ \
    ALIGN_STK_32(int16_t, tmp, 64 * 384,); \
    BF(dav1d_sgr_filter1, ext)(tmp, dst, dst_stride, left, lpf, lpf_stride, \
                               w, h, params->sgr.s1, edges); \
    BF(dav1d_sgr_weighted1, ext)(dst, dst_stride, tmp, w, h, params->sgr.w1); \
} \
static void BF(sgr_filter_mix, ext)(pixel *const dst, const ptrdiff_t dst_stride, \
                                    const pixel (*const left)[4], \
                                    const pixel *lpf, const ptrdiff_t lpf_stride, \
                                    const int w, const int h, \
                                    const LooprestorationParams *const params, \
                                    const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX) \
{ \
    ALIGN_STK_32(int16_t, tmp1, 64 * 384,); \
    ALIGN_STK_32(int16_t, tmp2, 64 * 384,); \
    BF(dav1d_sgr_filter2, ext)(tmp1, dst, dst_stride, left, lpf, lpf_stride, \
                               w, h, params->sgr.s0, edges); \
    BF(dav1d_sgr_filter1, ext)(tmp2, dst, dst_stride, left, lpf, lpf_stride, \
                               w, h, params->sgr.s1, edges); \
    const uint32_t wt = (params->sgr.w1 << 16) | (uint16_t) params->sgr.w0; \
    BF(dav1d_sgr_weighted2, ext)(dst, dst_stride, tmp1, tmp2, w, h, wt); \
}

decl_wiener_filter_fns(sse2);
decl_wiener_filter_fns(ssse3);
decl_wiener_filter_fns(avx2);
decl_sgr_filter_fns(avx2);

#if BITDEPTH == 8
SGR_FILTER_OLD(ssse3)
#endif

COLD void bitfn(dav1d_loop_restoration_dsp_init_x86)(Dav1dLoopRestorationDSPContext *const c,
                                                     const int bpc)
{
    const unsigned flags = dav1d_get_cpu_flags();

    if (!(flags & DAV1D_X86_CPU_FLAG_SSE2)) return;
#if BITDEPTH == 8
    c->wiener[0] = BF(dav1d_wiener_filter7, sse2);
    c->wiener[1] = BF(dav1d_wiener_filter5, sse2);
#endif

    if (!(flags & DAV1D_X86_CPU_FLAG_SSSE3)) return;
#if BITDEPTH == 8
    c->wiener[0] = BF(dav1d_wiener_filter7, ssse3);
    c->wiener[1] = BF(dav1d_wiener_filter5, ssse3);
    c->sgr[0] = BF(sgr_filter_5x5, ssse3);
    c->sgr[1] = BF(sgr_filter_3x3, ssse3);
    c->sgr[2] = BF(sgr_filter_mix, ssse3);
#endif

#if ARCH_X86_64
    if (!(flags & DAV1D_X86_CPU_FLAG_AVX2)) return;

    c->wiener[0] = BF(dav1d_wiener_filter7, avx2);
    c->wiener[1] = BF(dav1d_wiener_filter5, avx2);
    if (bpc <= 10) {
        c->sgr[0] = BF(dav1d_sgr_filter_5x5, avx2);
        c->sgr[1] = BF(dav1d_sgr_filter_3x3, avx2);
        c->sgr[2] = BF(dav1d_sgr_filter_mix, avx2);
    }
#endif
}
