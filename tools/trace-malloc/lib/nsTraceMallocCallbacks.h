/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* declarations needed by both nsTraceMalloc.c and nsWinTraceMalloc.cpp */

#ifndef NSTRACEMALLOCCALLBACKS_H
#define NSTRACEMALLOCCALLBACKS_H

#include "mozilla/StandardInteger.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Used by backtrace. */
typedef struct stack_buffer_info {
    void **buffer;
    size_t size;
    size_t entries;
} stack_buffer_info;

typedef struct tm_thread tm_thread;
struct tm_thread {
    /*
     * This counter suppresses tracing, in case any tracing code needs
     * to malloc.
     */
    uint32_t suppress_tracing;

    /* buffer for backtrace, below */
    stack_buffer_info backtrace_buf;
};

/* implemented in nsTraceMalloc.c */
tm_thread * tm_get_thread(void);

/* implemented in nsTraceMalloc.c */
PR_EXTERN(void) MallocCallback(void *aPtr, size_t aSize, uint32_t start, uint32_t end, tm_thread *t);
PR_EXTERN(void) CallocCallback(void *aPtr, size_t aCount, size_t aSize, uint32_t start, uint32_t end, tm_thread *t);
PR_EXTERN(void) ReallocCallback(void *aPin, void* aPout, size_t aSize, uint32_t start, uint32_t end, tm_thread *t);
PR_EXTERN(void) FreeCallback(void *aPtr, uint32_t start, uint32_t end, tm_thread *t);

#ifdef XP_WIN32
/* implemented in nsTraceMalloc.c */
PR_EXTERN(void) StartupHooker();
PR_EXTERN(void) ShutdownHooker();

/* implemented in nsWinTraceMalloc.cpp */
void* dhw_orig_malloc(size_t);
void* dhw_orig_calloc(size_t, size_t);
void* dhw_orig_realloc(void*, size_t);
void dhw_orig_free(void*);

#endif /* defined(XP_WIN32) */

#ifdef __cplusplus
}
#endif

#endif /* !defined(NSTRACEMALLOCCALLBACKS_H) */
