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

#include "config.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "common/attributes.h"
#include "common/intops.h"

#include "src/itx.h"

#include "src/itx_1d.c"

typedef void (*itx_1d_fn)(const coef *in, ptrdiff_t in_s,
                          coef *out, ptrdiff_t out_s, const int range);

static void NOINLINE
inv_txfm_add_c(pixel *dst, const ptrdiff_t stride,
               coef *const coeff, const int eob,
               const int w, const int h, const int shift,
               const itx_1d_fn first_1d_fn, const itx_1d_fn second_1d_fn,
               const int has_dconly HIGHBD_DECL_SUFFIX)
{
    int i, j;
    assert((h >= 4 && h <= 64) && (w >= 4 && w <= 64));
    const int is_rect2 = w * 2 == h || h * 2 == w;
    const int bitdepth = bitdepth_from_max(bitdepth_max);
    const int rnd = (1 << shift) >> 1;

    if (has_dconly && eob == 0) {
        int dc = coeff[0];
        coeff[0] = 0;
        if (is_rect2)
            dc = (dc * 2896 + 2048) >> 12;
        dc = (dc * 2896 + 2048) >> 12;
        dc = (dc + rnd) >> shift;
        dc = (dc * 2896 + 2048) >> 12;
        dc = (dc + 8) >> 4;
        for (j = 0; j < h; j++)
            for (i = 0; i < w; i++)
                dst[i + j * PXSTRIDE(stride)] =
                    iclip_pixel(dst[i + j * PXSTRIDE(stride)] + dc);
        return;
    }
    assert(eob > 0 || (eob == 0 && !has_dconly));

    const ptrdiff_t sh = imin(h, 32), sw = imin(w, 32);
    // Maximum value for h and w is 64
    coef tmp[4096 /* w * h */], out[64 /* h */], in_mem[64 /* w */];
    const int row_clip_max = (1 << (bitdepth + 8 - 1)) - 1;
    const int col_clip_max = (1 << (imax(bitdepth + 6, 16) - 1)) -1;

    if (w != sw) memset(&in_mem[sw], 0, (w - sw) * sizeof(*in_mem));
    for (i = 0; i < sh; i++) {
        if (w != sw || is_rect2) {
            for (j = 0; j < sw; j++) {
                in_mem[j] = coeff[i + j * sh];
                if (is_rect2)
                    in_mem[j] = (in_mem[j] * 2896 + 2048) >> 12;
            }
            first_1d_fn(in_mem, 1, &tmp[i * w], 1, row_clip_max);
        } else {
            first_1d_fn(&coeff[i], sh, &tmp[i * w], 1, row_clip_max);
        }
        for (j = 0; j < w; j++)
#if BITDEPTH == 8
            tmp[i * w + j] = (tmp[i * w + j] + rnd) >> shift;
#else
            tmp[i * w + j] = iclip((tmp[i * w + j] + rnd) >> shift,
                                   -col_clip_max - 1, col_clip_max);
#endif
    }

    if (h != sh) memset(&tmp[sh * w], 0, w * (h - sh) * sizeof(*tmp));
    for (i = 0; i < w; i++) {
        second_1d_fn(&tmp[i], w, out, 1, col_clip_max);
        for (j = 0; j < h; j++)
            dst[i + j * PXSTRIDE(stride)] =
                iclip_pixel(dst[i + j * PXSTRIDE(stride)] +
                            ((out[j] + 8) >> 4));
    }
    memset(coeff, 0, sizeof(*coeff) * sh * sw);
}

#define inv_txfm_fn(type1, type2, w, h, shift, has_dconly) \
static void \
inv_txfm_add_##type1##_##type2##_##w##x##h##_c(pixel *dst, \
                                               const ptrdiff_t stride, \
                                               coef *const coeff, \
                                               const int eob \
                                               HIGHBD_DECL_SUFFIX) \
{ \
    inv_txfm_add_c(dst, stride, coeff, eob, w, h, shift, \
                   inv_##type1##w##_1d, inv_##type2##h##_1d, has_dconly \
                   HIGHBD_TAIL_SUFFIX); \
}

#define inv_txfm_fn64(w, h, shift) \
inv_txfm_fn(dct, dct, w, h, shift, 1)

#define inv_txfm_fn32(w, h, shift) \
inv_txfm_fn64(w, h, shift) \
inv_txfm_fn(identity, identity, w, h, shift, 0)

#define inv_txfm_fn16(w, h, shift) \
inv_txfm_fn32(w, h, shift) \
inv_txfm_fn(adst,     dct,      w, h, shift, 0) \
inv_txfm_fn(dct,      adst,     w, h, shift, 0) \
inv_txfm_fn(adst,     adst,     w, h, shift, 0) \
inv_txfm_fn(dct,      flipadst, w, h, shift, 0) \
inv_txfm_fn(flipadst, dct,      w, h, shift, 0) \
inv_txfm_fn(adst,     flipadst, w, h, shift, 0) \
inv_txfm_fn(flipadst, adst,     w, h, shift, 0) \
inv_txfm_fn(flipadst, flipadst, w, h, shift, 0) \
inv_txfm_fn(identity, dct,      w, h, shift, 0) \
inv_txfm_fn(dct,      identity, w, h, shift, 0) \

#define inv_txfm_fn84(w, h, shift) \
inv_txfm_fn16(w, h, shift) \
inv_txfm_fn(identity, flipadst, w, h, shift, 0) \
inv_txfm_fn(flipadst, identity, w, h, shift, 0) \
inv_txfm_fn(identity, adst,     w, h, shift, 0) \
inv_txfm_fn(adst,     identity, w, h, shift, 0) \

