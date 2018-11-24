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

#include <stdlib.h>
#include <string.h>

#include "common/attributes.h"
#include "common/intops.h"

#include "src/mc.h"
#include "src/tables.h"

static NOINLINE void
put_c(pixel *dst, const ptrdiff_t dst_stride,
      const pixel *src, const ptrdiff_t src_stride, const int w, int h)
{
    do {
        pixel_copy(dst, src, w);

        dst += dst_stride;
        src += src_stride;
    } while (--h);
}

static NOINLINE void
prep_c(coef *tmp, const pixel *src, const ptrdiff_t src_stride,
       const int w, int h)
{
    do {
        for (int x = 0; x < w; x++)
            tmp[x] = src[x] << 4;

        tmp += w;
        src += src_stride;
    } while (--h);
}

#define FILTER_8TAP(src, x, F, stride) \
    (F[0] * src[x + -3 * stride] + \
     F[1] * src[x + -2 * stride] + \
     F[2] * src[x + -1 * stride] + \
     F[3] * src[x + +0 * stride] + \
     F[4] * src[x + +1 * stride] + \
     F[5] * src[x + +2 * stride] + \
     F[6] * src[x + +3 * stride] + \
     F[7] * src[x + +4 * stride])

#define FILTER_8TAP_RND(src, x, F, stride, sh) \
    ((FILTER_8TAP(src, x, F, stride) + ((1 << sh) >> 1)) >> sh)

#define FILTER_8TAP_CLIP(src, x, F, stride, sh) \
    iclip_pixel(FILTER_8TAP_RND(src, x, F, stride, sh))

#define GET_FILTERS() \
    const int8_t *const fh = !mx ? NULL : w > 4 ? \
        dav1d_mc_subpel_filters[filter_type & 3][mx - 1] : \
        dav1d_mc_subpel_filters[3 + (filter_type & 1)][mx - 1]; \
    const int8_t *const fv = !my ? NULL : h > 4 ? \
        dav1d_mc_subpel_filters[filter_type >> 2][my - 1] : \
        dav1d_mc_subpel_filters[3 + ((filter_type >> 2) & 1)][my - 1]; \

static NOINLINE void
put_8tap_c(pixel *dst, ptrdiff_t dst_stride,
           const pixel *src, ptrdiff_t src_stride,
           const int w, int h, const int mx, const int my,
           const int filter_type)
{
    GET_FILTERS();
    dst_stride = PXSTRIDE(dst_stride);
    src_stride = PXSTRIDE(src_stride);

    if (fh) {
        if (fv) {
            int tmp_h = h + 7;
            coef mid[128 * 135], *mid_ptr = mid;

            src -= src_stride * 3;
            do {
                for (int x = 0; x < w; x++)
                    mid_ptr[x] = FILTER_8TAP_RND(src, x, fh, 1, 2);

                mid_ptr += 128;
                src += src_stride;
            } while (--tmp_h);

            mid_ptr = mid + 128 * 3;
            do {
                for (int x = 0; x < w; x++)
                    dst[x] = FILTER_8TAP_CLIP(mid_ptr, x, fv, 128, 10);

                mid_ptr += 128;
                dst += dst_stride;
            } while (--h);
        } else {
            do {
                for (int x = 0; x < w; x++) {
                    const int px = FILTER_8TAP_RND(src, x, fh, 1, 2);
                    dst[x] = iclip_pixel((px + 8) >> 4);
                }

                dst += dst_stride;
                src += src_stride;
            } while (--h);
        }
    } else if (fv) {
        do {
            for (int x = 0; x < w; x++)
                dst[x] = FILTER_8TAP_CLIP(src, x, fv, src_stride, 6);

            dst += dst_stride;
            src += src_stride;
        } while (--h);
    } else
        put_c(dst, dst_stride, src, src_stride, w, h);
}

