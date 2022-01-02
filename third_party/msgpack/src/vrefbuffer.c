/*
 * MessagePack for C zero-copy buffer implementation
 *
 * Copyright (C) 2008-2009 FURUHASHI Sadayuki
 *
 *    Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *    http://www.boost.org/LICENSE_1_0.txt)
 */
#include "msgpack/vrefbuffer.h"
#include <stdlib.h>
#include <string.h>

#define MSGPACK_PACKER_MAX_BUFFER_SIZE 9

struct msgpack_vrefbuffer_chunk {
    struct msgpack_vrefbuffer_chunk* next;
    /* data ... */
};

bool msgpack_vrefbuffer_init(msgpack_vrefbuffer* vbuf,
        size_t ref_size, size_t chunk_size)
{
    size_t nfirst;
    struct iovec* array;
    msgpack_vrefbuffer_chunk* chunk;

    vbuf->chunk_size = chunk_size;
    vbuf->ref_size =
        ref_size > MSGPACK_PACKER_MAX_BUFFER_SIZE + 1 ?
        ref_size : MSGPACK_PACKER_MAX_BUFFER_SIZE + 1 ;

    nfirst = (sizeof(struct iovec) < 72/2) ?
            72 / sizeof(struct iovec) : 8;

    array = (struct iovec*)malloc(
            sizeof(struct iovec) * nfirst);
    if(array == NULL) {
        return false;
    }

    vbuf->tail  = array;
    vbuf->end   = array + nfirst;
    vbuf->array = array;

    chunk = (msgpack_vrefbuffer_chunk*)malloc(
            sizeof(msgpack_vrefbuffer_chunk) + chunk_size);
    if(chunk == NULL) {
        free(array);
        return false;
    }
    else {
        msgpack_vrefbuffer_inner_buffer* const ib = &vbuf->inner_buffer;

        ib->free = chunk_size;
        ib->ptr  = ((char*)chunk) + sizeof(msgpack_vrefbuffer_chunk);
        ib->head = chunk;
        chunk->next = NULL;

        return true;
    }
}

void msgpack_vrefbuffer_destroy(msgpack_vrefbuffer* vbuf)
{
    msgpack_vrefbuffer_chunk* c = vbuf->inner_buffer.head;
    while(true) {
        msgpack_vrefbuffer_chunk* n = c->next;
        free(c);
        if(n != NULL) {
            c = n;
        } else {
            break;
        }
    }
    free(vbuf->array);
}

void msgpack_vrefbuffer_clear(msgpack_vrefbuffer* vbuf)
{
    msgpack_vrefbuffer_chunk* c = vbuf->inner_buffer.head->next;
    msgpack_vrefbuffer_chunk* n;
    while(c != NULL) {
        n = c->next;
        free(c);
        c = n;
    }

    {
        msgpack_vrefbuffer_inner_buffer* const ib = &vbuf->inner_buffer;
        msgpack_vrefbuffer_chunk* chunk = ib->head;
        chunk->next = NULL;
        ib->free = vbuf->chunk_size;
        ib->ptr  = ((char*)chunk) + sizeof(msgpack_vrefbuffer_chunk);

        vbuf->tail = vbuf->array;
    }
}

int msgpack_vrefbuffer_append_ref(msgpack_vrefbuffer* vbuf,
        const char* buf, size_t len)
{
    if(vbuf->tail == vbuf->end) {
        const size_t nused = (size_t)(vbuf->tail - vbuf->array);
        const size_t nnext = nused * 2;

        struct iovec* nvec = (struct iovec*)realloc(
                vbuf->array, sizeof(struct iovec)*nnext);
        if(nvec == NULL) {
            return -1;
        }

        vbuf->array = nvec;
        vbuf->end   = nvec + nnext;
        vbuf->tail  = nvec + nused;
    }

    vbuf->tail->iov_base = (char*)buf;
    vbuf->tail->iov_len  = len;
    ++vbuf->tail;

    return 0;
}

int msgpack_vrefbuffer_append_copy(msgpack_vrefbuffer* vbuf,
        const char* buf, size_t len)
{
    msgpack_vrefbuffer_inner_buffer* const ib = &vbuf->inner_buffer;
    char* m;

    if(ib->free < len) {
        msgpack_vrefbuffer_chunk* chunk;
        size_t sz = vbuf->chunk_size;
        if(sz < len) {
            sz = len;
        }

        chunk = (msgpack_vrefbuffer_chunk*)malloc(
                sizeof(msgpack_vrefbuffer_chunk) + sz);
        if(chunk == NULL) {
            return -1;
        }

        chunk->next = ib->head;
        ib->head = chunk;
        ib->free = sz;
        ib->ptr  = ((char*)chunk) + sizeof(msgpack_vrefbuffer_chunk);
    }

    m = ib->ptr;
    memcpy(m, buf, len);
    ib->free -= len;
    ib->ptr  += len;

    if(vbuf->tail != vbuf->array && m ==
            (const char*)((vbuf->tail-1)->iov_base) + (vbuf->tail-1)->iov_len) {
        (vbuf->tail-1)->iov_len += len;
        return 0;
    } else {
        return msgpack_vrefbuffer_append_ref(vbuf, m, len);
    }
}

int msgpack_vrefbuffer_migrate(msgpack_vrefbuffer* vbuf, msgpack_vrefbuffer* to)
{
    size_t sz = vbuf->chunk_size;

    msgpack_vrefbuffer_chunk* empty = (msgpack_vrefbuffer_chunk*)malloc(
            sizeof(msgpack_vrefbuffer_chunk) + sz);
    if(empty == NULL) {
        return -1;
    }

    empty->next = NULL;

    {
        const size_t nused = (size_t)(vbuf->tail - vbuf->array);
        if(to->tail + nused < vbuf->end) {
            struct iovec* nvec;
            const size_t tosize = (size_t)(to->tail - to->array);
            const size_t reqsize = nused + tosize;
            size_t nnext = (size_t)(to->end - to->array) * 2;
            while(nnext < reqsize) {
                size_t tmp_nnext = nnext * 2;
                if (tmp_nnext <= nnext) {
                    nnext = reqsize;
                    break;
                }
                nnext = tmp_nnext;
            }

            nvec = (struct iovec*)realloc(
                    to->array, sizeof(struct iovec)*nnext);
            if(nvec == NULL) {
                free(empty);
                return -1;
            }

            to->array = nvec;
            to->end   = nvec + nnext;
            to->tail  = nvec + tosize;
        }

        memcpy(to->tail, vbuf->array, sizeof(struct iovec)*nused);

        to->tail += nused;
        vbuf->tail = vbuf->array;

        {
            msgpack_vrefbuffer_inner_buffer* const ib = &vbuf->inner_buffer;
            msgpack_vrefbuffer_inner_buffer* const toib = &to->inner_buffer;

            msgpack_vrefbuffer_chunk* last = ib->head;
            while(last->next != NULL) {
                last = last->next;
            }
            last->next = toib->head;
            toib->head = ib->head;

            if(toib->free < ib->free) {
                toib->free = ib->free;
                toib->ptr  = ib->ptr;
            }

            ib->head = empty;
            ib->free = sz;
            ib->ptr  = ((char*)empty) + sizeof(msgpack_vrefbuffer_chunk);
        }
    }

    return 0;
}
