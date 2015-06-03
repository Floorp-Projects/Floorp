/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2008 Carnegie Mellon University.  All rights 
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */

/**
 * @file sbthread.c
 * @brief Simple portable thread functions
 * @author David Huggins-Daines <dhuggins@cs.cmu.edu>
 */

#include <string.h>

#include "sphinxbase/sbthread.h"
#include "sphinxbase/ckd_alloc.h"
#include "sphinxbase/err.h"

/*
 * Platform-specific parts: threads, mutexes, and signals.
 */
#if (defined(_WIN32) || defined(__CYGWIN__)) && !defined(__SYMBIAN32__)
#define _WIN32_WINNT 0x0400
#include <windows.h>

struct sbthread_s {
    cmd_ln_t *config;
    sbmsgq_t *msgq;
    sbthread_main func;
    void *arg;
    HANDLE th;
    DWORD tid;
};

struct sbmsgq_s {
    /* Ringbuffer for passing messages. */
    char *data;
    size_t depth;
    size_t out;
    size_t nbytes;

    /* Current message is stored here. */
    char *msg;
    size_t msglen;
    CRITICAL_SECTION mtx;
    HANDLE evt;
};

struct sbevent_s {
    HANDLE evt;
};

struct sbmtx_s {
    CRITICAL_SECTION mtx;
};

DWORD WINAPI
sbthread_internal_main(LPVOID arg)
{
    sbthread_t *th = (sbthread_t *)arg;
    int rv;

    rv = (*th->func)(th);
    return (DWORD)rv;
}

sbthread_t *
sbthread_start(cmd_ln_t *config, sbthread_main func, void *arg)
{
    sbthread_t *th;

    th = ckd_calloc(1, sizeof(*th));
    th->config = config;
    th->func = func;
    th->arg = arg;
    th->msgq = sbmsgq_init(256);
    th->th = CreateThread(NULL, 0, sbthread_internal_main, th, 0, &th->tid);
    if (th->th == NULL) {
        sbthread_free(th);
        return NULL;
    }
    return th;
}

int
sbthread_wait(sbthread_t *th)
{
    DWORD rv, exit;

    /* It has already been joined. */
    if (th->th == NULL)
        return -1;

    rv = WaitForSingleObject(th->th, INFINITE);
    if (rv == WAIT_FAILED) {
        E_ERROR("Failed to join thread: WAIT_FAILED\n");
        return -1;
    }
    GetExitCodeThread(th->th, &exit);
    CloseHandle(th->th);
    th->th = NULL;
    return (int)exit;
}

static DWORD
cond_timed_wait(HANDLE cond, int sec, int nsec)
{
    DWORD rv;
    if (sec == -1) {
        rv = WaitForSingleObject(cond, INFINITE);
    }
    else {
        DWORD ms;

        ms = sec * 1000 + nsec / (1000*1000);
        rv = WaitForSingleObject(cond, ms);
    }
    return rv;
}

/* Updated to use Unicode */
sbevent_t *
sbevent_init(void)
{
    sbevent_t *evt;

    evt = ckd_calloc(1, sizeof(*evt));
    evt->evt = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (evt->evt == NULL) {
        ckd_free(evt);
        return NULL;
    }
    return evt;
}

void
sbevent_free(sbevent_t *evt)
{
    CloseHandle(evt->evt);
    ckd_free(evt);
}

int
sbevent_signal(sbevent_t *evt)
{
    return SetEvent(evt->evt) ? 0 : -1;
}

int
sbevent_wait(sbevent_t *evt, int sec, int nsec)
{
    DWORD rv;

    rv = cond_timed_wait(evt->evt, sec, nsec);
    return rv;
}

sbmtx_t *
sbmtx_init(void)
{
    sbmtx_t *mtx;

    mtx = ckd_calloc(1, sizeof(*mtx));
    InitializeCriticalSection(&mtx->mtx);
    return mtx;
}

int
sbmtx_trylock(sbmtx_t *mtx)
{
    return TryEnterCriticalSection(&mtx->mtx) ? 0 : -1;
}

int
sbmtx_lock(sbmtx_t *mtx)
{
    EnterCriticalSection(&mtx->mtx);
    return 0;
}

int
sbmtx_unlock(sbmtx_t *mtx)
{
    LeaveCriticalSection(&mtx->mtx);
    return 0;
}

void
sbmtx_free(sbmtx_t *mtx)
{
    DeleteCriticalSection(&mtx->mtx);
    ckd_free(mtx);
}

sbmsgq_t *
sbmsgq_init(size_t depth)
{
    sbmsgq_t *msgq;

    msgq = ckd_calloc(1, sizeof(*msgq));
    msgq->depth = depth;
    msgq->evt = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (msgq->evt == NULL) {
        ckd_free(msgq);
        return NULL;
    }
    InitializeCriticalSection(&msgq->mtx);
    msgq->data = ckd_calloc(depth, 1);
    msgq->msg = ckd_calloc(depth, 1);
    return msgq;
}

