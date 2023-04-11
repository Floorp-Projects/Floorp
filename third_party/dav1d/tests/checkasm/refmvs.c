/*
 * Copyright © 2021, VideoLAN and dav1d authors
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
#include "src/refmvs.h"

#include <stdio.h>

static inline int gen_mv(const int total_bits, int spel_bits) {
    int bits = rnd() & ((1 << spel_bits) - 1);
    do {
        bits |= (rnd() & 1) << spel_bits;
    } while (rnd() & 1 && ++spel_bits < total_bits);
    // the do/while makes it relatively more likely to be close to zero (fpel)
    // than far away
    return rnd() & 1 ? -bits : bits;
}

static void check_save_tmvs(const Dav1dRefmvsDSPContext *const c) {
    refmvs_block *rr[31];
    refmvs_block r[31 * 256];
    ALIGN_STK_64(refmvs_temporal_block, c_rp, 128 * 16,);
    ALIGN_STK_64(refmvs_temporal_block, a_rp, 128 * 16,);
    uint8_t ref_sign[7];

    for (int i = 0; i < 31; i++)
        rr[i] = &r[i * 256];

    declare_func(void, refmvs_temporal_block *rp, const ptrdiff_t stride,
                 refmvs_block *const *const rr, const uint8_t *const ref_sign,
                 int col_end8, int row_end8, int col_start8, int row_start8);

    if (check_func(c->save_tmvs, "save_tmvs")) {
        const int row_start8 = rnd() & 7;
        const int row_end8 = 8 + (rnd() & 7);
        const int col_start8 = rnd() & 31;
        const int col_end8 = 96 + (rnd() & 31);

        for (int i = 0; i < 7; i++)
            ref_sign[i] = rnd() & 1;

        for (int i = row_start8; i < row_end8; i++)
            for (int j = col_start8; j < col_end8;) {
                int bs = rnd() % N_BS_SIZES;
                while (j + ((dav1d_block_dimensions[bs][0] + 1) >> 1) > col_end8)
                    bs++;
                rr[i * 2][j * 2 + 1] = (refmvs_block) {
                    .mv.mv[0].x = gen_mv(14, 10),
                    .mv.mv[0].y = gen_mv(14, 10),
                    .mv.mv[1].x = gen_mv(14, 10),
                    .mv.mv[1].y = gen_mv(14, 10),
                    .ref.ref = { (rnd() % 9) - 1, (rnd() % 9) - 1 },
                    .bs = bs
                };
                for (int k = 0; k < (dav1d_block_dimensions[bs][0] + 1) >> 1; k++, j++) {
                    c_rp[i * 128 + j].mv.n = 0xdeadbeef;
                    c_rp[i * 128 + j].ref = 0xdd;
                }
            }

        call_ref(c_rp + row_start8 * 128, 128, rr, ref_sign,
                 col_end8, row_end8, col_start8, row_start8);
        call_new(a_rp + row_start8 * 128, 128, rr, ref_sign,
                 col_end8, row_end8, col_start8, row_start8);
        for (int i = row_start8; i < row_end8; i++)
            for (int j = col_start8; j < col_end8; j++)
                if (c_rp[i * 128 + j].mv.n != a_rp[i * 128 + j].mv.n ||
                    c_rp[i * 128 + j].ref != a_rp[i * 128 + j].ref)
                {
                    if (fail()) {
                        fprintf(stderr, "[%d][%d] c_rp.mv.x = 0x%x a_rp.mv.x = 0x%x\n",
                                i, j, c_rp[i * 128 + j].mv.x, a_rp[i * 128 + j].mv.x);
                        fprintf(stderr, "[%d][%d] c_rp.mv.y = 0x%x a_rp.mv.y = 0x%x\n",
                                i, j, c_rp[i * 128 + j].mv.y, a_rp[i * 128 + j].mv.y);
                        fprintf(stderr, "[%d][%d] c_rp.ref = %u a_rp.ref = %u\n",
                                i, j, c_rp[i * 128 + j].ref, a_rp[i * 128 + j].ref);
                    }
                }

        for (int bs = BS_4x4; bs < N_BS_SIZES; bs++) {
            const int bw8 = (dav1d_block_dimensions[bs][0] + 1) >> 1;
            for (int i = 0; i < 16; i++)
                for (int j = 0; j < 128; j += bw8) {
                    rr[i * 2][j * 2 + 1].ref.ref[0] = (rnd() % 9) - 1;
                    rr[i * 2][j * 2 + 1].ref.ref[1] = (rnd() % 9) - 1;
                    rr[i * 2][j * 2 + 1].bs = bs;
                }
            bench_new(alternate(c_rp, a_rp), 128, rr, ref_sign, 128, 16, 0, 0);
        }
    }

    report("save_tmvs");
}

static void check_splat_mv(const Dav1dRefmvsDSPContext *const c) {
    ALIGN_STK_64(refmvs_block, c_buf, 32 * 32,);
    ALIGN_STK_64(refmvs_block, a_buf, 32 * 32,);
    refmvs_block *c_dst[32];
    refmvs_block *a_dst[32];
    const size_t stride = 32 * sizeof(refmvs_block);

    for (int i = 0; i < 32; i++) {
        c_dst[i] = c_buf + 32 * i;
        a_dst[i] = a_buf + 32 * i;
    }

    declare_func(void, refmvs_block **rr, const refmvs_block *rmv,
                 int bx4, int bw4, int bh4);

    for (int w = 1; w <= 32; w *= 2) {
        if (check_func(c->splat_mv, "splat_mv_w%d", w)) {
            const int h_min = imax(w / 4, 1);
            const int h_max = imin(w * 4, 32);
            const int w_uint32 = w * sizeof(refmvs_block) / sizeof(uint32_t);
            for (int h = h_min; h <= h_max; h *= 2) {
                const int offset = (int) ((unsigned) w * rnd()) & 31;
                union {
                    refmvs_block rmv;
                    uint32_t u32[3];
                } ALIGN(tmp, 16);
                tmp.u32[0] = rnd();
                tmp.u32[1] = rnd();
                tmp.u32[2] = rnd();

                call_ref(c_dst, &tmp.rmv, offset, w, h);
                call_new(a_dst, &tmp.rmv, offset, w, h);
                checkasm_check(uint32_t, (uint32_t*)(c_buf + offset), stride,
                                         (uint32_t*)(a_buf + offset), stride,
                                         w_uint32, h, "dst");

                bench_new(a_dst, &tmp.rmv, 0, w, h);
            }
        }
    }
    report("splat_mv");
}

void checkasm_check_refmvs(void) {
    Dav1dRefmvsDSPContext c;
    dav1d_refmvs_dsp_init(&c);

    check_save_tmvs(&c);
    check_splat_mv(&c);
}
