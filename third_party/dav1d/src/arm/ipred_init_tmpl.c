/*
 * Copyright © 2018, VideoLAN and dav1d authors
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

decl_angular_ipred_fn(dav1d_ipred_dc_neon);
decl_angular_ipred_fn(dav1d_ipred_dc_128_neon);
decl_angular_ipred_fn(dav1d_ipred_dc_top_neon);
decl_angular_ipred_fn(dav1d_ipred_dc_left_neon);
decl_angular_ipred_fn(dav1d_ipred_h_neon);
decl_angular_ipred_fn(dav1d_ipred_v_neon);
decl_angular_ipred_fn(dav1d_ipred_paeth_neon);
decl_angular_ipred_fn(dav1d_ipred_smooth_neon);
decl_angular_ipred_fn(dav1d_ipred_smooth_v_neon);
decl_angular_ipred_fn(dav1d_ipred_smooth_h_neon);
decl_angular_ipred_fn(dav1d_ipred_filter_neon);

decl_cfl_pred_fn(dav1d_ipred_cfl_neon);
decl_cfl_pred_fn(dav1d_ipred_cfl_128_neon);
decl_cfl_pred_fn(dav1d_ipred_cfl_top_neon);
decl_cfl_pred_fn(dav1d_ipred_cfl_left_neon);

decl_cfl_ac_fn(dav1d_ipred_cfl_ac_420_neon);
decl_cfl_ac_fn(dav1d_ipred_cfl_ac_422_neon);

decl_pal_pred_fn(dav1d_pal_pred_neon);

COLD void bitfn(dav1d_intra_pred_dsp_init_arm)(Dav1dIntraPredDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();

    if (!(flags & DAV1D_ARM_CPU_FLAG_NEON)) return;

#if BITDEPTH == 8
    c->intra_pred[DC_PRED]       = dav1d_ipred_dc_neon;
    c->intra_pred[DC_128_PRED]   = dav1d_ipred_dc_128_neon;
    c->intra_pred[TOP_DC_PRED]   = dav1d_ipred_dc_top_neon;
    c->intra_pred[LEFT_DC_PRED]  = dav1d_ipred_dc_left_neon;
    c->intra_pred[HOR_PRED]      = dav1d_ipred_h_neon;
    c->intra_pred[VERT_PRED]     = dav1d_ipred_v_neon;
#if ARCH_AARCH64
    c->intra_pred[PAETH_PRED]    = dav1d_ipred_paeth_neon;
    c->intra_pred[SMOOTH_PRED]   = dav1d_ipred_smooth_neon;
    c->intra_pred[SMOOTH_V_PRED] = dav1d_ipred_smooth_v_neon;
    c->intra_pred[SMOOTH_H_PRED] = dav1d_ipred_smooth_h_neon;
    c->intra_pred[FILTER_PRED]   = dav1d_ipred_filter_neon;

    c->cfl_pred[DC_PRED]         = dav1d_ipred_cfl_neon;
    c->cfl_pred[DC_128_PRED]     = dav1d_ipred_cfl_128_neon;
    c->cfl_pred[TOP_DC_PRED]     = dav1d_ipred_cfl_top_neon;
    c->cfl_pred[LEFT_DC_PRED]    = dav1d_ipred_cfl_left_neon;

    c->cfl_ac[DAV1D_PIXEL_LAYOUT_I420 - 1] = dav1d_ipred_cfl_ac_420_neon;
    c->cfl_ac[DAV1D_PIXEL_LAYOUT_I422 - 1] = dav1d_ipred_cfl_ac_422_neon;

    c->pal_pred                  = dav1d_pal_pred_neon;
#endif
#endif
}
