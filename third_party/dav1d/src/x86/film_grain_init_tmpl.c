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
#include "src/film_grain.h"

decl_generate_grain_y_fn(dav1d_generate_grain_y_ssse3);
decl_generate_grain_uv_fn(dav1d_generate_grain_uv_420_ssse3);
decl_generate_grain_uv_fn(dav1d_generate_grain_uv_422_ssse3);
decl_generate_grain_uv_fn(dav1d_generate_grain_uv_444_ssse3);
decl_fgy_32x32xn_fn(dav1d_fgy_32x32xn_ssse3);
decl_fguv_32x32xn_fn(dav1d_fguv_32x32xn_i420_ssse3);
decl_fguv_32x32xn_fn(dav1d_fguv_32x32xn_i422_ssse3);
decl_fguv_32x32xn_fn(dav1d_fguv_32x32xn_i444_ssse3);

decl_generate_grain_y_fn(dav1d_generate_grain_y_avx2);
decl_generate_grain_uv_fn(dav1d_generate_grain_uv_420_avx2);
decl_generate_grain_uv_fn(dav1d_generate_grain_uv_422_avx2);
decl_generate_grain_uv_fn(dav1d_generate_grain_uv_444_avx2);
decl_fgy_32x32xn_fn(dav1d_fgy_32x32xn_avx2);
decl_fguv_32x32xn_fn(dav1d_fguv_32x32xn_i420_avx2);
decl_fguv_32x32xn_fn(dav1d_fguv_32x32xn_i422_avx2);
decl_fguv_32x32xn_fn(dav1d_fguv_32x32xn_i444_avx2);

decl_generate_grain_y_fn(dav1d_generate_grain_y_16bpc_avx2);
decl_generate_grain_uv_fn(dav1d_generate_grain_uv_420_16bpc_avx2);
decl_fgy_32x32xn_fn(dav1d_fgy_32x32xn_16bpc_avx2);
decl_fguv_32x32xn_fn(dav1d_fguv_32x32xn_i420_16bpc_avx2);

COLD void bitfn(dav1d_film_grain_dsp_init_x86)(Dav1dFilmGrainDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();

    if (!(flags & DAV1D_X86_CPU_FLAG_SSSE3)) return;

#if BITDEPTH == 8
    c->generate_grain_y = dav1d_generate_grain_y_ssse3;
    c->generate_grain_uv[DAV1D_PIXEL_LAYOUT_I420 - 1] = dav1d_generate_grain_uv_420_ssse3;
    c->generate_grain_uv[DAV1D_PIXEL_LAYOUT_I422 - 1] = dav1d_generate_grain_uv_422_ssse3;
    c->generate_grain_uv[DAV1D_PIXEL_LAYOUT_I444 - 1] = dav1d_generate_grain_uv_444_ssse3;
    c->fgy_32x32xn = dav1d_fgy_32x32xn_ssse3;
    c->fguv_32x32xn[DAV1D_PIXEL_LAYOUT_I420 - 1] = dav1d_fguv_32x32xn_i420_ssse3;
    c->fguv_32x32xn[DAV1D_PIXEL_LAYOUT_I422 - 1] = dav1d_fguv_32x32xn_i422_ssse3;
    c->fguv_32x32xn[DAV1D_PIXEL_LAYOUT_I444 - 1] = dav1d_fguv_32x32xn_i444_ssse3;
#endif

#if ARCH_X86_64
    if (!(flags & DAV1D_X86_CPU_FLAG_AVX2)) return;

#if BITDEPTH == 8
    c->generate_grain_y = dav1d_generate_grain_y_avx2;
    c->generate_grain_uv[DAV1D_PIXEL_LAYOUT_I420 - 1] = dav1d_generate_grain_uv_420_avx2;
    c->generate_grain_uv[DAV1D_PIXEL_LAYOUT_I422 - 1] = dav1d_generate_grain_uv_422_avx2;
    c->generate_grain_uv[DAV1D_PIXEL_LAYOUT_I444 - 1] = dav1d_generate_grain_uv_444_avx2;
    c->fgy_32x32xn = dav1d_fgy_32x32xn_avx2;
    c->fguv_32x32xn[DAV1D_PIXEL_LAYOUT_I420 - 1] = dav1d_fguv_32x32xn_i420_avx2;
    c->fguv_32x32xn[DAV1D_PIXEL_LAYOUT_I422 - 1] = dav1d_fguv_32x32xn_i422_avx2;
    c->fguv_32x32xn[DAV1D_PIXEL_LAYOUT_I444 - 1] = dav1d_fguv_32x32xn_i444_avx2;
#else
    c->generate_grain_y = dav1d_generate_grain_y_16bpc_avx2;
    c->generate_grain_uv[DAV1D_PIXEL_LAYOUT_I420 - 1] =
        dav1d_generate_grain_uv_420_16bpc_avx2;
    c->fgy_32x32xn = dav1d_fgy_32x32xn_16bpc_avx2;
    c->fguv_32x32xn[DAV1D_PIXEL_LAYOUT_I420 - 1] =
        dav1d_fguv_32x32xn_i420_16bpc_avx2;
#endif
#endif
}