void
sbmsgq_free(sbmsgq_t *msgq)
{
    CloseHandle(msgq->evt);
    ckd_free(msgq->data);
    ckd_free(msgq->msg);
    ckd_free(msgq);
}

int
sbmsgq_send(sbmsgq_t *q, size_t len, void const *data)
{
    char const *cdata = (char const *)data;
    size_t in;

    /* Don't allow things bigger than depth to be sent! */
    if (len + sizeof(len) > q->depth)
        return -1;

    if (q->nbytes + len + sizeof(len) > q->depth)
        WaitForSingleObject(q->evt, INFINITE);

    /* Lock things while we manipulate the buffer (FIXME: this
       actually should have been atomic with the wait above ...) */
    EnterCriticalSection(&q->mtx);
    in = (q->out + q->nbytes) % q->depth;
    /* First write the size of the message. */
    if (in + sizeof(len) > q->depth) {
        /* Handle the annoying case where the size field gets wrapped around. */
        size_t len1 = q->depth - in;
        memcpy(q->data + in, &len, len1);
        memcpy(q->data, ((char *)&len) + len1, sizeof(len) - len1);
        q->nbytes += sizeof(len);
        in = sizeof(len) - len1;
    }
    else {
        memcpy(q->data + in, &len, sizeof(len));
        q->nbytes += sizeof(len);
        in += sizeof(len);
    }

    /* Now write the message body. */
    if (in + len > q->depth) {
        /* Handle wraparound. */
        size_t len1 = q->depth - in;
        memcpy(q->data + in, cdata, len1);
        q->nbytes += len1;
        cdata += len1;
        len -= len1;
        in = 0;
    }
    memcpy(q->data + in, cdata, len);
    q->nbytes += len;

    /* Signal the condition variable. */
    SetEvent(q->evt);
    /* Unlock. */
    LeaveCriticalSection(&q->mtx);

    return 0;
}

void *
sbmsgq_wait(sbmsgq_t *q, size_t *out_len, int sec, int nsec)
{
    char *outptr;
    size_t len;

    /* Wait for data to be available. */
    if (q->nbytes == 0) {
        if (cond_timed_wait(q->evt, sec, nsec) == WAIT_FAILED)
            /* Timed out or something... */
            return NULL;
    }
    /* Lock to manipulate the queue (FIXME) */
    EnterCriticalSection(&q->mtx);
    /* Get the message size. */
    if (q->out + sizeof(q->msglen) > q->depth) {
        /* Handle annoying wraparound case. */
        size_t len1 = q->depth - q->out;
        memcpy(&q->msglen, q->data + q->out, len1);
        memcpy(((char *)&q->msglen) + len1, q->data,
               sizeof(q->msglen) - len1);
        q->out = sizeof(q->msglen) - len1;
    }
    else {
        memcpy(&q->msglen, q->data + q->out, sizeof(q->msglen));
        q->out += sizeof(q->msglen);
    }
    q->nbytes -= sizeof(q->msglen);
    /* Get the message body. */
    outptr = q->msg;
    len = q->msglen;
    if (q->out + q->msglen > q->depth) {
        /* Handle wraparound. */
        size_t len1 = q->depth - q->out;
        memcpy(outptr, q->data + q->out, len1);
        outptr += len1;
        len -= len1;
        q->nbytes -= len1;
        q->out = 0;
    }
    memcpy(outptr, q->data + q->out, len);
    q->nbytes -= len;
    q->out += len;

    /* Signal the condition variable. */
    SetEvent(q->evt);
    /* Unlock. */
    LeaveCriticalSection(&q->mtx);
    if (out_len)
        *out_len = q->msglen;
    return q->msg;
}

#else /* POSIX */
#include <pthread.h>
#include <sys/time.h>

struct sbthread_s {
    cmd_ln_t *config;
    sbmsgq_t *msgq;
    sbthread_main func;
    void *arg;
    pthread_t th;
};

struct sbmsgq_s {
    /* Ringbuffer for passing messages. */
    char *data;
    size_t depth;
    size_t out;
    size_t nbytes;

    /* Current message is stored here. */
    char *msg;
    size_t msglen;
    pthread_mutex_t mtx;
    pthread_cond_t cond;
};

struct sbevent_s {
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    int signalled;
};

struct sbmtx_s {
    pthread_mutex_t mtx;
};

static void *
sbthread_internal_main(void *arg)
{
    sbthread_t *th = (sbthread_t *)arg;
    int rv;

    rv = (*th->func)(th);
    return (void *)(long)rv;
}

