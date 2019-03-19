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

#include "common/attributes.h"

#if BITDEPTH == 8
// This calculates things slightly differently than the reference C version.
// This version calculates roughly this:
// int16_t sum = 0;
// for (int i = 0; i < 7; i++)
//     sum += src[idx] * fh[i];
// int16_t sum2 = (src[x] << 7) - (1 << (BITDEPTH + 6)) + rounding_off_h;
// sum = iclip(sum + sum2, INT16_MIN, INT16_MAX) >> round_bits_h;
// sum += 2048;
void dav1d_wiener_filter_h_neon(int16_t *dst, const pixel (*left)[4],
                                const pixel *src, ptrdiff_t stride,
                                const int16_t fh[7], const intptr_t w,
                                int h, enum LrEdgeFlags edges);
// This calculates things slightly differently than the reference C version.
// This version calculates roughly this:
// fv[3] += 128;
// int32_t sum = 0;
// for (int i = 0; i < 7; i++)
//     sum += mid[idx] * fv[i];
// sum = (sum + rounding_off_v) >> round_bits_v;
// This function assumes that the width is a multiple of 8.
void dav1d_wiener_filter_v_neon(pixel *dst, ptrdiff_t stride,
                                const int16_t *mid, int w, int h,
                                const int16_t fv[7], enum LrEdgeFlags edges,
                                ptrdiff_t mid_stride);
void dav1d_copy_narrow_neon(pixel *dst, ptrdiff_t stride,
                            const pixel *src, int w, int h);

static void wiener_filter_neon(pixel *const dst, const ptrdiff_t dst_stride,
                               const pixel (*const left)[4],
                               const pixel *lpf, const ptrdiff_t lpf_stride,
                               const int w, const int h, const int16_t fh[7],
                               const int16_t fv[7], const enum LrEdgeFlags edges)
{
    ALIGN_STK_16(int16_t, mid, 68 * 384,);
    int mid_stride = (w + 7) & ~7;

    // Horizontal filter
    dav1d_wiener_filter_h_neon(&mid[2 * mid_stride], left, dst, dst_stride,
                               fh, w, h, edges);
    if (edges & LR_HAVE_TOP)
        dav1d_wiener_filter_h_neon(mid, NULL, lpf, lpf_stride,
                                   fh, w, 2, edges);
    if (edges & LR_HAVE_BOTTOM)
        dav1d_wiener_filter_h_neon(&mid[(2 + h) * mid_stride], NULL,
                                   lpf + 6 * PXSTRIDE(lpf_stride), lpf_stride,
                                   fh, w, 2, edges);

    // Vertical filter
    if (w >= 8)
        dav1d_wiener_filter_v_neon(dst, dst_stride, &mid[2*mid_stride],
                                   w & ~7, h, fv, edges, mid_stride * sizeof(*mid));
    if (w & 7) {
        // For uneven widths, do a full 8 pixel wide filtering into a temp
        // buffer and copy out the narrow slice of pixels separately into dest.
        ALIGN_STK_16(pixel, tmp, 64 * 8,);
        dav1d_wiener_filter_v_neon(tmp, w & 7, &mid[2*mid_stride + (w & ~7)],
                                   w & 7, h, fv, edges, mid_stride * sizeof(*mid));
        dav1d_copy_narrow_neon(dst + (w & ~7), dst_stride, tmp, w & 7, h);
    }
}
#endif

void bitfn(dav1d_loop_restoration_dsp_init_arm)(Dav1dLoopRestorationDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();

    if (!(flags & DAV1D_ARM_CPU_FLAG_NEON)) return;

#if BITDEPTH == 8
    c->wiener = wiener_filter_neon;
#endif
}
