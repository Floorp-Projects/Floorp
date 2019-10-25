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

#include <stdio.h>

#include "common/intops.h"

#include "src/lr_apply.h"

enum LrRestorePlanes {
    LR_RESTORE_Y = 1 << 0,
    LR_RESTORE_U = 1 << 1,
    LR_RESTORE_V = 1 << 2,
};

// The loop filter buffer stores 12 rows of pixels. A superblock block will
// contain at most 2 stripes. Each stripe requires 4 rows pixels (2 above
// and 2 below) the final 4 rows are used to swap the bottom of the last
// stripe with the top of the next super block row.
static void backup_lpf(const Dav1dFrameContext *const f,
                       pixel *dst, const ptrdiff_t dst_stride,
                       const pixel *src, const ptrdiff_t src_stride,
                       const int ss_ver, const int sb128,
                       int row, const int row_h, const int src_w,
                       const int h, const int ss_hor)
{
    const int dst_w = f->frame_hdr->super_res.enabled ?
                      (f->frame_hdr->width[1] + ss_hor) >> ss_hor : src_w;

    // The first stripe of the frame is shorter by 8 luma pixel rows.
    int stripe_h = (64 - 8 * !row) >> ss_ver;

    if (row) {
        const int top = 4 << sb128;
        // Copy the top part of the stored loop filtered pixels from the
        // previous sb row needed above the first stripe of this sb row.
        pixel_copy(&dst[PXSTRIDE(dst_stride) *  0],
                   &dst[PXSTRIDE(dst_stride) *  top],      dst_w);
        pixel_copy(&dst[PXSTRIDE(dst_stride) *  1],
                   &dst[PXSTRIDE(dst_stride) * (top + 1)], dst_w);
        pixel_copy(&dst[PXSTRIDE(dst_stride) *  2],
                   &dst[PXSTRIDE(dst_stride) * (top + 2)], dst_w);
        pixel_copy(&dst[PXSTRIDE(dst_stride) *  3],
                   &dst[PXSTRIDE(dst_stride) * (top + 3)], dst_w);
    }

    dst += 4 * PXSTRIDE(dst_stride);
    src += (stripe_h - 2) * PXSTRIDE(src_stride);

    if (f->frame_hdr->super_res.enabled) {
        while (row + stripe_h <= row_h) {
            const int n_lines = 4 - (row + stripe_h + 1 == h);
            f->dsp->mc.resize(dst, dst_stride, src, src_stride,
                              dst_w, src_w, n_lines, f->resize_step[ss_hor],
                              f->resize_start[ss_hor] HIGHBD_CALL_SUFFIX);
            row += stripe_h; // unmodified stripe_h for the 1st stripe
            stripe_h = 64 >> ss_ver;
            src += stripe_h * PXSTRIDE(src_stride);
            dst += n_lines * PXSTRIDE(dst_stride);
            if (n_lines == 3) {
                pixel_copy(dst, &dst[-PXSTRIDE(dst_stride)], dst_w);
                dst += PXSTRIDE(dst_stride);
            }
        }
    } else {
        while (row + stripe_h <= row_h) {
            const int n_lines = 4 - (row + stripe_h + 1 == h);
            for (int i = 0; i < 4; i++) {
                pixel_copy(dst, i == n_lines ? &dst[-PXSTRIDE(dst_stride)] :
                                               src, src_w);
                dst += PXSTRIDE(dst_stride);
                src += PXSTRIDE(src_stride);
            }
            row += stripe_h; // unmodified stripe_h for the 1st stripe
            stripe_h = 64 >> ss_ver;
            src += (stripe_h - 4) * PXSTRIDE(src_stride);
        }
    }
}

void bytefn(dav1d_lr_copy_lpf)(Dav1dFrameContext *const f,
                               /*const*/ pixel *const src[3], const int sby)
{
    const int offset = 8 * !!sby;
    const ptrdiff_t *const src_stride = f->cur.stride;
    const ptrdiff_t lr_stride = ((f->sr_cur.p.p.w + 31) & ~31) * sizeof(pixel);

    // TODO Also check block level restore type to reduce copying.
    const int restore_planes = f->lf.restore_planes;

    if (restore_planes & LR_RESTORE_Y) {
        const int h = f->cur.p.h;
        const int w = f->bw << 2;
        const int row_h = imin((sby + 1) << (6 + f->seq_hdr->sb128), h - 1);
        const int y_stripe = (sby << (6 + f->seq_hdr->sb128)) - offset;
        backup_lpf(f, f->lf.lr_lpf_line[0], lr_stride,
                   src[0] - offset * PXSTRIDE(src_stride[0]), src_stride[0],
                   0, f->seq_hdr->sb128, y_stripe, row_h, w, h, 0);
    }
    if (restore_planes & (LR_RESTORE_U | LR_RESTORE_V)) {
        const int ss_ver = f->sr_cur.p.p.layout == DAV1D_PIXEL_LAYOUT_I420;
        const int ss_hor = f->sr_cur.p.p.layout != DAV1D_PIXEL_LAYOUT_I444;
        const int h = (f->cur.p.h + ss_ver) >> ss_ver;
        const int w = f->bw << (2 - ss_hor);
        const int row_h = imin((sby + 1) << ((6 - ss_ver) + f->seq_hdr->sb128), h - 1);
        const int offset_uv = offset >> ss_ver;
        const int y_stripe =
            (sby << ((6 - ss_ver) + f->seq_hdr->sb128)) - offset_uv;

        if (restore_planes & LR_RESTORE_U) {
            backup_lpf(f, f->lf.lr_lpf_line[1], lr_stride,
                       src[1] - offset_uv * PXSTRIDE(src_stride[1]), src_stride[1],
                       ss_ver, f->seq_hdr->sb128, y_stripe, row_h, w, h, ss_hor);
        }
        if (restore_planes & LR_RESTORE_V) {
            backup_lpf(f, f->lf.lr_lpf_line[2], lr_stride,
                       src[2] - offset_uv * PXSTRIDE(src_stride[1]), src_stride[1],
                       ss_ver, f->seq_hdr->sb128, y_stripe, row_h, w, h, ss_hor);
        }
    }
}

