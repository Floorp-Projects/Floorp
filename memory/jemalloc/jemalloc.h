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

#if defined(MOZ_MEMORY_DARWIN) || defined(MOZ_MEMORY_ANDROID) || \
    defined(WRAP_MALLOC) || defined(MOZ_MEMORY_WINDOWS)
void	*je_malloc(size_t size);
void	*je_valloc(size_t size);
void	*je_calloc(size_t num, size_t size);
void	*je_realloc(void *ptr, size_t size);
void	je_free(void *ptr);
void *je_memalign(size_t alignment, size_t size);
int	je_posix_memalign(void **memptr, size_t alignment, size_t size);
char    *je_strndup(const char *src, size_t len);
char    *je_strdup(const char *src);
#if defined(MOZ_MEMORY_ANDROID)
size_t  je_malloc_usable_size(void *ptr);
#else
size_t	je_malloc_usable_size(const void *ptr);
#endif
#endif

/* Linux has memalign and malloc_usable_size */
#if !defined(MOZ_MEMORY_LINUX)
void	*memalign(size_t alignment, size_t size);
size_t	malloc_usable_size(const void *ptr);
#endif /* MOZ_MEMORY_LINUX */

void	jemalloc_stats(jemalloc_stats_t *stats);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _JEMALLOC_H_ */
