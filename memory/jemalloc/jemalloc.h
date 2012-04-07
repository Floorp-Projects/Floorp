/* -*- Mode: C; tab-width: 8; c-basic-offset: 8 -*- */
/* vim:set softtabstop=8 shiftwidth=8: */
/*-
 * Copyright (C) 2006-2008 Jason Evans <jasone@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _JEMALLOC_H_
#define _JEMALLOC_H_

#if defined(MOZ_MEMORY_DARWIN)
#include <malloc/malloc.h>
#endif
#include "jemalloc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MOZ_MEMORY_LINUX)
__attribute__((weak))
#endif
void	jemalloc_stats(jemalloc_stats_t *stats);

/* Computes the usable size in advance. */
#if !defined(MOZ_MEMORY_DARWIN)
#if defined(MOZ_MEMORY_LINUX)
__attribute__((weak))
#endif
size_t je_malloc_good_size(size_t size);
#endif

static inline size_t je_malloc_usable_size_in_advance(size_t size) {
#if defined(MOZ_MEMORY_DARWIN)
  return malloc_good_size(size);
#else
  if (je_malloc_good_size)
    return je_malloc_good_size(size);
  else
    return size;
#endif
}

/*
 * On some operating systems (Mac), we use madvise(MADV_FREE) to hand pages
 * back to the operating system.  On Mac, the operating system doesn't take
 * this memory back immediately; instead, the OS takes it back only when the
 * machine is running out of physical memory.
 *
 * This is great from the standpoint of efficiency, but it makes measuring our
 * actual RSS difficult, because pages which we've MADV_FREE'd shouldn't count
 * against our RSS.
 *
 * This function explicitly purges any MADV_FREE'd pages from physical memory,
 * causing our reported RSS match the amount of memory we're actually using.
 *
 * Note that this call is expensive in two ways.  First, it may be slow to
 * execute, because it may make a number of slow syscalls to free memory.  This
 * function holds the big jemalloc locks, so basically all threads are blocked
 * while this function runs.
 *
 * This function is also expensive in that the next time we go to access a page
 * which we've just explicitly decommitted, the operating system has to attach
 * to it a physical page!  If we hadn't run this function, the OS would have
 * less work to do.
 *
 * If MALLOC_DOUBLE_PURGE is not defined, this function does nothing.
 */
#if defined(MOZ_MEMORY_LINUX)
static inline void jemalloc_purge_freed_pages() { }
#else
void    jemalloc_purge_freed_pages();
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _JEMALLOC_H_ */
