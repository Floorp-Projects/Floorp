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

#include "src/thread_task.h"

int dav1d_task_create_filter_sbrow(Dav1dFrameContext *const f) {
    struct PostFilterThreadData *const pftd = f->lf.thread.pftd;
    const int frame_idx = (int)(f - f->c->fc);

    const int has_deblock = f->frame_hdr->loopfilter.level_y[0] ||
                            f->frame_hdr->loopfilter.level_y[1] ||
                            f->lf.restore_planes;
    const int has_cdef = f->seq_hdr->cdef;
    const int has_resize = f->frame_hdr->width[0] != f->frame_hdr->width[1];
    const int has_lr = !!f->lf.restore_planes;
    f->lf.thread.npf = has_deblock + has_cdef + has_resize + has_lr;
    if (f->lf.thread.npf == 0) return 0;

    pthread_mutex_lock(&pftd->lock);

    Dav1dTask *tasks = f->lf.thread.tasks;
    int num_tasks = f->sbh * f->lf.thread.npf;
    if (num_tasks > f->lf.thread.num_tasks) {
        const size_t size = sizeof(Dav1dTask) * num_tasks;
        tasks = realloc(f->lf.thread.tasks, size);
        if (!tasks) {
            pthread_mutex_unlock(&pftd->lock);
            return -1;
        }
        memset(tasks, 0, size);
        f->lf.thread.tasks = tasks;
        f->lf.thread.num_tasks = num_tasks;
    }

#define create_task(task, ready_cond, start_cond) \
    do { \
        t = &tasks[num_tasks++]; \
        t->status = ready_cond ? DAV1D_TASK_READY : DAV1D_TASK_DEFAULT; \
        t->start = start_cond; \
        t->frame_id = frame_cnt; \
        t->frame_idx = frame_idx; \
        t->sby = sby; \
        t->fn = f->bd_fn.filter_sbrow_##task; \
        t->last_deps[0] = NULL; \
        t->last_deps[1] = NULL; \
        t->next_deps[0] = NULL; \
        t->next_deps[1] = NULL; \
        t->next_exec = NULL; \
    } while (0)

    Dav1dTask *last_sbrow_deblock = NULL;
    Dav1dTask *last_sbrow_cdef = NULL;
    Dav1dTask *last_sbrow_resize = NULL;
    Dav1dTask *last_sbrow_lr = NULL;
    num_tasks = 0;
    const int frame_cnt = pftd->frame_cnt++;

    for (int sby = 0; sby < f->sbh; ++sby) {
        Dav1dTask *t;
        Dav1dTask *last = NULL;
        if (has_deblock) {
            create_task(deblock, sby == 0, 0);
            if (sby) {
                t->last_deps[1] = last_sbrow_deblock;
                last_sbrow_deblock->next_deps[1] = t;
            }
            last = t;
            last_sbrow_deblock = t;
        }
        if (has_cdef) {
            create_task(cdef, sby == 0 && !has_deblock, has_deblock);
            if (has_deblock) {
                t->last_deps[0] = last;
                last->next_deps[0] = t;
            }
            if (sby) {
                t->last_deps[1] = last_sbrow_cdef;
                last_sbrow_cdef->next_deps[1] = t;
            }
            last = t;
            last_sbrow_cdef = t;
        };
        if (has_resize) {
            create_task(resize, sby == 0 && !last, !!last);
            if (last) {
                t->last_deps[0] = last;
                last->next_deps[0] = t;
            }
            if (sby) {
                t->last_deps[1] = last_sbrow_resize;
                last_sbrow_resize->next_deps[1] = t;
            }
            last = t;
            last_sbrow_resize = t;
        }
        if (has_lr) {
            create_task(lr, sby == 0 && !last, !!last);
            if (last) {
                t->last_deps[0] = last;
                last->next_deps[0] = t;
            }
            if (sby) {
                t->last_deps[1] = last_sbrow_lr;
                last_sbrow_lr->next_deps[1] = t;
            }
            last_sbrow_lr = t;
        }
    }
    f->lf.thread.done = 0;
    pthread_mutex_unlock(&pftd->lock);

    return 0;
}

void dav1d_task_schedule(struct PostFilterThreadData *const pftd,
                         Dav1dTask *const t)
{
    Dav1dTask **pt = &pftd->tasks;
    while (*pt &&
           ((*pt)->sby < t->sby ||
            ((*pt)->sby == t->sby && (*pt)->frame_id <= t->frame_id)))
        pt = &(*pt)->next_exec;
    t->next_exec = *pt;
    *pt = t;
    pthread_cond_signal(&pftd->cond);
}

