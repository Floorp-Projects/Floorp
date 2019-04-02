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

#include "common/attributes.h"
#include "src/cpu.h"
#include "src/cdef.h"

#if BITDEPTH == 8 && ARCH_AARCH64
decl_cdef_dir_fn(dav1d_cdef_find_dir_neon);

void dav1d_cdef_padding4_neon(uint16_t *tmp, const pixel *src,
                              ptrdiff_t src_stride, const pixel (*left)[2],
                              /*const*/ pixel *const top[2], int h,
                              enum CdefEdgeFlags edges);
void dav1d_cdef_padding8_neon(uint16_t *tmp, const pixel *src,
                              ptrdiff_t src_stride, const pixel (*left)[2],
                              /*const*/ pixel *const top[2], int h,
                              enum CdefEdgeFlags edges);

void dav1d_cdef_filter4_neon(pixel *dst, ptrdiff_t dst_stride,
                             const uint16_t *tmp, int pri_strength,
                             int sec_strength, int dir, int damping, int h);
void dav1d_cdef_filter8_neon(pixel *dst, ptrdiff_t dst_stride,
                             const uint16_t *tmp, int pri_strength,
                             int sec_strength, int dir, int damping, int h);

#define DEFINE_FILTER(w, h, tmp_stride)                                      \
static void                                                                  \
cdef_filter_##w##x##h##_neon(pixel *dst,                                     \
                             const ptrdiff_t stride,                         \
                             const pixel (*left)[2],                         \
                             /*const*/ pixel *const top[2],                  \
                             const int pri_strength,                         \
                             const int sec_strength,                         \
                             const int dir,                                  \
                             const int damping,                              \
                             const enum CdefEdgeFlags edges)                 \
{                                                                            \
    ALIGN_STK_16(uint16_t, tmp_buf, 12*tmp_stride,);                         \
    uint16_t *tmp = tmp_buf + 2 * tmp_stride + 2;                            \
    dav1d_cdef_padding##w##_neon(tmp, dst, stride, left, top, h, edges);     \
    dav1d_cdef_filter##w##_neon(dst, stride, tmp, pri_strength,              \
                                sec_strength, dir, damping, h);              \
}

DEFINE_FILTER(8, 8, 16)
DEFINE_FILTER(4, 8, 8)
DEFINE_FILTER(4, 4, 8)
#endif


void bitfn(dav1d_cdef_dsp_init_arm)(Dav1dCdefDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();

    if (!(flags & DAV1D_ARM_CPU_FLAG_NEON)) return;

#if BITDEPTH == 8 && ARCH_AARCH64
    c->dir = dav1d_cdef_find_dir_neon;
    c->fb[0] = cdef_filter_8x8_neon;
    c->fb[1] = cdef_filter_4x8_neon;
    c->fb[2] = cdef_filter_4x4_neon;
#endif
}