static NOINLINE void
prep_8tap_c(coef *tmp, const pixel *src, ptrdiff_t src_stride,
            const int w, int h, const int mx, const int my,
            const int filter_type)
{
    GET_FILTERS();
    src_stride = PXSTRIDE(src_stride);

    if (fh) {
        if (fv) {
            int tmp_h = h + 7;
            coef mid[128 * 135], *mid_ptr = mid;

            src -= src_stride * 3;
            do {
                for (int x = 0; x < w; x++)
                    mid_ptr[x] = FILTER_8TAP_RND(src, x, fh, 1, 2);

                mid_ptr += 128;
                src += src_stride;
            } while (--tmp_h);

            mid_ptr = mid + 128 * 3;
            do {
                for (int x = 0; x < w; x++)
                    tmp[x] = FILTER_8TAP_RND(mid_ptr, x, fv, 128, 6);

                mid_ptr += 128;
                tmp += w;
            } while (--h);
        } else {
            do {
                for (int x = 0; x < w; x++)
                    tmp[x] = FILTER_8TAP_RND(src, x, fh, 1, 2);

                tmp += w;
                src += src_stride;
            } while (--h);
        }
    } else if (fv) {
        do {
            for (int x = 0; x < w; x++)
                tmp[x] = FILTER_8TAP_RND(src, x, fv, src_stride, 2);

            tmp += w;
            src += src_stride;
        } while (--h);
    } else
        prep_c(tmp, src, src_stride, w, h);
}

#define filter_fns(type, type_h, type_v) \
static void put_8tap_##type##_c(pixel *const dst, \
                                const ptrdiff_t dst_stride, \
                                const pixel *const src, \
                                const ptrdiff_t src_stride, \
                                const int w, const int h, \
                                const int mx, const int my) \
{ \
    put_8tap_c(dst, dst_stride, src, src_stride, w, h, mx, my, \
               type_h | (type_v << 2)); \
} \
static void prep_8tap_##type##_c(coef *const tmp, \
                                 const pixel *const src, \
                                 const ptrdiff_t src_stride, \
                                 const int w, const int h, \
                                 const int mx, const int my) \
{ \
    prep_8tap_c(tmp, src, src_stride, w, h, mx, my, \
                type_h | (type_v << 2)); \
}

filter_fns(regular,        FILTER_8TAP_REGULAR, FILTER_8TAP_REGULAR)
filter_fns(regular_sharp,  FILTER_8TAP_REGULAR, FILTER_8TAP_SHARP)
filter_fns(regular_smooth, FILTER_8TAP_REGULAR, FILTER_8TAP_SMOOTH)
filter_fns(smooth,         FILTER_8TAP_SMOOTH,  FILTER_8TAP_SMOOTH)
filter_fns(smooth_regular, FILTER_8TAP_SMOOTH,  FILTER_8TAP_REGULAR)
filter_fns(smooth_sharp,   FILTER_8TAP_SMOOTH,  FILTER_8TAP_SHARP)
filter_fns(sharp,          FILTER_8TAP_SHARP,   FILTER_8TAP_SHARP)
filter_fns(sharp_regular,  FILTER_8TAP_SHARP,   FILTER_8TAP_REGULAR)
filter_fns(sharp_smooth,   FILTER_8TAP_SHARP,   FILTER_8TAP_SMOOTH)

#define FILTER_BILIN(src, x, mxy, stride) \
    (16 * src[x] + (mxy * (src[x + stride] - src[x])))

#define FILTER_BILIN_RND(src, x, mxy, stride, sh) \
    ((FILTER_BILIN(src, x, mxy, stride) + ((1 << sh) >> 1)) >> sh)

#define FILTER_BILIN_CLIP(src, x, mxy, stride, sh) \
    iclip_pixel(FILTER_BILIN_RND(src, x, mxy, stride, sh))