static inline void update_task(Dav1dTask *const t, const int dep_type,
                               Dav1dFrameContext *const f)
{
    if (!t->last_deps[!dep_type] ||
        t->last_deps[!dep_type]->status == DAV1D_TASK_DONE)
    {
        t->status = DAV1D_TASK_READY;
        if (t->start)
            dav1d_task_schedule(f->lf.thread.pftd, t);
    }
}

void *dav1d_frame_task(void *const data) {
    Dav1dFrameContext *const f = data;

    dav1d_set_thread_name("dav1d-frame");
    pthread_mutex_lock(&f->frame_thread.td.lock);
    for (;;) {
        while (!f->n_tile_data && !f->frame_thread.die) {
            pthread_cond_wait(&f->frame_thread.td.cond,
                              &f->frame_thread.td.lock);
        }
        if (f->frame_thread.die) break;
        pthread_mutex_unlock(&f->frame_thread.td.lock);

        if (dav1d_decode_frame(f))
            memset(f->frame_thread.cf, 0,
                   (size_t)f->frame_thread.cf_sz * 128 * 128 / 2);

        pthread_mutex_lock(&f->frame_thread.td.lock);
        f->n_tile_data = 0;
        pthread_cond_signal(&f->frame_thread.td.cond);
    }
    pthread_mutex_unlock(&f->frame_thread.td.lock);

    return NULL;
}

void *dav1d_tile_task(void *const data) {
    Dav1dTileContext *const t = data;
    struct FrameTileThreadData *const fttd = t->tile_thread.fttd;
    const Dav1dFrameContext *const f = t->f;
    const int tile_thread_idx = (int) (t - f->tc);
    const uint64_t mask = 1ULL << tile_thread_idx;

    dav1d_set_thread_name("dav1d-tile");

    for (;;) {
        pthread_mutex_lock(&fttd->lock);
        fttd->available |= mask;
        int did_signal = 0;
        while (!fttd->tasks_left && !t->tile_thread.die) {
            if (!did_signal) {
                did_signal = 1;
                pthread_cond_signal(&fttd->icond);
            }
            pthread_cond_wait(&fttd->cond, &fttd->lock);
        }
        if (t->tile_thread.die) {
            pthread_cond_signal(&fttd->icond);
            pthread_mutex_unlock(&fttd->lock);
            break;
        }
        fttd->available &= ~mask;
        const int task_idx = fttd->num_tasks - fttd->tasks_left--;
        pthread_mutex_unlock(&fttd->lock);

        if (f->frame_thread.pass == 1 || f->n_tc >= f->frame_hdr->tiling.cols) {
            // we can (or in fact, if >, we need to) do full tile decoding.
            // loopfilter happens in the main thread
            Dav1dTileState *const ts = t->ts = &f->ts[task_idx];
            for (t->by = ts->tiling.row_start; t->by < ts->tiling.row_end;
                 t->by += f->sb_step)
            {
                const int error = dav1d_decode_tile_sbrow(t);
                const int progress = error ? TILE_ERROR : 1 + (t->by >> f->sb_shift);

                // signal progress
                pthread_mutex_lock(&ts->tile_thread.lock);
                atomic_store(&ts->progress, progress);
                pthread_cond_signal(&ts->tile_thread.cond);
                pthread_mutex_unlock(&ts->tile_thread.lock);
                if (error) break;
            }
        } else {
            const int sby = f->tile_thread.task_idx_to_sby_and_tile_idx[task_idx][0];
            const int tile_idx = f->tile_thread.task_idx_to_sby_and_tile_idx[task_idx][1];
            Dav1dTileState *const ts = &f->ts[tile_idx];
            int progress;

            // the interleaved decoding can sometimes cause dependency issues
            // if one part of the frame decodes signifcantly faster than others.
            // Ideally, we'd "skip" tile_sbrows where dependencies are missing,
            // and resume them later as dependencies are met. This also would
            // solve the broadcast() below and allow us to use signal(). However,
            // for now, we use linear dependency tracking because it's simpler.
            if ((progress = atomic_load(&ts->progress)) < sby) {
                pthread_mutex_lock(&ts->tile_thread.lock);
                while ((progress = atomic_load(&ts->progress)) < sby)
                    pthread_cond_wait(&ts->tile_thread.cond,
                                      &ts->tile_thread.lock);
                pthread_mutex_unlock(&ts->tile_thread.lock);
            }
            if (progress == TILE_ERROR) continue;

            // we need to interleave sbrow decoding for all tile cols in a
            // tile row, since otherwise subsequent threads will be blocked
            // waiting for the post-filter to complete
            t->ts = ts;
            t->by = sby << f->sb_shift;
            const int error = dav1d_decode_tile_sbrow(t);
            progress = error ? TILE_ERROR : 1 + sby;

            // signal progress
            pthread_mutex_lock(&ts->tile_thread.lock);
            atomic_store(&ts->progress, progress);
            pthread_cond_broadcast(&ts->tile_thread.cond);
            pthread_mutex_unlock(&ts->tile_thread.lock);
        }
    }

    return NULL;
}

