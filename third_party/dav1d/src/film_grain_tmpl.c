/*
 * Copyright © 2018, Niklas Haas
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
#include <stdint.h>

#include "common.h"
#include "common/intops.h"
#include "common/bitdepth.h"
#include "tables.h"

#include "film_grain.h"

#if BITDEPTH == 8
typedef int8_t entry;
#else
typedef int16_t entry;
#endif

enum {
    GRAIN_WIDTH  = 82,
    GRAIN_HEIGHT = 73,
    SUB_GRAIN_WIDTH = 44,
    SUB_GRAIN_HEIGHT = 38,
    SUB_GRAIN_OFFSET = 6,
    BLOCK_SIZE = 32,
#if BITDEPTH == 8
    SCALING_SIZE = 256
#else
    SCALING_SIZE = 4096
#endif
};

static inline int get_random_number(const int bits, unsigned *state) {
    const int r = *state;
    unsigned bit = ((r >> 0) ^ (r >> 1) ^ (r >> 3) ^ (r >> 12)) & 1;
    *state = (r >> 1) | (bit << 15);

    return (*state >> (16 - bits)) & ((1 << bits) - 1);
}

static inline int round2(const int x, const int shift) {
    return (x + ((1 << shift) >> 1)) >> shift;
}

static void generate_grain_y(const Dav1dPicture *const in,
                             entry buf[GRAIN_HEIGHT][GRAIN_WIDTH])
{
    const Dav1dFilmGrainData *data = &in->frame_hdr->film_grain.data;
    unsigned seed = data->seed;
    const int shift = 12 - in->p.bpc + data->grain_scale_shift;
    const int grain_ctr = 128 << (in->p.bpc - 8);
    const int grain_min = -grain_ctr, grain_max = grain_ctr - 1;

    for (int y = 0; y < GRAIN_HEIGHT; y++) {
        for (int x = 0; x < GRAIN_WIDTH; x++) {
            const int value = get_random_number(11, &seed);
            buf[y][x] = round2(dav1d_gaussian_sequence[ value ], shift);
        }
    }

    const int ar_pad = 3;
    const int ar_lag = data->ar_coeff_lag;

    for (int y = ar_pad; y < GRAIN_HEIGHT; y++) {
        for (int x = ar_pad; x < GRAIN_WIDTH - ar_pad; x++) {
            const int8_t *coeff = data->ar_coeffs_y;
            int sum = 0;
            for (int dy = -ar_lag; dy <= 0; dy++) {
                for (int dx = -ar_lag; dx <= ar_lag; dx++) {
                    if (!dx && !dy)
                        break;
                    sum += *(coeff++) * buf[y + dy][x + dx];
                }
            }

            int grain = buf[y][x] + round2(sum, data->ar_coeff_shift);
            buf[y][x] = iclip(grain, grain_min, grain_max);
        }
    }
}

static void generate_grain_uv(const Dav1dPicture *const in, int uv,
                              entry buf[GRAIN_HEIGHT][GRAIN_WIDTH],
                              entry buf_y[GRAIN_HEIGHT][GRAIN_WIDTH])
{
    const Dav1dFilmGrainData *data = &in->frame_hdr->film_grain.data;
    unsigned seed = data->seed ^ (uv ? 0x49d8 : 0xb524);
    const int shift = 12 - in->p.bpc + data->grain_scale_shift;
    const int grain_ctr = 128 << (in->p.bpc - 8);
    const int grain_min = -grain_ctr, grain_max = grain_ctr - 1;

    const int subx = in->p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int suby = in->p.layout == DAV1D_PIXEL_LAYOUT_I420;

    const int chromaW = subx ? SUB_GRAIN_WIDTH  : GRAIN_WIDTH;
    const int chromaH = suby ? SUB_GRAIN_HEIGHT : GRAIN_HEIGHT;

    for (int y = 0; y < chromaH; y++) {
        for (int x = 0; x < chromaW; x++) {
            const int value = get_random_number(11, &seed);
            buf[y][x] = round2(dav1d_gaussian_sequence[ value ], shift);
        }
    }

    const int ar_pad = 3;
    const int ar_lag = data->ar_coeff_lag;

    for (int y = ar_pad; y < chromaH; y++) {
        for (int x = ar_pad; x < chromaW - ar_pad; x++) {
            const int8_t *coeff = data->ar_coeffs_uv[uv];
            int sum = 0;
            for (int dy = -ar_lag; dy <= 0; dy++) {
                for (int dx = -ar_lag; dx <= ar_lag; dx++) {
                    // For the final (current) pixel, we need to add in the
                    // contribution from the luma grain texture
                    if (!dx && !dy) {
                        if (!data->num_y_points)
                            break;
                        int luma = 0;
                        const int lumaX = ((x - ar_pad) << subx) + ar_pad;
                        const int lumaY = ((y - ar_pad) << suby) + ar_pad;
                        for (int i = 0; i <= suby; i++) {
                            for (int j = 0; j <= subx; j++) {
                                luma += buf_y[lumaY + i][lumaX + j];
                            }
                        }
                        luma = round2(luma, subx + suby);
                        sum += luma * (*coeff);
                        break;
                    }

                    sum += *(coeff++) * buf[y + dy][x + dx];
                }
            }

            const int grain = buf[y][x] + round2(sum, data->ar_coeff_shift);
            buf[y][x] = iclip(grain, grain_min, grain_max);
        }
    }
}

static void generate_scaling(const int bitdepth,
                             const uint8_t points[][2], int num,
                             uint8_t scaling[SCALING_SIZE])
{
    const int shift_x = bitdepth - 8;
    const int scaling_size = 1 << bitdepth;

    // Fill up the preceding entries with the initial value
    for (int i = 0; i < points[0][0] << shift_x; i++)
        scaling[i] = points[0][1];

    // Linearly interpolate the values in the middle
    for (int i = 0; i < num - 1; i++) {
        const int bx = points[i][0] << shift_x;
        const int by = points[i][1];
        const int ex = points[i+1][0] << shift_x;
        const int ey = points[i+1][1];
        const int dx = ex - bx;
        const int dy = ey - by;
        const int delta = dy * ((0xFFFF + (dx >> 1))) / dx;
        for (int x = 0; x < dx; x++) {
            const int v = by + ((x * delta + 0x8000) >> 16);
            scaling[bx + x] = v;
        }
    }

    // Fill up the remaining entries with the final value
    for (int i = points[num - 1][0] << shift_x; i < scaling_size; i++)
        scaling[i] = points[num - 1][1];
}

// samples from the correct block of a grain LUT, while taking into account the
// offsets provided by the offsets cache
static inline entry sample_lut(entry grain_lut[GRAIN_HEIGHT][GRAIN_WIDTH],
                               int offsets[2][2], int subx, int suby,
                               int bx, int by, int x, int y)
{
    const int randval = offsets[bx][by];
    const int offx = 3 + (2 >> subx) * (3 + (randval >> 4));
    const int offy = 3 + (2 >> suby) * (3 + (randval & 0xF));
    return grain_lut[offy + y + (BLOCK_SIZE >> suby) * by]
                    [offx + x + (BLOCK_SIZE >> subx) * bx];
}

static void apply_to_row_y(Dav1dPicture *const out, const Dav1dPicture *const in,
                           entry grain_lut[GRAIN_HEIGHT][GRAIN_WIDTH],
                           uint8_t scaling[SCALING_SIZE], int row_num)
{
    const Dav1dFilmGrainData *const data = &out->frame_hdr->film_grain.data;
    const int rows = 1 + (data->overlap_flag && row_num > 0);
    const int bitdepth_min_8 = in->p.bpc - 8;
    const int grain_ctr = 128 << bitdepth_min_8;
    const int grain_min = -grain_ctr, grain_max = grain_ctr - 1;

    int min_value, max_value;
    if (data->clip_to_restricted_range) {
        min_value = 16 << bitdepth_min_8;
        max_value = 235 << bitdepth_min_8;
    } else {
        min_value = 0;
        max_value = (1U << in->p.bpc) - 1;
    }

    // seed[0] contains the current row, seed[1] contains the previous
    unsigned seed[2];
    for (int i = 0; i < rows; i++) {
        seed[i] = data->seed;
        seed[i] ^= (((row_num - i) * 37  + 178) & 0xFF) << 8;
        seed[i] ^= (((row_num - i) * 173 + 105) & 0xFF);
    }

    const ptrdiff_t stride = out->stride[0];
    assert(stride % (BLOCK_SIZE * sizeof(pixel)) == 0);
    assert(stride == in->stride[0]);
    pixel *const src_row = (pixel *)  in->data[0] + PXSTRIDE(stride) * row_num * BLOCK_SIZE;
    pixel *const dst_row = (pixel *) out->data[0] + PXSTRIDE(stride) * row_num * BLOCK_SIZE;

    int offsets[2 /* col offset */][2 /* row offset */];

    // process this row in BLOCK_SIZE^2 blocks
    const int bh = imin(out->p.h - row_num * BLOCK_SIZE, BLOCK_SIZE);
    for (int bx = 0; bx < out->p.w; bx += BLOCK_SIZE) {
        const int bw = imin(BLOCK_SIZE, out->p.w - bx);

        if (data->overlap_flag && bx) {
            // shift previous offsets left
            for (int i = 0; i < rows; i++)
                offsets[1][i] = offsets[0][i];
        }

        // update current offsets
        for (int i = 0; i < rows; i++)
            offsets[0][i] = get_random_number(8, &seed[i]);

        // x/y block offsets to compensate for overlapped regions
        const int ystart = data->overlap_flag && row_num ? imin(2, bh) : 0;
        const int xstart = data->overlap_flag && bx      ? imin(2, bw) : 0;

        static const int w[2][2] = { { 27, 17 }, { 17, 27 } };

#define add_noise_y(x, y, grain)                                                \
            pixel *src = src_row + (y) * PXSTRIDE(stride) + (bx + (x));         \
            pixel *dst = dst_row + (y) * PXSTRIDE(stride) + (bx + (x));         \
            int noise = round2(scaling[ *src ] * (grain), data->scaling_shift); \
            *dst = iclip(*src + noise, min_value, max_value);

        for (int y = ystart; y < bh; y++) {
            // Non-overlapped image region (straightforward)
            for (int x = xstart; x < bw; x++) {
                int grain = sample_lut(grain_lut, offsets, 0, 0, 0, 0, x, y);
                add_noise_y(x, y, grain);
            }

            // Special case for overlapped column
            for (int x = 0; x < xstart; x++) {
                int grain = sample_lut(grain_lut, offsets, 0, 0, 0, 0, x, y);
                int old   = sample_lut(grain_lut, offsets, 0, 0, 1, 0, x, y);
                grain = round2(old * w[x][0] + grain * w[x][1], 5);
                grain = iclip(grain, grain_min, grain_max);
                add_noise_y(x, y, grain);
            }
        }

        for (int y = 0; y < ystart; y++) {
            // Special case for overlapped row (sans corner)
            for (int x = xstart; x < bw; x++) {
                int grain = sample_lut(grain_lut, offsets, 0, 0, 0, 0, x, y);
                int old   = sample_lut(grain_lut, offsets, 0, 0, 0, 1, x, y);
                grain = round2(old * w[y][0] + grain * w[y][1], 5);
                grain = iclip(grain, grain_min, grain_max);
                add_noise_y(x, y, grain);
            }

            // Special case for doubly-overlapped corner
            for (int x = 0; x < xstart; x++) {
                // Blend the top pixel with the top left block
                int top = sample_lut(grain_lut, offsets, 0, 0, 0, 1, x, y);
                int old = sample_lut(grain_lut, offsets, 0, 0, 1, 1, x, y);
                top = round2(old * w[x][0] + top * w[x][1], 5);
                top = iclip(top, grain_min, grain_max);

                // Blend the current pixel with the left block
                int grain = sample_lut(grain_lut, offsets, 0, 0, 0, 0, x, y);
                old = sample_lut(grain_lut, offsets, 0, 0, 1, 0, x, y);
                grain = round2(old * w[x][0] + grain * w[x][1], 5);
                grain = iclip(grain, grain_min, grain_max);

                // Mix the row rows together and apply grain
                grain = round2(top * w[y][0] + grain * w[y][1], 5);
                grain = iclip(grain, grain_min, grain_max);
                add_noise_y(x, y, grain);
            }
        }
    }
}

