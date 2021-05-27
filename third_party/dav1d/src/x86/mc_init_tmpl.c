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
#include "src/mc.h"

#if BITDEPTH == 8
#define decl_fn(type, name) \
    decl_##type##_fn(name##_sse2); \
    decl_##type##_fn(name##_ssse3); \
    decl_##type##_fn(name##_avx2); \
    decl_##type##_fn(name##_avx512icl);
#define init_mc_fn(type, name, suffix) \
    c->mc[type] = dav1d_put_##name##_##suffix
#define init_mct_fn(type, name, suffix) \
    c->mct[type] = dav1d_prep_##name##_##suffix
#define init_mc_scaled_fn(type, name, suffix) \
    c->mc_scaled[type] = dav1d_put_##name##_##suffix
#define init_mct_scaled_fn(type, name, suffix) \
    c->mct_scaled[type] = dav1d_prep_##name##_##suffix
#else
#define decl_fn(type, name) \
    decl_##type##_fn(name##_16bpc_sse2); \
    decl_##type##_fn(name##_16bpc_ssse3); \
    decl_##type##_fn(name##_16bpc_avx2); \
    decl_##type##_fn(name##_avx512icl);
#define init_mc_fn(type, name, suffix) \
    c->mc[type] = dav1d_put_##name##_16bpc_##suffix
#define init_mct_fn(type, name, suffix) \
    c->mct[type] = dav1d_prep_##name##_16bpc_##suffix
#define init_mc_scaled_fn(type, name, suffix) \
    c->mc_scaled[type] = dav1d_put_##name##_16bpc_##suffix
#define init_mct_scaled_fn(type, name, suffix) \
    c->mct_scaled[type] = dav1d_prep_##name##_16bpc_##suffix
#endif

decl_fn(mc, dav1d_put_8tap_regular);
decl_fn(mc, dav1d_put_8tap_regular_smooth);
decl_fn(mc, dav1d_put_8tap_regular_sharp);
decl_fn(mc, dav1d_put_8tap_smooth);
decl_fn(mc, dav1d_put_8tap_smooth_regular);
decl_fn(mc, dav1d_put_8tap_smooth_sharp);
decl_fn(mc, dav1d_put_8tap_sharp);
decl_fn(mc, dav1d_put_8tap_sharp_regular);
decl_fn(mc, dav1d_put_8tap_sharp_smooth);
decl_fn(mc, dav1d_put_bilin);

decl_fn(mct, dav1d_prep_8tap_regular);
decl_fn(mct, dav1d_prep_8tap_regular_smooth);
decl_fn(mct, dav1d_prep_8tap_regular_sharp);
decl_fn(mct, dav1d_prep_8tap_smooth);
decl_fn(mct, dav1d_prep_8tap_smooth_regular);
decl_fn(mct, dav1d_prep_8tap_smooth_sharp);
decl_fn(mct, dav1d_prep_8tap_sharp);
decl_fn(mct, dav1d_prep_8tap_sharp_regular);
decl_fn(mct, dav1d_prep_8tap_sharp_smooth);
decl_fn(mct, dav1d_prep_bilin);

decl_fn(mc_scaled, dav1d_put_8tap_scaled_regular);
decl_fn(mc_scaled, dav1d_put_8tap_scaled_regular_smooth);
decl_fn(mc_scaled, dav1d_put_8tap_scaled_regular_sharp);
decl_fn(mc_scaled, dav1d_put_8tap_scaled_smooth);
decl_fn(mc_scaled, dav1d_put_8tap_scaled_smooth_regular);
decl_fn(mc_scaled, dav1d_put_8tap_scaled_smooth_sharp);
decl_fn(mc_scaled, dav1d_put_8tap_scaled_sharp);
decl_fn(mc_scaled, dav1d_put_8tap_scaled_sharp_regular);
decl_fn(mc_scaled, dav1d_put_8tap_scaled_sharp_smooth);
decl_fn(mc_scaled, dav1d_put_bilin_scaled);

decl_fn(mct_scaled, dav1d_prep_8tap_scaled_regular);
decl_fn(mct_scaled, dav1d_prep_8tap_scaled_regular_smooth);
decl_fn(mct_scaled, dav1d_prep_8tap_scaled_regular_sharp);
decl_fn(mct_scaled, dav1d_prep_8tap_scaled_smooth);
decl_fn(mct_scaled, dav1d_prep_8tap_scaled_smooth_regular);
decl_fn(mct_scaled, dav1d_prep_8tap_scaled_smooth_sharp);
decl_fn(mct_scaled, dav1d_prep_8tap_scaled_sharp);
decl_fn(mct_scaled, dav1d_prep_8tap_scaled_sharp_regular);
decl_fn(mct_scaled, dav1d_prep_8tap_scaled_sharp_smooth);
decl_fn(mct_scaled, dav1d_prep_bilin_scaled);