static inline int handle_abortion(Dav1dPostFilterContext *const pf,
                                  Dav1dContext *const c,
                                  struct PostFilterThreadData *const pftd)
{
    const int flush = atomic_load_explicit(c->flush, memory_order_acquire);
    if (flush) {
        pthread_mutex_lock(&pf->td.lock);
        pf->flushed = 0;
        pthread_mutex_unlock(&pf->td.lock);
    }
    for (unsigned i = 0; i < c->n_fc; i++) {
        Dav1dFrameContext *const f = &c->fc[i];
        int send_signal;
        if (flush) // TODO before merge, see if this can be safely merged
            send_signal = f->lf.thread.done != 1 && f->lf.thread.num_tasks != 0;
        else
            send_signal = f->lf.thread.done == -1;
        for (int j = 0; send_signal && j < f->lf.thread.num_tasks; j++) {
            Dav1dTask *const t = &f->lf.thread.tasks[j];
            if (t->status == DAV1D_TASK_RUNNING ||
                (t->status == DAV1D_TASK_DONE && t->start != -1))
                send_signal = 0;
        }
        if (send_signal) {
            if (!flush) {
                Dav1dTask **pt = &pftd->tasks;
                while (*pt) {
                    if ((*pt)->frame_idx == i)
                        *pt = (*pt)->next_exec;
                    else
                        pt = &(*pt)->next_exec;
                }
            }
            f->lf.thread.done = 1;
            pthread_cond_signal(&f->lf.thread.cond);
        }
    }
    if (flush) {
        pthread_mutex_lock(&pf->td.lock);
        pf->flushed = 1;
        pthread_cond_signal(&pf->td.cond);
        pthread_mutex_unlock(&pf->td.lock);
    }
    return !flush;
}

void *dav1d_postfilter_task(void *data) {
    Dav1dPostFilterContext *const pf = data;
    Dav1dContext *const c = pf->c;
    struct PostFilterThreadData *pftd = &c->postfilter_thread;

    dav1d_set_thread_name("dav1d-postfilter");

    int exec = 1;
    pthread_mutex_lock(&pftd->lock);
    for (;;) {
        if (!exec && !pf->die)
            pthread_cond_wait(&pftd->cond, &pftd->lock);
        if (!(exec = handle_abortion(pf, c, pftd))) continue;
        if (pf->die) break;

        Dav1dTask *const t = pftd->tasks;
        if (!t) { exec = 0; continue; }
        pftd->tasks = t->next_exec;
        t->status = DAV1D_TASK_RUNNING;

        pthread_mutex_unlock(&pftd->lock);
        Dav1dFrameContext *const f = &c->fc[t->frame_idx];
        t->fn(f, t->sby);
        exec = 1;
        pthread_mutex_lock(&pftd->lock);

        if (t->next_deps[0])
            update_task(t->next_deps[0], 0, f);
        if (t->next_deps[1])
            update_task(t->next_deps[1], 1, f);
        t->status = DAV1D_TASK_DONE;
        if (!t->next_deps[0]) {
            const enum PlaneType progress_plane_type =
                c->n_fc > 1 && f->frame_hdr->refresh_context ?
                PLANE_TYPE_Y : PLANE_TYPE_ALL;
            const int y = (t->sby + 1) * f->sb_step * 4;
            dav1d_thread_picture_signal(&f->sr_cur, y, progress_plane_type);
            if (t->sby + 1 == f->sbh) {
                f->lf.thread.done = 1;
                pthread_cond_signal(&f->lf.thread.cond);
            }
        }
        t->start = -1;
    }
    pthread_mutex_unlock(&pftd->lock);

    return NULL;
}