static void apply_to_row_uv(Dav1dPicture *const out, const Dav1dPicture *const in,
                            entry grain_lut[GRAIN_HEIGHT][GRAIN_WIDTH],
                            uint8_t scaling[SCALING_SIZE], int uv, int row_num)
{
    const Dav1dFilmGrainData *const data = &out->frame_hdr->film_grain.data;
    const int rows = 1 + (data->overlap_flag && row_num > 0);
    const int bitdepth_max = (1 << in->p.bpc) - 1;
    const int bitdepth_min_8 = in->p.bpc - 8;
    const int grain_ctr = 128 << bitdepth_min_8;
    const int grain_min = -grain_ctr, grain_max = grain_ctr - 1;

    int min_value, max_value;
    if (data->clip_to_restricted_range) {
        min_value = 16 << bitdepth_min_8;
        if (out->seq_hdr->mtrx == DAV1D_MC_IDENTITY) {
            max_value = 235 << bitdepth_min_8;
        } else {
            max_value = 240 << bitdepth_min_8;
        }
    } else {
        min_value = 0;
        max_value = bitdepth_max;
    }

    const int sx = in->p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int sy = in->p.layout == DAV1D_PIXEL_LAYOUT_I420;

    // seed[0] contains the current row, seed[1] contains the previous
    unsigned seed[2];
    for (int i = 0; i < rows; i++) {
        seed[i] = data->seed;
        seed[i] ^= (((row_num - i) * 37  + 178) & 0xFF) << 8;
        seed[i] ^= (((row_num - i) * 173 + 105) & 0xFF);
    }

    const ptrdiff_t stride = out->stride[1];
    assert(stride % (BLOCK_SIZE * sizeof(pixel)) == 0);
    assert(stride == in->stride[1]);

    const int by = row_num * (BLOCK_SIZE >> sy);
    pixel *const dst_row = (pixel *) out->data[1 + uv] + PXSTRIDE(stride) * by;
    pixel *const src_row = (pixel *)  in->data[1 + uv] + PXSTRIDE(stride) * by;
    pixel *const luma_row = (pixel *) out->data[0] + PXSTRIDE(out->stride[0]) * row_num * BLOCK_SIZE;

    int offsets[2 /* col offset */][2 /* row offset */];

    // process this row in BLOCK_SIZE^2 blocks (subsampled)
    const int bh = (imin(out->p.h - row_num * BLOCK_SIZE, BLOCK_SIZE) + sy) >> sy;
    for (int bx = 0; bx < (out->p.w + sx) >> sx; bx += BLOCK_SIZE >> sx) {
        const int bw = (imin(BLOCK_SIZE, out->p.w - (bx << sx)) + sx) >> sx;
        if (data->overlap_flag && bx) {
            // shift previous offsets left
            for (int i = 0; i < rows; i++)
                offsets[1][i] = offsets[0][i];
        }

        // update current offsets
        for (int i = 0; i < rows; i++)
            offsets[0][i] = get_random_number(8, &seed[i]);

        // x/y block offsets to compensate for overlapped regions
        const int ystart = data->overlap_flag && row_num ? imin(2 >> sy, bh) : 0;
        const int xstart = data->overlap_flag && bx      ? imin(2 >> sx, bw) : 0;

        static const int w[2 /* sub */][2 /* off */][2] = {
            { { 27, 17 }, { 17, 27 } },
            { { 23, 22 } },
        };

#define add_noise_uv(x, y, grain)                                               \
            const int lx = (bx + x) << sx;                                      \
            const int ly = y << sy;                                             \
            pixel *luma = luma_row + ly * PXSTRIDE(out->stride[0]) + lx;        \
            pixel avg = luma[0];                                                \
            if (sx && lx + 1 < out->p.w)                                        \
                avg = (avg + luma[1] + 1) >> 1;                                 \
                                                                                \
            pixel *src = src_row + (y) * PXSTRIDE(stride) + (bx + (x));         \
            pixel *dst = dst_row + (y) * PXSTRIDE(stride) + (bx + (x));         \
            int val = avg;                                                      \
            if (!data->chroma_scaling_from_luma) {                              \
                int combined = avg * data->uv_luma_mult[uv] +                   \
                               *src * data->uv_mult[uv];                        \
                val = iclip_pixel( (combined >> 6) +                            \
                                   (data->uv_offset[uv] * (1 << bitdepth_min_8)) );   \
            }                                                                   \
                                                                                \
            int noise = round2(scaling[ val ] * (grain), data->scaling_shift);  \
            *dst = iclip(*src + noise, min_value, max_value);

        for (int y = ystart; y < bh; y++) {
            // Non-overlapped image region (straightforward)
            for (int x = xstart; x < bw; x++) {
                int grain = sample_lut(grain_lut, offsets, sx, sy, 0, 0, x, y);
                add_noise_uv(x, y, grain);
            }

            // Special case for overlapped column
            for (int x = 0; x < xstart; x++) {
                int grain = sample_lut(grain_lut, offsets, sx, sy, 0, 0, x, y);
                int old   = sample_lut(grain_lut, offsets, sx, sy, 1, 0, x, y);
                grain = (old * w[sx][x][0] + grain * w[sx][x][1] + 16) >> 5;
                grain = iclip(grain, grain_min, grain_max);
                add_noise_uv(x, y, grain);
            }
        }

        for (int y = 0; y < ystart; y++) {
            // Special case for overlapped row (sans corner)
            for (int x = xstart; x < bw; x++) {
                int grain = sample_lut(grain_lut, offsets, sx, sy, 0, 0, x, y);
                int old   = sample_lut(grain_lut, offsets, sx, sy, 0, 1, x, y);
                grain = (old * w[sy][y][0] + grain * w[sy][y][1] + 16) >> 5;
                grain = iclip(grain, grain_min, grain_max);
                add_noise_uv(x, y, grain);
            }

            // Special case for doubly-overlapped corner
            for (int x = 0; x < xstart; x++) {
                // Blend the top pixel with the top left block
                int top = sample_lut(grain_lut, offsets, sx, sy, 0, 1, x, y);
                int old = sample_lut(grain_lut, offsets, sx, sy, 1, 1, x, y);
                top = (old * w[sx][x][0] + top * w[sx][x][1] + 16) >> 5;
                top = iclip(top, grain_min, grain_max);

                // Blend the current pixel with the left block
                int grain = sample_lut(grain_lut, offsets, sx, sy, 0, 0, x, y);
                old = sample_lut(grain_lut, offsets, sx, sy, 1, 0, x, y);
                grain = (old * w[sx][x][0] + grain * w[sx][x][1] + 16) >> 5;
                grain = iclip(grain, grain_min, grain_max);

                // Mix the row rows together and apply to image
                grain = (top * w[sy][y][0] + grain * w[sy][y][1] + 16) >> 5;
                grain = iclip(grain, grain_min, grain_max);
                add_noise_uv(x, y, grain);
            }
        }
    }
}

