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
#include "src/ipred.h"

decl_angular_ipred_fn(dav1d_ipred_dc_avx2);
decl_angular_ipred_fn(dav1d_ipred_dc_128_avx2);
decl_angular_ipred_fn(dav1d_ipred_dc_top_avx2);
decl_angular_ipred_fn(dav1d_ipred_dc_left_avx2);
decl_angular_ipred_fn(dav1d_ipred_h_avx2);
decl_angular_ipred_fn(dav1d_ipred_v_avx2);
decl_angular_ipred_fn(dav1d_ipred_paeth_avx2);
decl_angular_ipred_fn(dav1d_ipred_smooth_avx2);
decl_angular_ipred_fn(dav1d_ipred_smooth_v_avx2);
decl_angular_ipred_fn(dav1d_ipred_smooth_h_avx2);
decl_angular_ipred_fn(dav1d_ipred_z1_avx2);
decl_angular_ipred_fn(dav1d_ipred_z3_avx2);
decl_angular_ipred_fn(dav1d_ipred_filter_avx2);

decl_cfl_pred_fn(dav1d_ipred_cfl_avx2);
decl_cfl_pred_fn(dav1d_ipred_cfl_128_avx2);
decl_cfl_pred_fn(dav1d_ipred_cfl_top_avx2);
decl_cfl_pred_fn(dav1d_ipred_cfl_left_avx2);

decl_cfl_ac_fn(dav1d_ipred_cfl_ac_420_avx2);
decl_cfl_ac_fn(dav1d_ipred_cfl_ac_422_avx2);

decl_pal_pred_fn(dav1d_pal_pred_avx2);

decl_angular_ipred_fn(dav1d_ipred_dc_ssse3);
decl_angular_ipred_fn(dav1d_ipred_dc_128_ssse3);
decl_angular_ipred_fn(dav1d_ipred_dc_top_ssse3);
decl_angular_ipred_fn(dav1d_ipred_dc_left_ssse3);
decl_angular_ipred_fn(dav1d_ipred_h_ssse3);
decl_angular_ipred_fn(dav1d_ipred_v_ssse3);
decl_angular_ipred_fn(dav1d_ipred_paeth_ssse3);
decl_angular_ipred_fn(dav1d_ipred_smooth_ssse3);
decl_angular_ipred_fn(dav1d_ipred_smooth_v_ssse3);
decl_angular_ipred_fn(dav1d_ipred_smooth_h_ssse3);
decl_angular_ipred_fn(dav1d_ipred_filter_ssse3);

decl_cfl_pred_fn(dav1d_ipred_cfl_ssse3);
decl_cfl_pred_fn(dav1d_ipred_cfl_128_ssse3);
decl_cfl_pred_fn(dav1d_ipred_cfl_top_ssse3);
decl_cfl_pred_fn(dav1d_ipred_cfl_left_ssse3);

decl_cfl_ac_fn(dav1d_ipred_cfl_ac_420_ssse3);
decl_cfl_ac_fn(dav1d_ipred_cfl_ac_422_ssse3);
decl_cfl_ac_fn(dav1d_ipred_cfl_ac_444_ssse3);

decl_pal_pred_fn(dav1d_pal_pred_ssse3);

void bitfn(dav1d_intra_pred_dsp_init_x86)(Dav1dIntraPredDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();

    if (!(flags & DAV1D_X86_CPU_FLAG_SSSE3)) return;

#if BITDEPTH == 8
    c->intra_pred[DC_PRED]       = dav1d_ipred_dc_ssse3;
    c->intra_pred[DC_128_PRED]   = dav1d_ipred_dc_128_ssse3;
    c->intra_pred[TOP_DC_PRED]   = dav1d_ipred_dc_top_ssse3;
    c->intra_pred[LEFT_DC_PRED]  = dav1d_ipred_dc_left_ssse3;
    c->intra_pred[HOR_PRED]      = dav1d_ipred_h_ssse3;
    c->intra_pred[VERT_PRED]     = dav1d_ipred_v_ssse3;
    c->intra_pred[PAETH_PRED]    = dav1d_ipred_paeth_ssse3;
    c->intra_pred[SMOOTH_PRED]   = dav1d_ipred_smooth_ssse3;
    c->intra_pred[SMOOTH_V_PRED] = dav1d_ipred_smooth_v_ssse3;
    c->intra_pred[SMOOTH_H_PRED] = dav1d_ipred_smooth_h_ssse3;
    c->intra_pred[FILTER_PRED]   = dav1d_ipred_filter_ssse3;

    c->cfl_pred[DC_PRED]         = dav1d_ipred_cfl_ssse3;
    c->cfl_pred[DC_128_PRED]     = dav1d_ipred_cfl_128_ssse3;
    c->cfl_pred[TOP_DC_PRED]     = dav1d_ipred_cfl_top_ssse3;
    c->cfl_pred[LEFT_DC_PRED]    = dav1d_ipred_cfl_left_ssse3;

    c->cfl_ac[DAV1D_PIXEL_LAYOUT_I420 - 1] = dav1d_ipred_cfl_ac_420_ssse3;
    c->cfl_ac[DAV1D_PIXEL_LAYOUT_I422 - 1] = dav1d_ipred_cfl_ac_422_ssse3;
    c->cfl_ac[DAV1D_PIXEL_LAYOUT_I444 - 1] = dav1d_ipred_cfl_ac_444_ssse3;

    c->pal_pred                  = dav1d_pal_pred_ssse3;
#endif

    if (!(flags & DAV1D_X86_CPU_FLAG_AVX2)) return;

#if BITDEPTH == 8 && ARCH_X86_64
    c->intra_pred[DC_PRED]       = dav1d_ipred_dc_avx2;
    c->intra_pred[DC_128_PRED]   = dav1d_ipred_dc_128_avx2;
    c->intra_pred[TOP_DC_PRED]   = dav1d_ipred_dc_top_avx2;
    c->intra_pred[LEFT_DC_PRED]  = dav1d_ipred_dc_left_avx2;
    c->intra_pred[HOR_PRED]      = dav1d_ipred_h_avx2;
    c->intra_pred[VERT_PRED]     = dav1d_ipred_v_avx2;
    c->intra_pred[PAETH_PRED]    = dav1d_ipred_paeth_avx2;
    c->intra_pred[SMOOTH_PRED]   = dav1d_ipred_smooth_avx2;
    c->intra_pred[SMOOTH_V_PRED] = dav1d_ipred_smooth_v_avx2;
    c->intra_pred[SMOOTH_H_PRED] = dav1d_ipred_smooth_h_avx2;
    c->intra_pred[Z1_PRED]       = dav1d_ipred_z1_avx2;
    c->intra_pred[Z3_PRED]       = dav1d_ipred_z3_avx2;
    c->intra_pred[FILTER_PRED]   = dav1d_ipred_filter_avx2;

    c->cfl_pred[DC_PRED]      = dav1d_ipred_cfl_avx2;
    c->cfl_pred[DC_128_PRED]  = dav1d_ipred_cfl_128_avx2;
    c->cfl_pred[TOP_DC_PRED]  = dav1d_ipred_cfl_top_avx2;
    c->cfl_pred[LEFT_DC_PRED] = dav1d_ipred_cfl_left_avx2;

    c->cfl_ac[DAV1D_PIXEL_LAYOUT_I420 - 1] = dav1d_ipred_cfl_ac_420_avx2;
    c->cfl_ac[DAV1D_PIXEL_LAYOUT_I422 - 1] = dav1d_ipred_cfl_ac_422_avx2;

    c->pal_pred = dav1d_pal_pred_avx2;
#endif
}
