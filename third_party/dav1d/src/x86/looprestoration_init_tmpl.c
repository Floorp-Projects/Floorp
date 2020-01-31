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
#include "src/tables.h"

// Future potential optimizations:
// - special chroma versions which don't filter [0]/[6];
// - running filter_h_avx2 transposed (one col of 32 pixels per iteration, top
//   to bottom) instead of scanline-ordered should be faster since then the
//   if (have_left) and similar conditions run only once instead of per line;
// - filter_v_avx2 currently runs 16 pixels per iteration, it should be possible
//   to run 32 (like filter_h_avx2), and then all vpermqs can go;
// - maybe split out the top/bottom filter_h_avx2 from the main body filter_h_avx2,
//   since then the have_left condition can be inlined;
// - consider having the wrapper (wiener_filter_${ext}) also in hand-written
//   assembly, so the setup overhead is minimized.

#define WIENER_FILTER(ext) \
\
void dav1d_wiener_filter_h_##ext(int16_t *dst, const pixel (*left)[4], \
                                 const pixel *src, ptrdiff_t stride, \
                                 const int16_t fh[7], const intptr_t w, \
                                 int h, enum LrEdgeFlags edges); \
void dav1d_wiener_filter_v_##ext(pixel *dst, ptrdiff_t stride, \
                                 const int16_t *mid, int w, int h, \
                                 const int16_t fv[7], enum LrEdgeFlags edges); \
\
static void wiener_filter_##ext(pixel *const dst, const ptrdiff_t dst_stride, \
                                const pixel (*const left)[4], \
                                const pixel *lpf, const ptrdiff_t lpf_stride, \
                                const int w, const int h, const int16_t fh[7], \
                                const int16_t fv[7], const enum LrEdgeFlags edges) \
{ \
    ALIGN_STK_32(int16_t, mid, 68 * 384,); \
\
    /* horizontal filter */ \
    dav1d_wiener_filter_h_##ext(&mid[2 * 384], left, dst, dst_stride, \
                               fh, w, h, edges); \
    if (edges & LR_HAVE_TOP) \
        dav1d_wiener_filter_h_##ext(mid, NULL, lpf, lpf_stride, \
                                   fh, w, 2, edges); \
    if (edges & LR_HAVE_BOTTOM) \
        dav1d_wiener_filter_h_##ext(&mid[(2 + h) * 384], NULL, \
                                   lpf + 6 * PXSTRIDE(lpf_stride), lpf_stride, \
                                   fh, w, 2, edges); \
\
    dav1d_wiener_filter_v_##ext(dst, dst_stride, &mid[2*384], w, h, fv, edges); \
}

#define SGR_FILTER(ext) \
\
void dav1d_sgr_box3_h_##ext(int32_t *sumsq, int16_t *sum, \
                            const pixel (*left)[4], \
                            const pixel *src, const ptrdiff_t stride, \
                            const int w, const int h, \
                            const enum LrEdgeFlags edges); \
void dav1d_sgr_box3_v_##ext(int32_t *sumsq, int16_t *sum, \
                            const int w, const int h, \
                            const enum LrEdgeFlags edges); \
void dav1d_sgr_calc_ab1_##ext(int32_t *a, int16_t *b, \
                              const int w, const int h, const int strength); \
void dav1d_sgr_finish_filter1_##ext(coef *tmp, \
                                    const pixel *src, const ptrdiff_t stride, \
                                    const int32_t *a, const int16_t *b, \
                                    const int w, const int h); \
\
/* filter with a 3x3 box (radius=1) */ \
static void dav1d_sgr_filter1_##ext(coef *tmp, \
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
    dav1d_sgr_box3_h_##ext(sumsq, sum, left, src, stride, w, h, edges); \
    if (edges & LR_HAVE_TOP) \
        dav1d_sgr_box3_h_##ext(&sumsq[-2 * (384 + 16)], &sum[-2 * (384 + 16)], \
                              NULL, lpf, lpf_stride, w, 2, edges); \
\
    if (edges & LR_HAVE_BOTTOM) \
        dav1d_sgr_box3_h_##ext(&sumsq[h * (384 + 16)], &sum[h * (384 + 16)], \
                              NULL, lpf + 6 * PXSTRIDE(lpf_stride), \
                              lpf_stride, w, 2, edges); \
\
    dav1d_sgr_box3_v_##ext(sumsq, sum, w, h, edges); \
    dav1d_sgr_calc_ab1_##ext(a, b, w, h, strength); \
    dav1d_sgr_finish_filter1_##ext(tmp, src, stride, a, b, w, h); \
} \
\
void dav1d_sgr_box5_h_##ext(int32_t *sumsq, int16_t *sum, \
                            const pixel (*left)[4], \
                            const pixel *src, const ptrdiff_t stride, \
                            const int w, const int h, \
                            const enum LrEdgeFlags edges); \
void dav1d_sgr_box5_v_##ext(int32_t *sumsq, int16_t *sum, \
                            const int w, const int h, \
                            const enum LrEdgeFlags edges); \