sbthread_t *
sbthread_start(cmd_ln_t *config, sbthread_main func, void *arg)
{
    sbthread_t *th;
    int rv;

    th = ckd_calloc(1, sizeof(*th));
    th->config = config;
    th->func = func;
    th->arg = arg;
    th->msgq = sbmsgq_init(1024);
    if ((rv = pthread_create(&th->th, NULL, &sbthread_internal_main, th)) != 0) {
        E_ERROR("Failed to create thread: %d\n", rv);
        sbthread_free(th);
        return NULL;
    }
    return th;
}

int
sbthread_wait(sbthread_t *th)
{
    void *exit;
    int rv;

    /* It has already been joined. */
    if (th->th == (pthread_t)-1)
        return -1;

    rv = pthread_join(th->th, &exit);
    if (rv != 0) {
        E_ERROR("Failed to join thread: %d\n", rv);
        return -1;
    }
    th->th = (pthread_t)-1;
    return (int)(long)exit;
}

sbmsgq_t *
sbmsgq_init(size_t depth)
{
    sbmsgq_t *msgq;

    msgq = ckd_calloc(1, sizeof(*msgq));
    msgq->depth = depth;
    if (pthread_cond_init(&msgq->cond, NULL) != 0) {
        ckd_free(msgq);
        return NULL;
    }
    if (pthread_mutex_init(&msgq->mtx, NULL) != 0) {
        pthread_cond_destroy(&msgq->cond);
        ckd_free(msgq);
        return NULL;
    }
    msgq->data = ckd_calloc(depth, 1);
    msgq->msg = ckd_calloc(depth, 1);
    return msgq;
}

void
sbmsgq_free(sbmsgq_t *msgq)
{
    pthread_mutex_destroy(&msgq->mtx);
    pthread_cond_destroy(&msgq->cond);
    ckd_free(msgq->data);
    ckd_free(msgq->msg);
    ckd_free(msgq);
}

int
sbmsgq_send(sbmsgq_t *q, size_t len, void const *data)
{
    size_t in;

    /* Don't allow things bigger than depth to be sent! */
    if (len + sizeof(len) > q->depth)
        return -1;

    /* Lock the condition variable while we manipulate the buffer. */
    pthread_mutex_lock(&q->mtx);
    if (q->nbytes + len + sizeof(len) > q->depth) {
        /* Unlock and wait for space to be available. */
        if (pthread_cond_wait(&q->cond, &q->mtx) != 0) {
            /* Timed out, don't send anything. */
            pthread_mutex_unlock(&q->mtx);
            return -1;
        }
        /* Condition is now locked again. */
    }
    in = (q->out + q->nbytes) % q->depth;

    /* First write the size of the message. */
    if (in + sizeof(len) > q->depth) {
        /* Handle the annoying case where the size field gets wrapped around. */
        size_t len1 = q->depth - in;
        memcpy(q->data + in, &len, len1);
        memcpy(q->data, ((char *)&len) + len1, sizeof(len) - len1);
        q->nbytes += sizeof(len);
        in = sizeof(len) - len1;
    }
    else {
        memcpy(q->data + in, &len, sizeof(len));
        q->nbytes += sizeof(len);
        in += sizeof(len);
    }

    /* Now write the message body. */
    if (in + len > q->depth) {
        /* Handle wraparound. */
        size_t len1 = q->depth - in;
        memcpy(q->data + in, data, len1);
        q->nbytes += len1;
        data = (char const *)data + len1;
        len -= len1;
        in = 0;
    }
    memcpy(q->data + in, data, len);
    q->nbytes += len;

    /* Signal the condition variable. */
    pthread_cond_signal(&q->cond);
    /* Unlock it, we have nothing else to do. */
    pthread_mutex_unlock(&q->mtx);
    return 0;
}

static int
cond_timed_wait(pthread_cond_t *cond, pthread_mutex_t *mtx, int sec, int nsec)
{
    int rv;
    if (sec == -1) {
        rv = pthread_cond_wait(cond, mtx);
    }
    else {
        struct timeval now;
        struct timespec end;

        gettimeofday(&now, NULL);
        end.tv_sec = now.tv_sec + sec;
        end.tv_nsec = now.tv_usec * 1000 + nsec;
        if (end.tv_nsec > (1000*1000*1000)) {
            sec += end.tv_nsec / (1000*1000*1000);
            end.tv_nsec = end.tv_nsec % (1000*1000*1000);
        }
        rv = pthread_cond_timedwait(cond, mtx, &end);
    }
    return rv;
}

