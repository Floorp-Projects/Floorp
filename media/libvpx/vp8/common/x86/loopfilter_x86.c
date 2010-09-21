/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_ports/config.h"
#include "loopfilter.h"

prototype_loopfilter(vp8_loop_filter_horizontal_edge_c);
prototype_loopfilter(vp8_loop_filter_vertical_edge_c);
prototype_loopfilter(vp8_mbloop_filter_horizontal_edge_c);
prototype_loopfilter(vp8_mbloop_filter_vertical_edge_c);
prototype_loopfilter(vp8_loop_filter_simple_horizontal_edge_c);
prototype_loopfilter(vp8_loop_filter_simple_vertical_edge_c);

prototype_loopfilter(vp8_mbloop_filter_vertical_edge_mmx);
prototype_loopfilter(vp8_mbloop_filter_horizontal_edge_mmx);
prototype_loopfilter(vp8_loop_filter_vertical_edge_mmx);
prototype_loopfilter(vp8_loop_filter_horizontal_edge_mmx);
prototype_loopfilter(vp8_loop_filter_simple_vertical_edge_mmx);
prototype_loopfilter(vp8_loop_filter_simple_horizontal_edge_mmx);

prototype_loopfilter(vp8_loop_filter_vertical_edge_sse2);
prototype_loopfilter(vp8_loop_filter_horizontal_edge_sse2);
prototype_loopfilter(vp8_mbloop_filter_vertical_edge_sse2);
prototype_loopfilter(vp8_mbloop_filter_horizontal_edge_sse2);
prototype_loopfilter(vp8_loop_filter_simple_vertical_edge_sse2);
prototype_loopfilter(vp8_loop_filter_simple_horizontal_edge_sse2);
prototype_loopfilter(vp8_fast_loop_filter_vertical_edges_sse2);

extern loop_filter_uvfunction vp8_loop_filter_horizontal_edge_uv_sse2;
extern loop_filter_uvfunction vp8_loop_filter_vertical_edge_uv_sse2;
extern loop_filter_uvfunction vp8_mbloop_filter_horizontal_edge_uv_sse2;
extern loop_filter_uvfunction vp8_mbloop_filter_vertical_edge_uv_sse2;

#if HAVE_MMX
// Horizontal MB filtering
void vp8_loop_filter_mbh_mmx(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                             int y_stride, int uv_stride, loop_filter_info *lfi, int simpler_lpf)
{
    (void) simpler_lpf;
    vp8_mbloop_filter_horizontal_edge_mmx(y_ptr, y_stride, lfi->mbflim, lfi->lim, lfi->mbthr, 2);

    if (u_ptr)
        vp8_mbloop_filter_horizontal_edge_mmx(u_ptr, uv_stride, lfi->uvmbflim, lfi->uvlim, lfi->uvmbthr, 1);

    if (v_ptr)
        vp8_mbloop_filter_horizontal_edge_mmx(v_ptr, uv_stride, lfi->uvmbflim, lfi->uvlim, lfi->uvmbthr, 1);
}


void vp8_loop_filter_mbhs_mmx(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                              int y_stride, int uv_stride, loop_filter_info *lfi, int simpler_lpf)
{
    (void) u_ptr;
    (void) v_ptr;
    (void) uv_stride;
    (void) simpler_lpf;
    vp8_loop_filter_simple_horizontal_edge_mmx(y_ptr, y_stride, lfi->mbflim, lfi->lim, lfi->mbthr, 2);
}


// Vertical MB Filtering
void vp8_loop_filter_mbv_mmx(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                             int y_stride, int uv_stride, loop_filter_info *lfi, int simpler_lpf)
{
    (void) simpler_lpf;
    vp8_mbloop_filter_vertical_edge_mmx(y_ptr, y_stride, lfi->mbflim, lfi->lim, lfi->mbthr, 2);

    if (u_ptr)
        vp8_mbloop_filter_vertical_edge_mmx(u_ptr, uv_stride, lfi->uvmbflim, lfi->uvlim, lfi->uvmbthr, 1);

    if (v_ptr)
        vp8_mbloop_filter_vertical_edge_mmx(v_ptr, uv_stride, lfi->uvmbflim, lfi->uvlim, lfi->uvmbthr, 1);
}


