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
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/intops.h"
#include "common/mem.h"
#include "common/validate.h"

#include "src/internal.h"
#include "src/log.h"
#include "src/picture.h"
#include "src/ref.h"
#include "src/thread.h"
#include "src/thread_task.h"

int dav1d_default_picture_alloc(Dav1dPicture *const p, void *const cookie) {
    assert(cookie == NULL);
    const int hbd = p->p.bpc > 8;
    const int aligned_w = (p->p.w + 127) & ~127;
    const int aligned_h = (p->p.h + 127) & ~127;
    const int has_chroma = p->p.layout != DAV1D_PIXEL_LAYOUT_I400;
    const int ss_ver = p->p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = p->p.layout != DAV1D_PIXEL_LAYOUT_I444;
    p->stride[0] = aligned_w << hbd;
    p->stride[1] = has_chroma ? (aligned_w >> ss_hor) << hbd : 0;
    const size_t y_sz = p->stride[0] * aligned_h;
    const size_t uv_sz = p->stride[1] * (aligned_h >> ss_ver);
    const size_t pic_size = y_sz + 2 * uv_sz;

    uint8_t *data = dav1d_alloc_aligned(pic_size + DAV1D_PICTURE_ALIGNMENT,
                                        DAV1D_PICTURE_ALIGNMENT);
    if (data == NULL) {
        return DAV1D_ERR(ENOMEM);
    }

    p->data[0] = data;
    p->data[1] = has_chroma ? data + y_sz : NULL;
    p->data[2] = has_chroma ? data + y_sz + uv_sz : NULL;

#ifndef NDEBUG /* safety check */
    p->allocator_data = data;
#endif

    return 0;
}

void dav1d_default_picture_release(Dav1dPicture *const p, void *const cookie) {
    assert(cookie == NULL);
#ifndef NDEBUG /* safety check */
    assert(p->allocator_data == p->data[0]);
#endif
    dav1d_free_aligned(p->data[0]);
}

struct pic_ctx_context {
    Dav1dPicAllocator allocator;
    Dav1dPicture pic;
    void *extra_ptr; /* MUST BE AT THE END */
};

static void free_buffer(const uint8_t *const data, void *const user_data) {
    struct pic_ctx_context *pic_ctx = user_data;

    pic_ctx->allocator.release_picture_callback(&pic_ctx->pic,
                                                pic_ctx->allocator.cookie);
    free(pic_ctx);
}

static int picture_alloc_with_edges(Dav1dContext *const c, Dav1dPicture *const p,
                                    const int w, const int h,
                                    Dav1dSequenceHeader *seq_hdr, Dav1dRef *seq_hdr_ref,
                                    Dav1dFrameHeader *frame_hdr,  Dav1dRef *frame_hdr_ref,
                                    Dav1dContentLightLevel *content_light, Dav1dRef *content_light_ref,
                                    Dav1dMasteringDisplay *mastering_display, Dav1dRef *mastering_display_ref,
                                    const int bpc, const Dav1dDataProps *props,
                                    Dav1dPicAllocator *const p_allocator,
                                    const size_t extra, void **const extra_ptr)
{
    if (p->data[0]) {
        dav1d_log(c, "Picture already allocated!\n");
        return -1;
    }
    assert(bpc > 0 && bpc <= 16);

    struct pic_ctx_context *pic_ctx = malloc(extra + sizeof(struct pic_ctx_context));
    if (pic_ctx == NULL) {
        return DAV1D_ERR(ENOMEM);
    }

    p->p.w = w;
    p->p.h = h;
    p->seq_hdr = seq_hdr;
    p->frame_hdr = frame_hdr;
    p->content_light = content_light;
    p->mastering_display = mastering_display;
    p->p.layout = seq_hdr->layout;
    p->p.bpc = bpc;
    dav1d_data_props_set_defaults(&p->m);
    int res = p_allocator->alloc_picture_callback(p, p_allocator->cookie);
    if (res < 0) {
        free(pic_ctx);
        return res;
    }

    pic_ctx->allocator = *p_allocator;
    pic_ctx->pic = *p;

    if (!(p->ref = dav1d_ref_wrap(p->data[0], free_buffer, pic_ctx))) {
        p_allocator->release_picture_callback(p, p_allocator->cookie);
        free(pic_ctx);
        dav1d_log(c, "Failed to wrap picture: %s\n", strerror(errno));
        return DAV1D_ERR(ENOMEM);
    }

    p->seq_hdr_ref = seq_hdr_ref;
    if (seq_hdr_ref) dav1d_ref_inc(seq_hdr_ref);

    p->frame_hdr_ref = frame_hdr_ref;
    if (frame_hdr_ref) dav1d_ref_inc(frame_hdr_ref);

    dav1d_data_props_copy(&p->m, props);

    if (extra && extra_ptr)
        *extra_ptr = &pic_ctx->extra_ptr;

    p->content_light_ref = content_light_ref;
    if (content_light_ref) dav1d_ref_inc(content_light_ref);

    p->mastering_display_ref = mastering_display_ref;
    if (mastering_display_ref) dav1d_ref_inc(mastering_display_ref);

    return 0;
}

