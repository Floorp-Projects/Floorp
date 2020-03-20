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

#include "tests/checkasm/checkasm.h"

#include <string.h>

#include "src/levels.h"
#include "src/looprestoration.h"
#include "src/tables.h"

static void init_tmp(pixel *buf, const ptrdiff_t stride,
                     const int w, const int h, const int bitdepth_max)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++)
            buf[x] = rnd() & bitdepth_max;
        buf += PXSTRIDE(stride);
    }
}

static void check_wiener(Dav1dLoopRestorationDSPContext *const c, const int bpc) {
    ALIGN_STK_64(pixel, c_dst, 448 * 64,);
    ALIGN_STK_64(pixel, a_dst, 448 * 64,);
    ALIGN_STK_64(pixel, h_edge, 448 * 8,);
    pixel left[64][4];

    declare_func(void, pixel *dst, ptrdiff_t dst_stride,
                 const pixel (*const left)[4],
                 const pixel *lpf, ptrdiff_t lpf_stride,
                 int w, int h, const int16_t filterh[7],
                 const int16_t filterv[7], enum LrEdgeFlags edges
                 HIGHBD_DECL_SUFFIX);

    for (int pl = 0; pl < 2; pl++) {
        if (check_func(c->wiener, "wiener_%s_%dbpc",
                       pl ? "chroma" : "luma", bpc))
        {
            int16_t filter[2][3], filter_v[7], filter_h[7];

            filter[0][0] = pl ? 0 : (rnd() & 15) - 5;
            filter[0][1] = (rnd() & 31) - 23;
            filter[0][2] = (rnd() & 63) - 17;
            filter[1][0] = pl ? 0 : (rnd() & 15) - 5;
            filter[1][1] = (rnd() & 31) - 23;
            filter[1][2] = (rnd() & 63) - 17;

            filter_h[0] = filter_h[6] = filter[0][0];
            filter_h[1] = filter_h[5] = filter[0][1];
            filter_h[2] = filter_h[4] = filter[0][2];
            filter_h[3] = -((filter_h[0] + filter_h[1] + filter_h[2]) * 2);

            filter_v[0] = filter_v[6] = filter[1][0];
            filter_v[1] = filter_v[5] = filter[1][1];
            filter_v[2] = filter_v[4] = filter[1][2];
            filter_v[3] = -((filter_v[0] + filter_v[1] + filter_v[2]) * 2);

            const int base_w = 1 + (rnd() % 384);
            const int base_h = 1 + (rnd() & 63);
            const int bitdepth_max = (1 << bpc) - 1;

            init_tmp(c_dst, 448 * sizeof(pixel), 448, 64, bitdepth_max);
            init_tmp(h_edge, 448 * sizeof(pixel), 448, 8, bitdepth_max);
            init_tmp((pixel *) left, 4 * sizeof(pixel), 4, 64, bitdepth_max);

            for (enum LrEdgeFlags edges = 0; edges <= 0xf; edges++) {
                const int w = edges & LR_HAVE_RIGHT ? 256 : base_w;
                const int h = edges & LR_HAVE_BOTTOM ? 64 : base_h;

                memcpy(a_dst, c_dst, 448 * 64 * sizeof(pixel));

                call_ref(c_dst + 32, 448 * sizeof(pixel), left,
                         h_edge + 32, 448 * sizeof(pixel),
                         w, h, filter_h, filter_v, edges HIGHBD_TAIL_SUFFIX);
                call_new(a_dst + 32, 448 * sizeof(pixel), left,
                         h_edge + 32, 448 * sizeof(pixel),
                         w, h, filter_h, filter_v, edges HIGHBD_TAIL_SUFFIX);
                checkasm_check_pixel(c_dst + 32, 448 * sizeof(pixel),
                                     a_dst + 32, 448 * sizeof(pixel),
                                     w, h, "dst");
            }
            bench_new(a_dst + 32, 448 * sizeof(pixel), left,
                      h_edge + 32, 448 * sizeof(pixel),
                      256, 64, filter_h, filter_v, 0xf HIGHBD_TAIL_SUFFIX);
        }
    }
}