decl_fn(avg, dav1d_avg);
decl_fn(w_avg, dav1d_w_avg);
decl_fn(mask, dav1d_mask);
decl_fn(w_mask, dav1d_w_mask_420);
decl_fn(w_mask, dav1d_w_mask_422);
decl_fn(w_mask, dav1d_w_mask_444);
decl_fn(blend, dav1d_blend);
decl_fn(blend_dir, dav1d_blend_v);
decl_fn(blend_dir, dav1d_blend_h);

decl_fn(warp8x8, dav1d_warp_affine_8x8);
decl_warp8x8_fn(dav1d_warp_affine_8x8_sse4);
decl_fn(warp8x8t, dav1d_warp_affine_8x8t);
decl_warp8x8t_fn(dav1d_warp_affine_8x8t_sse4);

decl_fn(emu_edge, dav1d_emu_edge);

decl_resize_fn(dav1d_resize_avx2);
decl_resize_fn(dav1d_resize_ssse3);

COLD void bitfn(dav1d_mc_dsp_init_x86)(Dav1dMCDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();

    if(!(flags & DAV1D_X86_CPU_FLAG_SSE2))
        return;

#if BITDEPTH == 8
    init_mct_fn(FILTER_2D_BILINEAR,            bilin,               sse2);
    init_mct_fn(FILTER_2D_8TAP_REGULAR,        8tap_regular,        sse2);
    init_mct_fn(FILTER_2D_8TAP_REGULAR_SMOOTH, 8tap_regular_smooth, sse2);
    init_mct_fn(FILTER_2D_8TAP_REGULAR_SHARP,  8tap_regular_sharp,  sse2);
    init_mct_fn(FILTER_2D_8TAP_SMOOTH_REGULAR, 8tap_smooth_regular, sse2);
    init_mct_fn(FILTER_2D_8TAP_SMOOTH,         8tap_smooth,         sse2);
    init_mct_fn(FILTER_2D_8TAP_SMOOTH_SHARP,   8tap_smooth_sharp,   sse2);
    init_mct_fn(FILTER_2D_8TAP_SHARP_REGULAR,  8tap_sharp_regular,  sse2);
    init_mct_fn(FILTER_2D_8TAP_SHARP_SMOOTH,   8tap_sharp_smooth,   sse2);
    init_mct_fn(FILTER_2D_8TAP_SHARP,          8tap_sharp,          sse2);

    c->warp8x8  = dav1d_warp_affine_8x8_sse2;
    c->warp8x8t = dav1d_warp_affine_8x8t_sse2;
#endif

    if(!(flags & DAV1D_X86_CPU_FLAG_SSSE3))
        return;

#if BITDEPTH == 8
    init_mc_fn(FILTER_2D_BILINEAR,            bilin,               ssse3);
    init_mc_fn(FILTER_2D_8TAP_REGULAR,        8tap_regular,        ssse3);
    init_mc_fn(FILTER_2D_8TAP_REGULAR_SMOOTH, 8tap_regular_smooth, ssse3);
    init_mc_fn(FILTER_2D_8TAP_REGULAR_SHARP,  8tap_regular_sharp,  ssse3);
    init_mc_fn(FILTER_2D_8TAP_SMOOTH_REGULAR, 8tap_smooth_regular, ssse3);
    init_mc_fn(FILTER_2D_8TAP_SMOOTH,         8tap_smooth,         ssse3);
    init_mc_fn(FILTER_2D_8TAP_SMOOTH_SHARP,   8tap_smooth_sharp,   ssse3);
    init_mc_fn(FILTER_2D_8TAP_SHARP_REGULAR,  8tap_sharp_regular,  ssse3);
    init_mc_fn(FILTER_2D_8TAP_SHARP_SMOOTH,   8tap_sharp_smooth,   ssse3);
    init_mc_fn(FILTER_2D_8TAP_SHARP,          8tap_sharp,          ssse3);

    init_mct_fn(FILTER_2D_BILINEAR,            bilin,               ssse3);
    init_mct_fn(FILTER_2D_8TAP_REGULAR,        8tap_regular,        ssse3);
    init_mct_fn(FILTER_2D_8TAP_REGULAR_SMOOTH, 8tap_regular_smooth, ssse3);
    init_mct_fn(FILTER_2D_8TAP_REGULAR_SHARP,  8tap_regular_sharp,  ssse3);
    init_mct_fn(FILTER_2D_8TAP_SMOOTH_REGULAR, 8tap_smooth_regular, ssse3);
    init_mct_fn(FILTER_2D_8TAP_SMOOTH,         8tap_smooth,         ssse3);
    init_mct_fn(FILTER_2D_8TAP_SMOOTH_SHARP,   8tap_smooth_sharp,   ssse3);
    init_mct_fn(FILTER_2D_8TAP_SHARP_REGULAR,  8tap_sharp_regular,  ssse3);
    init_mct_fn(FILTER_2D_8TAP_SHARP_SMOOTH,   8tap_sharp_smooth,   ssse3);
    init_mct_fn(FILTER_2D_8TAP_SHARP,          8tap_sharp,          ssse3);

#if ARCH_X86_64
    init_mc_scaled_fn(FILTER_2D_8TAP_REGULAR,        8tap_scaled_regular,        ssse3);
    init_mc_scaled_fn(FILTER_2D_8TAP_REGULAR_SMOOTH, 8tap_scaled_regular_smooth, ssse3);
    init_mc_scaled_fn(FILTER_2D_8TAP_REGULAR_SHARP,  8tap_scaled_regular_sharp,  ssse3);
    init_mc_scaled_fn(FILTER_2D_8TAP_SMOOTH_REGULAR, 8tap_scaled_smooth_regular, ssse3);
    init_mc_scaled_fn(FILTER_2D_8TAP_SMOOTH,         8tap_scaled_smooth,         ssse3);
    init_mc_scaled_fn(FILTER_2D_8TAP_SMOOTH_SHARP,   8tap_scaled_smooth_sharp,   ssse3);
    init_mc_scaled_fn(FILTER_2D_8TAP_SHARP_REGULAR,  8tap_scaled_sharp_regular,  ssse3);
    init_mc_scaled_fn(FILTER_2D_8TAP_SHARP_SMOOTH,   8tap_scaled_sharp_smooth,   ssse3);
    init_mc_scaled_fn(FILTER_2D_8TAP_SHARP,          8tap_scaled_sharp,          ssse3);
    init_mc_scaled_fn(FILTER_2D_BILINEAR,            bilin_scaled,               ssse3);

    init_mct_scaled_fn(FILTER_2D_8TAP_REGULAR,        8tap_scaled_regular,        ssse3);
    init_mct_scaled_fn(FILTER_2D_8TAP_REGULAR_SMOOTH, 8tap_scaled_regular_smooth, ssse3);
    init_mct_scaled_fn(FILTER_2D_8TAP_REGULAR_SHARP,  8tap_scaled_regular_sharp,  ssse3);
    init_mct_scaled_fn(FILTER_2D_8TAP_SMOOTH_REGULAR, 8tap_scaled_smooth_regular, ssse3);
    init_mct_scaled_fn(FILTER_2D_8TAP_SMOOTH,         8tap_scaled_smooth,         ssse3);
    init_mct_scaled_fn(FILTER_2D_8TAP_SMOOTH_SHARP,   8tap_scaled_smooth_sharp,   ssse3);
    init_mct_scaled_fn(FILTER_2D_8TAP_SHARP_REGULAR,  8tap_scaled_sharp_regular,  ssse3);
    init_mct_scaled_fn(FILTER_2D_8TAP_SHARP_SMOOTH,   8tap_scaled_sharp_smooth,   ssse3);
    init_mct_scaled_fn(FILTER_2D_8TAP_SHARP,          8tap_scaled_sharp,          ssse3);
    init_mct_scaled_fn(FILTER_2D_BILINEAR,            bilin_scaled,               ssse3);
#endif

    c->avg = dav1d_avg_ssse3;
    c->w_avg = dav1d_w_avg_ssse3;
    c->mask = dav1d_mask_ssse3;
    c->w_mask[2] = dav1d_w_mask_420_ssse3;
    c->blend = dav1d_blend_ssse3;
    c->blend_v = dav1d_blend_v_ssse3;
    c->blend_h = dav1d_blend_h_ssse3;

    c->warp8x8  = dav1d_warp_affine_8x8_ssse3;
    c->warp8x8t = dav1d_warp_affine_8x8t_ssse3;

    c->emu_edge = dav1d_emu_edge_ssse3;
    c->resize = dav1d_resize_ssse3;
#endif

    if(!(flags & DAV1D_X86_CPU_FLAG_SSE41))
        return;

#if BITDEPTH == 8
    c->warp8x8  = dav1d_warp_affine_8x8_sse4;
    c->warp8x8t = dav1d_warp_affine_8x8t_sse4;
#endif

#if ARCH_X86_64
    if (!(flags & DAV1D_X86_CPU_FLAG_AVX2))
        return;

    init_mc_fn(FILTER_2D_8TAP_REGULAR,        8tap_regular,        avx2);
    init_mc_fn(FILTER_2D_8TAP_REGULAR_SMOOTH, 8tap_regular_smooth, avx2);
    init_mc_fn(FILTER_2D_8TAP_REGULAR_SHARP,  8tap_regular_sharp,  avx2);
    init_mc_fn(FILTER_2D_8TAP_SMOOTH_REGULAR, 8tap_smooth_regular, avx2);
    init_mc_fn(FILTER_2D_8TAP_SMOOTH,         8tap_smooth,         avx2);
    init_mc_fn(FILTER_2D_8TAP_SMOOTH_SHARP,   8tap_smooth_sharp,   avx2);
    init_mc_fn(FILTER_2D_8TAP_SHARP_REGULAR,  8tap_sharp_regular,  avx2);
    init_mc_fn(FILTER_2D_8TAP_SHARP_SMOOTH,   8tap_sharp_smooth,   avx2);
    init_mc_fn(FILTER_2D_8TAP_SHARP,          8tap_sharp,          avx2);
    init_mc_fn(FILTER_2D_BILINEAR,            bilin,               avx2);

    init_mct_fn(FILTER_2D_8TAP_REGULAR,        8tap_regular,        avx2);
    init_mct_fn(FILTER_2D_8TAP_REGULAR_SMOOTH, 8tap_regular_smooth, avx2);
    init_mct_fn(FILTER_2D_8TAP_REGULAR_SHARP,  8tap_regular_sharp,  avx2);
    init_mct_fn(FILTER_2D_8TAP_SMOOTH_REGULAR, 8tap_smooth_regular, avx2);
    init_mct_fn(FILTER_2D_8TAP_SMOOTH,         8tap_smooth,         avx2);
    init_mct_fn(FILTER_2D_8TAP_SMOOTH_SHARP,   8tap_smooth_sharp,   avx2);
    init_mct_fn(FILTER_2D_8TAP_SHARP_REGULAR,  8tap_sharp_regular,  avx2);
    init_mct_fn(FILTER_2D_8TAP_SHARP_SMOOTH,   8tap_sharp_smooth,   avx2);
    init_mct_fn(FILTER_2D_8TAP_SHARP,          8tap_sharp,          avx2);
    init_mct_fn(FILTER_2D_BILINEAR,            bilin,               avx2);

#if BITDEPTH == 8
    init_mc_scaled_fn(FILTER_2D_8TAP_REGULAR,        8tap_scaled_regular,        avx2);
    init_mc_scaled_fn(FILTER_2D_8TAP_REGULAR_SMOOTH, 8tap_scaled_regular_smooth, avx2);
    init_mc_scaled_fn(FILTER_2D_8TAP_REGULAR_SHARP,  8tap_scaled_regular_sharp,  avx2);
    init_mc_scaled_fn(FILTER_2D_8TAP_SMOOTH_REGULAR, 8tap_scaled_smooth_regular, avx2);
    init_mc_scaled_fn(FILTER_2D_8TAP_SMOOTH,         8tap_scaled_smooth,         avx2);
    init_mc_scaled_fn(FILTER_2D_8TAP_SMOOTH_SHARP,   8tap_scaled_smooth_sharp,   avx2);
    init_mc_scaled_fn(FILTER_2D_8TAP_SHARP_REGULAR,  8tap_scaled_sharp_regular,  avx2);
    init_mc_scaled_fn(FILTER_2D_8TAP_SHARP_SMOOTH,   8tap_scaled_sharp_smooth,   avx2);
    init_mc_scaled_fn(FILTER_2D_8TAP_SHARP,          8tap_scaled_sharp,          avx2);
    init_mc_scaled_fn(FILTER_2D_BILINEAR,            bilin_scaled,               avx2);

    init_mct_scaled_fn(FILTER_2D_8TAP_REGULAR,        8tap_scaled_regular,        avx2);
    init_mct_scaled_fn(FILTER_2D_8TAP_REGULAR_SMOOTH, 8tap_scaled_regular_smooth, avx2);
    init_mct_scaled_fn(FILTER_2D_8TAP_REGULAR_SHARP,  8tap_scaled_regular_sharp,  avx2);
    init_mct_scaled_fn(FILTER_2D_8TAP_SMOOTH_REGULAR, 8tap_scaled_smooth_regular, avx2);
    init_mct_scaled_fn(FILTER_2D_8TAP_SMOOTH,         8tap_scaled_smooth,         avx2);
    init_mct_scaled_fn(FILTER_2D_8TAP_SMOOTH_SHARP,   8tap_scaled_smooth_sharp,   avx2);
    init_mct_scaled_fn(FILTER_2D_8TAP_SHARP_REGULAR,  8tap_scaled_sharp_regular,  avx2);
    init_mct_scaled_fn(FILTER_2D_8TAP_SHARP_SMOOTH,   8tap_scaled_sharp_smooth,   avx2);
    init_mct_scaled_fn(FILTER_2D_8TAP_SHARP,          8tap_scaled_sharp,          avx2);
    init_mct_scaled_fn(FILTER_2D_BILINEAR,            bilin_scaled,               avx2);

    c->avg = dav1d_avg_avx2;
    c->w_avg = dav1d_w_avg_avx2;
    c->mask = dav1d_mask_avx2;
    c->w_mask[0] = dav1d_w_mask_444_avx2;
    c->w_mask[1] = dav1d_w_mask_422_avx2;
    c->w_mask[2] = dav1d_w_mask_420_avx2;
    c->blend = dav1d_blend_avx2;
    c->blend_v = dav1d_blend_v_avx2;
    c->blend_h = dav1d_blend_h_avx2;

    c->warp8x8  = dav1d_warp_affine_8x8_avx2;
    c->warp8x8t = dav1d_warp_affine_8x8t_avx2;

    c->emu_edge = dav1d_emu_edge_avx2;
    c->resize = dav1d_resize_avx2;
#else
    c->avg = dav1d_avg_16bpc_avx2;
    c->w_avg = dav1d_w_avg_16bpc_avx2;
    c->mask = dav1d_mask_16bpc_avx2;
    c->w_mask[0] = dav1d_w_mask_444_16bpc_avx2;
    c->w_mask[1] = dav1d_w_mask_422_16bpc_avx2;
    c->w_mask[2] = dav1d_w_mask_420_16bpc_avx2;
    c->blend = dav1d_blend_16bpc_avx2;
    c->blend_v = dav1d_blend_v_16bpc_avx2;
    c->blend_h = dav1d_blend_h_16bpc_avx2;
    c->warp8x8  = dav1d_warp_affine_8x8_16bpc_avx2;
    c->warp8x8t = dav1d_warp_affine_8x8t_16bpc_avx2;
    c->emu_edge = dav1d_emu_edge_16bpc_avx2;
#endif

    if (!(flags & DAV1D_X86_CPU_FLAG_AVX512ICL))
        return;

#if HAVE_AVX512ICL && BITDEPTH == 8
    init_mct_fn(FILTER_2D_8TAP_REGULAR,        8tap_regular,        avx512icl);
    init_mct_fn(FILTER_2D_8TAP_REGULAR_SMOOTH, 8tap_regular_smooth, avx512icl);
    init_mct_fn(FILTER_2D_8TAP_REGULAR_SHARP,  8tap_regular_sharp,  avx512icl);
    init_mct_fn(FILTER_2D_8TAP_SMOOTH_REGULAR, 8tap_smooth_regular, avx512icl);
    init_mct_fn(FILTER_2D_8TAP_SMOOTH,         8tap_smooth,         avx512icl);
    init_mct_fn(FILTER_2D_8TAP_SMOOTH_SHARP,   8tap_smooth_sharp,   avx512icl);
    init_mct_fn(FILTER_2D_8TAP_SHARP_REGULAR,  8tap_sharp_regular,  avx512icl);
    init_mct_fn(FILTER_2D_8TAP_SHARP_SMOOTH,   8tap_sharp_smooth,   avx512icl);
    init_mct_fn(FILTER_2D_8TAP_SHARP,          8tap_sharp,          avx512icl);
    init_mct_fn(FILTER_2D_BILINEAR,            bilin,               avx512icl);

    c->avg = dav1d_avg_avx512icl;
    c->w_avg = dav1d_w_avg_avx512icl;
    c->mask = dav1d_mask_avx512icl;
    c->w_mask[0] = dav1d_w_mask_444_avx512icl;
    c->w_mask[1] = dav1d_w_mask_422_avx512icl;
    c->w_mask[2] = dav1d_w_mask_420_avx512icl;
#endif
#endif
}