static void lr_stripe(const Dav1dFrameContext *const f, pixel *p,
                      const pixel (*left)[4], int x, int y,
                      const int plane, const int unit_w, const int row_h,
                      const Av1RestorationUnit *const lr, enum LrEdgeFlags edges)
{
    const Dav1dDSPContext *const dsp = f->dsp;
    const int chroma = !!plane;
    const int ss_ver = chroma & (f->sr_cur.p.p.layout == DAV1D_PIXEL_LAYOUT_I420);
    const int sbrow_has_bottom = (edges & LR_HAVE_BOTTOM);
    const pixel *lpf = f->lf.lr_lpf_line[plane] + x;
    const ptrdiff_t p_stride = f->sr_cur.p.stride[chroma];
    const ptrdiff_t lpf_stride = sizeof(pixel) * ((f->sr_cur.p.p.w + 31) & ~31);

    // The first stripe of the frame is shorter by 8 luma pixel rows.
    int stripe_h = imin((64 - 8 * !y) >> ss_ver, row_h - y);

    // FIXME [8] might be easier for SIMD
    int16_t filterh[7], filterv[7];
    if (lr->type == DAV1D_RESTORATION_WIENER) {
        filterh[0] = filterh[6] = lr->filter_h[0];
        filterh[1] = filterh[5] = lr->filter_h[1];
        filterh[2] = filterh[4] = lr->filter_h[2];
        filterh[3] = -((filterh[0] + filterh[1] + filterh[2]) * 2);

        filterv[0] = filterv[6] = lr->filter_v[0];
        filterv[1] = filterv[5] = lr->filter_v[1];
        filterv[2] = filterv[4] = lr->filter_v[2];
        filterv[3] = -((filterv[0] + filterv[1] + filterv[2]) * 2);
    }

    while (y + stripe_h <= row_h) {
        // Change HAVE_BOTTOM bit in edges to (y + stripe_h != row_h)
        edges ^= (-(y + stripe_h != row_h) ^ edges) & LR_HAVE_BOTTOM;
        if (lr->type == DAV1D_RESTORATION_WIENER) {
            dsp->lr.wiener(p, p_stride, left, lpf, lpf_stride, unit_w, stripe_h,
                           filterh, filterv, edges HIGHBD_CALL_SUFFIX);
        } else {
            assert(lr->type == DAV1D_RESTORATION_SGRPROJ);
            dsp->lr.selfguided(p, p_stride, left, lpf, lpf_stride, unit_w, stripe_h,
                               lr->sgr_idx, lr->sgr_weights, edges HIGHBD_CALL_SUFFIX);
        }

        left += stripe_h;
        y += stripe_h;
        if (y + stripe_h > row_h && sbrow_has_bottom) break;
        p += stripe_h * PXSTRIDE(p_stride);
        edges |= LR_HAVE_TOP;
        stripe_h = imin(64 >> ss_ver, row_h - y);
        if (stripe_h == 0) break;
        lpf += 4 * PXSTRIDE(lpf_stride);
    }
}

static void backup4xU(pixel (*dst)[4], const pixel *src, const ptrdiff_t src_stride,
                      int u)
{
    for (; u > 0; u--, dst++, src += PXSTRIDE(src_stride))
        pixel_copy(dst, src, 4);
}

