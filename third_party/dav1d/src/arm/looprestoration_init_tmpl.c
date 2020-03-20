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
#include "src/tables.h"

#if BITDEPTH == 8 || ARCH_AARCH64
// The 8bpc version calculates things slightly differently than the reference
// C version. That version calculates roughly this:
// int16_t sum = 0;
// for (int i = 0; i < 7; i++)
//     sum += src[idx] * fh[i];
// int16_t sum2 = (src[x] << 7) - (1 << (bitdepth + 6)) + rounding_off_h;
// sum = iclip(sum + sum2, INT16_MIN, INT16_MAX) >> round_bits_h;
// sum += 1 << (bitdepth + 6 - round_bits_h);
// Compared to the reference C version, this is the output of the first pass
// _subtracted_ by 1 << (bitdepth + 6 - round_bits_h) = 2048, i.e.
// with round_offset precompensated.
// The 16bpc version calculates things pretty much the same way as the
// reference C version, but with the end result subtracted by
// 1 << (bitdepth + 6 - round_bits_h).
void BF(dav1d_wiener_filter_h, neon)(int16_t *dst, const pixel (*left)[4],
                                     const pixel *src, ptrdiff_t stride,
                                     const int16_t fh[7], const intptr_t w,
                                     int h, enum LrEdgeFlags edges
                                     HIGHBD_DECL_SUFFIX);
// This calculates things slightly differently than the reference C version.
// This version calculates roughly this:
// fv[3] += 128;
// int32_t sum = 0;
// for (int i = 0; i < 7; i++)
//     sum += mid[idx] * fv[i];
// sum = (sum + rounding_off_v) >> round_bits_v;
// This function assumes that the width is a multiple of 8.
void BF(dav1d_wiener_filter_v, neon)(pixel *dst, ptrdiff_t stride,
                                     const int16_t *mid, int w, int h,
                                     const int16_t fv[7], enum LrEdgeFlags edges,
                                     ptrdiff_t mid_stride HIGHBD_DECL_SUFFIX);
void BF(dav1d_copy_narrow, neon)(pixel *dst, ptrdiff_t stride,
                                 const pixel *src, int w, int h);

static void wiener_filter_neon(pixel *const dst, const ptrdiff_t dst_stride,
                               const pixel (*const left)[4],
                               const pixel *lpf, const ptrdiff_t lpf_stride,
                               const int w, const int h, const int16_t fh[7],
                               const int16_t fv[7], const enum LrEdgeFlags edges
                               HIGHBD_DECL_SUFFIX)
{
    ALIGN_STK_16(int16_t, mid, 68 * 384,);
    int mid_stride = (w + 7) & ~7;

    // Horizontal filter
    BF(dav1d_wiener_filter_h, neon)(&mid[2 * mid_stride], left, dst, dst_stride,
                                    fh, w, h, edges HIGHBD_TAIL_SUFFIX);
    if (edges & LR_HAVE_TOP)
        BF(dav1d_wiener_filter_h, neon)(mid, NULL, lpf, lpf_stride,
                                        fh, w, 2, edges HIGHBD_TAIL_SUFFIX);
    if (edges & LR_HAVE_BOTTOM)
        BF(dav1d_wiener_filter_h, neon)(&mid[(2 + h) * mid_stride], NULL,
                                        lpf + 6 * PXSTRIDE(lpf_stride),
                                        lpf_stride, fh, w, 2, edges
                                        HIGHBD_TAIL_SUFFIX);

    // Vertical filter
    if (w >= 8)
        BF(dav1d_wiener_filter_v, neon)(dst, dst_stride, &mid[2*mid_stride],
                                        w & ~7, h, fv, edges,
                                        mid_stride * sizeof(*mid)
                                        HIGHBD_TAIL_SUFFIX);
    if (w & 7) {
        // For uneven widths, do a full 8 pixel wide filtering into a temp
        // buffer and copy out the narrow slice of pixels separately into dest.
        ALIGN_STK_16(pixel, tmp, 64 * 8,);
        BF(dav1d_wiener_filter_v, neon)(tmp, (w & 7) * sizeof(pixel),
                                        &mid[2*mid_stride + (w & ~7)],
                                        w & 7, h, fv, edges,
                                        mid_stride * sizeof(*mid)
                                        HIGHBD_TAIL_SUFFIX);
        BF(dav1d_copy_narrow, neon)(dst + (w & ~7), dst_stride, tmp, w & 7, h);
    }
}

