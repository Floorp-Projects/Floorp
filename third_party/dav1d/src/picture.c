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

#include "src/picture.h"
#include "src/ref.h"
#include "src/thread.h"
#include "src/thread_task.h"

int default_picture_allocator(Dav1dPicture *const p, void *cookie) {
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

    uint8_t *data = dav1d_alloc_aligned(pic_size, 32);
    if (data == NULL) {
        fprintf(stderr, "Failed to allocate memory of size %zu: %s\n",
                pic_size, strerror(errno));
        return -1;
    }

    p->data[0] = data;
    p->data[1] = has_chroma ? data + y_sz : NULL;
    p->data[2] = has_chroma ? data + y_sz + uv_sz : NULL;

#ifndef NDEBUG /* safety check */
    p->allocator_data = data;
#endif

    return 0;
}

void default_picture_release(Dav1dPicture *const p, void *cookie) {
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

static int picture_alloc_with_edges(Dav1dPicture *const p,
                                    const int w, const int h,
                                    const enum Dav1dPixelLayout layout,
                                    const int bpc,
                                    Dav1dPicAllocator *const p_allocator,
                                    const size_t extra, void **const extra_ptr)
{
    if (p->data[0]) {
        fprintf(stderr, "Picture already allocated!\n");
        return -1;
    }
    assert(bpc > 0 && bpc <= 16);

    struct pic_ctx_context *pic_ctx = malloc(extra + sizeof(struct pic_ctx_context));
    if (pic_ctx == NULL) {
        return -ENOMEM;
    }

    p->p.w = w;
    p->p.h = h;
    p->m.timestamp = INT64_MIN;
    p->m.duration = 0;
    p->m.offset = -1;
    p->p.layout = layout;
    p->p.bpc = bpc;
    int res = p_allocator->alloc_picture_callback(p, p_allocator->cookie);
    if (res < 0) {
        free(pic_ctx);
        return -ENOMEM;
    }

    pic_ctx->allocator = *p_allocator;
    pic_ctx->pic = *p;

    if (!(p->ref = dav1d_ref_wrap(p->data[0], free_buffer, pic_ctx))) {
        p_allocator->release_picture_callback(p, p_allocator->cookie);
        fprintf(stderr, "Failed to wrap picture: %s\n", strerror(errno));
        return -ENOMEM;
    }

    if (extra && extra_ptr)
        *extra_ptr = &pic_ctx->extra_ptr;

    return 0;
}

int dav1d_thread_picture_alloc(Dav1dThreadPicture *const p,
                               const int w, const int h,
                               const enum Dav1dPixelLayout layout, const int bpc,
                               struct thread_data *const t, const int visible,
                               Dav1dPicAllocator *const p_allocator)
{
    p->t = t;

    const int res =
        picture_alloc_with_edges(&p->p, w, h, layout, bpc, p_allocator,
                                 t != NULL ? sizeof(atomic_int) * 2 : 0,
                                 (void **) &p->progress);
    if (res) return res;

    p->visible = visible;
    if (t) {
        atomic_init(&p->progress[0], 0);
        atomic_init(&p->progress[1], 0);
    }
    return res;
}

int dav1d_picture_alloc_copy(Dav1dPicture *const dst, const int w,
                             const Dav1dPicture *const src)
{
    struct pic_ctx_context *const pic_ctx = src->ref->user_data;
    const int res = picture_alloc_with_edges(dst, w, src->p.h, src->p.layout,
                                             src->p.bpc, &pic_ctx->allocator,
                                             0, NULL);

    if (!res) {
        dst->p = src->p;
        dst->m = src->m;
        dst->p.w = w;
        dst->frame_hdr = src->frame_hdr;
        dst->frame_hdr_ref = src->frame_hdr_ref;
        if (dst->frame_hdr_ref) dav1d_ref_inc(dst->frame_hdr_ref);
        dst->seq_hdr = src->seq_hdr;
        dst->seq_hdr_ref = src->seq_hdr_ref;
        if (dst->seq_hdr_ref) dav1d_ref_inc(dst->seq_hdr_ref);
    }

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

void dav1d_picture_unref(Dav1dPicture *const p) {
    validate_input(p != NULL);

    if (p->ref) {
        validate_input(p->data[0] != NULL);
        dav1d_ref_dec(&p->ref);
        dav1d_ref_dec(&p->seq_hdr_ref);
        dav1d_ref_dec(&p->frame_hdr_ref);
    }
    memset(p, 0, sizeof(*p));
}

void dav1d_thread_picture_unref(Dav1dThreadPicture *const p) {
    dav1d_picture_unref(&p->p);

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