inv_txfm_fn84( 4,  4, 0)
inv_txfm_fn84( 4,  8, 0)
inv_txfm_fn84( 4, 16, 1)
inv_txfm_fn84( 8,  4, 0)
inv_txfm_fn84( 8,  8, 1)
inv_txfm_fn84( 8, 16, 1)
inv_txfm_fn32( 8, 32, 2)
inv_txfm_fn84(16,  4, 1)
inv_txfm_fn84(16,  8, 1)
inv_txfm_fn16(16, 16, 2)
inv_txfm_fn32(16, 32, 1)
inv_txfm_fn64(16, 64, 2)
inv_txfm_fn32(32,  8, 2)
inv_txfm_fn32(32, 16, 1)
inv_txfm_fn32(32, 32, 2)
inv_txfm_fn64(32, 64, 1)
inv_txfm_fn64(64, 16, 2)
inv_txfm_fn64(64, 32, 1)
inv_txfm_fn64(64, 64, 2)

static void inv_txfm_add_wht_wht_4x4_c(pixel *dst, const ptrdiff_t stride,
                                       coef *const coeff, const int eob
                                       HIGHBD_DECL_SUFFIX)
{
    const int bitdepth = bitdepth_from_max(bitdepth_max);
    const int col_clip_max = (1 << (imax(bitdepth + 6, 16) - 1)) -1;
    const int col_clip_min = -col_clip_max - 1;
    coef tmp[4 * 4], out[4];

    for (int i = 0; i < 4; i++)
        inv_wht4_1d(&coeff[i], 4, &tmp[i * 4], 1, 0);
    for (int k = 0; k < 4 * 4; k++)
        tmp[k] = iclip(tmp[k], col_clip_min, col_clip_max);

    for (int i = 0; i < 4; i++) {
        inv_wht4_1d(&tmp[i], 4, out, 1, 1);
        for (int j = 0; j < 4; j++)
            dst[i + j * PXSTRIDE(stride)] =
                iclip_pixel(dst[i + j * PXSTRIDE(stride)] + out[j]);
    }
    memset(coeff, 0, sizeof(*coeff) * 4 * 4);
}

void bitfn(dav1d_itx_dsp_init)(Dav1dInvTxfmDSPContext *const c) {
#define assign_itx_all_fn64(w, h, pfx) \
    c->itxfm_add[pfx##TX_##w##X##h][DCT_DCT  ] = \
        inv_txfm_add_dct_dct_##w##x##h##_c

#define assign_itx_all_fn32(w, h, pfx) \
    assign_itx_all_fn64(w, h, pfx); \
    c->itxfm_add[pfx##TX_##w##X##h][IDTX] = \
        inv_txfm_add_identity_identity_##w##x##h##_c

#define assign_itx_all_fn16(w, h, pfx) \
    assign_itx_all_fn32(w, h, pfx); \
    c->itxfm_add[pfx##TX_##w##X##h][DCT_ADST ] = \
        inv_txfm_add_adst_dct_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][ADST_DCT ] = \
        inv_txfm_add_dct_adst_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][ADST_ADST] = \
        inv_txfm_add_adst_adst_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][ADST_FLIPADST] = \
        inv_txfm_add_flipadst_adst_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][FLIPADST_ADST] = \
        inv_txfm_add_adst_flipadst_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][DCT_FLIPADST] = \
        inv_txfm_add_flipadst_dct_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][FLIPADST_DCT] = \
        inv_txfm_add_dct_flipadst_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][FLIPADST_FLIPADST] = \
        inv_txfm_add_flipadst_flipadst_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][H_DCT] = \
        inv_txfm_add_dct_identity_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][V_DCT] = \
        inv_txfm_add_identity_dct_##w##x##h##_c

#define assign_itx_all_fn84(w, h, pfx) \
    assign_itx_all_fn16(w, h, pfx); \
    c->itxfm_add[pfx##TX_##w##X##h][H_FLIPADST] = \
        inv_txfm_add_flipadst_identity_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][V_FLIPADST] = \
        inv_txfm_add_identity_flipadst_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][H_ADST] = \
        inv_txfm_add_adst_identity_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][V_ADST] = \
        inv_txfm_add_identity_adst_##w##x##h##_c; \

    memset(c, 0, sizeof(*c)); /* Zero unused function pointer elements. */

    c->itxfm_add[TX_4X4][WHT_WHT] = inv_txfm_add_wht_wht_4x4_c;
    assign_itx_all_fn84( 4,  4, );
    assign_itx_all_fn84( 4,  8, R);
    assign_itx_all_fn84( 4, 16, R);
    assign_itx_all_fn84( 8,  4, R);
    assign_itx_all_fn84( 8,  8, );
    assign_itx_all_fn84( 8, 16, R);
    assign_itx_all_fn32( 8, 32, R);
    assign_itx_all_fn84(16,  4, R);
    assign_itx_all_fn84(16,  8, R);
    assign_itx_all_fn16(16, 16, );
    assign_itx_all_fn32(16, 32, R);
    assign_itx_all_fn64(16, 64, R);
    assign_itx_all_fn32(32,  8, R);
    assign_itx_all_fn32(32, 16, R);
    assign_itx_all_fn32(32, 32, );
    assign_itx_all_fn64(32, 64, R);
    assign_itx_all_fn64(64, 16, R);
    assign_itx_all_fn64(64, 32, R);
    assign_itx_all_fn64(64, 64, );

#if HAVE_ASM && ARCH_X86
    bitfn(dav1d_itx_dsp_init_x86)(c);
#endif
}
