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

/* grab size_t */
#ifdef _MSC_VER
#include <crtdefs.h>
#else
#include <stddef.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char jemalloc_bool;

extern const char	*_malloc_options;

/*
 * jemalloc_stats() is not a stable interface.  When using jemalloc_stats_t, be
 * sure that the compiled results of jemalloc.c are in sync with this header
 * file.
 */
typedef struct {
	/*
	 * Run-time configuration settings.
	 */
	jemalloc_bool	opt_abort;	/* abort(3) on error? */
	jemalloc_bool	opt_junk;	/* Fill allocated/free memory with 0xa5/0x5a? */
	jemalloc_bool	opt_utrace;	/* Trace all allocation events? */
	jemalloc_bool	opt_sysv;	/* SysV semantics? */
	jemalloc_bool	opt_xmalloc;	/* abort(3) on OOM? */
	jemalloc_bool	opt_zero;	/* Fill allocated memory with 0x0? */
	size_t	narenas;	/* Number of arenas. */
	size_t	balance_threshold; /* Arena contention rebalance threshold. */
	size_t	quantum;	/* Allocation quantum. */
	size_t	small_max;	/* Max quantum-spaced allocation size. */
	size_t	large_max;	/* Max sub-chunksize allocation size. */
	size_t	chunksize;	/* Size of each virtual memory mapping. */
	size_t	dirty_max;	/* Max dirty pages per arena. */
	size_t	reserve_min;	/* reserve_low callback threshold. */
	size_t	reserve_max;	/* Maximum reserve size before unmapping. */

	/*
	 * Current memory usage statistics.
	 */
	size_t	mapped;		/* Bytes mapped (not necessarily committed). */
	size_t	committed;	/* Bytes committed (readable/writable). */
	size_t	allocated;	/* Bytes allocted (in use by application). */
	size_t	dirty;		/* Bytes dirty (committed unused pages). */
	size_t	reserve_cur;	/* Current memory reserve. */
} jemalloc_stats_t;

#ifndef MOZ_MEMORY_DARWIN
void	*malloc(size_t size);
void	*valloc(size_t size);
void	*calloc(size_t num, size_t size);
void	*realloc(void *ptr, size_t size);
void	free(void *ptr);
#endif

int	posix_memalign(void **memptr, size_t alignment, size_t size);
void	*memalign(size_t alignment, size_t size);
size_t	malloc_usable_size(const void *ptr);
void	jemalloc_stats(jemalloc_stats_t *stats);

/* The x*() functions never return NULL. */
void	*xmalloc(size_t size);
void	*xcalloc(size_t num, size_t size);
void	*xrealloc(void *ptr, size_t size);
void	*xmemalign(size_t alignment, size_t size);

/*
 * The allocator maintains a memory reserve that is used to satisfy allocation
 * requests when no additional memory can be acquired from the operating
 * system.  Under normal operating conditions, the reserve size is at least
 * reserve_min bytes.  If the reserve is depleted or insufficient to satisfy an
 * allocation request, then condition notifications are sent to one or more of
 * the registered callback functions:
 *
 *   RESERVE_CND_LOW: The reserve had to be used to satisfy an allocation
 *                    request, which dropped the reserve size below the
 *                    minimum.  The callee should try to free memory in order
 *                    to restore the reserve.
 *
 *   RESERVE_CND_CRIT: The reserve was not large enough to satisfy a pending
 *                     allocation request.  Some callee must free adequate
 *                     memory in order to prevent application failure (unless
 *                     the condition spontaneously desists due to concurrent
 *                     deallocation).
 *
 *   RESERVE_CND_FAIL: An allocation request could not be satisfied, despite all
 *                     attempts.  The allocator is about to terminate the
 *                     application.
 *
 * The order in which the callback functions are called is only loosely
 * specified: in the absence of interposing callback
 * registrations/unregistrations, enabled callbacks will be called in an
 * arbitrary round-robin order.
 *
 * Condition notifications are sent to callbacks only while conditions exist.
 * For example, just before the allocator sends a RESERVE_CND_LOW condition
 * notification to a callback, the reserve is in fact depleted.  However, due
 * to allocator concurrency, the reserve may have been restored by the time the
 * callback function executes.  Furthermore, if the reserve is restored at some
 * point during the delivery of condition notifications to callbacks, no
 * further deliveries will occur, since the condition no longer exists.
 *
 * Callback functions can freely call back into the allocator (i.e. the
 * allocator releases all internal resources before calling each callback
 * function), though allocation is discouraged, since recursive callbacks are
 * likely to result, which places extra burden on the application to avoid
 * deadlock.
 *
 * Callback functions must be thread-safe, since it is possible that multiple
 * threads will call into the same callback function concurrently.
 */

/* Memory reserve condition types. */
typedef enum {
	RESERVE_CND_LOW,
	RESERVE_CND_CRIT,
	RESERVE_CND_FAIL
} reserve_cnd_t;

/*
 * Reserve condition notification callback function type definition.
 *
 * Inputs:
 *   ctx: Opaque application data, as passed to reserve_cb_register().
 *   cnd: Condition type being delivered.
 *   size: Allocation request size for the allocation that caused the condition.
 */
typedef void reserve_cb_t(void *ctx, reserve_cnd_t cnd, size_t size);

/*
 * Register a callback function.
 *
 * Inputs:
 *   cb: Callback function pointer.
 *   ctx: Opaque application data, passed to cb().
 *
 * Output:
 *   ret: If true, failure due to OOM; success otherwise.
 */
jemalloc_bool	reserve_cb_register(reserve_cb_t *cb, void *ctx);

/*
 * Unregister a callback function.
 *
 * Inputs:
 *   cb: Callback function pointer.
 *   ctx: Opaque application data, same as that passed to reserve_cb_register().
 *
 * Output:
 *   ret: False upon success, true if the {cb,ctx} registration could not be
 *        found.
 */
jemalloc_bool	reserve_cb_unregister(reserve_cb_t *cb, void *ctx);

/*
 * Get the current reserve size.
 *
 * ret: Current reserve size.
 */
size_t	reserve_cur_get(void);

/*
 * Get the minimum acceptable reserve size.  If the reserve drops below this
 * value, the RESERVE_CND_LOW condition notification is sent to the callbacks.
 *
 * ret: Minimum acceptable reserve size.
 */
size_t	reserve_min_get(void);

/*
 * Set the minimum acceptable reserve size.
 *
 * min: Reserve threshold.  This value may be internally rounded up.
 * ret: False if the reserve was successfully resized; true otherwise.  Note
 *      that failure to resize the reserve also results in a RESERVE_CND_LOW
 *      condition.
 */
jemalloc_bool	reserve_min_set(size_t min);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _JEMALLOC_H_ */