void BF(dav1d_sgr_box3_h, neon)(int32_t *sumsq, int16_t *sum,
                                const pixel (*left)[4],
                                const pixel *src, const ptrdiff_t stride,
                                const int w, const int h,
                                const enum LrEdgeFlags edges);
void dav1d_sgr_box3_v_neon(int32_t *sumsq, int16_t *sum,
                           const int w, const int h,
                           const enum LrEdgeFlags edges);
void dav1d_sgr_calc_ab1_neon(int32_t *a, int16_t *b,
                             const int w, const int h, const int strength,
                             const int bitdepth_max);
void BF(dav1d_sgr_finish_filter1, neon)(int16_t *tmp,
                                        const pixel *src, const ptrdiff_t stride,
                                        const int32_t *a, const int16_t *b,
                                        const int w, const int h);

/* filter with a 3x3 box (radius=1) */
static void dav1d_sgr_filter1_neon(int16_t *tmp,
                                   const pixel *src, const ptrdiff_t stride,
                                   const pixel (*left)[4],
                                   const pixel *lpf, const ptrdiff_t lpf_stride,
                                   const int w, const int h, const int strength,
                                   const enum LrEdgeFlags edges
                                   HIGHBD_DECL_SUFFIX)
{
    ALIGN_STK_16(int32_t, sumsq_mem, (384 + 16) * 68 + 8,);
    int32_t *const sumsq = &sumsq_mem[(384 + 16) * 2 + 8], *const a = sumsq;
    ALIGN_STK_16(int16_t, sum_mem, (384 + 16) * 68 + 16,);
    int16_t *const sum = &sum_mem[(384 + 16) * 2 + 16], *const b = sum;

    BF(dav1d_sgr_box3_h, neon)(sumsq, sum, left, src, stride, w, h, edges);
    if (edges & LR_HAVE_TOP)
        BF(dav1d_sgr_box3_h, neon)(&sumsq[-2 * (384 + 16)], &sum[-2 * (384 + 16)],
                                   NULL, lpf, lpf_stride, w, 2, edges);

    if (edges & LR_HAVE_BOTTOM)
        BF(dav1d_sgr_box3_h, neon)(&sumsq[h * (384 + 16)], &sum[h * (384 + 16)],
                                   NULL, lpf + 6 * PXSTRIDE(lpf_stride),
                                   lpf_stride, w, 2, edges);

    dav1d_sgr_box3_v_neon(sumsq, sum, w, h, edges);
    dav1d_sgr_calc_ab1_neon(a, b, w, h, strength, BITDEPTH_MAX);
    BF(dav1d_sgr_finish_filter1, neon)(tmp, src, stride, a, b, w, h);
}

void BF(dav1d_sgr_box5_h, neon)(int32_t *sumsq, int16_t *sum,
                                const pixel (*left)[4],
                                const pixel *src, const ptrdiff_t stride,
                                const int w, const int h,
                                const enum LrEdgeFlags edges);
void dav1d_sgr_box5_v_neon(int32_t *sumsq, int16_t *sum,
                           const int w, const int h,
                           const enum LrEdgeFlags edges);
void dav1d_sgr_calc_ab2_neon(int32_t *a, int16_t *b,
                             const int w, const int h, const int strength,
                             const int bitdepth_max);
void BF(dav1d_sgr_finish_filter2, neon)(int16_t *tmp,
                                        const pixel *src, const ptrdiff_t stride,
                                        const int32_t *a, const int16_t *b,
                                        const int w, const int h);

