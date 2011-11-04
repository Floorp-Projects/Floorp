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

#include "jemalloc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const char *_malloc_options;

/* Darwin and Linux already have memory allocation functions */
#if (!defined(MOZ_MEMORY_DARWIN) && !defined(MOZ_MEMORY_LINUX))
void	*malloc(size_t size);
void	*valloc(size_t size);
void	*calloc(size_t num, size_t size);
void	*realloc(void *ptr, size_t size);
void	free(void *ptr);
int	posix_memalign(void **memptr, size_t alignment, size_t size);
#endif /* MOZ_MEMORY_DARWIN, MOZ_MEMORY_LINUX */

/* Android doesn't have posix_memalign */
#ifdef MOZ_MEMORY_ANDROID
int	posix_memalign(void **memptr, size_t alignment, size_t size);
/* Android < 2.3 doesn't have pthread_atfork, so we need to call these
 * when forking the child process. See bug 680190 */
void    _malloc_prefork(void);
void    _malloc_postfork(void);
#endif

#if defined(MOZ_MEMORY_DARWIN) || defined(MOZ_MEMORY_WINDOWS)
void	*je_malloc(size_t size);
void	*je_valloc(size_t size);
void	*je_calloc(size_t num, size_t size);
void	*je_realloc(void *ptr, size_t size);
void	je_free(void *ptr);
void *je_memalign(size_t alignment, size_t size);
int	je_posix_memalign(void **memptr, size_t alignment, size_t size);
char    *je_strndup(const char *src, size_t len);
char    *je_strdup(const char *src);
size_t	je_malloc_usable_size(const void *ptr);
#endif

/* Linux has memalign and malloc_usable_size */
#if !defined(MOZ_MEMORY_LINUX)
void	*memalign(size_t alignment, size_t size);
size_t	malloc_usable_size(const void *ptr);
#endif /* MOZ_MEMORY_LINUX */

void	jemalloc_stats(jemalloc_stats_t *stats);

/* Computes the usable size in advance. */
size_t	je_malloc_usable_size_in_advance(size_t size);

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
void    jemalloc_purge_freed_pages();

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _JEMALLOC_H_ */
