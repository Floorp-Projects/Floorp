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
 * @file sbthread.h 
 * @brief Simple portable thread functions.
 * @author David Huggins-Daines <dhuggins@cs.cmu.edu>
 **/

#ifndef __SBTHREAD_H__
#define __SBTHREAD_H__

#include <sphinx_config.h>

#include <sphinxbase/sphinxbase_export.h>
#include <sphinxbase/cmd_ln.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

/**
 * Thread object.
 */
typedef struct sbthread_s sbthread_t;

/**
 * Asynchronous message queue object.
 */
typedef struct sbmsgq_s sbmsgq_t;

/**
 * Mutex (critical section) object.
 */
typedef struct sbmtx_s sbmtx_t;

/**
 * Event object.
 */
typedef struct sbevent_s sbevent_t;

/**
 * Entry point for a thread.
 */
typedef int (*sbthread_main)(sbthread_t *th);

/**
 * Start a new thread.
 */
SPHINXBASE_EXPORT
sbthread_t *sbthread_start(cmd_ln_t *config, sbthread_main func, void *arg);

/**
 * Wait for a thread to complete.
 */
SPHINXBASE_EXPORT
int sbthread_wait(sbthread_t *th);

/**
 * Free a thread object.
 */
SPHINXBASE_EXPORT
void sbthread_free(sbthread_t *th);

/**
 * Get configuration object from a thread.
 */
SPHINXBASE_EXPORT
cmd_ln_t *sbthread_config(sbthread_t *th);

/**
 * Get argument pointer from a thread.
 */
SPHINXBASE_EXPORT
void *sbthread_arg(sbthread_t *th);

/**
 * Get message queue from a thread.
 */
SPHINXBASE_EXPORT
sbmsgq_t *sbthread_msgq(sbthread_t *th);

/**
 * Wait for a thread to complete.
 */
SPHINXBASE_EXPORT
int sbthread_wait(sbthread_t *th);

/**
 * Send an asynchronous message to a thread.
 *
 * Each thread gets a message queue by default, so this is just a
 * wrapper around sbmsgq_send().
 */
SPHINXBASE_EXPORT
int sbthread_send(sbthread_t *th, size_t len, void const *data);

/**
 * Create a message queue.
 *
 * @param depth Depth of the queue.
 */
SPHINXBASE_EXPORT
sbmsgq_t *sbmsgq_init(size_t depth);

/**
 * Free a message queue.
 */
SPHINXBASE_EXPORT
void sbmsgq_free(sbmsgq_t *q);

/**
 * Post a message to a queue.
 */
SPHINXBASE_EXPORT
int sbmsgq_send(sbmsgq_t *q, size_t len, void const *data);

/**
 * Wait for a message from a queue.
 */
SPHINXBASE_EXPORT
void *sbmsgq_wait(sbmsgq_t *q, size_t *out_len, int sec, int nsec);

/**
 * Create a mutex.
 */
SPHINXBASE_EXPORT
sbmtx_t *sbmtx_init(void);

/**
 * Try to acquire a mutex.
 */
SPHINXBASE_EXPORT
int sbmtx_trylock(sbmtx_t *mtx);

/**
 * Acquire a mutex.
 */
SPHINXBASE_EXPORT
int sbmtx_lock(sbmtx_t *mtx);

/**
 * Release a mutex.
 */
SPHINXBASE_EXPORT
int sbmtx_unlock(sbmtx_t *mtx);

/**
 * Dispose of a mutex.
 */
SPHINXBASE_EXPORT
void sbmtx_free(sbmtx_t *mtx);

/**
 * Initialize an event.
 */
SPHINXBASE_EXPORT
sbevent_t *sbevent_init(void);

/**
 * Free an event.
 */
SPHINXBASE_EXPORT
void sbevent_free(sbevent_t *evt);

/**
 * Signal an event.
 */
SPHINXBASE_EXPORT
int sbevent_signal(sbevent_t *evt);

/**
 * Wait for an event to be signalled.
 */
SPHINXBASE_EXPORT
int sbevent_wait(sbevent_t *evt, int sec, int nsec);


#ifdef __cplusplus
}
#endif


#endif /* __SBTHREAD_H__ */