/* filter with a 5x5 box (radius=2) */
static void dav1d_sgr_filter2_neon(int16_t *tmp,
                                   const pixel *src, const ptrdiff_t stride,
                                   const pixel (*left)[4],
                                   const pixel *lpf, const ptrdiff_t lpf_stride,
                                   const int w, const int h, const int strength,
                                   const enum LrEdgeFlags edges
                                   HIGHBD_DECL_SUFFIX)
{
    ALIGN_STK_16(int32_t, sumsq_mem, (384 + 16) * 68 + 8,);
    int32_t *const sumsq = &sumsq_mem[(384 + 16) * 2 + 8], *const a = sumsq;
    ALIGN_STK_16(int16_t, sum_mem, (384 + 16) * 68 + 16,);
    int16_t *const sum = &sum_mem[(384 + 16) * 2 + 16], *const b = sum;

    BF(dav1d_sgr_box5_h, neon)(sumsq, sum, left, src, stride, w, h, edges);
    if (edges & LR_HAVE_TOP)
        BF(dav1d_sgr_box5_h, neon)(&sumsq[-2 * (384 + 16)], &sum[-2 * (384 + 16)],
                                   NULL, lpf, lpf_stride, w, 2, edges);

    if (edges & LR_HAVE_BOTTOM)
        BF(dav1d_sgr_box5_h, neon)(&sumsq[h * (384 + 16)], &sum[h * (384 + 16)],
                                   NULL, lpf + 6 * PXSTRIDE(lpf_stride),
                                   lpf_stride, w, 2, edges);

    dav1d_sgr_box5_v_neon(sumsq, sum, w, h, edges);
    dav1d_sgr_calc_ab2_neon(a, b, w, h, strength, BITDEPTH_MAX);
    BF(dav1d_sgr_finish_filter2, neon)(tmp, src, stride, a, b, w, h);
}

void BF(dav1d_sgr_weighted1, neon)(pixel *dst, const ptrdiff_t dst_stride,
                                   const pixel *src, const ptrdiff_t src_stride,
                                   const int16_t *t1, const int w, const int h,
                                   const int wt HIGHBD_DECL_SUFFIX);
void BF(dav1d_sgr_weighted2, neon)(pixel *dst, const ptrdiff_t dst_stride,
                                   const pixel *src, const ptrdiff_t src_stride,
                                   const int16_t *t1, const int16_t *t2,
                                   const int w, const int h,
                                   const int16_t wt[2] HIGHBD_DECL_SUFFIX);