static void put_bilin_c(pixel *dst, ptrdiff_t dst_stride,
                        const pixel *src, ptrdiff_t src_stride,
                        const int w, int h, const int mx, const int my)
{
    dst_stride = PXSTRIDE(dst_stride);
    src_stride = PXSTRIDE(src_stride);

    if (mx) {
        if (my) {
            coef mid[128 * 129], *mid_ptr = mid;
            int tmp_h = h + 1;

            do {
                for (int x = 0; x < w; x++)
                    mid_ptr[x] = FILTER_BILIN(src, x, mx, 1);

                mid_ptr += 128;
                src += src_stride;
            } while (--tmp_h);

            mid_ptr = mid;
            do {
                for (int x = 0; x < w; x++)
                    dst[x] = FILTER_BILIN_CLIP(mid_ptr, x, my, 128, 8);

                mid_ptr += 128;
                dst += dst_stride;
            } while (--h);
        } else {
            do {
                for (int x = 0; x < w; x++)
                    dst[x] = FILTER_BILIN_CLIP(src, x, mx, 1, 4);

                dst += dst_stride;
                src += src_stride;
            } while (--h);
        }
    } else if (my) {
        do {
            for (int x = 0; x < w; x++)
                dst[x] = FILTER_BILIN_CLIP(src, x, my, src_stride, 4);

            dst += dst_stride;
            src += src_stride;
        } while (--h);
    } else
        put_c(dst, dst_stride, src, src_stride, w, h);
}

static void prep_bilin_c(coef *tmp,
                         const pixel *src, ptrdiff_t src_stride,
                         const int w, int h, const int mx, const int my)
{
    src_stride = PXSTRIDE(src_stride);

    if (mx) {
        if (my) {
            coef mid[128 * 129], *mid_ptr = mid;
            int tmp_h = h + 1;

            do {
                for (int x = 0; x < w; x++)
                    mid_ptr[x] = FILTER_BILIN(src, x, mx, 1);

                mid_ptr += 128;
                src += src_stride;
            } while (--tmp_h);

            mid_ptr = mid;
            do {
                for (int x = 0; x < w; x++)
                    tmp[x] = FILTER_BILIN_RND(mid_ptr, x, my, 128, 4);

                mid_ptr += 128;
                tmp += w;
            } while (--h);
        } else {
            do {
                for (int x = 0; x < w; x++)
                    tmp[x] = FILTER_BILIN(src, x, mx, 1);

                tmp += w;
                src += src_stride;
            } while (--h);
        }
    } else if (my) {
        do {
            for (int x = 0; x < w; x++)
                tmp[x] = FILTER_BILIN(src, x, my, src_stride);

            tmp += w;
            src += src_stride;
        } while (--h);
    } else
        prep_c(tmp, src, src_stride, w, h);
}

static void avg_c(pixel *dst, const ptrdiff_t dst_stride,
                  const coef *tmp1, const coef *tmp2, const int w, int h)
{
    do {
        for (int x = 0; x < w; x++)
            dst[x] = iclip_pixel((tmp1[x] + tmp2[x] + 16) >> 5);

        tmp1 += w;
        tmp2 += w;
        dst += PXSTRIDE(dst_stride);
    } while (--h);
}

static void w_avg_c(pixel *dst, const ptrdiff_t dst_stride,
                    const coef *tmp1, const coef *tmp2, const int w, int h,
                    const int weight)
{
    do {
        for (int x = 0; x < w; x++)
            dst[x] = iclip_pixel((tmp1[x] * weight +
                                  tmp2[x] * (16 - weight) + 128) >> 8);

        tmp1 += w;
        tmp2 += w;
        dst += PXSTRIDE(dst_stride);
    } while (--h);
}

static void mask_c(pixel *dst, const ptrdiff_t dst_stride,
                   const coef *tmp1, const coef *tmp2, const int w, int h,
                   const uint8_t *mask)
{
    do {
        for (int x = 0; x < w; x++)
            dst[x] = iclip_pixel((tmp1[x] * mask[x] +
                                  tmp2[x] * (64 - mask[x]) + 512) >> 10);

        tmp1 += w;
        tmp2 += w;
        mask += w;
        dst += PXSTRIDE(dst_stride);
    } while (--h);
}

