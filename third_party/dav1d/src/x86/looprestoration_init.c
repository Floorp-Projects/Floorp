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
#include "common/intops.h"

#if BITDEPTH == 8 && ARCH_X86_64
void dav1d_wiener_filter_h_avx2(int16_t *dst, const pixel (*left)[4],
                                const pixel *src, ptrdiff_t stride,
                                const int16_t fh[7], const intptr_t w,
                                int h, enum LrEdgeFlags edges);
void dav1d_wiener_filter_v_avx2(pixel *dst, ptrdiff_t stride,
                                const int16_t *mid, int w, int h,
                                const int16_t fv[7], enum LrEdgeFlags edges);

// Future potential optimizations:
// - special chroma versions which don't filter [0]/[6];
// - running filter_h_avx2 transposed (one col of 32 pixels per iteration, top
//   to bottom) instead of scanline-ordered should be faster since then the
//   if (have_left) and similar conditions run only once instead of per line;
// - filter_v_avx2 currently runs 16 pixels per iteration, it should be possible
//   to run 32 (like filter_h_avx2), and then all vpermqs can go;
// - maybe split out the top/bottom filter_h_avx2 from the main body filter_h_avx2,
//   since then the have_left condition can be inlined;
// - consider having the wrapper (wiener_filter_avx2) also in hand-written
//   assembly, so the setup overhead is minimized.

static void wiener_filter_avx2(pixel *const dst, const ptrdiff_t dst_stride,
                               const pixel (*const left)[4],
                               const pixel *lpf, const ptrdiff_t lpf_stride,
                               const int w, const int h, const int16_t fh[7],
                               const int16_t fv[7], const enum LrEdgeFlags edges)
{
    ALIGN_STK_32(int16_t, mid, 68 * 384,);

    // horizontal filter
    dav1d_wiener_filter_h_avx2(&mid[2 * 384], left, dst, dst_stride,
                               fh, w, h, edges);
    if (edges & LR_HAVE_TOP)
        dav1d_wiener_filter_h_avx2(mid, NULL, lpf, lpf_stride,
                                   fh, w, 2, edges);
    if (edges & LR_HAVE_BOTTOM)
        dav1d_wiener_filter_h_avx2(&mid[(2 + h) * 384], NULL,
                                   lpf + 6 * PXSTRIDE(lpf_stride), lpf_stride,
                                   fh, w, 2, edges);

    dav1d_wiener_filter_v_avx2(dst, dst_stride, &mid[2*384], w, h, fv, edges);
}
#endif

void bitfn(dav1d_loop_restoration_dsp_init_x86)(Dav1dLoopRestorationDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();

    if (!(flags & DAV1D_X86_CPU_FLAG_AVX2)) return;

#if BITDEPTH == 8 && ARCH_X86_64
    c->wiener = wiener_filter_avx2;
#endif
}
