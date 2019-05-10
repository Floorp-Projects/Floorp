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
#include "vcs_version.h"

#include <errno.h>
#include <string.h>

#include "dav1d/dav1d.h"
#include "dav1d/data.h"

#include "common/mem.h"
#include "common/validate.h"

#include "src/internal.h"
#include "src/log.h"
#include "src/obu.h"
#include "src/qm.h"
#include "src/ref.h"
#include "src/thread_task.h"
#include "src/wedge.h"
#include "src/film_grain.h"

static void init_internal(void) {
    dav1d_init_wedge_masks();
    dav1d_init_interintra_masks();
    dav1d_init_qm_tables();
}

const char *dav1d_version(void) {
    return DAV1D_VERSION;
}

void dav1d_default_settings(Dav1dSettings *const s) {
    s->n_frame_threads = 1;
    s->n_tile_threads = 1;
    s->apply_grain = 1;
    s->allocator.cookie = NULL;
    s->allocator.alloc_picture_callback = dav1d_default_picture_alloc;
    s->allocator.release_picture_callback = dav1d_default_picture_release;
    s->logger.cookie = NULL;
    s->logger.callback = dav1d_log_default_callback;
    s->operating_point = 0;
    s->all_layers = 1; // just until the tests are adjusted
}

static void close_internal(Dav1dContext **const c_out, int flush);