static void blend_c(pixel *dst, const ptrdiff_t dst_stride,
                    const pixel *tmp, const ptrdiff_t tmp_stride,
                    const int w, const int h,
                    const uint8_t *mask, const ptrdiff_t m_stride)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
#define blend_px(a, b, m) (((a * (64 - m) + b * m) + 32) >> 6)
            dst[x] = blend_px(dst[x], tmp[x], mask[m_stride == 1 ? 0 : x]);
        }
        dst += PXSTRIDE(dst_stride);
        tmp += PXSTRIDE(tmp_stride);
        mask += m_stride;
    }
}

static void w_mask_c(pixel *dst, const ptrdiff_t dst_stride,
                     const coef *tmp1, const coef *tmp2, const int w, int h,
                     uint8_t *mask, const int sign,
                     const int ss_hor, const int ss_ver)
{
    // store mask at 2x2 resolution, i.e. store 2x1 sum for even rows,
    // and then load this intermediate to calculate final value for odd rows
    const int rnd = 8 << (BITDEPTH - 8);
    do {
        for (int x = 0; x < w; x++) {
            const int m = imin(38 + ((abs(tmp1[x] - tmp2[x]) + rnd) >> BITDEPTH), 64);
            dst[x] = iclip_pixel((tmp1[x] * m +
                                  tmp2[x] * (64 - m) + 512) >> 10);

            if (ss_hor) {
                x++;

                const int n = imin(38 + ((abs(tmp1[x] - tmp2[x]) + rnd) >> BITDEPTH), 64);
                dst[x] = iclip_pixel((tmp1[x] * n +
                                      tmp2[x] * (64 - n) + 512) >> 10);

                if (h & ss_ver) {
                    mask[x >> 1] = (m + n + mask[x >> 1] + 2 - sign) >> 2;
                } else if (ss_ver) {
                    mask[x >> 1] = m + n;
                } else {
                    mask[x >> 1] = (m + n + 1 - sign) >> 1;
                }
            } else {
                mask[x] = m;
            }
        }

        tmp1 += w;
        tmp2 += w;
        dst += PXSTRIDE(dst_stride);
        if (!ss_ver || (h & 1)) mask += w >> ss_hor;
    } while (--h);
}

#define w_mask_fns(ssn, ss_hor, ss_ver) \
static void w_mask_##ssn##_c(pixel *const dst, const ptrdiff_t dst_stride, \
                             const coef *const tmp1, const coef *const tmp2, \
                             const int w, const int h, uint8_t *mask, \
                             const int sign) \
{ \
    w_mask_c(dst, dst_stride, tmp1, tmp2, w, h, mask, sign, ss_hor, ss_ver); \
}

w_mask_fns(444, 0, 0);
w_mask_fns(422, 1, 0);
w_mask_fns(420, 1, 1);

#undef w_mask_fns

static void warp_affine_8x8_c(pixel *dst, const ptrdiff_t dst_stride,
                              const pixel *src, const ptrdiff_t src_stride,
                              const int16_t *const abcd, int mx, int my)
{
    coef mid[15 * 8], *mid_ptr = mid;

    src -= 3 * PXSTRIDE(src_stride);
    for (int y = 0; y < 15; y++, mx += abcd[1]) {
        for (int x = 0, tmx = mx; x < 8; x++, tmx += abcd[0]) {
            const int8_t *const filter =
                dav1d_mc_warp_filter[64 + ((tmx + 512) >> 10)];

            mid_ptr[x] = FILTER_8TAP_RND(src, x, filter, 1, 3);
        }
        src += PXSTRIDE(src_stride);
        mid_ptr += 8;
    }

    mid_ptr = &mid[3 * 8];
    for (int y = 0; y < 8; y++, my += abcd[3]) {
        for (int x = 0, tmy = my; x < 8; x++, tmy += abcd[2]) {
            const int8_t *const filter =
                dav1d_mc_warp_filter[64 + ((tmy + 512) >> 10)];

            dst[x] = FILTER_8TAP_CLIP(mid_ptr, x, filter, 8, 11);
        }
        mid_ptr += 8;
        dst += PXSTRIDE(dst_stride);
    }
}

