/*
 * Copyright © 2018, Niklas Haas
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * Copyright © 2021, Martin Storsjo
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

#if BITDEPTH == 8 && ARCH_AARCH64

// Use ptrdiff_t instead of int for the last few parameters, to get the
// same layout of parameters on the stack across platforms.
#define FGY(suff) \
void BF(dav1d_fgy_32x32_ ## suff, neon)(pixel *const dst, \
                                        const pixel *const src, \
                                        const ptrdiff_t stride, \
                                        const uint8_t scaling[SCALING_SIZE], \
                                        const int scaling_shift, \
                                        const entry grain_lut[][GRAIN_WIDTH], \
                                        const int offsets[][2], \
                                        const int h, const ptrdiff_t clip \
                                        HIGHBD_DECL_SUFFIX)

FGY(00);
FGY(01);
FGY(10);
FGY(11);

static inline int get_random_number(const int bits, unsigned *const state) {
    const int r = *state;
    unsigned bit = ((r >> 0) ^ (r >> 1) ^ (r >> 3) ^ (r >> 12)) & 1;
    *state = (r >> 1) | (bit << 15);

    return (*state >> (16 - bits)) & ((1 << bits) - 1);
}

static void fgy_32x32xn_neon(pixel *const dst_row, const pixel *const src_row,
                             const ptrdiff_t stride,
                             const Dav1dFilmGrainData *const data, const size_t pw,
                             const uint8_t scaling[SCALING_SIZE],
                             const entry grain_lut[][GRAIN_WIDTH],
                             const int bh, const int row_num HIGHBD_DECL_SUFFIX)
{
    const int rows = 1 + (data->overlap_flag && row_num > 0);

    // seed[0] contains the current row, seed[1] contains the previous
    unsigned seed[2];
    for (int i = 0; i < rows; i++) {
        seed[i] = data->seed;
        seed[i] ^= (((row_num - i) * 37  + 178) & 0xFF) << 8;
        seed[i] ^= (((row_num - i) * 173 + 105) & 0xFF);
    }

    int offsets[2 /* col offset */][2 /* row offset */];

    // process this row in BLOCK_SIZE^2 blocks
    for (unsigned bx = 0; bx < pw; bx += BLOCK_SIZE) {

        if (data->overlap_flag && bx) {
            // shift previous offsets left
            for (int i = 0; i < rows; i++)
                offsets[1][i] = offsets[0][i];
        }

        // update current offsets
        for (int i = 0; i < rows; i++)
            offsets[0][i] = get_random_number(8, &seed[i]);

        if (data->overlap_flag && row_num) {
            if (data->overlap_flag && bx)
                BF(dav1d_fgy_32x32_11, neon)(dst_row + bx, src_row + bx, stride,
                                             scaling, data->scaling_shift,
                                             grain_lut, offsets, bh,
                                             data->clip_to_restricted_range
                                             HIGHBD_TAIL_SUFFIX);
            else
                BF(dav1d_fgy_32x32_01, neon)(dst_row + bx, src_row + bx, stride,
                                             scaling, data->scaling_shift,
                                             grain_lut, offsets, bh,
                                             data->clip_to_restricted_range
                                             HIGHBD_TAIL_SUFFIX);
        } else {
            if (data->overlap_flag && bx)
                BF(dav1d_fgy_32x32_10, neon)(dst_row + bx, src_row + bx, stride,
                                             scaling, data->scaling_shift,
                                             grain_lut, offsets, bh,
                                             data->clip_to_restricted_range
                                             HIGHBD_TAIL_SUFFIX);
            else
                BF(dav1d_fgy_32x32_00, neon)(dst_row + bx, src_row + bx, stride,
                                             scaling, data->scaling_shift,
                                             grain_lut, offsets, bh,
                                             data->clip_to_restricted_range
                                             HIGHBD_TAIL_SUFFIX);
        }
    }
}

#endif

COLD void bitfn(dav1d_film_grain_dsp_init_arm)(Dav1dFilmGrainDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();

    if (!(flags & DAV1D_ARM_CPU_FLAG_NEON)) return;

#if BITDEPTH == 8 && ARCH_AARCH64
    c->fgy_32x32xn = fgy_32x32xn_neon;
#endif
}