void dav1d_sgr_calc_ab2_##ext(int32_t *a, int16_t *b, \
                              const int w, const int h, const int strength); \
void dav1d_sgr_finish_filter2_##ext(coef *tmp, \
                                    const pixel *src, const ptrdiff_t stride, \
                                    const int32_t *a, const int16_t *b, \
                                    const int w, const int h); \
\
/* filter with a 5x5 box (radius=2) */ \
static void dav1d_sgr_filter2_##ext(coef *tmp, \
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
    dav1d_sgr_box5_h_##ext(sumsq, sum, left, src, stride, w, h, edges); \
    if (edges & LR_HAVE_TOP) \
        dav1d_sgr_box5_h_##ext(&sumsq[-2 * (384 + 16)], &sum[-2 * (384 + 16)], \
                              NULL, lpf, lpf_stride, w, 2, edges); \
\
    if (edges & LR_HAVE_BOTTOM) \
        dav1d_sgr_box5_h_##ext(&sumsq[h * (384 + 16)], &sum[h * (384 + 16)], \
                              NULL, lpf + 6 * PXSTRIDE(lpf_stride), \
                              lpf_stride, w, 2, edges); \
\
    dav1d_sgr_box5_v_##ext(sumsq, sum, w, h, edges); \
    dav1d_sgr_calc_ab2_##ext(a, b, w, h, strength); \
    dav1d_sgr_finish_filter2_##ext(tmp, src, stride, a, b, w, h); \
} \
\
void dav1d_sgr_weighted1_##ext(pixel *dst, const ptrdiff_t stride, \
                               const coef *t1, const int w, const int h, \
                               const int wt); \
void dav1d_sgr_weighted2_##ext(pixel *dst, const ptrdiff_t stride, \
                               const coef *t1, const coef *t2, \
                               const int w, const int h, \
                               const int16_t wt[2]); \
\
static void sgr_filter_##ext(pixel *const dst, const ptrdiff_t dst_stride, \
                             const pixel (*const left)[4], \
                             const pixel *lpf, const ptrdiff_t lpf_stride, \
                             const int w, const int h, const int sgr_idx, \
                             const int16_t sgr_wt[7], const enum LrEdgeFlags edges) \
{ \
    if (!dav1d_sgr_params[sgr_idx][0]) { \
        ALIGN_STK_32(coef, tmp, 64 * 384,); \
        dav1d_sgr_filter1_##ext(tmp, dst, dst_stride, left, lpf, lpf_stride, \
                               w, h, dav1d_sgr_params[sgr_idx][3], edges); \
        dav1d_sgr_weighted1_##ext(dst, dst_stride, tmp, w, h, (1 << 7) - sgr_wt[1]); \
    } else if (!dav1d_sgr_params[sgr_idx][1]) { \
        ALIGN_STK_32(coef, tmp, 64 * 384,); \
        dav1d_sgr_filter2_##ext(tmp, dst, dst_stride, left, lpf, lpf_stride, \
                               w, h, dav1d_sgr_params[sgr_idx][2], edges); \
        dav1d_sgr_weighted1_##ext(dst, dst_stride, tmp, w, h, sgr_wt[0]); \
    } else { \
        ALIGN_STK_32(coef, tmp1, 64 * 384,); \
        ALIGN_STK_32(coef, tmp2, 64 * 384,); \
        dav1d_sgr_filter2_##ext(tmp1, dst, dst_stride, left, lpf, lpf_stride, \
                               w, h, dav1d_sgr_params[sgr_idx][2], edges); \
        dav1d_sgr_filter1_##ext(tmp2, dst, dst_stride, left, lpf, lpf_stride, \
                               w, h, dav1d_sgr_params[sgr_idx][3], edges); \
        const int16_t wt[2] = { sgr_wt[0], 128 - sgr_wt[0] - sgr_wt[1] }; \
        dav1d_sgr_weighted2_##ext(dst, dst_stride, tmp1, tmp2, w, h, wt); \
    } \
}

#define DEF_LR_FILTERS(ext) \
WIENER_FILTER(ext) \
SGR_FILTER(ext)

#if BITDEPTH == 8
WIENER_FILTER(sse2)
DEF_LR_FILTERS(ssse3)
# if ARCH_X86_64
DEF_LR_FILTERS(avx2)
# endif
#endif

COLD void bitfn(dav1d_loop_restoration_dsp_init_x86)(Dav1dLoopRestorationDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();

    if (!(flags & DAV1D_X86_CPU_FLAG_SSE2)) return;
#if BITDEPTH == 8
    c->wiener = wiener_filter_sse2;
#endif

    if (!(flags & DAV1D_X86_CPU_FLAG_SSSE3)) return;
#if BITDEPTH == 8
    c->wiener = wiener_filter_ssse3;
    c->selfguided = sgr_filter_ssse3;
#endif

    if (!(flags & DAV1D_X86_CPU_FLAG_AVX2)) return;
#if BITDEPTH == 8 && ARCH_X86_64
    c->wiener = wiener_filter_avx2;
    c->selfguided = sgr_filter_avx2;
#endif
}