int dav1d_open(Dav1dContext **const c_out,
               const Dav1dSettings *const s)
{
    static pthread_once_t initted = PTHREAD_ONCE_INIT;
    pthread_once(&initted, init_internal);

    validate_input_or_ret(c_out != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(s != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(s->n_tile_threads >= 1 &&
                          s->n_tile_threads <= DAV1D_MAX_TILE_THREADS, DAV1D_ERR(EINVAL));
    validate_input_or_ret(s->n_frame_threads >= 1 &&
                          s->n_frame_threads <= DAV1D_MAX_FRAME_THREADS, DAV1D_ERR(EINVAL));
    validate_input_or_ret(s->allocator.alloc_picture_callback != NULL,
                          DAV1D_ERR(EINVAL));
    validate_input_or_ret(s->allocator.release_picture_callback != NULL,
                          DAV1D_ERR(EINVAL));
    validate_input_or_ret(s->operating_point >= 0 &&
                          s->operating_point <= 31, DAV1D_ERR(EINVAL));

    pthread_attr_t thread_attr;
    if (pthread_attr_init(&thread_attr)) return DAV1D_ERR(ENOMEM);
    pthread_attr_setstacksize(&thread_attr, 512 * 1024);

    Dav1dContext *const c = *c_out = dav1d_alloc_aligned(sizeof(*c), 32);
    if (!c) goto error;
    memset(c, 0, sizeof(*c));

    c->allocator = s->allocator;
    c->logger = s->logger;
    c->apply_grain = s->apply_grain;
    c->operating_point = s->operating_point;
    c->all_layers = s->all_layers;
    c->frame_thread.flush = &c->frame_thread.flush_mem;
    atomic_init(c->frame_thread.flush, 0);
    c->n_fc = s->n_frame_threads;
    c->fc = dav1d_alloc_aligned(sizeof(*c->fc) * s->n_frame_threads, 32);
    if (!c->fc) goto error;
    memset(c->fc, 0, sizeof(*c->fc) * s->n_frame_threads);
    if (c->n_fc > 1) {
        c->frame_thread.out_delayed =
            malloc(sizeof(*c->frame_thread.out_delayed) * c->n_fc);
        if (!c->frame_thread.out_delayed) goto error;
        memset(c->frame_thread.out_delayed, 0,
               sizeof(*c->frame_thread.out_delayed) * c->n_fc);
    }
    for (int n = 0; n < s->n_frame_threads; n++) {
        Dav1dFrameContext *const f = &c->fc[n];
        f->c = c;
        f->lf.last_sharpness = -1;
        f->n_tc = s->n_tile_threads;
        f->tc = dav1d_alloc_aligned(sizeof(*f->tc) * s->n_tile_threads, 32);
        if (!f->tc) goto error;
        memset(f->tc, 0, sizeof(*f->tc) * s->n_tile_threads);
        if (f->n_tc > 1) {
            if (pthread_mutex_init(&f->tile_thread.lock, NULL)) goto error;
            if (pthread_cond_init(&f->tile_thread.cond, NULL)) {
                pthread_mutex_destroy(&f->tile_thread.lock);
                goto error;
            }
            if (pthread_cond_init(&f->tile_thread.icond, NULL)) {
                pthread_mutex_destroy(&f->tile_thread.lock);
                pthread_cond_destroy(&f->tile_thread.cond);
                goto error;
            }
            f->tile_thread.inited = 1;
        }
        for (int m = 0; m < s->n_tile_threads; m++) {
            Dav1dTileContext *const t = &f->tc[m];
            t->f = f;
            t->cf = dav1d_alloc_aligned(32 * 32 * sizeof(int32_t), 32);
            if (!t->cf) goto error;
            t->scratch.mem = dav1d_alloc_aligned(128 * 128 * 4, 32);
            if (!t->scratch.mem) goto error;
            memset(t->cf, 0, 32 * 32 * sizeof(int32_t));
            t->emu_edge =
                dav1d_alloc_aligned(320 * (256 + 7) * sizeof(uint16_t), 32);
            if (!t->emu_edge) goto error;
            if (f->n_tc > 1) {
                if (pthread_mutex_init(&t->tile_thread.td.lock, NULL)) goto error;
                if (pthread_cond_init(&t->tile_thread.td.cond, NULL)) {
                    pthread_mutex_destroy(&t->tile_thread.td.lock);
                    goto error;
                }
                t->tile_thread.fttd = &f->tile_thread;
                if (pthread_create(&t->tile_thread.td.thread, &thread_attr, dav1d_tile_task, t)) {
                    pthread_cond_destroy(&t->tile_thread.td.cond);
                    pthread_mutex_destroy(&t->tile_thread.td.lock);
                    goto error;
                }
                t->tile_thread.td.inited = 1;
            }
        }
        f->libaom_cm = dav1d_alloc_ref_mv_common();
        if (!f->libaom_cm) goto error;
        if (c->n_fc > 1) {
            if (pthread_mutex_init(&f->frame_thread.td.lock, NULL)) goto error;
            if (pthread_cond_init(&f->frame_thread.td.cond, NULL)) {
                pthread_mutex_destroy(&f->frame_thread.td.lock);
                goto error;
            }
            if (pthread_create(&f->frame_thread.td.thread, &thread_attr, dav1d_frame_task, f)) {
                pthread_cond_destroy(&f->frame_thread.td.cond);
                pthread_mutex_destroy(&f->frame_thread.td.lock);
                goto error;
            }
            f->frame_thread.td.inited = 1;
        }
    }

    // intra edge tree
    c->intra_edge.root[BL_128X128] = &c->intra_edge.branch_sb128[0].node;
    dav1d_init_mode_tree(c->intra_edge.root[BL_128X128], c->intra_edge.tip_sb128, 1);
    c->intra_edge.root[BL_64X64] = &c->intra_edge.branch_sb64[0].node;
    dav1d_init_mode_tree(c->intra_edge.root[BL_64X64], c->intra_edge.tip_sb64, 0);

    pthread_attr_destroy(&thread_attr);

    return 0;

error:
    if (c) close_internal(c_out, 0);
    pthread_attr_destroy(&thread_attr);
    return DAV1D_ERR(ENOMEM);
}

static void dummy_free(const uint8_t *const data, void *const user_data) {
    assert(data && !user_data);
}

int dav1d_parse_sequence_header(Dav1dSequenceHeader *const out,
                                const uint8_t *const ptr, const size_t sz)
{
    Dav1dData buf = { 0 };
    int res;

    validate_input_or_ret(out != NULL, DAV1D_ERR(EINVAL));

    Dav1dSettings s;
    dav1d_default_settings(&s);
    s.logger.callback = NULL;

    Dav1dContext *c;
    res = dav1d_open(&c, &s);
    if (res < 0) return res;

    if (ptr) {
        res = dav1d_data_wrap_internal(&buf, ptr, sz, dummy_free, NULL);
        if (res < 0) goto error;
    }

    while (buf.sz > 0) {
        res = dav1d_parse_obus(c, &buf, 1);
        if (res < 0) goto error;

        assert((size_t)res <= buf.sz);
        buf.sz -= res;
        buf.data += res;
    }

    if (!c->seq_hdr) {
        res = DAV1D_ERR(EINVAL);
        goto error;
    }

    memcpy(out, c->seq_hdr, sizeof(*out));

    res = 0;
error:
    dav1d_data_unref_internal(&buf);
    dav1d_close(&c);

    return res;
}

int dav1d_send_data(Dav1dContext *const c, Dav1dData *const in)
{
    validate_input_or_ret(c != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(in != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(in->data == NULL || in->sz, DAV1D_ERR(EINVAL));

    c->drain = 0;

    if (c->in.data)
        return DAV1D_ERR(EAGAIN);
    dav1d_data_move_ref(&c->in, in);

    return 0;
}

static int output_image(Dav1dContext *const c, Dav1dPicture *const out,
                        Dav1dPicture *const in)
{
    const Dav1dFilmGrainData *fgdata = &in->frame_hdr->film_grain.data;
    int has_grain = fgdata->num_y_points || fgdata->num_uv_points[0] ||
                    fgdata->num_uv_points[1];

    // If there is nothing to be done, skip the allocation/copy
    if (!c->apply_grain || !has_grain) {
        dav1d_picture_move_ref(out, in);
        return 0;
    }

    // Apply film grain to a new copy of the image to avoid corrupting refs
    int res = dav1d_picture_alloc_copy(c, out, in->p.w, in);
    if (res < 0) {
        dav1d_picture_unref_internal(in);
        dav1d_picture_unref_internal(out);
        return res;
    }

    switch (out->p.bpc) {
#if CONFIG_8BPC
    case 8:
        dav1d_apply_grain_8bpc(out, in);
        break;
#endif
#if CONFIG_16BPC
    case 10:
    case 12:
        dav1d_apply_grain_16bpc(out, in);
        break;
#endif
    default:
        assert(0);
    }

    dav1d_picture_unref_internal(in);
    return 0;
}

static int output_picture_ready(Dav1dContext *const c) {

    if (!c->out.data[0]) return 0;

    // skip lower spatial layers
    if (c->operating_point_idc && !c->all_layers) {
        const int max_spatial_id = ulog2(c->operating_point_idc >> 8);
        if (max_spatial_id > c->out.frame_hdr->spatial_id) {
            dav1d_picture_unref_internal(&c->out);
            return 0;
        }
    }

    return 1;
}

static int drain_picture(Dav1dContext *const c, Dav1dPicture *const out) {
    unsigned drain_count = 0;
    do {
        const unsigned next = c->frame_thread.next;
        Dav1dFrameContext *const f = &c->fc[next];
        pthread_mutex_lock(&f->frame_thread.td.lock);
        while (f->n_tile_data > 0)
            pthread_cond_wait(&f->frame_thread.td.cond,
                              &f->frame_thread.td.lock);
        pthread_mutex_unlock(&f->frame_thread.td.lock);
        Dav1dThreadPicture *const out_delayed =
            &c->frame_thread.out_delayed[next];
        if (++c->frame_thread.next == c->n_fc)
            c->frame_thread.next = 0;
        if (out_delayed->p.data[0]) {
            const unsigned progress =
                atomic_load_explicit(&out_delayed->progress[1],
                                     memory_order_relaxed);
            if (out_delayed->visible && progress != FRAME_ERROR)
                dav1d_picture_ref(&c->out, &out_delayed->p);
            dav1d_thread_picture_unref(out_delayed);
            if (output_picture_ready(c))
                return output_image(c, out, &c->out);
        }
    } while (++drain_count < c->n_fc);

    return DAV1D_ERR(EAGAIN);
}

int dav1d_get_picture(Dav1dContext *const c, Dav1dPicture *const out)
{
    int res;

    validate_input_or_ret(c != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(out != NULL, DAV1D_ERR(EINVAL));

    const int drain = c->drain;
    c->drain = 1;

    Dav1dData *const in = &c->in;
    if (!in->data) {
        if (c->n_fc == 1) return DAV1D_ERR(EAGAIN);
        return drain_picture(c, out);
    }

    while (in->sz > 0) {
        res = dav1d_parse_obus(c, in, 0);
        if (res < 0) {
            dav1d_data_unref_internal(in);
        } else {
            assert((size_t)res <= in->sz);
            in->sz -= res;
            in->data += res;
            if (!in->sz) dav1d_data_unref_internal(in);
        }
        if (output_picture_ready(c))
            break;
        if (res < 0)
            return res;
    }

    if (output_picture_ready(c))
        return output_image(c, out, &c->out);

    if (c->n_fc > 1 && drain)
        return drain_picture(c, out);

    return DAV1D_ERR(EAGAIN);
}

void dav1d_flush(Dav1dContext *const c) {
    dav1d_data_unref_internal(&c->in);
    c->drain = 0;

    for (int i = 0; i < 8; i++) {
        if (c->refs[i].p.p.data[0])
            dav1d_thread_picture_unref(&c->refs[i].p);
        dav1d_ref_dec(&c->refs[i].segmap);
        dav1d_ref_dec(&c->refs[i].refmvs);
        dav1d_cdf_thread_unref(&c->cdf[i]);
    }
    c->frame_hdr = NULL;
    c->seq_hdr = NULL;
    dav1d_ref_dec(&c->seq_hdr_ref);

    c->mastering_display = NULL;
    c->content_light = NULL;
    dav1d_ref_dec(&c->mastering_display_ref);
    dav1d_ref_dec(&c->content_light_ref);

    if (c->n_fc == 1) return;

    // mark each currently-running frame as flushing, so that we
    // exit out as quickly as the running thread checks this flag
    atomic_store(c->frame_thread.flush, 1);
    for (unsigned n = 0, next = c->frame_thread.next; n < c->n_fc; n++, next++) {
        if (next == c->n_fc) next = 0;
        Dav1dFrameContext *const f = &c->fc[next];
        pthread_mutex_lock(&f->frame_thread.td.lock);
        if (f->n_tile_data > 0) {
            while (f->n_tile_data > 0)
                pthread_cond_wait(&f->frame_thread.td.cond,
                                  &f->frame_thread.td.lock);
            assert(!f->cur.data[0]);
        }
        pthread_mutex_unlock(&f->frame_thread.td.lock);
        Dav1dThreadPicture *const out_delayed = &c->frame_thread.out_delayed[next];
        if (out_delayed->p.data[0])
            dav1d_thread_picture_unref(out_delayed);
    }
    atomic_store(c->frame_thread.flush, 0);

    c->frame_thread.next = 0;
}

void dav1d_close(Dav1dContext **const c_out) {
    validate_input(c_out != NULL);
    close_internal(c_out, 1);
}

static void close_internal(Dav1dContext **const c_out, int flush) {
    Dav1dContext *const c = *c_out;
    if (!c) return;

    if (flush) dav1d_flush(c);

    for (unsigned n = 0; c->fc && n < c->n_fc; n++) {
        Dav1dFrameContext *const f = &c->fc[n];

        // clean-up threading stuff
        if (c->n_fc > 1 && f->frame_thread.td.inited) {
            pthread_mutex_lock(&f->frame_thread.td.lock);
            f->frame_thread.die = 1;
            pthread_cond_signal(&f->frame_thread.td.cond);
            pthread_mutex_unlock(&f->frame_thread.td.lock);
            pthread_join(f->frame_thread.td.thread, NULL);
            freep(&f->frame_thread.b);
            dav1d_freep_aligned(&f->frame_thread.pal_idx);
            dav1d_freep_aligned(&f->frame_thread.cf);
            freep(&f->frame_thread.tile_start_off);
            dav1d_freep_aligned(&f->frame_thread.pal);
            freep(&f->frame_thread.cbi);
            pthread_mutex_destroy(&f->frame_thread.td.lock);
            pthread_cond_destroy(&f->frame_thread.td.cond);
        }
        if (f->n_tc > 1 && f->tc && f->tile_thread.inited) {
            pthread_mutex_lock(&f->tile_thread.lock);
            for (int m = 0; m < f->n_tc; m++) {
                Dav1dTileContext *const t = &f->tc[m];
                t->tile_thread.die = 1;
                // mark not created tile threads as available
                if (!t->tile_thread.td.inited)
                    f->tile_thread.available |= 1ULL<<m;
            }
            pthread_cond_broadcast(&f->tile_thread.cond);
            while (f->tile_thread.available != ~0ULL >> (64 - f->n_tc))
                pthread_cond_wait(&f->tile_thread.icond,
                                  &f->tile_thread.lock);
            pthread_mutex_unlock(&f->tile_thread.lock);
            for (int m = 0; m < f->n_tc; m++) {
                Dav1dTileContext *const t = &f->tc[m];
                if (f->n_tc > 1 && t->tile_thread.td.inited) {
                    pthread_join(t->tile_thread.td.thread, NULL);
                    pthread_mutex_destroy(&t->tile_thread.td.lock);
                    pthread_cond_destroy(&t->tile_thread.td.cond);
                }
            }
            pthread_mutex_destroy(&f->tile_thread.lock);
            pthread_cond_destroy(&f->tile_thread.cond);
            pthread_cond_destroy(&f->tile_thread.icond);
            freep(&f->tile_thread.task_idx_to_sby_and_tile_idx);
        }
        for (int m = 0; f->tc && m < f->n_tc; m++) {
            Dav1dTileContext *const t = &f->tc[m];
            dav1d_free_aligned(t->cf);
            dav1d_free_aligned(t->scratch.mem);
            dav1d_free_aligned(t->emu_edge);
        }
        for (int m = 0; f->ts && m < f->n_ts; m++) {
            Dav1dTileState *const ts = &f->ts[m];
            pthread_cond_destroy(&ts->tile_thread.cond);
            pthread_mutex_destroy(&ts->tile_thread.lock);
        }
        free(f->ts);
        dav1d_free_aligned(f->tc);
        dav1d_free_aligned(f->ipred_edge[0]);
        free(f->a);
        free(f->tile);
        free(f->lf.mask);
        free(f->lf.lr_mask);
        free(f->lf.level);
        free(f->lf.tx_lpf_right_edge[0]);
        if (f->libaom_cm) dav1d_free_ref_mv_common(f->libaom_cm);
        dav1d_free_aligned(f->lf.cdef_line);
        dav1d_free_aligned(f->lf.lr_lpf_line);
    }
    dav1d_free_aligned(c->fc);
    dav1d_data_unref_internal(&c->in);
    if (c->n_fc > 1 && c->frame_thread.out_delayed) {
        for (unsigned n = 0; n < c->n_fc; n++)
            if (c->frame_thread.out_delayed[n].p.data[0])
                dav1d_thread_picture_unref(&c->frame_thread.out_delayed[n]);
        free(c->frame_thread.out_delayed);
    }
    for (int n = 0; n < c->n_tile_data; n++)
        dav1d_data_unref_internal(&c->tile[n].data);
    free(c->tile);
    for (int n = 0; n < 8; n++) {
        dav1d_cdf_thread_unref(&c->cdf[n]);
        if (c->refs[n].p.p.data[0])
            dav1d_thread_picture_unref(&c->refs[n].p);
        dav1d_ref_dec(&c->refs[n].refmvs);
        dav1d_ref_dec(&c->refs[n].segmap);
    }
    dav1d_ref_dec(&c->seq_hdr_ref);
    dav1d_ref_dec(&c->frame_hdr_ref);

    dav1d_ref_dec(&c->mastering_display_ref);
    dav1d_ref_dec(&c->content_light_ref);

    dav1d_freep_aligned(c_out);
}

void dav1d_picture_unref(Dav1dPicture *const p) {
    dav1d_picture_unref_internal(p);
}

uint8_t *dav1d_data_create(Dav1dData *const buf, const size_t sz) {
    return dav1d_data_create_internal(buf, sz);
}

int dav1d_data_wrap(Dav1dData *const buf, const uint8_t *const ptr,
                    const size_t sz,
                    void (*const free_callback)(const uint8_t *data,
                                                void *user_data),
                    void *const user_data)
{
    return dav1d_data_wrap_internal(buf, ptr, sz, free_callback, user_data);
}

int dav1d_data_wrap_user_data(Dav1dData *const buf,
                              const uint8_t *const user_data,
                              void (*const free_callback)(const uint8_t *user_data,
                                                          void *cookie),
                              void *const cookie)
{
    return dav1d_data_wrap_user_data_internal(buf,
                                              user_data,
                                              free_callback,
                                              cookie);
}

void dav1d_data_unref(Dav1dData *const buf) {
    dav1d_data_unref_internal(buf);
}