static void check_sgr(Dav1dLoopRestorationDSPContext *const c, const int bpc) {
    ALIGN_STK_64(pixel, c_dst, 448 * 64,);
    ALIGN_STK_64(pixel, a_dst, 448 * 64,);
    ALIGN_STK_64(pixel, h_edge, 448 * 8,);
    pixel left[64][4];

    declare_func(void, pixel *dst, ptrdiff_t dst_stride,
                 const pixel (*const left)[4],
                 const pixel *lpf, ptrdiff_t lpf_stride,
                 int w, int h, int sgr_idx,
                 const int16_t sgr_wt[7], enum LrEdgeFlags edges
                 HIGHBD_DECL_SUFFIX);

    for (int sgr_idx = 14; sgr_idx >= 6; sgr_idx -= 4) {
        if (check_func(c->selfguided, "selfguided_%s_%dbpc",
                       sgr_idx == 6 ? "mix" : sgr_idx == 10 ? "3x3" : "5x5", bpc))
        {
            int16_t sgr_wt[2];

            sgr_wt[0] = dav1d_sgr_params[sgr_idx][0] ? (rnd() & 127) - 96 : 0;
            sgr_wt[1] = dav1d_sgr_params[sgr_idx][1] ? (rnd() & 127) - 32 :
                            iclip(128 - sgr_wt[0], -32, 95);

            const int base_w = 1 + (rnd() % 384);
            const int base_h = 1 + (rnd() & 63);
            const int bitdepth_max = (1 << bpc) - 1;

            init_tmp(c_dst, 448 * sizeof(pixel), 448, 64, bitdepth_max);
            init_tmp(h_edge, 448 * sizeof(pixel), 448, 8, bitdepth_max);
            init_tmp((pixel *) left, 4 * sizeof(pixel), 4, 64, bitdepth_max);

            for (enum LrEdgeFlags edges = 0; edges <= 0xf; edges++) {
                const int w = edges & LR_HAVE_RIGHT ? 256 : base_w;
                const int h = edges & LR_HAVE_BOTTOM ? 64 : base_h;

                memcpy(a_dst, c_dst, 448 * 64 * sizeof(pixel));

                call_ref(c_dst + 32, 448 * sizeof(pixel), left,
                         h_edge + 32, 448 * sizeof(pixel),
                         w, h, sgr_idx, sgr_wt, edges HIGHBD_TAIL_SUFFIX);
                call_new(a_dst + 32, 448 * sizeof(pixel), left,
                         h_edge + 32, 448 * sizeof(pixel),
                         w, h, sgr_idx, sgr_wt, edges HIGHBD_TAIL_SUFFIX);
                checkasm_check_pixel(c_dst + 32, 448 * sizeof(pixel),
                                     a_dst + 32, 448 * sizeof(pixel),
                                     w, h, "dst");
            }
            bench_new(a_dst + 32, 448 * sizeof(pixel), left,
                      h_edge + 32, 448 * sizeof(pixel),
                      256, 64, sgr_idx, sgr_wt, 0xf HIGHBD_TAIL_SUFFIX);
        }
    }
}

void bitfn(checkasm_check_looprestoration)(void) {
#if BITDEPTH == 16
    const int bpc_min = 10, bpc_max = 12;
#else
    const int bpc_min = 8, bpc_max = 8;
#endif
    for (int bpc = bpc_min; bpc <= bpc_max; bpc += 2) {
        Dav1dLoopRestorationDSPContext c;
        bitfn(dav1d_loop_restoration_dsp_init)(&c, bpc);
        check_wiener(&c, bpc);
    }
    report("wiener");
    for (int bpc = bpc_min; bpc <= bpc_max; bpc += 2) {
        Dav1dLoopRestorationDSPContext c;
        bitfn(dav1d_loop_restoration_dsp_init)(&c, bpc);
        check_sgr(&c, bpc);
    }
    report("sgr");
}