void *
sbmsgq_wait(sbmsgq_t *q, size_t *out_len, int sec, int nsec)
{
    char *outptr;
    size_t len;

    /* Lock the condition variable while we manipulate nmsg. */
    pthread_mutex_lock(&q->mtx);
    if (q->nbytes == 0) {
        /* Unlock the condition variable and wait for a signal. */
        if (cond_timed_wait(&q->cond, &q->mtx, sec, nsec) != 0) {
            /* Timed out or something... */
            pthread_mutex_unlock(&q->mtx);
            return NULL;
        }
        /* Condition variable is now locked again. */
    }
    /* Get the message size. */
    if (q->out + sizeof(q->msglen) > q->depth) {
        /* Handle annoying wraparound case. */
        size_t len1 = q->depth - q->out;
        memcpy(&q->msglen, q->data + q->out, len1);
        memcpy(((char *)&q->msglen) + len1, q->data,
               sizeof(q->msglen) - len1);
        q->out = sizeof(q->msglen) - len1;
    }
    else {
        memcpy(&q->msglen, q->data + q->out, sizeof(q->msglen));
        q->out += sizeof(q->msglen);
    }
    q->nbytes -= sizeof(q->msglen);
    /* Get the message body. */
    outptr = q->msg;
    len = q->msglen;
    if (q->out + q->msglen > q->depth) {
        /* Handle wraparound. */
        size_t len1 = q->depth - q->out;
        memcpy(outptr, q->data + q->out, len1);
        outptr += len1;
        len -= len1;
        q->nbytes -= len1;
        q->out = 0;
    }
    memcpy(outptr, q->data + q->out, len);
    q->nbytes -= len;
    q->out += len;

    /* Signal the condition variable. */
    pthread_cond_signal(&q->cond);
    /* Unlock the condition variable, we are done. */
    pthread_mutex_unlock(&q->mtx);
    if (out_len)
        *out_len = q->msglen;
    return q->msg;
}

sbevent_t *
sbevent_init(void)
{
    sbevent_t *evt;
    int rv;

    evt = ckd_calloc(1, sizeof(*evt));
    if ((rv = pthread_mutex_init(&evt->mtx, NULL)) != 0) {
        E_ERROR("Failed to initialize mutex: %d\n", rv);
        ckd_free(evt);
        return NULL;
    }
    if ((rv = pthread_cond_init(&evt->cond, NULL)) != 0) {
        E_ERROR_SYSTEM("Failed to initialize mutex: %d\n", rv);
        pthread_mutex_destroy(&evt->mtx);
        ckd_free(evt);
        return NULL;
    }
    return evt;
}

void
sbevent_free(sbevent_t *evt)
{
    pthread_mutex_destroy(&evt->mtx);
    pthread_cond_destroy(&evt->cond);
    ckd_free(evt);
}

int
sbevent_signal(sbevent_t *evt)
{
    int rv;

    pthread_mutex_lock(&evt->mtx);
    evt->signalled = TRUE;
    rv = pthread_cond_signal(&evt->cond);
    pthread_mutex_unlock(&evt->mtx);
    return rv;
}

int
sbevent_wait(sbevent_t *evt, int sec, int nsec)
{
    int rv = 0;

    /* Lock the mutex before we check its signalled state. */
    pthread_mutex_lock(&evt->mtx);
    /* If it's not signalled, then wait until it is. */
    if (!evt->signalled)
        rv = cond_timed_wait(&evt->cond, &evt->mtx, sec, nsec);
    /* Set its state to unsignalled if we were successful. */
    if (rv == 0)
        evt->signalled = FALSE;
    /* And unlock its mutex. */
    pthread_mutex_unlock(&evt->mtx);

    return rv;
}

sbmtx_t *
sbmtx_init(void)
{
    sbmtx_t *mtx;

    mtx = ckd_calloc(1, sizeof(*mtx));
    if (pthread_mutex_init(&mtx->mtx, NULL) != 0) {
        ckd_free(mtx);
        return NULL;
    }
    return mtx;
}

int
sbmtx_trylock(sbmtx_t *mtx)
{
    return pthread_mutex_trylock(&mtx->mtx);
}

int
sbmtx_lock(sbmtx_t *mtx)
{
    return pthread_mutex_lock(&mtx->mtx);
}

int
sbmtx_unlock(sbmtx_t *mtx)
{
    return pthread_mutex_unlock(&mtx->mtx);
}

void
sbmtx_free(sbmtx_t *mtx)
{
    pthread_mutex_destroy(&mtx->mtx);
    ckd_free(mtx);
}
#endif /* not WIN32 */

cmd_ln_t *
sbthread_config(sbthread_t *th)
{
    return th->config;
}

void *
sbthread_arg(sbthread_t *th)
{
    return th->arg;
}

sbmsgq_t *
sbthread_msgq(sbthread_t *th)
{
    return th->msgq;
}

int
sbthread_send(sbthread_t *th, size_t len, void const *data)
{
    return sbmsgq_send(th->msgq, len, data);
}

void
sbthread_free(sbthread_t *th)
{
    sbthread_wait(th);
    sbmsgq_free(th->msgq);
    ckd_free(th);
}