static void lr_sbrow(const Dav1dFrameContext *const f, pixel *p, const int y,
                     const int w, const int h, const int row_h, const int plane)
{
    const int chroma = !!plane;
    const int ss_ver = chroma & (f->sr_cur.p.p.layout == DAV1D_PIXEL_LAYOUT_I420);
    const int ss_hor = chroma & (f->sr_cur.p.p.layout != DAV1D_PIXEL_LAYOUT_I444);
    const ptrdiff_t p_stride = f->sr_cur.p.stride[chroma];

    const int unit_size_log2 = f->frame_hdr->restoration.unit_size[!!plane];
    const int unit_size = 1 << unit_size_log2;
    const int half_unit_size = unit_size >> 1;
    const int max_unit_size = unit_size + half_unit_size;

    // Y coordinate of the sbrow (y is 8 luma pixel rows above row_y)
    const int row_y = y + ((8 >> ss_ver) * !!y);

    // FIXME This is an ugly hack to lookup the proper AV1Filter unit for
    // chroma planes. Question: For Multithreaded decoding, is it better
    // to store the chroma LR information with collocated Luma information?
    // In other words. For a chroma restoration unit locate at 128,128 and
    // with a 4:2:0 chroma subsampling, do we store the filter information at
    // the AV1Filter unit located at (128,128) or (256,256)
    // TODO Support chroma subsampling.
    const int shift_hor = 7 - ss_hor;

    pixel pre_lr_border[2][128 + 8 /* maximum sbrow height is 128 + 8 rows offset */][4];
    const Av1RestorationUnit *lr[2];

    enum LrEdgeFlags edges = (y > 0 ? LR_HAVE_TOP : 0) | LR_HAVE_RIGHT |
                             (row_h < h ? LR_HAVE_BOTTOM : 0);

    int aligned_unit_pos = row_y & ~(unit_size - 1);
    if (aligned_unit_pos && aligned_unit_pos + half_unit_size > h)
        aligned_unit_pos -= unit_size;
    aligned_unit_pos <<= ss_ver;
    const int sb_idx = (aligned_unit_pos >> 7) * f->sr_sb128w;
    const int unit_idx = ((aligned_unit_pos >> 6) & 1) << 1;
    lr[0] = &f->lf.lr_mask[sb_idx].lr[plane][unit_idx];
    int restore = lr[0]->type != DAV1D_RESTORATION_NONE;
    int x = 0, bit = 0;
    for (; x + max_unit_size <= w; p += unit_size, edges |= LR_HAVE_LEFT, bit ^= 1) {
        const int next_x = x + unit_size;
        const int next_u_idx = unit_idx + ((next_x >> (shift_hor - 1)) & 1);
        lr[!bit] =
            &f->lf.lr_mask[sb_idx + (next_x >> shift_hor)].lr[plane][next_u_idx];
        const int restore_next = lr[!bit]->type != DAV1D_RESTORATION_NONE;
        if (restore_next)
            backup4xU(pre_lr_border[bit], p + unit_size - 4, p_stride, row_h - y);
        if (restore)
            lr_stripe(f, p, pre_lr_border[!bit], x, y, plane, unit_size, row_h,
                      lr[bit], edges);
        x = next_x;
        restore = restore_next;
    }
    if (restore) {
        edges &= ~LR_HAVE_RIGHT;
        const int unit_w = w - x;
        lr_stripe(f, p, pre_lr_border[!bit], x, y, plane, unit_w, row_h, lr[bit], edges);
    }
}

void bytefn(dav1d_lr_sbrow)(Dav1dFrameContext *const f, pixel *const dst[3],
                            const int sby)
{
    const int offset_y = 8 * !!sby;
    const ptrdiff_t *const dst_stride = f->sr_cur.p.stride;
    const int restore_planes = f->lf.restore_planes;

    if (restore_planes & LR_RESTORE_Y) {
        const int h = f->sr_cur.p.p.h;
        const int w = f->sr_cur.p.p.w;
        const int row_h = imin((sby + 1) << (6 + f->seq_hdr->sb128), h);
        const int y_stripe = (sby << (6 + f->seq_hdr->sb128)) - offset_y;
        lr_sbrow(f, dst[0] - offset_y * PXSTRIDE(dst_stride[0]), y_stripe, w,
                 h, row_h, 0);
    }
    if (restore_planes & (LR_RESTORE_U | LR_RESTORE_V)) {
        const int ss_ver = f->sr_cur.p.p.layout == DAV1D_PIXEL_LAYOUT_I420;
        const int ss_hor = f->sr_cur.p.p.layout != DAV1D_PIXEL_LAYOUT_I444;
        const int h = (f->sr_cur.p.p.h + ss_ver) >> ss_ver;
        const int w = (f->sr_cur.p.p.w + ss_hor) >> ss_hor;
        const int row_h = imin((sby + 1) << ((6 - ss_ver) + f->seq_hdr->sb128), h);
        const int offset_uv = offset_y >> ss_ver;
        const int y_stripe =
            (sby << ((6 - ss_ver) + f->seq_hdr->sb128)) - offset_uv;
        if (restore_planes & LR_RESTORE_U)
            lr_sbrow(f, dst[1] - offset_uv * PXSTRIDE(dst_stride[1]), y_stripe,
                     w, h, row_h, 1);

        if (restore_planes & LR_RESTORE_V)
            lr_sbrow(f, dst[2] - offset_uv * PXSTRIDE(dst_stride[1]), y_stripe,
                     w, h, row_h, 2);
    }
}