static void warp_affine_8x8t_c(coef *tmp, const ptrdiff_t tmp_stride,
                               const pixel *src, const ptrdiff_t src_stride,
                               const int16_t *const abcd, int mx, int my)
{
    coef mid[15 * 8], *mid_ptr = mid;

    src -= 3 * PXSTRIDE(src_stride);
    for (int y = 0; y < 15; y++, mx += abcd[1]) {
        for (int x = 0, tmx = mx; x < 8; x++, tmx += abcd[0]) {
            const int8_t *const filter =
                dav1d_mc_warp_filter[64 + ((tmx + 512) >> 10)];

            mid_ptr[x] = FILTER_8TAP_RND(src, x, filter, 1, 3);
        }
        src += PXSTRIDE(src_stride);
        mid_ptr += 8;
    }

    mid_ptr = &mid[3 * 8];
    for (int y = 0; y < 8; y++, my += abcd[3]) {
        for (int x = 0, tmy = my; x < 8; x++, tmy += abcd[2]) {
            const int8_t *const filter =
                dav1d_mc_warp_filter[64 + ((tmy + 512) >> 10)];

            tmp[x] = FILTER_8TAP_RND(mid_ptr, x, filter, 8, 7);
        }
        mid_ptr += 8;
        tmp += tmp_stride;
    }
}

void bitfn(dav1d_mc_dsp_init)(Dav1dMCDSPContext *const c) {
#define init_mc_fns(type, name) do { \
    c->mc [type] = put_##name##_c; \
    c->mct[type] = prep_##name##_c; \
} while (0)

    init_mc_fns(FILTER_2D_8TAP_REGULAR,        8tap_regular);
    init_mc_fns(FILTER_2D_8TAP_REGULAR_SMOOTH, 8tap_regular_smooth);
    init_mc_fns(FILTER_2D_8TAP_REGULAR_SHARP,  8tap_regular_sharp);
    init_mc_fns(FILTER_2D_8TAP_SHARP_REGULAR,  8tap_sharp_regular);
    init_mc_fns(FILTER_2D_8TAP_SHARP_SMOOTH,   8tap_sharp_smooth);
    init_mc_fns(FILTER_2D_8TAP_SHARP,          8tap_sharp);
    init_mc_fns(FILTER_2D_8TAP_SMOOTH_REGULAR, 8tap_smooth_regular);
    init_mc_fns(FILTER_2D_8TAP_SMOOTH,         8tap_smooth);
    init_mc_fns(FILTER_2D_8TAP_SMOOTH_SHARP,   8tap_smooth_sharp);
    init_mc_fns(FILTER_2D_BILINEAR,            bilin);

    c->avg      = avg_c;
    c->w_avg    = w_avg_c;
    c->mask     = mask_c;
    c->blend    = blend_c;
    c->w_mask[0] = w_mask_444_c;
    c->w_mask[1] = w_mask_422_c;
    c->w_mask[2] = w_mask_420_c;
    c->warp8x8  = warp_affine_8x8_c;
    c->warp8x8t = warp_affine_8x8t_c;

#if HAVE_ASM
#if ARCH_AARCH64 || ARCH_ARM
    bitfn(dav1d_mc_dsp_init_arm)(c);
#elif ARCH_X86
    bitfn(dav1d_mc_dsp_init_x86)(c);
#endif
#endif
}
