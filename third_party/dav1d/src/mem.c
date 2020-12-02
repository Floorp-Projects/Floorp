/*
 * Copyright © 2020, VideoLAN and dav1d authors
 * Copyright © 2020, Two Orioles, LLC
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

#include <stdint.h>

#include "src/mem.h"
#include "src/thread.h"

void dav1d_mem_pool_push(Dav1dMemPool *const pool, Dav1dMemPoolBuffer *const buf) {
    pthread_mutex_lock(&pool->lock);
    buf->next = pool->buf;
    pool->buf = buf;
    pthread_mutex_unlock(&pool->lock);
}

Dav1dMemPoolBuffer *dav1d_mem_pool_pop(Dav1dMemPool *const pool, const size_t size) {
    pthread_mutex_lock(&pool->lock);
    Dav1dMemPoolBuffer *buf = pool->buf;
    uint8_t *data;
    if (buf) {
        pool->buf = buf->next;
        pthread_mutex_unlock(&pool->lock);
        data = buf->data;
        if ((uintptr_t)buf - (uintptr_t)data != size) {
            dav1d_free_aligned(data);
            goto alloc;
        }
    } else {
        pthread_mutex_unlock(&pool->lock);
alloc:
        data = dav1d_alloc_aligned(size + sizeof(Dav1dMemPoolBuffer), 64);
        if (!data) return NULL;
        buf = (Dav1dMemPoolBuffer*)(data + size);
        buf->data = data;
    }

    return buf;
}

COLD void dav1d_mem_pool_destroy(Dav1dMemPool *const pool) {
    pthread_mutex_destroy(&pool->lock);
    Dav1dMemPoolBuffer *buf = pool->buf;
    while (buf) {
        void *const data = buf->data;
        buf = buf->next;
        dav1d_free_aligned(data);
    }
}
