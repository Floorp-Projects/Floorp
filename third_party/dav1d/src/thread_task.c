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