void vp8_loop_filter_mbvs_mmx(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                              int y_stride, int uv_stride, loop_filter_info *lfi, int simpler_lpf)
{
    (void) u_ptr;
    (void) v_ptr;
    (void) uv_stride;
    (void) simpler_lpf;
    vp8_loop_filter_simple_vertical_edge_mmx(y_ptr, y_stride, lfi->mbflim, lfi->lim, lfi->mbthr, 2);
}


// Horizontal B Filtering
void vp8_loop_filter_bh_mmx(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                            int y_stride, int uv_stride, loop_filter_info *lfi, int simpler_lpf)
{
    (void) simpler_lpf;
    vp8_loop_filter_horizontal_edge_mmx(y_ptr + 4 * y_stride, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_horizontal_edge_mmx(y_ptr + 8 * y_stride, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_horizontal_edge_mmx(y_ptr + 12 * y_stride, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);

    if (u_ptr)
        vp8_loop_filter_horizontal_edge_mmx(u_ptr + 4 * uv_stride, uv_stride, lfi->uvflim, lfi->uvlim, lfi->uvthr, 1);

    if (v_ptr)
        vp8_loop_filter_horizontal_edge_mmx(v_ptr + 4 * uv_stride, uv_stride, lfi->uvflim, lfi->uvlim, lfi->uvthr, 1);
}


void vp8_loop_filter_bhs_mmx(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                             int y_stride, int uv_stride, loop_filter_info *lfi, int simpler_lpf)
{
    (void) u_ptr;
    (void) v_ptr;
    (void) uv_stride;
    (void) simpler_lpf;
    vp8_loop_filter_simple_horizontal_edge_mmx(y_ptr + 4 * y_stride, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_simple_horizontal_edge_mmx(y_ptr + 8 * y_stride, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_simple_horizontal_edge_mmx(y_ptr + 12 * y_stride, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
}


// Vertical B Filtering
void vp8_loop_filter_bv_mmx(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                            int y_stride, int uv_stride, loop_filter_info *lfi, int simpler_lpf)
{
    (void) simpler_lpf;
    vp8_loop_filter_vertical_edge_mmx(y_ptr + 4, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_vertical_edge_mmx(y_ptr + 8, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_vertical_edge_mmx(y_ptr + 12, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);

    if (u_ptr)
        vp8_loop_filter_vertical_edge_mmx(u_ptr + 4, uv_stride, lfi->uvflim, lfi->uvlim, lfi->uvthr, 1);

    if (v_ptr)
        vp8_loop_filter_vertical_edge_mmx(v_ptr + 4, uv_stride, lfi->uvflim, lfi->uvlim, lfi->uvthr, 1);
}


void vp8_loop_filter_bvs_mmx(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                             int y_stride, int uv_stride, loop_filter_info *lfi, int simpler_lpf)
{
    (void) u_ptr;
    (void) v_ptr;
    (void) uv_stride;
    (void) simpler_lpf;
    vp8_loop_filter_simple_vertical_edge_mmx(y_ptr + 4, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_simple_vertical_edge_mmx(y_ptr + 8, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_simple_vertical_edge_mmx(y_ptr + 12, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
}
#endif


// Horizontal MB filtering
#if HAVE_SSE2
void vp8_loop_filter_mbh_sse2(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                              int y_stride, int uv_stride, loop_filter_info *lfi, int simpler_lpf)
{
    (void) simpler_lpf;
    vp8_mbloop_filter_horizontal_edge_sse2(y_ptr, y_stride, lfi->mbflim, lfi->lim, lfi->mbthr, 2);

    if (u_ptr)
        vp8_mbloop_filter_horizontal_edge_uv_sse2(u_ptr, uv_stride, lfi->uvmbflim, lfi->uvlim, lfi->uvmbthr, v_ptr);
}


void vp8_loop_filter_mbhs_sse2(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                               int y_stride, int uv_stride, loop_filter_info *lfi, int simpler_lpf)
{
    (void) u_ptr;
    (void) v_ptr;
    (void) uv_stride;
    (void) simpler_lpf;
    vp8_loop_filter_simple_horizontal_edge_sse2(y_ptr, y_stride, lfi->mbflim, lfi->lim, lfi->mbthr, 2);
}


// Vertical MB Filtering
void vp8_loop_filter_mbv_sse2(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                              int y_stride, int uv_stride, loop_filter_info *lfi, int simpler_lpf)
{
    (void) simpler_lpf;
    vp8_mbloop_filter_vertical_edge_sse2(y_ptr, y_stride, lfi->mbflim, lfi->lim, lfi->mbthr, 2);

    if (u_ptr)
        vp8_mbloop_filter_vertical_edge_uv_sse2(u_ptr, uv_stride, lfi->uvmbflim, lfi->uvlim, lfi->uvmbthr, v_ptr);
}


void vp8_loop_filter_mbvs_sse2(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                               int y_stride, int uv_stride, loop_filter_info *lfi, int simpler_lpf)
{
    (void) u_ptr;
    (void) v_ptr;
    (void) uv_stride;
    (void) simpler_lpf;
    vp8_loop_filter_simple_vertical_edge_sse2(y_ptr, y_stride, lfi->mbflim, lfi->lim, lfi->mbthr, 2);
}


// Horizontal B Filtering
void vp8_loop_filter_bh_sse2(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                             int y_stride, int uv_stride, loop_filter_info *lfi, int simpler_lpf)
{
    (void) simpler_lpf;
    vp8_loop_filter_horizontal_edge_sse2(y_ptr + 4 * y_stride, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_horizontal_edge_sse2(y_ptr + 8 * y_stride, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_horizontal_edge_sse2(y_ptr + 12 * y_stride, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);

    if (u_ptr)
        vp8_loop_filter_horizontal_edge_uv_sse2(u_ptr + 4 * uv_stride, uv_stride, lfi->uvflim, lfi->uvlim, lfi->uvthr, v_ptr + 4 * uv_stride);
}


void vp8_loop_filter_bhs_sse2(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                              int y_stride, int uv_stride, loop_filter_info *lfi, int simpler_lpf)
{
    (void) u_ptr;
    (void) v_ptr;
    (void) uv_stride;
    (void) simpler_lpf;
    vp8_loop_filter_simple_horizontal_edge_sse2(y_ptr + 4 * y_stride, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_simple_horizontal_edge_sse2(y_ptr + 8 * y_stride, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_simple_horizontal_edge_sse2(y_ptr + 12 * y_stride, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
}


// Vertical B Filtering
void vp8_loop_filter_bv_sse2(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                             int y_stride, int uv_stride, loop_filter_info *lfi, int simpler_lpf)
{
    (void) simpler_lpf;
    vp8_loop_filter_vertical_edge_sse2(y_ptr + 4, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_vertical_edge_sse2(y_ptr + 8, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_vertical_edge_sse2(y_ptr + 12, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);

    if (u_ptr)
        vp8_loop_filter_vertical_edge_uv_sse2(u_ptr + 4, uv_stride, lfi->uvflim, lfi->uvlim, lfi->uvthr, v_ptr + 4);
}


void vp8_loop_filter_bvs_sse2(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                              int y_stride, int uv_stride, loop_filter_info *lfi, int simpler_lpf)
{
    (void) u_ptr;
    (void) v_ptr;
    (void) uv_stride;
    (void) simpler_lpf;
    vp8_loop_filter_simple_vertical_edge_sse2(y_ptr + 4, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_simple_vertical_edge_sse2(y_ptr + 8, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_simple_vertical_edge_sse2(y_ptr + 12, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
}

#endif

#if 0
void vp8_fast_loop_filter_vertical_edges_sse(unsigned char *y_ptr,
        int y_stride,
        loop_filter_info *lfi)
{

    vp8_loop_filter_simple_vertical_edge_sse2(y_ptr + 4, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_simple_vertical_edge_sse2(y_ptr + 8, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
    vp8_loop_filter_simple_vertical_edge_sse2(y_ptr + 12, y_stride, lfi->flim, lfi->lim, lfi->thr, 2);
}
#endif