static void sgr_filter_neon(pixel *const dst, const ptrdiff_t dst_stride,
                             const pixel (*const left)[4],
                             const pixel *lpf, const ptrdiff_t lpf_stride,
                             const int w, const int h, const int sgr_idx,
                             const int16_t sgr_wt[7], const enum LrEdgeFlags edges
                             HIGHBD_DECL_SUFFIX)
{
    if (!dav1d_sgr_params[sgr_idx][0]) {
        ALIGN_STK_16(int16_t, tmp, 64 * 384,);
        dav1d_sgr_filter1_neon(tmp, dst, dst_stride, left, lpf, lpf_stride,
                               w, h, dav1d_sgr_params[sgr_idx][3], edges
                               HIGHBD_TAIL_SUFFIX);
        if (w >= 8)
            BF(dav1d_sgr_weighted1, neon)(dst, dst_stride, dst, dst_stride,
                                          tmp, w & ~7, h, (1 << 7) - sgr_wt[1]
                                          HIGHBD_TAIL_SUFFIX);
        if (w & 7) {
            // For uneven widths, do a full 8 pixel wide filtering into a temp
            // buffer and copy out the narrow slice of pixels separately into
            // dest.
            ALIGN_STK_16(pixel, stripe, 64 * 8,);
            BF(dav1d_sgr_weighted1, neon)(stripe, (w & 7) * sizeof(pixel),
                                          dst + (w & ~7), dst_stride,
                                          tmp + (w & ~7), w & 7, h,
                                          (1 << 7) - sgr_wt[1]
                                          HIGHBD_TAIL_SUFFIX);
            BF(dav1d_copy_narrow, neon)(dst + (w & ~7), dst_stride, stripe,
                                        w & 7, h);
        }
    } else if (!dav1d_sgr_params[sgr_idx][1]) {
        ALIGN_STK_16(int16_t, tmp, 64 * 384,);
        dav1d_sgr_filter2_neon(tmp, dst, dst_stride, left, lpf, lpf_stride,
                               w, h, dav1d_sgr_params[sgr_idx][2], edges
                               HIGHBD_TAIL_SUFFIX);
        if (w >= 8)
            BF(dav1d_sgr_weighted1, neon)(dst, dst_stride, dst, dst_stride,
                                          tmp, w & ~7, h, sgr_wt[0]
                                          HIGHBD_TAIL_SUFFIX);
        if (w & 7) {
            // For uneven widths, do a full 8 pixel wide filtering into a temp
            // buffer and copy out the narrow slice of pixels separately into
            // dest.
            ALIGN_STK_16(pixel, stripe, 64 * 8,);
            BF(dav1d_sgr_weighted1, neon)(stripe, (w & 7) * sizeof(pixel),
                                          dst + (w & ~7), dst_stride,
                                          tmp + (w & ~7), w & 7, h, sgr_wt[0]
                                          HIGHBD_TAIL_SUFFIX);
            BF(dav1d_copy_narrow, neon)(dst + (w & ~7), dst_stride, stripe,
                                        w & 7, h);
        }
    } else {
        ALIGN_STK_16(int16_t, tmp1, 64 * 384,);
        ALIGN_STK_16(int16_t, tmp2, 64 * 384,);
        dav1d_sgr_filter2_neon(tmp1, dst, dst_stride, left, lpf, lpf_stride,
                               w, h, dav1d_sgr_params[sgr_idx][2], edges
                               HIGHBD_TAIL_SUFFIX);
        dav1d_sgr_filter1_neon(tmp2, dst, dst_stride, left, lpf, lpf_stride,
                               w, h, dav1d_sgr_params[sgr_idx][3], edges
                               HIGHBD_TAIL_SUFFIX);
        const int16_t wt[2] = { sgr_wt[0], 128 - sgr_wt[0] - sgr_wt[1] };
        if (w >= 8)
            BF(dav1d_sgr_weighted2, neon)(dst, dst_stride, dst, dst_stride,
                                          tmp1, tmp2, w & ~7, h, wt
                                          HIGHBD_TAIL_SUFFIX);
        if (w & 7) {
            // For uneven widths, do a full 8 pixel wide filtering into a temp
            // buffer and copy out the narrow slice of pixels separately into
            // dest.
            ALIGN_STK_16(pixel, stripe, 64 * 8,);
            BF(dav1d_sgr_weighted2, neon)(stripe, (w & 7) * sizeof(pixel),
                                          dst + (w & ~7), dst_stride,
                                          tmp1 + (w & ~7), tmp2 + (w & ~7),
                                          w & 7, h, wt HIGHBD_TAIL_SUFFIX);
            BF(dav1d_copy_narrow, neon)(dst + (w & ~7), dst_stride, stripe,
                                        w & 7, h);
        }
    }
}
#endif // BITDEPTH == 8

COLD void bitfn(dav1d_loop_restoration_dsp_init_arm)(Dav1dLoopRestorationDSPContext *const c, int bpc) {
    const unsigned flags = dav1d_get_cpu_flags();

    if (!(flags & DAV1D_ARM_CPU_FLAG_NEON)) return;

#if BITDEPTH == 8 || ARCH_AARCH64
    c->wiener = wiener_filter_neon;
    if (bpc <= 10)
        c->selfguided = sgr_filter_neon;
#endif
}