int dav1d_thread_picture_alloc(Dav1dContext *const c, Dav1dFrameContext *const f,
                               const int bpc)
{
    Dav1dThreadPicture *const p = &f->sr_cur;
    p->t = c->n_fc > 1 ? &f->frame_thread.td : NULL;

    const int res =
        picture_alloc_with_edges(c, &p->p, f->frame_hdr->width[1], f->frame_hdr->height,
                                 f->seq_hdr, f->seq_hdr_ref,
                                 f->frame_hdr, f->frame_hdr_ref,
                                 c->content_light, c->content_light_ref,
                                 c->mastering_display, c->mastering_display_ref,
                                 bpc, &f->tile[0].data.m, &c->allocator,
                                 p->t != NULL ? sizeof(atomic_int) * 2 : 0,
                                 (void **) &p->progress);
    if (res) return res;

    p->visible = f->frame_hdr->show_frame;
    if (p->t) {
        atomic_init(&p->progress[0], 0);
        atomic_init(&p->progress[1], 0);
    }
    return res;
}

int dav1d_picture_alloc_copy(Dav1dContext *const c, Dav1dPicture *const dst, const int w,
                             const Dav1dPicture *const src)
{
    struct pic_ctx_context *const pic_ctx = src->ref->user_data;
    const int res = picture_alloc_with_edges(c, dst, w, src->p.h,
                                             src->seq_hdr, src->seq_hdr_ref,
                                             src->frame_hdr, src->frame_hdr_ref,
                                             src->content_light, src->content_light_ref,
                                             src->mastering_display, src->mastering_display_ref,
                                             src->p.bpc, &src->m, &pic_ctx->allocator,
                                             0, NULL);
    return res;
}

void dav1d_picture_ref(Dav1dPicture *const dst, const Dav1dPicture *const src) {
    validate_input(dst != NULL);
    validate_input(dst->data[0] == NULL);
    validate_input(src != NULL);

    if (src->ref) {
        validate_input(src->data[0] != NULL);
        dav1d_ref_inc(src->ref);
        if (src->frame_hdr_ref) dav1d_ref_inc(src->frame_hdr_ref);
        if (src->seq_hdr_ref) dav1d_ref_inc(src->seq_hdr_ref);
        if (src->m.user_data.ref) dav1d_ref_inc(src->m.user_data.ref);
        if (src->content_light_ref) dav1d_ref_inc(src->content_light_ref);
        if (src->mastering_display_ref) dav1d_ref_inc(src->mastering_display_ref);
    }
    *dst = *src;
}

void dav1d_picture_move_ref(Dav1dPicture *const dst, Dav1dPicture *const src) {
    validate_input(dst != NULL);
    validate_input(dst->data[0] == NULL);
    validate_input(src != NULL);

    if (src->ref)
        validate_input(src->data[0] != NULL);

    *dst = *src;
    memset(src, 0, sizeof(*src));
}

void dav1d_thread_picture_ref(Dav1dThreadPicture *dst,
                              const Dav1dThreadPicture *src)
{
    dav1d_picture_ref(&dst->p, &src->p);
    dst->t = src->t;
    dst->visible = src->visible;
    dst->progress = src->progress;
}

void dav1d_picture_unref_internal(Dav1dPicture *const p) {
    validate_input(p != NULL);

    if (p->ref) {
        validate_input(p->data[0] != NULL);
        dav1d_ref_dec(&p->ref);
        dav1d_ref_dec(&p->seq_hdr_ref);
        dav1d_ref_dec(&p->frame_hdr_ref);
        dav1d_ref_dec(&p->m.user_data.ref);
        dav1d_ref_dec(&p->content_light_ref);
        dav1d_ref_dec(&p->mastering_display_ref);
    }
    memset(p, 0, sizeof(*p));
}

void dav1d_thread_picture_unref(Dav1dThreadPicture *const p) {
    dav1d_picture_unref_internal(&p->p);

    p->t = NULL;
    p->progress = NULL;
}

int dav1d_thread_picture_wait(const Dav1dThreadPicture *const p,
                              int y_unclipped, const enum PlaneType plane_type)
{
    assert(plane_type != PLANE_TYPE_ALL);

    if (!p->t)
        return 0;

    // convert to luma units; include plane delay from loopfilters; clip
    const int ss_ver = p->p.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    y_unclipped *= 1 << (plane_type & ss_ver); // we rely here on PLANE_TYPE_UV being 1
    y_unclipped += (plane_type != PLANE_TYPE_BLOCK) * 8; // delay imposed by loopfilter
    const unsigned y = iclip(y_unclipped, 1, p->p.p.h);
    atomic_uint *const progress = &p->progress[plane_type != PLANE_TYPE_BLOCK];
    unsigned state;

    if ((state = atomic_load_explicit(progress, memory_order_acquire)) >= y)
        return state == FRAME_ERROR;

    pthread_mutex_lock(&p->t->lock);
    while ((state = atomic_load_explicit(progress, memory_order_relaxed)) < y)
        pthread_cond_wait(&p->t->cond, &p->t->lock);
    pthread_mutex_unlock(&p->t->lock);
    return state == FRAME_ERROR;
}

void dav1d_thread_picture_signal(const Dav1dThreadPicture *const p,
                                 const int y, // in pixel units
                                 const enum PlaneType plane_type)
{
    assert(plane_type != PLANE_TYPE_UV);

    if (!p->t)
        return;

    pthread_mutex_lock(&p->t->lock);
    if (plane_type != PLANE_TYPE_Y)
        atomic_store(&p->progress[0], y);
    if (plane_type != PLANE_TYPE_BLOCK)
        atomic_store(&p->progress[1], y);
    pthread_cond_broadcast(&p->t->cond);
    pthread_mutex_unlock(&p->t->lock);
}