void bitfn(dav1d_apply_grain)(Dav1dPicture *const out,
                              const Dav1dPicture *const in)
{
    const Dav1dFilmGrainData *const data = &out->frame_hdr->film_grain.data;

    entry grain_lut[3][GRAIN_HEIGHT][GRAIN_WIDTH];
    uint8_t scaling[3][SCALING_SIZE];

    // Generate grain LUTs as needed
    generate_grain_y(out, grain_lut[0]); // always needed
    if (data->num_uv_points[0] || data->chroma_scaling_from_luma)
        generate_grain_uv(out, 0, grain_lut[1], grain_lut[0]);
    if (data->num_uv_points[1] || data->chroma_scaling_from_luma)
        generate_grain_uv(out, 1, grain_lut[2], grain_lut[0]);

    // Generate scaling LUTs as needed
    if (data->num_y_points)
        generate_scaling(in->p.bpc, data->y_points, data->num_y_points, scaling[0]);
    if (data->num_uv_points[0])
        generate_scaling(in->p.bpc, data->uv_points[0], data->num_uv_points[0], scaling[1]);
    if (data->num_uv_points[1])
        generate_scaling(in->p.bpc, data->uv_points[1], data->num_uv_points[1], scaling[2]);

    // Copy over the non-modified planes
    // TODO: eliminate in favor of per-plane refs
    if (!data->num_y_points) {
        assert(out->stride[0] == in->stride[0]);
        memcpy(out->data[0], in->data[0], out->p.h * out->stride[0]);
    }

    if (in->p.layout != DAV1D_PIXEL_LAYOUT_I400) {
        for (int i = 0; i < 2; i++) {
            if (!data->num_uv_points[i] && !data->chroma_scaling_from_luma) {
                const int suby = in->p.layout == DAV1D_PIXEL_LAYOUT_I420;
                assert(out->stride[1] == in->stride[1]);
                memcpy(out->data[1+i], in->data[1+i],
                       (out->p.h >> suby) * out->stride[1]);
            }
        }
    }

    // Synthesize grain for the affected planes
    int rows = (out->p.h + 31) >> 5;
    for (int row = 0; row < rows; row++) {
        if (data->num_y_points)
            apply_to_row_y(out, in, grain_lut[0], scaling[0], row);

        if (data->chroma_scaling_from_luma) {
            apply_to_row_uv(out, in, grain_lut[1], scaling[0], 0, row);
            apply_to_row_uv(out, in, grain_lut[2], scaling[0], 1, row);
        } else {
            if (data->num_uv_points[0])
                apply_to_row_uv(out, in, grain_lut[1], scaling[1], 0, row);
            if (data->num_uv_points[1])
                apply_to_row_uv(out, in, grain_lut[2], scaling[2], 1, row);
        }
    }
}
