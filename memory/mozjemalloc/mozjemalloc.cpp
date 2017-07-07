/* -*- Mode: C; tab-width: 8; c-basic-offset: 8; indent-tabs-mode: t -*- */
/* vim:set softtabstop=8 shiftwidth=8 noet: */
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
 *
 *******************************************************************************
 *
 * This allocator implementation is designed to provide scalable performance
 * for multi-threaded programs on multi-processor systems.  The following
 * features are included for this purpose:
 *
 *   + Multiple arenas are used if there are multiple CPUs, which reduces lock
 *     contention and cache sloshing.
 *
 *   + Cache line sharing between arenas is avoided for internal data
 *     structures.
 *
 *   + Memory is managed in chunks and runs (chunks can be split into runs),
 *     rather than as individual pages.  This provides a constant-time
 *     mechanism for associating allocations with particular arenas.
 *
 * Allocation requests are rounded up to the nearest size class, and no record
 * of the original request size is maintained.  Allocations are broken into
 * categories according to size class.  Assuming runtime defaults, 4 kB pages
 * and a 16 byte quantum on a 32-bit system, the size classes in each category
 * are as follows:
 *
 *   |=====================================|
 *   | Category | Subcategory    |    Size |
 *   |=====================================|
 *   | Small    | Tiny           |       2 |
 *   |          |                |       4 |
 *   |          |                |       8 |
 *   |          |----------------+---------|
 *   |          | Quantum-spaced |      16 |
 *   |          |                |      32 |
 *   |          |                |      48 |
 *   |          |                |     ... |
 *   |          |                |     480 |
 *   |          |                |     496 |
 *   |          |                |     512 |
 *   |          |----------------+---------|
 *   |          | Sub-page       |    1 kB |
 *   |          |                |    2 kB |
 *   |=====================================|
 *   | Large                     |    4 kB |
 *   |                           |    8 kB |
 *   |                           |   12 kB |
 *   |                           |     ... |
 *   |                           | 1012 kB |
 *   |                           | 1016 kB |
 *   |                           | 1020 kB |
 *   |=====================================|
 *   | Huge                      |    1 MB |
 *   |                           |    2 MB |
 *   |                           |    3 MB |
 *   |                           |     ... |
 *   |=====================================|
 *
 * NOTE: Due to Mozilla bug 691003, we cannot reserve less than one word for an
 * allocation on Linux or Mac.  So on 32-bit *nix, the smallest bucket size is
 * 4 bytes, and on 64-bit, the smallest bucket size is 8 bytes.
 *
 * A different mechanism is used for each category:
 *
 *   Small : Each size class is segregated into its own set of runs.  Each run
 *           maintains a bitmap of which regions are free/allocated.
 *
 *   Large : Each allocation is backed by a dedicated run.  Metadata are stored
 *           in the associated arena chunk header maps.
 *
 *   Huge : Each allocation is backed by a dedicated contiguous set of chunks.
 *          Metadata are stored in a separate red-black tree.
 *
 *******************************************************************************
 */

#include "mozmemory_wrap.h"
#include "mozilla/Sprintf.h"

#ifdef MOZ_MEMORY_ANDROID
#define NO_TLS
#endif

/*
 * On Linux, we use madvise(MADV_DONTNEED) to release memory back to the
 * operating system.  If we release 1MB of live pages with MADV_DONTNEED, our
 * RSS will decrease by 1MB (almost) immediately.
 *
 * On Mac, we use madvise(MADV_FREE).  Unlike MADV_DONTNEED on Linux, MADV_FREE
 * on Mac doesn't cause the OS to release the specified pages immediately; the
 * OS keeps them in our process until the machine comes under memory pressure.
 *
 * It's therefore difficult to measure the process's RSS on Mac, since, in the
 * absence of memory pressure, the contribution from the heap to RSS will not
 * decrease due to our madvise calls.
 *
 * We therefore define MALLOC_DOUBLE_PURGE on Mac.  This causes jemalloc to
 * track which pages have been MADV_FREE'd.  You can then call
 * jemalloc_purge_freed_pages(), which will force the OS to release those
 * MADV_FREE'd pages, making the process's RSS reflect its true memory usage.
 *
 * The jemalloc_purge_freed_pages definition in memory/build/mozmemory.h needs
 * to be adjusted if MALLOC_DOUBLE_PURGE is ever enabled on Linux.
 */
#ifdef MOZ_MEMORY_DARWIN
#define MALLOC_DOUBLE_PURGE
#endif

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef MOZ_MEMORY_WINDOWS

/* Some defines from the CRT internal headers that we need here. */
#define _CRT_SPINCOUNT 5000
#include <io.h>
#include <windows.h>
#include <intrin.h>
#include <algorithm>

#define	SIZE_T_MAX SIZE_MAX
#define	STDERR_FILENO 2

#ifndef NO_TLS
static unsigned long tlsIndex = 0xffffffff;
#endif

/* use MSVC intrinsics */
#pragma intrinsic(_BitScanForward)
static __forceinline int
ffs(int x)
{
	unsigned long i;

	if (_BitScanForward(&i, x) != 0)
		return (i + 1);

	return (0);
}

/* Implement getenv without using malloc */
static char mozillaMallocOptionsBuf[64];

#define	getenv xgetenv
static char *
getenv(const char *name)
{

	if (GetEnvironmentVariableA(name, (LPSTR)&mozillaMallocOptionsBuf,
		    sizeof(mozillaMallocOptionsBuf)) > 0)
		return (mozillaMallocOptionsBuf);

	return nullptr;
}

#if defined(_WIN64)
typedef long long ssize_t;
#else
typedef long ssize_t;
#endif

#define	MALLOC_DECOMMIT
#endif

#ifndef MOZ_MEMORY_WINDOWS
#ifndef MOZ_MEMORY_SOLARIS
#include <sys/cdefs.h>
#endif
#include <sys/mman.h>
#ifndef MADV_FREE
#  define MADV_FREE	MADV_DONTNEED
#endif
#ifndef MAP_NOSYNC
#  define MAP_NOSYNC	0
#endif
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#if !defined(MOZ_MEMORY_SOLARIS) && !defined(MOZ_MEMORY_ANDROID)
#include <sys/sysctl.h>
#endif
#include <sys/uio.h>

#include <errno.h>
#include <limits.h>
#ifndef SIZE_T_MAX
#  define SIZE_T_MAX	SIZE_MAX
#endif
#include <pthread.h>
#include <sched.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifndef MOZ_MEMORY_DARWIN
#include <strings.h>
#endif
#include <unistd.h>

#ifdef MOZ_MEMORY_DARWIN
#include <libkern/OSAtomic.h>
#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#include <malloc/malloc.h>
#endif

#endif

#include "mozjemalloc_types.h"
#include "linkedlist.h"

extern "C" void moz_abort();

/* Some tools, such as /dev/dsp wrappers, LD_PRELOAD libraries that
 * happen to override mmap() and call dlsym() from their overridden
 * mmap(). The problem is that dlsym() calls malloc(), and this ends
 * up in a dead lock in jemalloc.
 * On these systems, we prefer to directly use the system call.
 * We do that for Linux systems and kfreebsd with GNU userland.
 * Note sanity checks are not done (alignment of offset, ...) because
 * the uses of mmap are pretty limited, in jemalloc.
 *
 * On Alpha, glibc has a bug that prevents syscall() to work for system
 * calls with 6 arguments
 */
#if (defined(MOZ_MEMORY_LINUX) && !defined(__alpha__)) || \
    (defined(MOZ_MEMORY_BSD) && defined(__GLIBC__))
#include <sys/syscall.h>
#if defined(SYS_mmap) || defined(SYS_mmap2)
static inline
void *_mmap(void *addr, size_t length, int prot, int flags,
            int fd, off_t offset)
{
/* S390 only passes one argument to the mmap system call, which is a
 * pointer to a structure containing the arguments */
#ifdef __s390__
	struct {
		void *addr;
		size_t length;
		long prot;
		long flags;
		long fd;
		off_t offset;
	} args = { addr, length, prot, flags, fd, offset };
	return (void *) syscall(SYS_mmap, &args);
#else
#if defined(MOZ_MEMORY_ANDROID) && defined(__aarch64__) && defined(SYS_mmap2)
/* Android NDK defines SYS_mmap2 for AArch64 despite it not supporting mmap2 */
#undef SYS_mmap2
#endif
#ifdef SYS_mmap2
	return (void *) syscall(SYS_mmap2, addr, length, prot, flags,
	                       fd, offset >> 12);
#else
	return (void *) syscall(SYS_mmap, addr, length, prot, flags,
                               fd, offset);
#endif
#endif
}
#define mmap _mmap
#define munmap(a, l) syscall(SYS_munmap, a, l)
#endif
#endif

#ifdef MOZ_MEMORY_DARWIN
static pthread_key_t tlsIndex;
#endif

#ifdef MOZ_MEMORY_WINDOWS
   /* MSVC++ does not support C99 variable-length arrays. */
#  define RB_NO_C99_VARARRAYS
#endif
#include "rb.h"

#ifdef MOZ_DEBUG
   /* Disable inlining to make debugging easier. */
#ifdef inline
#undef inline
#endif

#  define inline
#endif

/* Size of stack-allocated buffer passed to strerror_r(). */
#define	STRERROR_BUF		64

/* Minimum alignment of non-tiny allocations is 2^QUANTUM_2POW_MIN bytes. */
#  define QUANTUM_2POW_MIN      4
#if defined(_WIN64) || defined(__LP64__)
#  define SIZEOF_PTR_2POW       3
#else
#  define SIZEOF_PTR_2POW       2
#endif
#define PIC

#define	SIZEOF_PTR		(1U << SIZEOF_PTR_2POW)

/* sizeof(int) == (1U << SIZEOF_INT_2POW). */
#ifndef SIZEOF_INT_2POW
#  define SIZEOF_INT_2POW	2
#endif

/* We can't use TLS in non-PIC programs, since TLS relies on loader magic. */
#if (!defined(PIC) && !defined(NO_TLS))
#  define NO_TLS
#endif

/*
 * Size and alignment of memory chunks that are allocated by the OS's virtual
 * memory system.
 */
#define	CHUNK_2POW_DEFAULT	20
/* Maximum number of dirty pages per arena. */
#define	DIRTY_MAX_DEFAULT	(1U << 8)

/*
 * Maximum size of L1 cache line.  This is used to avoid cache line aliasing,
 * so over-estimates are okay (up to a point), but under-estimates will
 * negatively affect performance.
 */
#define	CACHELINE_2POW		6
#define	CACHELINE		((size_t)(1U << CACHELINE_2POW))

/*
 * Smallest size class to support.  On Windows the smallest allocation size
 * must be 8 bytes on 32-bit, 16 bytes on 64-bit.  On Linux and Mac, even
 * malloc(1) must reserve a word's worth of memory (see Mozilla bug 691003).
 */
#ifdef MOZ_MEMORY_WINDOWS
#define TINY_MIN_2POW           (sizeof(void*) == 8 ? 4 : 3)
#else
#define TINY_MIN_2POW           (sizeof(void*) == 8 ? 3 : 2)
#endif

/*
 * Maximum size class that is a multiple of the quantum, but not (necessarily)
 * a power of 2.  Above this size, allocations are rounded up to the nearest
 * power of 2.
 */
#define	SMALL_MAX_2POW_DEFAULT	9
#define	SMALL_MAX_DEFAULT	(1U << SMALL_MAX_2POW_DEFAULT)

/*
 * RUN_MAX_OVRHD indicates maximum desired run header overhead.  Runs are sized
 * as small as possible such that this setting is still honored, without
 * violating other constraints.  The goal is to make runs as small as possible
 * without exceeding a per run external fragmentation threshold.
 *
 * We use binary fixed point math for overhead computations, where the binary
 * point is implicitly RUN_BFP bits to the left.
 *
 * Note that it is possible to set RUN_MAX_OVRHD low enough that it cannot be
 * honored for some/all object sizes, since there is one bit of header overhead
 * per object (plus a constant).  This constraint is relaxed (ignored) for runs
 * that are so small that the per-region overhead is greater than:
 *
 *   (RUN_MAX_OVRHD / (reg_size << (3+RUN_BFP))
 */
#define	RUN_BFP			12
/*                                    \/   Implicit binary fixed point. */
#define	RUN_MAX_OVRHD		0x0000003dU
#define	RUN_MAX_OVRHD_RELAX	0x00001800U

/******************************************************************************/

/* MALLOC_DECOMMIT and MALLOC_DOUBLE_PURGE are mutually exclusive. */
#if defined(MALLOC_DECOMMIT) && defined(MALLOC_DOUBLE_PURGE)
#error MALLOC_DECOMMIT and MALLOC_DOUBLE_PURGE are mutually exclusive.
#endif

/*
 * Mutexes based on spinlocks.  We can't use normal pthread spinlocks in all
 * places, because they require malloc()ed memory, which causes bootstrapping
 * issues in some cases.
 */
#if defined(MOZ_MEMORY_WINDOWS)
#define malloc_mutex_t CRITICAL_SECTION
#define malloc_spinlock_t CRITICAL_SECTION
#elif defined(MOZ_MEMORY_DARWIN)
typedef struct {
	OSSpinLock	lock;
} malloc_mutex_t;
typedef struct {
	OSSpinLock	lock;
} malloc_spinlock_t;
#else
typedef pthread_mutex_t malloc_mutex_t;
typedef pthread_mutex_t malloc_spinlock_t;
#endif

/* Set to true once the allocator has been initialized. */
static bool malloc_initialized = false;

#if defined(MOZ_MEMORY_WINDOWS)
/* No init lock for Windows. */
#elif defined(MOZ_MEMORY_DARWIN)
static malloc_mutex_t init_lock = {OS_SPINLOCK_INIT};
#elif defined(MOZ_MEMORY_LINUX) && !defined(MOZ_MEMORY_ANDROID)
static malloc_mutex_t init_lock = PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP;
#else
static malloc_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

/******************************************************************************/
/*
 * Statistics data structures.
 */

typedef struct malloc_bin_stats_s malloc_bin_stats_t;
struct malloc_bin_stats_s {
	/*
	 * Number of allocation requests that corresponded to the size of this
	 * bin.
	 */
	uint64_t	nrequests;

	/* Total number of runs created for this bin's size class. */
	uint64_t	nruns;

	/*
	 * Total number of runs reused by extracting them from the runs tree for
	 * this bin's size class.
	 */
	uint64_t	reruns;

	/* High-water mark for this bin. */
	unsigned long	highruns;

	/* Current number of runs in this bin. */
	unsigned long	curruns;
};

typedef struct arena_stats_s arena_stats_t;
struct arena_stats_s {
	/* Number of bytes currently mapped. */
	size_t		mapped;

	/*
	 * Total number of purge sweeps, total number of madvise calls made,
	 * and total pages purged in order to keep dirty unused memory under
	 * control.
	 */
	uint64_t	npurge;
	uint64_t	nmadvise;
	uint64_t	purged;
#ifdef MALLOC_DECOMMIT
	/*
	 * Total number of decommit/commit operations, and total number of
	 * pages decommitted.
	 */
	uint64_t	ndecommit;
	uint64_t	ncommit;
	uint64_t	decommitted;
#endif

	/* Current number of committed pages. */
	size_t		committed;

	/* Per-size-category statistics. */
	size_t		allocated_small;
	uint64_t	nmalloc_small;
	uint64_t	ndalloc_small;

	size_t		allocated_large;
	uint64_t	nmalloc_large;
	uint64_t	ndalloc_large;
};

/******************************************************************************/
/*
 * Extent data structures.
 */

enum ChunkType {
  UNKNOWN_CHUNK,
  ZEROED_CHUNK,   // chunk only contains zeroes
  ARENA_CHUNK,    // used to back arena runs created by arena_run_alloc
  HUGE_CHUNK,     // used to back huge allocations (e.g. huge_malloc)
  RECYCLED_CHUNK, // chunk has been stored for future use by chunk_recycle
};

/* Tree of extents. */
typedef struct extent_node_s extent_node_t;
struct extent_node_s {
	/* Linkage for the size/address-ordered tree. */
	rb_node(extent_node_t) link_szad;

	/* Linkage for the address-ordered tree. */
	rb_node(extent_node_t) link_ad;

	/* Pointer to the extent that this tree node is responsible for. */
	void	*addr;

	/* Total region size. */
	size_t	size;

	/* What type of chunk is there; used by chunk recycling code. */
	ChunkType chunk_type;
};
typedef rb_tree(extent_node_t) extent_tree_t;

/******************************************************************************/
/*
 * Radix tree data structures.
 */

/*
 * Size of each radix tree node (must be a power of 2).  This impacts tree
 * depth.
 */
#if (SIZEOF_PTR == 4)
#  define MALLOC_RTREE_NODESIZE (1U << 14)
#else
#  define MALLOC_RTREE_NODESIZE CACHELINE
#endif

typedef struct malloc_rtree_s malloc_rtree_t;
struct malloc_rtree_s {
	malloc_spinlock_t	lock;
	void			**root;
	unsigned		height;
	unsigned		level2bits[1]; /* Dynamically sized. */
};

/******************************************************************************/
/*
 * Arena data structures.
 */

typedef struct arena_s arena_t;
typedef struct arena_bin_s arena_bin_t;

/* Each element of the chunk map corresponds to one page within the chunk. */
typedef struct arena_chunk_map_s arena_chunk_map_t;
struct arena_chunk_map_s {
	/*
	 * Linkage for run trees.  There are two disjoint uses:
	 *
	 * 1) arena_t's runs_avail tree.
	 * 2) arena_run_t conceptually uses this linkage for in-use non-full
	 *    runs, rather than directly embedding linkage.
	 */
	rb_node(arena_chunk_map_t)	link;

	/*
	 * Run address (or size) and various flags are stored together.  The bit
	 * layout looks like (assuming 32-bit system):
	 *
	 *   ???????? ???????? ????---- -mckdzla
	 *
	 * ? : Unallocated: Run address for first/last pages, unset for internal
	 *                  pages.
	 *     Small: Run address.
	 *     Large: Run size for first page, unset for trailing pages.
	 * - : Unused.
	 * m : MADV_FREE/MADV_DONTNEED'ed?
	 * c : decommitted?
	 * k : key?
	 * d : dirty?
	 * z : zeroed?
	 * l : large?
	 * a : allocated?
	 *
	 * Following are example bit patterns for the three types of runs.
	 *
	 * r : run address
	 * s : run size
	 * x : don't care
	 * - : 0
	 * [cdzla] : bit set
	 *
	 *   Unallocated:
	 *     ssssssss ssssssss ssss---- --c-----
	 *     xxxxxxxx xxxxxxxx xxxx---- ----d---
	 *     ssssssss ssssssss ssss---- -----z--
	 *
	 *   Small:
	 *     rrrrrrrr rrrrrrrr rrrr---- -------a
	 *     rrrrrrrr rrrrrrrr rrrr---- -------a
	 *     rrrrrrrr rrrrrrrr rrrr---- -------a
	 *
	 *   Large:
	 *     ssssssss ssssssss ssss---- ------la
	 *     -------- -------- -------- ------la
	 *     -------- -------- -------- ------la
	 */
	size_t				bits;

/* Note that CHUNK_MAP_DECOMMITTED's meaning varies depending on whether
 * MALLOC_DECOMMIT and MALLOC_DOUBLE_PURGE are defined.
 *
 * If MALLOC_DECOMMIT is defined, a page which is CHUNK_MAP_DECOMMITTED must be
 * re-committed with pages_commit() before it may be touched.  If
 * MALLOC_DECOMMIT is defined, MALLOC_DOUBLE_PURGE may not be defined.
 *
 * If neither MALLOC_DECOMMIT nor MALLOC_DOUBLE_PURGE is defined, pages which
 * are madvised (with either MADV_DONTNEED or MADV_FREE) are marked with
 * CHUNK_MAP_MADVISED.
 *
 * Otherwise, if MALLOC_DECOMMIT is not defined and MALLOC_DOUBLE_PURGE is
 * defined, then a page which is madvised is marked as CHUNK_MAP_MADVISED.
 * When it's finally freed with jemalloc_purge_freed_pages, the page is marked
 * as CHUNK_MAP_DECOMMITTED.
 */
#define	CHUNK_MAP_MADVISED	((size_t)0x40U)
#define	CHUNK_MAP_DECOMMITTED	((size_t)0x20U)
#define	CHUNK_MAP_MADVISED_OR_DECOMMITTED (CHUNK_MAP_MADVISED | CHUNK_MAP_DECOMMITTED)
#define	CHUNK_MAP_KEY		((size_t)0x10U)
#define	CHUNK_MAP_DIRTY		((size_t)0x08U)
#define	CHUNK_MAP_ZEROED	((size_t)0x04U)
#define	CHUNK_MAP_LARGE		((size_t)0x02U)
#define	CHUNK_MAP_ALLOCATED	((size_t)0x01U)
};
typedef rb_tree(arena_chunk_map_t) arena_avail_tree_t;
typedef rb_tree(arena_chunk_map_t) arena_run_tree_t;

/* Arena chunk header. */
typedef struct arena_chunk_s arena_chunk_t;
struct arena_chunk_s {
	/* Arena that owns the chunk. */
	arena_t		*arena;

	/* Linkage for the arena's chunks_dirty tree. */
	rb_node(arena_chunk_t) link_dirty;

#ifdef MALLOC_DOUBLE_PURGE
	/* If we're double-purging, we maintain a linked list of chunks which
	 * have pages which have been madvise(MADV_FREE)'d but not explicitly
	 * purged.
	 *
	 * We're currently lazy and don't remove a chunk from this list when
	 * all its madvised pages are recommitted. */
	LinkedList	chunks_madvised_elem;
#endif

	/* Number of dirty pages. */
	size_t		ndirty;

	/* Map of pages within chunk that keeps track of free/large/small. */
	arena_chunk_map_t map[1]; /* Dynamically sized. */
};
typedef rb_tree(arena_chunk_t) arena_chunk_tree_t;

typedef struct arena_run_s arena_run_t;
struct arena_run_s {
#if defined(MOZ_DEBUG) || defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
	uint32_t	magic;
#  define ARENA_RUN_MAGIC 0x384adf93
#endif

	/* Bin this run is associated with. */
	arena_bin_t	*bin;

	/* Index of first element that might have a free region. */
	unsigned	regs_minelm;

	/* Number of free regions in run. */
	unsigned	nfree;

	/* Bitmask of in-use regions (0: in use, 1: free). */
	unsigned	regs_mask[1]; /* Dynamically sized. */
};

struct arena_bin_s {
	/*
	 * Current run being used to service allocations of this bin's size
	 * class.
	 */
	arena_run_t	*runcur;

	/*
	 * Tree of non-full runs.  This tree is used when looking for an
	 * existing run when runcur is no longer usable.  We choose the
	 * non-full run that is lowest in memory; this policy tends to keep
	 * objects packed well, and it can also help reduce the number of
	 * almost-empty chunks.
	 */
	arena_run_tree_t runs;

	/* Size of regions in a run for this bin's size class. */
	size_t		reg_size;

	/* Total size of a run for this bin's size class. */
	size_t		run_size;

	/* Total number of regions in a run for this bin's size class. */
	uint32_t	nregs;

	/* Number of elements in a run's regs_mask for this bin's size class. */
	uint32_t	regs_mask_nelms;

	/* Offset of first region in a run for this bin's size class. */
	uint32_t	reg0_offset;

	/* Bin statistics. */
	malloc_bin_stats_t stats;
};

struct arena_s {
#if defined(MOZ_DEBUG) || defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
	uint32_t		magic;
#  define ARENA_MAGIC 0x947d3d24
#endif

	/* All operations on this arena require that lock be locked. */
	malloc_spinlock_t	lock;

	arena_stats_t		stats;

	/* Tree of dirty-page-containing chunks this arena manages. */
	arena_chunk_tree_t	chunks_dirty;

#ifdef MALLOC_DOUBLE_PURGE
	/* Head of a linked list of MADV_FREE'd-page-containing chunks this
	 * arena manages. */
	LinkedList		chunks_madvised;
#endif

	/*
	 * In order to avoid rapid chunk allocation/deallocation when an arena
	 * oscillates right on the cusp of needing a new chunk, cache the most
	 * recently freed chunk.  The spare is left in the arena's chunk trees
	 * until it is deleted.
	 *
	 * There is one spare chunk per arena, rather than one spare total, in
	 * order to avoid interactions between multiple threads that could make
	 * a single spare inadequate.
	 */
	arena_chunk_t		*spare;

	/*
	 * Current count of pages within unused runs that are potentially
	 * dirty, and for which madvise(... MADV_FREE) has not been called.  By
	 * tracking this, we can institute a limit on how much dirty unused
	 * memory is mapped for each arena.
	 */
	size_t			ndirty;

	/*
	 * Size/address-ordered tree of this arena's available runs.  This tree
	 * is used for first-best-fit run allocation.
	 */
	arena_avail_tree_t	runs_avail;

	/*
	 * bins is used to store rings of free regions of the following sizes,
	 * assuming a 16-byte quantum, 4kB pagesize, and default MALLOC_OPTIONS.
	 *
	 *   bins[i] | size |
	 *   --------+------+
	 *        0  |    2 |
	 *        1  |    4 |
	 *        2  |    8 |
	 *   --------+------+
	 *        3  |   16 |
	 *        4  |   32 |
	 *        5  |   48 |
	 *        6  |   64 |
	 *           :      :
	 *           :      :
	 *       33  |  496 |
	 *       34  |  512 |
	 *   --------+------+
	 *       35  | 1024 |
	 *       36  | 2048 |
	 *   --------+------+
	 */
	arena_bin_t		bins[1]; /* Dynamically sized. */
};

/******************************************************************************/
/*
 * Data.
 */

/*
 * When MALLOC_STATIC_SIZES is defined most of the parameters
 * controlling the malloc behavior are defined as compile-time constants
 * for best performance and cannot be altered at runtime.
 */
#if !defined(__ia64__) && !defined(__sparc__) && !defined(__mips__) && !defined(__aarch64__)
#define MALLOC_STATIC_SIZES 1
#endif

#ifdef MALLOC_STATIC_SIZES

/*
 * VM page size. It must divide the runtime CPU page size or the code
 * will abort.
 * Platform specific page size conditions copied from js/public/HeapAPI.h
 */
#if (defined(SOLARIS) || defined(__FreeBSD__)) && \
    (defined(__sparc) || defined(__sparcv9) || defined(__ia64))
#define pagesize_2pow			((size_t) 13)
#elif defined(__powerpc64__)
#define pagesize_2pow			((size_t) 16)
#else
#define pagesize_2pow			((size_t) 12)
#endif
#define pagesize			((size_t) 1 << pagesize_2pow)
#define pagesize_mask			(pagesize - 1)

/* Various quantum-related settings. */

#define QUANTUM_DEFAULT 		((size_t) 1 << QUANTUM_2POW_MIN)
static const size_t	quantum	=	QUANTUM_DEFAULT;
static const size_t	quantum_mask =	QUANTUM_DEFAULT - 1;

/* Various bin-related settings. */

static const size_t	small_min =	(QUANTUM_DEFAULT >> 1) + 1;
static const size_t	small_max =	(size_t) SMALL_MAX_DEFAULT;

/* Max size class for bins. */
static const size_t	bin_maxclass =	pagesize >> 1;

 /* Number of (2^n)-spaced tiny bins. */
static const unsigned	ntbins =	(unsigned)
					(QUANTUM_2POW_MIN - TINY_MIN_2POW);

 /* Number of quantum-spaced bins. */
static const unsigned	nqbins =	(unsigned)
					(SMALL_MAX_DEFAULT >> QUANTUM_2POW_MIN);

/* Number of (2^n)-spaced sub-page bins. */
static const unsigned	nsbins =	(unsigned)
					(pagesize_2pow -
					 SMALL_MAX_2POW_DEFAULT - 1);

#else /* !MALLOC_STATIC_SIZES */

/* VM page size. */
static size_t		pagesize;
static size_t		pagesize_mask;
static size_t		pagesize_2pow;

/* Various bin-related settings. */
static size_t		bin_maxclass; /* Max size class for bins. */
static unsigned		ntbins; /* Number of (2^n)-spaced tiny bins. */
static unsigned		nqbins; /* Number of quantum-spaced bins. */
static unsigned		nsbins; /* Number of (2^n)-spaced sub-page bins. */
static size_t		small_min;
static size_t		small_max;

/* Various quantum-related settings. */
static size_t		quantum;
static size_t		quantum_mask; /* (quantum - 1). */

#endif

/* Various chunk-related settings. */

/*
 * Compute the header size such that it is large enough to contain the page map
 * and enough nodes for the worst case: one node per non-header page plus one
 * extra for situations where we briefly have one more node allocated than we
 * will need.
 */
#define calculate_arena_header_size()					\
	(sizeof(arena_chunk_t) + sizeof(arena_chunk_map_t) * (chunk_npages - 1))

#define calculate_arena_header_pages()					\
	((calculate_arena_header_size() >> pagesize_2pow) +		\
	 ((calculate_arena_header_size() & pagesize_mask) ? 1 : 0))

/* Max size class for arenas. */
#define calculate_arena_maxclass()					\
	(chunksize - (arena_chunk_header_npages << pagesize_2pow))

/*
 * Recycle at most 128 chunks. With 1 MiB chunks, this means we retain at most
 * 6.25% of the process address space on a 32-bit OS for later use.
 */
#define CHUNK_RECYCLE_LIMIT 128

#ifdef MALLOC_STATIC_SIZES
#define CHUNKSIZE_DEFAULT		((size_t) 1 << CHUNK_2POW_DEFAULT)
static const size_t	chunksize =	CHUNKSIZE_DEFAULT;
static const size_t	chunksize_mask =CHUNKSIZE_DEFAULT - 1;
static const size_t	chunk_npages =	CHUNKSIZE_DEFAULT >> pagesize_2pow;
#define arena_chunk_header_npages	calculate_arena_header_pages()
#define arena_maxclass			calculate_arena_maxclass()
static const size_t	recycle_limit = CHUNK_RECYCLE_LIMIT * CHUNKSIZE_DEFAULT;
#else
static size_t		chunksize;
static size_t		chunksize_mask; /* (chunksize - 1). */
static size_t		chunk_npages;
static size_t		arena_chunk_header_npages;
static size_t		arena_maxclass; /* Max size class for arenas. */
static size_t		recycle_limit;
#endif

/* The current amount of recycled bytes, updated atomically. */
static size_t recycled_size;

/********/
/*
 * Chunks.
 */

static malloc_rtree_t *chunk_rtree;

/* Protects chunk-related data structures. */
static malloc_mutex_t	chunks_mtx;

/*
 * Trees of chunks that were previously allocated (trees differ only in node
 * ordering).  These are used when allocating chunks, in an attempt to re-use
 * address space.  Depending on function, different tree orderings are needed,
 * which is why there are two trees with the same contents.
 */
static extent_tree_t	chunks_szad_mmap;
static extent_tree_t	chunks_ad_mmap;

/* Protects huge allocation-related data structures. */
static malloc_mutex_t	huge_mtx;

/* Tree of chunks that are stand-alone huge allocations. */
static extent_tree_t	huge;

/* Huge allocation statistics. */
static uint64_t		huge_nmalloc;
static uint64_t		huge_ndalloc;
static size_t		huge_allocated;
static size_t		huge_mapped;

/****************************/
/*
 * base (internal allocation).
 */

/*
 * Current pages that are being used for internal memory allocations.  These
 * pages are carved up in cacheline-size quanta, so that there is no chance of
 * false cache line sharing.
 */
static void		*base_pages;
static void		*base_next_addr;
static void		*base_next_decommitted;
static void		*base_past_addr; /* Addr immediately past base_pages. */
static extent_node_t	*base_nodes;
static malloc_mutex_t	base_mtx;
static size_t		base_mapped;
static size_t		base_committed;

/********/
/*
 * Arenas.
 */

/*
 * Arenas that are used to service external requests.  Not all elements of the
 * arenas array are necessarily used; arenas are created lazily as needed.
 */
static arena_t		**arenas;
static unsigned		narenas;
static malloc_spinlock_t arenas_lock; /* Protects arenas initialization. */

#ifndef NO_TLS
/*
 * Map of pthread_self() --> arenas[???], used for selecting an arena to use
 * for allocations.
 */
#if !defined(MOZ_MEMORY_WINDOWS) && !defined(MOZ_MEMORY_DARWIN)
static __thread arena_t	*arenas_map;
#endif
#endif

/*******************************/
/*
 * Runtime configuration options.
 */
const uint8_t kAllocJunk = 0xe4;
const uint8_t kAllocPoison = 0xe5;

#ifdef MOZ_DEBUG
static bool	opt_abort = true;
static bool	opt_junk = true;
static bool	opt_zero = false;
#else
static bool	opt_abort = false;
static const bool	opt_junk = false;
static const bool	opt_zero = false;
#endif

static size_t	opt_dirty_max = DIRTY_MAX_DEFAULT;
#ifdef MALLOC_STATIC_SIZES
#define opt_quantum_2pow	QUANTUM_2POW_MIN
#define opt_small_max_2pow	SMALL_MAX_2POW_DEFAULT
#define opt_chunk_2pow		CHUNK_2POW_DEFAULT
#else
static size_t	opt_quantum_2pow = QUANTUM_2POW_MIN;
static size_t	opt_small_max_2pow = SMALL_MAX_2POW_DEFAULT;
static size_t	opt_chunk_2pow = CHUNK_2POW_DEFAULT;
#endif

/******************************************************************************/
/*
 * Begin forward declarations.
 */

static void	*chunk_alloc(size_t size, size_t alignment, bool base, bool zero);
static void	chunk_dealloc(void *chunk, size_t size, ChunkType chunk_type);
static arena_t	*arenas_extend();
static void	*huge_malloc(size_t size, bool zero);
static void	*huge_palloc(size_t size, size_t alignment, bool zero);
static void	*huge_ralloc(void *ptr, size_t size, size_t oldsize);
static void	huge_dalloc(void *ptr);
#ifdef MOZ_MEMORY_WINDOWS
extern "C"
#else
static
#endif
bool		malloc_init_hard(void);

#ifdef MOZ_MEMORY_DARWIN
#define FORK_HOOK extern "C"
#else
#define FORK_HOOK static
#endif
FORK_HOOK void _malloc_prefork(void);
FORK_HOOK void _malloc_postfork_parent(void);
FORK_HOOK void _malloc_postfork_child(void);

/*
 * End forward declarations.
 */
/******************************************************************************/

static inline size_t
load_acquire_z(size_t *p)
{
	volatile size_t result = *p;
#  ifdef MOZ_MEMORY_WINDOWS
	/*
	 * We use InterlockedExchange with a dummy value to insert a memory
	 * barrier. This has been confirmed to generate the right instruction
	 * and is also used by MinGW.
	 */
	volatile long dummy = 0;
	InterlockedExchange(&dummy, 1);
#  else
	__sync_synchronize();
#  endif
	return result;
}

/*
 * umax2s() provides minimal integer printing functionality, which is
 * especially useful for situations where allocation in vsnprintf() calls would
 * potentially cause deadlock.
 */
#define	UMAX2S_BUFSIZE	65
char *
umax2s(uintmax_t x, char *s)
{
	unsigned i;

	i = UMAX2S_BUFSIZE - 1;
	s[i] = '\0';
	do {
		i--;
		s[i] = "0123456789"[x % 10];
		x /= 10;
	} while (x > 0);
	return (&s[i]);
}

static void
_malloc_message(const char *p1, const char *p2, const char *p3, const char *p4)
{
#if !defined(MOZ_MEMORY_WINDOWS)
#define	_write	write
#endif
	// Pretend to check _write() errors to suppress gcc warnings about
	// warn_unused_result annotations in some versions of glibc headers.
	if (_write(STDERR_FILENO, p1, (unsigned int) strlen(p1)) < 0)
		return;
	if (_write(STDERR_FILENO, p2, (unsigned int) strlen(p2)) < 0)
		return;
	if (_write(STDERR_FILENO, p3, (unsigned int) strlen(p3)) < 0)
		return;
	if (_write(STDERR_FILENO, p4, (unsigned int) strlen(p4)) < 0)
		return;
}

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/TaggedAnonymousMemory.h"
// Note: MozTaggedAnonymousMmap() could call an LD_PRELOADed mmap
// instead of the one defined here; use only MozTagAnonymousMemory().

#ifdef MOZ_MEMORY_ANDROID
// Android's pthread.h does not declare pthread_atfork() until SDK 21.
extern "C" MOZ_EXPORT
int pthread_atfork(void (*)(void), void (*)(void), void(*)(void));
#endif

/******************************************************************************/
/*
 * Begin mutex.  We can't use normal pthread mutexes in all places, because
 * they require malloc()ed memory, which causes bootstrapping issues in some
 * cases.
 */

static bool
malloc_mutex_init(malloc_mutex_t *mutex)
{
#if defined(MOZ_MEMORY_WINDOWS)
	if (!InitializeCriticalSectionAndSpinCount(mutex, _CRT_SPINCOUNT))
		return (true);
#elif defined(MOZ_MEMORY_DARWIN)
	mutex->lock = OS_SPINLOCK_INIT;
#elif defined(MOZ_MEMORY_LINUX) && !defined(MOZ_MEMORY_ANDROID)
	pthread_mutexattr_t attr;
	if (pthread_mutexattr_init(&attr) != 0)
		return (true);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ADAPTIVE_NP);
	if (pthread_mutex_init(mutex, &attr) != 0) {
		pthread_mutexattr_destroy(&attr);
		return (true);
	}
	pthread_mutexattr_destroy(&attr);
#else
	if (pthread_mutex_init(mutex, nullptr) != 0)
		return (true);
#endif
	return (false);
}

static inline void
malloc_mutex_lock(malloc_mutex_t *mutex)
{

#if defined(MOZ_MEMORY_WINDOWS)
	EnterCriticalSection(mutex);
#elif defined(MOZ_MEMORY_DARWIN)
	OSSpinLockLock(&mutex->lock);
#else
	pthread_mutex_lock(mutex);
#endif
}

static inline void
malloc_mutex_unlock(malloc_mutex_t *mutex)
{

#if defined(MOZ_MEMORY_WINDOWS)
	LeaveCriticalSection(mutex);
#elif defined(MOZ_MEMORY_DARWIN)
	OSSpinLockUnlock(&mutex->lock);
#else
	pthread_mutex_unlock(mutex);
#endif
}

#if (defined(__GNUC__))
__attribute__((unused))
#  endif
static bool
malloc_spin_init(malloc_spinlock_t *lock)
{
#if defined(MOZ_MEMORY_WINDOWS)
	if (!InitializeCriticalSectionAndSpinCount(lock, _CRT_SPINCOUNT))
			return (true);
#elif defined(MOZ_MEMORY_DARWIN)
	lock->lock = OS_SPINLOCK_INIT;
#elif defined(MOZ_MEMORY_LINUX) && !defined(MOZ_MEMORY_ANDROID)
	pthread_mutexattr_t attr;
	if (pthread_mutexattr_init(&attr) != 0)
		return (true);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ADAPTIVE_NP);
	if (pthread_mutex_init(lock, &attr) != 0) {
		pthread_mutexattr_destroy(&attr);
		return (true);
	}
	pthread_mutexattr_destroy(&attr);
#else
	if (pthread_mutex_init(lock, nullptr) != 0)
		return (true);
#endif
	return (false);
}

static inline void
malloc_spin_lock(malloc_spinlock_t *lock)
{

#if defined(MOZ_MEMORY_WINDOWS)
	EnterCriticalSection(lock);
#elif defined(MOZ_MEMORY_DARWIN)
	OSSpinLockLock(&lock->lock);
#else
	pthread_mutex_lock(lock);
#endif
}

static inline void
malloc_spin_unlock(malloc_spinlock_t *lock)
{
#if defined(MOZ_MEMORY_WINDOWS)
	LeaveCriticalSection(lock);
#elif defined(MOZ_MEMORY_DARWIN)
	OSSpinLockUnlock(&lock->lock);
#else
	pthread_mutex_unlock(lock);
#endif
}

/*
 * End mutex.
 */
/******************************************************************************/
/*
 * Begin spin lock.  Spin locks here are actually adaptive mutexes that block
 * after a period of spinning, because unbounded spinning would allow for
 * priority inversion.
 */

#if !defined(MOZ_MEMORY_DARWIN)
#  define	malloc_spin_init	malloc_mutex_init
#  define	malloc_spin_lock	malloc_mutex_lock
#  define	malloc_spin_unlock	malloc_mutex_unlock
#endif

/*
 * End spin lock.
 */
/******************************************************************************/
/*
 * Begin Utility functions/macros.
 */

/* Return the chunk address for allocation address a. */
#define	CHUNK_ADDR2BASE(a)						\
	((void *)((uintptr_t)(a) & ~chunksize_mask))

/* Return the chunk offset of address a. */
#define	CHUNK_ADDR2OFFSET(a)						\
	((size_t)((uintptr_t)(a) & chunksize_mask))

/* Return the smallest chunk multiple that is >= s. */
#define	CHUNK_CEILING(s)						\
	(((s) + chunksize_mask) & ~chunksize_mask)

/* Return the smallest cacheline multiple that is >= s. */
#define	CACHELINE_CEILING(s)						\
	(((s) + (CACHELINE - 1)) & ~(CACHELINE - 1))

/* Return the smallest quantum multiple that is >= a. */
#define	QUANTUM_CEILING(a)						\
	(((a) + quantum_mask) & ~quantum_mask)

/* Return the smallest pagesize multiple that is >= s. */
#define	PAGE_CEILING(s)							\
	(((s) + pagesize_mask) & ~pagesize_mask)

/* Compute the smallest power of 2 that is >= x. */
static inline size_t
pow2_ceil(size_t x)
{

	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
#if (SIZEOF_PTR == 8)
	x |= x >> 32;
#endif
	x++;
	return (x);
}

static inline const char *
_getprogname(void)
{

	return ("<jemalloc>");
}

/******************************************************************************/

static inline void
pages_decommit(void *addr, size_t size)
{

#ifdef MOZ_MEMORY_WINDOWS
	/*
	* The region starting at addr may have been allocated in multiple calls
	* to VirtualAlloc and recycled, so decommitting the entire region in one
	* go may not be valid. However, since we allocate at least a chunk at a
	* time, we may touch any region in chunksized increments.
	*/
	size_t pages_size = std::min(size, chunksize -
		CHUNK_ADDR2OFFSET((uintptr_t)addr));
	while (size > 0) {
		if (!VirtualFree(addr, pages_size, MEM_DECOMMIT))
			moz_abort();
		addr = (void *)((uintptr_t)addr + pages_size);
		size -= pages_size;
		pages_size = std::min(size, chunksize);
	}
#else
	if (mmap(addr, size, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1,
	    0) == MAP_FAILED)
		moz_abort();
	MozTagAnonymousMemory(addr, size, "jemalloc-decommitted");
#endif
}

static inline void
pages_commit(void *addr, size_t size)
{

#  ifdef MOZ_MEMORY_WINDOWS
	/*
	* The region starting at addr may have been allocated in multiple calls
	* to VirtualAlloc and recycled, so committing the entire region in one
	* go may not be valid. However, since we allocate at least a chunk at a
	* time, we may touch any region in chunksized increments.
	*/
	size_t pages_size = std::min(size, chunksize -
		CHUNK_ADDR2OFFSET((uintptr_t)addr));
	while (size > 0) {
		if (!VirtualAlloc(addr, pages_size, MEM_COMMIT, PAGE_READWRITE))
			moz_abort();
		addr = (void *)((uintptr_t)addr + pages_size);
		size -= pages_size;
		pages_size = std::min(size, chunksize);
	}
#  else
	if (mmap(addr, size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE |
	    MAP_ANON, -1, 0) == MAP_FAILED)
		moz_abort();
	MozTagAnonymousMemory(addr, size, "jemalloc");
#  endif
}

static bool
base_pages_alloc(size_t minsize)
{
	size_t csize;
	size_t pminsize;

	MOZ_ASSERT(minsize != 0);
	csize = CHUNK_CEILING(minsize);
	base_pages = chunk_alloc(csize, chunksize, true, false);
	if (!base_pages)
		return (true);
	base_next_addr = base_pages;
	base_past_addr = (void *)((uintptr_t)base_pages + csize);
	/*
	 * Leave enough pages for minsize committed, since otherwise they would
	 * have to be immediately recommitted.
	 */
	pminsize = PAGE_CEILING(minsize);
	base_next_decommitted = (void *)((uintptr_t)base_pages + pminsize);
#  if defined(MALLOC_DECOMMIT)
	if (pminsize < csize)
		pages_decommit(base_next_decommitted, csize - pminsize);
#  endif
	base_mapped += csize;
	base_committed += pminsize;

	return (false);
}

static void *
base_alloc(size_t size)
{
	void *ret;
	size_t csize;

	/* Round size up to nearest multiple of the cacheline size. */
	csize = CACHELINE_CEILING(size);

	malloc_mutex_lock(&base_mtx);
	/* Make sure there's enough space for the allocation. */
	if ((uintptr_t)base_next_addr + csize > (uintptr_t)base_past_addr) {
		if (base_pages_alloc(csize)) {
			malloc_mutex_unlock(&base_mtx);
			return nullptr;
		}
	}
	/* Allocate. */
	ret = base_next_addr;
	base_next_addr = (void *)((uintptr_t)base_next_addr + csize);
	/* Make sure enough pages are committed for the new allocation. */
	if ((uintptr_t)base_next_addr > (uintptr_t)base_next_decommitted) {
		void *pbase_next_addr =
		    (void *)(PAGE_CEILING((uintptr_t)base_next_addr));

#  ifdef MALLOC_DECOMMIT
		pages_commit(base_next_decommitted, (uintptr_t)pbase_next_addr -
		    (uintptr_t)base_next_decommitted);
#  endif
		base_next_decommitted = pbase_next_addr;
		base_committed += (uintptr_t)pbase_next_addr -
		    (uintptr_t)base_next_decommitted;
	}
	malloc_mutex_unlock(&base_mtx);

	return (ret);
}

static void *
base_calloc(size_t number, size_t size)
{
	void *ret;

	ret = base_alloc(number * size);
	memset(ret, 0, number * size);

	return (ret);
}

static extent_node_t *
base_node_alloc(void)
{
	extent_node_t *ret;

	malloc_mutex_lock(&base_mtx);
	if (base_nodes) {
		ret = base_nodes;
		base_nodes = *(extent_node_t **)ret;
		malloc_mutex_unlock(&base_mtx);
	} else {
		malloc_mutex_unlock(&base_mtx);
		ret = (extent_node_t *)base_alloc(sizeof(extent_node_t));
	}

	return (ret);
}

static void
base_node_dealloc(extent_node_t *node)
{

	malloc_mutex_lock(&base_mtx);
	*(extent_node_t **)node = base_nodes;
	base_nodes = node;
	malloc_mutex_unlock(&base_mtx);
}

/*
 * End Utility functions/macros.
 */
/******************************************************************************/
/*
 * Begin extent tree code.
 */

static inline int
extent_szad_comp(extent_node_t *a, extent_node_t *b)
{
	int ret;
	size_t a_size = a->size;
	size_t b_size = b->size;

	ret = (a_size > b_size) - (a_size < b_size);
	if (ret == 0) {
		uintptr_t a_addr = (uintptr_t)a->addr;
		uintptr_t b_addr = (uintptr_t)b->addr;

		ret = (a_addr > b_addr) - (a_addr < b_addr);
	}

	return (ret);
}

/* Wrap red-black tree macros in functions. */
rb_wrap(static, extent_tree_szad_, extent_tree_t, extent_node_t,
    link_szad, extent_szad_comp)

static inline int
extent_ad_comp(extent_node_t *a, extent_node_t *b)
{
	uintptr_t a_addr = (uintptr_t)a->addr;
	uintptr_t b_addr = (uintptr_t)b->addr;

	return ((a_addr > b_addr) - (a_addr < b_addr));
}

/* Wrap red-black tree macros in functions. */
rb_wrap(static, extent_tree_ad_, extent_tree_t, extent_node_t, link_ad,
    extent_ad_comp)

/*
 * End extent tree code.
 */
/******************************************************************************/
/*
 * Begin chunk management functions.
 */

#ifdef MOZ_MEMORY_WINDOWS

static void *
pages_map(void *addr, size_t size)
{
	void *ret = nullptr;
	ret = VirtualAlloc(addr, size, MEM_COMMIT | MEM_RESERVE,
	    PAGE_READWRITE);
	return (ret);
}

static void
pages_unmap(void *addr, size_t size)
{
	if (VirtualFree(addr, 0, MEM_RELEASE) == 0) {
		_malloc_message(_getprogname(),
		    ": (malloc) Error in VirtualFree()\n", "", "");
		if (opt_abort)
			moz_abort();
	}
}
#else

static void *
pages_map(void *addr, size_t size)
{
	void *ret;
#if defined(__ia64__) || (defined(__sparc__) && defined(__arch64__) && defined(__linux__))
        /*
         * The JS engine assumes that all allocated pointers have their high 17 bits clear,
         * which ia64's mmap doesn't support directly. However, we can emulate it by passing
         * mmap an "addr" parameter with those bits clear. The mmap will return that address,
         * or the nearest available memory above that address, providing a near-guarantee
         * that those bits are clear. If they are not, we return nullptr below to indicate
         * out-of-memory.
         *
         * The addr is chosen as 0x0000070000000000, which still allows about 120TB of virtual
         * address space.
         *
         * See Bug 589735 for more information.
         */
	bool check_placement = true;
        if (!addr) {
		addr = (void*)0x0000070000000000;
		check_placement = false;
	}
#endif

#if defined(__sparc__) && defined(__arch64__) && defined(__linux__)
    const uintptr_t start = 0x0000070000000000ULL;
    const uintptr_t end   = 0x0000800000000000ULL;

    /* Copied from js/src/gc/Memory.cpp and adapted for this source */

    uintptr_t hint;
    void* region = MAP_FAILED;
    for (hint = start; region == MAP_FAILED && hint + size <= end; hint += chunksize) {
           region = mmap((void*)hint, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
           if (region != MAP_FAILED) {
                   if (((size_t) region + (size - 1)) & 0xffff800000000000) {
                           if (munmap(region, size)) {
                                   MOZ_ASSERT(errno == ENOMEM);
                           }
                           region = MAP_FAILED;
                   }
           }
    }
    ret = region;
#else

	/*
	 * We don't use MAP_FIXED here, because it can cause the *replacement*
	 * of existing mappings, and we only want to create new mappings.
	 */
	ret = mmap(addr, size, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANON, -1, 0);
	MOZ_ASSERT(ret);
#endif
	if (ret == MAP_FAILED) {
		ret = nullptr;
	}
#if defined(__ia64__) || (defined(__sparc__) && defined(__arch64__) && defined(__linux__))
        /*
         * If the allocated memory doesn't have its upper 17 bits clear, consider it
         * as out of memory.
        */
        else if ((long long)ret & 0xffff800000000000) {
		munmap(ret, size);
                ret = nullptr;
        }
        /* If the caller requested a specific memory location, verify that's what mmap returned. */
	else if (check_placement && ret != addr) {
#else
	else if (addr && ret != addr) {
#endif
		/*
		 * We succeeded in mapping memory, but not in the right place.
		 */
		if (munmap(ret, size) == -1) {
			char buf[STRERROR_BUF];

			if (strerror_r(errno, buf, sizeof(buf)) == 0) {
				_malloc_message(_getprogname(),
					": (malloc) Error in munmap(): ", buf, "\n");
			}
			if (opt_abort)
				moz_abort();
		}
		ret = nullptr;
	}
	if (ret) {
		MozTagAnonymousMemory(ret, size, "jemalloc");
	}

#if defined(__ia64__) || (defined(__sparc__) && defined(__arch64__) && defined(__linux__))
	MOZ_ASSERT(!ret || (!check_placement && ret)
	    || (check_placement && ret == addr));
#else
	MOZ_ASSERT(!ret || (!addr && ret != addr)
	    || (addr && ret == addr));
#endif
	return (ret);
}

static void
pages_unmap(void *addr, size_t size)
{

	if (munmap(addr, size) == -1) {
		char buf[STRERROR_BUF];

		if (strerror_r(errno, buf, sizeof(buf)) == 0) {
			_malloc_message(_getprogname(),
				": (malloc) Error in munmap(): ", buf, "\n");
		}
		if (opt_abort)
			moz_abort();
	}
}
#endif

#ifdef MOZ_MEMORY_DARWIN
#define	VM_COPY_MIN (pagesize << 5)
static inline void
pages_copy(void *dest, const void *src, size_t n)
{

	MOZ_ASSERT((void *)((uintptr_t)dest & ~pagesize_mask) == dest);
	MOZ_ASSERT(n >= VM_COPY_MIN);
	MOZ_ASSERT((void *)((uintptr_t)src & ~pagesize_mask) == src);

	vm_copy(mach_task_self(), (vm_address_t)src, (vm_size_t)n,
	    (vm_address_t)dest);
}
#endif

static inline malloc_rtree_t *
malloc_rtree_new(unsigned bits)
{
	malloc_rtree_t *ret;
	unsigned bits_per_level, height, i;

	bits_per_level = ffs(pow2_ceil((MALLOC_RTREE_NODESIZE /
	    sizeof(void *)))) - 1;
	height = bits / bits_per_level;
	if (height * bits_per_level != bits)
		height++;
	MOZ_DIAGNOSTIC_ASSERT(height * bits_per_level >= bits);

	ret = (malloc_rtree_t*)base_calloc(1, sizeof(malloc_rtree_t) +
	    (sizeof(unsigned) * (height - 1)));
	if (!ret)
		return nullptr;

	malloc_spin_init(&ret->lock);
	ret->height = height;
	if (bits_per_level * height > bits)
		ret->level2bits[0] = bits % bits_per_level;
	else
		ret->level2bits[0] = bits_per_level;
	for (i = 1; i < height; i++)
		ret->level2bits[i] = bits_per_level;

	ret->root = (void**)base_calloc(1, sizeof(void *) << ret->level2bits[0]);
	if (!ret->root) {
		/*
		 * We leak the rtree here, since there's no generic base
		 * deallocation.
		 */
		return nullptr;
	}

	return (ret);
}

#define	MALLOC_RTREE_GET_GENERATE(f)					\
/* The least significant bits of the key are ignored. */		\
static inline void *							\
f(malloc_rtree_t *rtree, uintptr_t key)					\
{									\
	void *ret;							\
	uintptr_t subkey;						\
	unsigned i, lshift, height, bits;				\
	void **node, **child;						\
									\
	MALLOC_RTREE_LOCK(&rtree->lock);				\
	for (i = lshift = 0, height = rtree->height, node = rtree->root;\
	    i < height - 1;						\
	    i++, lshift += bits, node = child) {			\
		bits = rtree->level2bits[i];				\
		subkey = (key << lshift) >> ((SIZEOF_PTR << 3) - bits);	\
		child = (void**)node[subkey];				\
		if (!child) {						\
			MALLOC_RTREE_UNLOCK(&rtree->lock);		\
			return nullptr;					\
		}							\
	}								\
									\
	/*								\
	 * node is a leaf, so it contains values rather than node	\
	 * pointers.							\
	 */								\
	bits = rtree->level2bits[i];					\
	subkey = (key << lshift) >> ((SIZEOF_PTR << 3) - bits);		\
	ret = node[subkey];						\
	MALLOC_RTREE_UNLOCK(&rtree->lock);				\
									\
	MALLOC_RTREE_GET_VALIDATE					\
	return (ret);							\
}

#ifdef MOZ_DEBUG
#  define MALLOC_RTREE_LOCK(l)		malloc_spin_lock(l)
#  define MALLOC_RTREE_UNLOCK(l)	malloc_spin_unlock(l)
#  define MALLOC_RTREE_GET_VALIDATE
MALLOC_RTREE_GET_GENERATE(malloc_rtree_get_locked)
#  undef MALLOC_RTREE_LOCK
#  undef MALLOC_RTREE_UNLOCK
#  undef MALLOC_RTREE_GET_VALIDATE
#endif

#define	MALLOC_RTREE_LOCK(l)
#define	MALLOC_RTREE_UNLOCK(l)
#ifdef MOZ_DEBUG
   /*
    * Suppose that it were possible for a jemalloc-allocated chunk to be
    * munmap()ped, followed by a different allocator in another thread re-using
    * overlapping virtual memory, all without invalidating the cached rtree
    * value.  The result would be a false positive (the rtree would claim that
    * jemalloc owns memory that it had actually discarded).  I don't think this
    * scenario is possible, but the following assertion is a prudent sanity
    * check.
    */
#  define MALLOC_RTREE_GET_VALIDATE					\
	MOZ_ASSERT(malloc_rtree_get_locked(rtree, key) == ret);
#else
#  define MALLOC_RTREE_GET_VALIDATE
#endif
MALLOC_RTREE_GET_GENERATE(malloc_rtree_get)
#undef MALLOC_RTREE_LOCK
#undef MALLOC_RTREE_UNLOCK
#undef MALLOC_RTREE_GET_VALIDATE

static inline bool
malloc_rtree_set(malloc_rtree_t *rtree, uintptr_t key, void *val)
{
	uintptr_t subkey;
	unsigned i, lshift, height, bits;
	void **node, **child;

	malloc_spin_lock(&rtree->lock);
	for (i = lshift = 0, height = rtree->height, node = rtree->root;
	    i < height - 1;
	    i++, lshift += bits, node = child) {
		bits = rtree->level2bits[i];
		subkey = (key << lshift) >> ((SIZEOF_PTR << 3) - bits);
		child = (void**)node[subkey];
		if (!child) {
			child = (void**)base_calloc(1, sizeof(void *) <<
			    rtree->level2bits[i+1]);
			if (!child) {
				malloc_spin_unlock(&rtree->lock);
				return (true);
			}
			node[subkey] = child;
		}
	}

	/* node is a leaf, so it contains values rather than node pointers. */
	bits = rtree->level2bits[i];
	subkey = (key << lshift) >> ((SIZEOF_PTR << 3) - bits);
	node[subkey] = val;
	malloc_spin_unlock(&rtree->lock);

	return (false);
}

/* pages_trim, chunk_alloc_mmap_slow and chunk_alloc_mmap were cherry-picked
 * from upstream jemalloc 3.4.1 to fix Mozilla bug 956501. */

/* Return the offset between a and the nearest aligned address at or below a. */
#define        ALIGNMENT_ADDR2OFFSET(a, alignment)                                \
        ((size_t)((uintptr_t)(a) & (alignment - 1)))

/* Return the smallest alignment multiple that is >= s. */
#define        ALIGNMENT_CEILING(s, alignment)                                        \
        (((s) + (alignment - 1)) & (~(alignment - 1)))

static void *
pages_trim(void *addr, size_t alloc_size, size_t leadsize, size_t size)
{
        void *ret = (void *)((uintptr_t)addr + leadsize);

        MOZ_ASSERT(alloc_size >= leadsize + size);
#ifdef MOZ_MEMORY_WINDOWS
        {
                void *new_addr;

                pages_unmap(addr, alloc_size);
                new_addr = pages_map(ret, size);
                if (new_addr == ret)
                        return (ret);
                if (new_addr)
                        pages_unmap(new_addr, size);
                return nullptr;
        }
#else
        {
                size_t trailsize = alloc_size - leadsize - size;

                if (leadsize != 0)
                        pages_unmap(addr, leadsize);
                if (trailsize != 0)
                        pages_unmap((void *)((uintptr_t)ret + size), trailsize);
                return (ret);
        }
#endif
}

static void *
chunk_alloc_mmap_slow(size_t size, size_t alignment)
{
        void *ret, *pages;
        size_t alloc_size, leadsize;

        alloc_size = size + alignment - pagesize;
        /* Beware size_t wrap-around. */
        if (alloc_size < size)
                return nullptr;
        do {
                pages = pages_map(nullptr, alloc_size);
                if (!pages)
                        return nullptr;
                leadsize = ALIGNMENT_CEILING((uintptr_t)pages, alignment) -
                        (uintptr_t)pages;
                ret = pages_trim(pages, alloc_size, leadsize, size);
        } while (!ret);

        MOZ_ASSERT(ret);
        return (ret);
}

static void *
chunk_alloc_mmap(size_t size, size_t alignment)
{
        void *ret;
        size_t offset;

        /*
         * Ideally, there would be a way to specify alignment to mmap() (like
         * NetBSD has), but in the absence of such a feature, we have to work
         * hard to efficiently create aligned mappings. The reliable, but
         * slow method is to create a mapping that is over-sized, then trim the
         * excess. However, that always results in one or two calls to
         * pages_unmap().
         *
         * Optimistically try mapping precisely the right amount before falling
         * back to the slow method, with the expectation that the optimistic
         * approach works most of the time.
         */

        ret = pages_map(nullptr, size);
        if (!ret)
                return nullptr;
        offset = ALIGNMENT_ADDR2OFFSET(ret, alignment);
        if (offset != 0) {
                pages_unmap(ret, size);
                return (chunk_alloc_mmap_slow(size, alignment));
        }

        MOZ_ASSERT(ret);
        return (ret);
}

/* Purge and release the pages in the chunk of length `length` at `addr` to
 * the OS.
 * Returns whether the pages are guaranteed to be full of zeroes when the
 * function returns.
 * The force_zero argument explicitly requests that the memory is guaranteed
 * to be full of zeroes when the function returns.
 */
static bool
pages_purge(void *addr, size_t length, bool force_zero)
{
#ifdef MALLOC_DECOMMIT
	pages_decommit(addr, length);
	return true;
#else
#  ifndef MOZ_MEMORY_LINUX
	if (force_zero)
		memset(addr, 0, length);
#  endif
#  ifdef MOZ_MEMORY_WINDOWS
	/*
	* The region starting at addr may have been allocated in multiple calls
	* to VirtualAlloc and recycled, so resetting the entire region in one
	* go may not be valid. However, since we allocate at least a chunk at a
	* time, we may touch any region in chunksized increments.
	*/
	size_t pages_size = std::min(length, chunksize -
		CHUNK_ADDR2OFFSET((uintptr_t)addr));
	while (length > 0) {
		VirtualAlloc(addr, pages_size, MEM_RESET, PAGE_READWRITE);
		addr = (void *)((uintptr_t)addr + pages_size);
		length -= pages_size;
		pages_size = std::min(length, chunksize);
	}
	return force_zero;
#  else
#    ifdef MOZ_MEMORY_LINUX
#      define JEMALLOC_MADV_PURGE MADV_DONTNEED
#      define JEMALLOC_MADV_ZEROS true
#    else /* FreeBSD and Darwin. */
#      define JEMALLOC_MADV_PURGE MADV_FREE
#      define JEMALLOC_MADV_ZEROS force_zero
#    endif
	int err = madvise(addr, length, JEMALLOC_MADV_PURGE);
	return JEMALLOC_MADV_ZEROS && err == 0;
#    undef JEMALLOC_MADV_PURGE
#    undef JEMALLOC_MADV_ZEROS
#  endif
#endif
}

static void *
chunk_recycle(extent_tree_t *chunks_szad, extent_tree_t *chunks_ad, size_t size,
    size_t alignment, bool base, bool *zero)
{
	void *ret;
	extent_node_t *node;
	extent_node_t key;
	size_t alloc_size, leadsize, trailsize;
	ChunkType chunk_type;

	if (base) {
		/*
		 * This function may need to call base_node_{,de}alloc(), but
		 * the current chunk allocation request is on behalf of the
		 * base allocator.  Avoid deadlock (and if that weren't an
		 * issue, potential for infinite recursion) by returning nullptr.
		 */
		return nullptr;
	}

	alloc_size = size + alignment - chunksize;
	/* Beware size_t wrap-around. */
	if (alloc_size < size)
		return nullptr;
	key.addr = nullptr;
	key.size = alloc_size;
	malloc_mutex_lock(&chunks_mtx);
	node = extent_tree_szad_nsearch(chunks_szad, &key);
	if (!node) {
		malloc_mutex_unlock(&chunks_mtx);
		return nullptr;
	}
	leadsize = ALIGNMENT_CEILING((uintptr_t)node->addr, alignment) -
	    (uintptr_t)node->addr;
	MOZ_ASSERT(node->size >= leadsize + size);
	trailsize = node->size - leadsize - size;
	ret = (void *)((uintptr_t)node->addr + leadsize);
	chunk_type = node->chunk_type;
	if (chunk_type == ZEROED_CHUNK)
	    *zero = true;
	/* Remove node from the tree. */
	extent_tree_szad_remove(chunks_szad, node);
	extent_tree_ad_remove(chunks_ad, node);
	if (leadsize != 0) {
		/* Insert the leading space as a smaller chunk. */
		node->size = leadsize;
		extent_tree_szad_insert(chunks_szad, node);
		extent_tree_ad_insert(chunks_ad, node);
		node = nullptr;
	}
	if (trailsize != 0) {
		/* Insert the trailing space as a smaller chunk. */
		if (!node) {
			/*
			 * An additional node is required, but
			 * base_node_alloc() can cause a new base chunk to be
			 * allocated.  Drop chunks_mtx in order to avoid
			 * deadlock, and if node allocation fails, deallocate
			 * the result before returning an error.
			 */
			malloc_mutex_unlock(&chunks_mtx);
			node = base_node_alloc();
			if (!node) {
				chunk_dealloc(ret, size, chunk_type);
				return nullptr;
			}
			malloc_mutex_lock(&chunks_mtx);
		}
		node->addr = (void *)((uintptr_t)(ret) + size);
		node->size = trailsize;
		node->chunk_type = chunk_type;
		extent_tree_szad_insert(chunks_szad, node);
		extent_tree_ad_insert(chunks_ad, node);
		node = nullptr;
	}

	recycled_size -= size;

	malloc_mutex_unlock(&chunks_mtx);

	if (node)
		base_node_dealloc(node);
#ifdef MALLOC_DECOMMIT
	pages_commit(ret, size);
#endif
	if (*zero) {
		if (chunk_type != ZEROED_CHUNK)
			memset(ret, 0, size);
#ifdef DEBUG
		else {
			size_t i;
			size_t *p = (size_t *)(uintptr_t)ret;

			for (i = 0; i < size / sizeof(size_t); i++)
				MOZ_ASSERT(p[i] == 0);
		}
#endif
	}
	return (ret);
}

#ifdef MOZ_MEMORY_WINDOWS
/*
 * On Windows, calls to VirtualAlloc and VirtualFree must be matched, making it
 * awkward to recycle allocations of varying sizes. Therefore we only allow
 * recycling when the size equals the chunksize, unless deallocation is entirely
 * disabled.
 */
#define CAN_RECYCLE(size) (size == chunksize)
#else
#define CAN_RECYCLE(size) true
#endif

static void *
chunk_alloc(size_t size, size_t alignment, bool base, bool zero)
{
	void *ret;

	MOZ_ASSERT(size != 0);
	MOZ_ASSERT((size & chunksize_mask) == 0);
	MOZ_ASSERT(alignment != 0);
	MOZ_ASSERT((alignment & chunksize_mask) == 0);

	if (CAN_RECYCLE(size)) {
		ret = chunk_recycle(&chunks_szad_mmap, &chunks_ad_mmap,
			size, alignment, base, &zero);
		if (ret)
			goto RETURN;
	}
	ret = chunk_alloc_mmap(size, alignment);
	if (ret) {
		goto RETURN;
	}

	/* All strategies for allocation failed. */
	ret = nullptr;
RETURN:

	if (ret && base == false) {
		if (malloc_rtree_set(chunk_rtree, (uintptr_t)ret, ret)) {
			chunk_dealloc(ret, size, UNKNOWN_CHUNK);
			return nullptr;
		}
	}

	MOZ_ASSERT(CHUNK_ADDR2BASE(ret) == ret);
	return (ret);
}

static void
chunk_record(extent_tree_t *chunks_szad, extent_tree_t *chunks_ad, void *chunk,
    size_t size, ChunkType chunk_type)
{
	extent_node_t *xnode, *node, *prev, *xprev, key;

	if (chunk_type != ZEROED_CHUNK) {
		if (pages_purge(chunk, size, chunk_type == HUGE_CHUNK)) {
			chunk_type = ZEROED_CHUNK;
		}
	}

	/*
	 * Allocate a node before acquiring chunks_mtx even though it might not
	 * be needed, because base_node_alloc() may cause a new base chunk to
	 * be allocated, which could cause deadlock if chunks_mtx were already
	 * held.
	 */
	xnode = base_node_alloc();
	/* Use xprev to implement conditional deferred deallocation of prev. */
	xprev = nullptr;

	malloc_mutex_lock(&chunks_mtx);
	key.addr = (void *)((uintptr_t)chunk + size);
	node = extent_tree_ad_nsearch(chunks_ad, &key);
	/* Try to coalesce forward. */
	if (node && node->addr == key.addr) {
		/*
		 * Coalesce chunk with the following address range.  This does
		 * not change the position within chunks_ad, so only
		 * remove/insert from/into chunks_szad.
		 */
		extent_tree_szad_remove(chunks_szad, node);
		node->addr = chunk;
		node->size += size;
		if (node->chunk_type != chunk_type) {
			node->chunk_type = RECYCLED_CHUNK;
		}
		extent_tree_szad_insert(chunks_szad, node);
	} else {
		/* Coalescing forward failed, so insert a new node. */
		if (!xnode) {
			/*
			 * base_node_alloc() failed, which is an exceedingly
			 * unlikely failure.  Leak chunk; its pages have
			 * already been purged, so this is only a virtual
			 * memory leak.
			 */
			goto label_return;
		}
		node = xnode;
		xnode = nullptr; /* Prevent deallocation below. */
		node->addr = chunk;
		node->size = size;
		node->chunk_type = chunk_type;
		extent_tree_ad_insert(chunks_ad, node);
		extent_tree_szad_insert(chunks_szad, node);
	}

	/* Try to coalesce backward. */
	prev = extent_tree_ad_prev(chunks_ad, node);
	if (prev && (void *)((uintptr_t)prev->addr + prev->size) ==
	    chunk) {
		/*
		 * Coalesce chunk with the previous address range.  This does
		 * not change the position within chunks_ad, so only
		 * remove/insert node from/into chunks_szad.
		 */
		extent_tree_szad_remove(chunks_szad, prev);
		extent_tree_ad_remove(chunks_ad, prev);

		extent_tree_szad_remove(chunks_szad, node);
		node->addr = prev->addr;
		node->size += prev->size;
		if (node->chunk_type != prev->chunk_type) {
			node->chunk_type = RECYCLED_CHUNK;
		}
		extent_tree_szad_insert(chunks_szad, node);

		xprev = prev;
	}

	recycled_size += size;

label_return:
	malloc_mutex_unlock(&chunks_mtx);
	/*
	 * Deallocate xnode and/or xprev after unlocking chunks_mtx in order to
	 * avoid potential deadlock.
	 */
	if (xnode)
		base_node_dealloc(xnode);
	if (xprev)
		base_node_dealloc(xprev);
}

static bool
chunk_dalloc_mmap(void *chunk, size_t size)
{
	if (CAN_RECYCLE(size) && load_acquire_z(&recycled_size) < recycle_limit)
		return true;

	pages_unmap(chunk, size);
	return false;
}

#undef CAN_RECYCLE

static void
chunk_dealloc(void *chunk, size_t size, ChunkType type)
{

	MOZ_ASSERT(chunk);
	MOZ_ASSERT(CHUNK_ADDR2BASE(chunk) == chunk);
	MOZ_ASSERT(size != 0);
	MOZ_ASSERT((size & chunksize_mask) == 0);

	malloc_rtree_set(chunk_rtree, (uintptr_t)chunk, nullptr);

	if (chunk_dalloc_mmap(chunk, size))
		chunk_record(&chunks_szad_mmap, &chunks_ad_mmap, chunk, size, type);
}

/*
 * End chunk management functions.
 */
/******************************************************************************/
/*
 * Begin arena.
 */

static inline arena_t *
thread_local_arena(bool enabled)
{
#ifndef NO_TLS
	arena_t *arena;

	if (enabled) {
		/* The arena will essentially be leaked if this function is
		 * called with `false`, but it doesn't matter at the moment.
		 * because in practice nothing actually calls this function
		 * with `false`, except maybe at shutdown. */
		arena = arenas_extend();
	} else {
		malloc_spin_lock(&arenas_lock);
		arena = arenas[0];
		malloc_spin_unlock(&arenas_lock);
	}
#ifdef MOZ_MEMORY_WINDOWS
	TlsSetValue(tlsIndex, arena);
#elif defined(MOZ_MEMORY_DARWIN)
	pthread_setspecific(tlsIndex, arena);
#else
	arenas_map = arena;
#endif

	return arena;
#else
	return arenas[0];
#endif
}

MOZ_JEMALLOC_API void
jemalloc_thread_local_arena_impl(bool enabled)
{
	thread_local_arena(enabled);
}

/*
 * Choose an arena based on a per-thread value.
 */
static inline arena_t *
choose_arena(void)
{
	arena_t *ret;

	/*
	 * We can only use TLS if this is a PIC library, since for the static
	 * library version, libc's malloc is used by TLS allocation, which
	 * introduces a bootstrapping issue.
	 */
#ifndef NO_TLS

#  ifdef MOZ_MEMORY_WINDOWS
	ret = (arena_t*)TlsGetValue(tlsIndex);
#  elif defined(MOZ_MEMORY_DARWIN)
	ret = (arena_t*)pthread_getspecific(tlsIndex);
#  else
	ret = arenas_map;
#  endif

	if (!ret) {
                ret = thread_local_arena(false);
	}
#else
	ret = arenas[0];
#endif
	MOZ_DIAGNOSTIC_ASSERT(ret);
	return (ret);
}

static inline int
arena_chunk_comp(arena_chunk_t *a, arena_chunk_t *b)
{
	uintptr_t a_chunk = (uintptr_t)a;
	uintptr_t b_chunk = (uintptr_t)b;

	MOZ_ASSERT(a);
	MOZ_ASSERT(b);

	return ((a_chunk > b_chunk) - (a_chunk < b_chunk));
}

/* Wrap red-black tree macros in functions. */
rb_wrap(static, arena_chunk_tree_dirty_, arena_chunk_tree_t,
    arena_chunk_t, link_dirty, arena_chunk_comp)

static inline int
arena_run_comp(arena_chunk_map_t *a, arena_chunk_map_t *b)
{
	uintptr_t a_mapelm = (uintptr_t)a;
	uintptr_t b_mapelm = (uintptr_t)b;

	MOZ_ASSERT(a);
	MOZ_ASSERT(b);

	return ((a_mapelm > b_mapelm) - (a_mapelm < b_mapelm));
}

/* Wrap red-black tree macros in functions. */
rb_wrap(static, arena_run_tree_, arena_run_tree_t, arena_chunk_map_t, link,
    arena_run_comp)

static inline int
arena_avail_comp(arena_chunk_map_t *a, arena_chunk_map_t *b)
{
	int ret;
	size_t a_size = a->bits & ~pagesize_mask;
	size_t b_size = b->bits & ~pagesize_mask;

	ret = (a_size > b_size) - (a_size < b_size);
	if (ret == 0) {
		uintptr_t a_mapelm, b_mapelm;

		if ((a->bits & CHUNK_MAP_KEY) == 0)
			a_mapelm = (uintptr_t)a;
		else {
			/*
			 * Treat keys as though they are lower than anything
			 * else.
			 */
			a_mapelm = 0;
		}
		b_mapelm = (uintptr_t)b;

		ret = (a_mapelm > b_mapelm) - (a_mapelm < b_mapelm);
	}

	return (ret);
}

/* Wrap red-black tree macros in functions. */
rb_wrap(static, arena_avail_tree_, arena_avail_tree_t, arena_chunk_map_t, link,
    arena_avail_comp)

static inline void *
arena_run_reg_alloc(arena_run_t *run, arena_bin_t *bin)
{
	void *ret;
	unsigned i, mask, bit, regind;

	MOZ_ASSERT(run->magic == ARENA_RUN_MAGIC);
	MOZ_ASSERT(run->regs_minelm < bin->regs_mask_nelms);

	/*
	 * Move the first check outside the loop, so that run->regs_minelm can
	 * be updated unconditionally, without the possibility of updating it
	 * multiple times.
	 */
	i = run->regs_minelm;
	mask = run->regs_mask[i];
	if (mask != 0) {
		/* Usable allocation found. */
		bit = ffs((int)mask) - 1;

		regind = ((i << (SIZEOF_INT_2POW + 3)) + bit);
		MOZ_ASSERT(regind < bin->nregs);
		ret = (void *)(((uintptr_t)run) + bin->reg0_offset
		    + (bin->reg_size * regind));

		/* Clear bit. */
		mask ^= (1U << bit);
		run->regs_mask[i] = mask;

		return (ret);
	}

	for (i++; i < bin->regs_mask_nelms; i++) {
		mask = run->regs_mask[i];
		if (mask != 0) {
			/* Usable allocation found. */
			bit = ffs((int)mask) - 1;

			regind = ((i << (SIZEOF_INT_2POW + 3)) + bit);
			MOZ_ASSERT(regind < bin->nregs);
			ret = (void *)(((uintptr_t)run) + bin->reg0_offset
			    + (bin->reg_size * regind));

			/* Clear bit. */
			mask ^= (1U << bit);
			run->regs_mask[i] = mask;

			/*
			 * Make a note that nothing before this element
			 * contains a free region.
			 */
			run->regs_minelm = i; /* Low payoff: + (mask == 0); */

			return (ret);
		}
	}
	/* Not reached. */
	MOZ_DIAGNOSTIC_ASSERT(0);
	return nullptr;
}

static inline void
arena_run_reg_dalloc(arena_run_t *run, arena_bin_t *bin, void *ptr, size_t size)
{
	/*
	 * To divide by a number D that is not a power of two we multiply
	 * by (2^21 / D) and then right shift by 21 positions.
	 *
	 *   X / D
	 *
	 * becomes
	 *
	 *   (X * size_invs[(D >> QUANTUM_2POW_MIN) - 3]) >> SIZE_INV_SHIFT
	 */
#define	SIZE_INV_SHIFT 21
#define	SIZE_INV(s) (((1U << SIZE_INV_SHIFT) / (s << QUANTUM_2POW_MIN)) + 1)
	static const unsigned size_invs[] = {
	    SIZE_INV(3),
	    SIZE_INV(4), SIZE_INV(5), SIZE_INV(6), SIZE_INV(7),
	    SIZE_INV(8), SIZE_INV(9), SIZE_INV(10), SIZE_INV(11),
	    SIZE_INV(12),SIZE_INV(13), SIZE_INV(14), SIZE_INV(15),
	    SIZE_INV(16),SIZE_INV(17), SIZE_INV(18), SIZE_INV(19),
	    SIZE_INV(20),SIZE_INV(21), SIZE_INV(22), SIZE_INV(23),
	    SIZE_INV(24),SIZE_INV(25), SIZE_INV(26), SIZE_INV(27),
	    SIZE_INV(28),SIZE_INV(29), SIZE_INV(30), SIZE_INV(31)
#if (QUANTUM_2POW_MIN < 4)
	    ,
	    SIZE_INV(32), SIZE_INV(33), SIZE_INV(34), SIZE_INV(35),
	    SIZE_INV(36), SIZE_INV(37), SIZE_INV(38), SIZE_INV(39),
	    SIZE_INV(40), SIZE_INV(41), SIZE_INV(42), SIZE_INV(43),
	    SIZE_INV(44), SIZE_INV(45), SIZE_INV(46), SIZE_INV(47),
	    SIZE_INV(48), SIZE_INV(49), SIZE_INV(50), SIZE_INV(51),
	    SIZE_INV(52), SIZE_INV(53), SIZE_INV(54), SIZE_INV(55),
	    SIZE_INV(56), SIZE_INV(57), SIZE_INV(58), SIZE_INV(59),
	    SIZE_INV(60), SIZE_INV(61), SIZE_INV(62), SIZE_INV(63)
#endif
	};
	unsigned diff, regind, elm, bit;

	MOZ_ASSERT(run->magic == ARENA_RUN_MAGIC);
	MOZ_ASSERT(((sizeof(size_invs)) / sizeof(unsigned)) + 3
	    >= (SMALL_MAX_DEFAULT >> QUANTUM_2POW_MIN));

	/*
	 * Avoid doing division with a variable divisor if possible.  Using
	 * actual division here can reduce allocator throughput by over 20%!
	 */
	diff = (unsigned)((uintptr_t)ptr - (uintptr_t)run - bin->reg0_offset);
	if ((size & (size - 1)) == 0) {
		/*
		 * log2_table allows fast division of a power of two in the
		 * [1..128] range.
		 *
		 * (x / divisor) becomes (x >> log2_table[divisor - 1]).
		 */
		static const unsigned char log2_table[] = {
		    0, 1, 0, 2, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 4,
		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5,
		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6,
		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7
		};

		if (size <= 128)
			regind = (diff >> log2_table[size - 1]);
		else if (size <= 32768)
			regind = diff >> (8 + log2_table[(size >> 8) - 1]);
		else {
			/*
			 * The run size is too large for us to use the lookup
			 * table.  Use real division.
			 */
			regind = diff / size;
		}
	} else if (size <= ((sizeof(size_invs) / sizeof(unsigned))
	    << QUANTUM_2POW_MIN) + 2) {
		regind = size_invs[(size >> QUANTUM_2POW_MIN) - 3] * diff;
		regind >>= SIZE_INV_SHIFT;
	} else {
		/*
		 * size_invs isn't large enough to handle this size class, so
		 * calculate regind using actual division.  This only happens
		 * if the user increases small_max via the 'S' runtime
		 * configuration option.
		 */
		regind = diff / size;
	};
	MOZ_DIAGNOSTIC_ASSERT(diff == regind * size);
	MOZ_DIAGNOSTIC_ASSERT(regind < bin->nregs);

	elm = regind >> (SIZEOF_INT_2POW + 3);
	if (elm < run->regs_minelm)
		run->regs_minelm = elm;
	bit = regind - (elm << (SIZEOF_INT_2POW + 3));
	MOZ_DIAGNOSTIC_ASSERT((run->regs_mask[elm] & (1U << bit)) == 0);
	run->regs_mask[elm] |= (1U << bit);
#undef SIZE_INV
#undef SIZE_INV_SHIFT
}

static void
arena_run_split(arena_t *arena, arena_run_t *run, size_t size, bool large,
    bool zero)
{
	arena_chunk_t *chunk;
	size_t old_ndirty, run_ind, total_pages, need_pages, rem_pages, i;

	chunk = (arena_chunk_t *)CHUNK_ADDR2BASE(run);
	old_ndirty = chunk->ndirty;
	run_ind = (unsigned)(((uintptr_t)run - (uintptr_t)chunk)
	    >> pagesize_2pow);
	total_pages = (chunk->map[run_ind].bits & ~pagesize_mask) >>
	    pagesize_2pow;
	need_pages = (size >> pagesize_2pow);
	MOZ_ASSERT(need_pages > 0);
	MOZ_ASSERT(need_pages <= total_pages);
	rem_pages = total_pages - need_pages;

	arena_avail_tree_remove(&arena->runs_avail, &chunk->map[run_ind]);

	/* Keep track of trailing unused pages for later use. */
	if (rem_pages > 0) {
		chunk->map[run_ind+need_pages].bits = (rem_pages <<
		    pagesize_2pow) | (chunk->map[run_ind+need_pages].bits &
		    pagesize_mask);
		chunk->map[run_ind+total_pages-1].bits = (rem_pages <<
		    pagesize_2pow) | (chunk->map[run_ind+total_pages-1].bits &
		    pagesize_mask);
		arena_avail_tree_insert(&arena->runs_avail,
		    &chunk->map[run_ind+need_pages]);
	}

	for (i = 0; i < need_pages; i++) {
		/*
		 * Commit decommitted pages if necessary.  If a decommitted
		 * page is encountered, commit all needed adjacent decommitted
		 * pages in one operation, in order to reduce system call
		 * overhead.
		 */
		if (chunk->map[run_ind + i].bits & CHUNK_MAP_MADVISED_OR_DECOMMITTED) {
			size_t j;

			/*
			 * Advance i+j to just past the index of the last page
			 * to commit.  Clear CHUNK_MAP_DECOMMITTED and
			 * CHUNK_MAP_MADVISED along the way.
			 */
			for (j = 0; i + j < need_pages && (chunk->map[run_ind +
			    i + j].bits & CHUNK_MAP_MADVISED_OR_DECOMMITTED); j++) {
				/* DECOMMITTED and MADVISED are mutually exclusive. */
				MOZ_ASSERT(!(chunk->map[run_ind + i + j].bits & CHUNK_MAP_DECOMMITTED &&
					     chunk->map[run_ind + i + j].bits & CHUNK_MAP_MADVISED));

				chunk->map[run_ind + i + j].bits &=
				    ~CHUNK_MAP_MADVISED_OR_DECOMMITTED;
			}

#  ifdef MALLOC_DECOMMIT
			pages_commit((void *)((uintptr_t)chunk + ((run_ind + i)
			    << pagesize_2pow)), (j << pagesize_2pow));
			arena->stats.ncommit++;
#  endif

			arena->stats.committed += j;

#  ifndef MALLOC_DECOMMIT
                }
#  else
		} else /* No need to zero since commit zeros. */
#  endif

		/* Zero if necessary. */
		if (zero) {
			if ((chunk->map[run_ind + i].bits & CHUNK_MAP_ZEROED)
			    == 0) {
				memset((void *)((uintptr_t)chunk + ((run_ind
				    + i) << pagesize_2pow)), 0, pagesize);
				/* CHUNK_MAP_ZEROED is cleared below. */
			}
		}

		/* Update dirty page accounting. */
		if (chunk->map[run_ind + i].bits & CHUNK_MAP_DIRTY) {
			chunk->ndirty--;
			arena->ndirty--;
			/* CHUNK_MAP_DIRTY is cleared below. */
		}

		/* Initialize the chunk map. */
		if (large) {
			chunk->map[run_ind + i].bits = CHUNK_MAP_LARGE
			    | CHUNK_MAP_ALLOCATED;
		} else {
			chunk->map[run_ind + i].bits = (size_t)run
			    | CHUNK_MAP_ALLOCATED;
		}
	}

	/*
	 * Set the run size only in the first element for large runs.  This is
	 * primarily a debugging aid, since the lack of size info for trailing
	 * pages only matters if the application tries to operate on an
	 * interior pointer.
	 */
	if (large)
		chunk->map[run_ind].bits |= size;

	if (chunk->ndirty == 0 && old_ndirty > 0)
		arena_chunk_tree_dirty_remove(&arena->chunks_dirty, chunk);
}

static void
arena_chunk_init(arena_t *arena, arena_chunk_t *chunk)
{
	size_t i;

	arena->stats.mapped += chunksize;

	chunk->arena = arena;

	/*
	 * Claim that no pages are in use, since the header is merely overhead.
	 */
	chunk->ndirty = 0;

	/* Initialize the map to contain one maximal free untouched run. */
#ifdef MALLOC_DECOMMIT
	arena_run_t *run = (arena_run_t *)((uintptr_t)chunk +
	                   (arena_chunk_header_npages << pagesize_2pow));
#endif

	for (i = 0; i < arena_chunk_header_npages; i++)
		chunk->map[i].bits = 0;
	chunk->map[i].bits = arena_maxclass | CHUNK_MAP_DECOMMITTED | CHUNK_MAP_ZEROED;
	for (i++; i < chunk_npages-1; i++) {
		chunk->map[i].bits = CHUNK_MAP_DECOMMITTED | CHUNK_MAP_ZEROED;
	}
	chunk->map[chunk_npages-1].bits = arena_maxclass | CHUNK_MAP_DECOMMITTED | CHUNK_MAP_ZEROED;

#ifdef MALLOC_DECOMMIT
	/*
	 * Start out decommitted, in order to force a closer correspondence
	 * between dirty pages and committed untouched pages.
	 */
	pages_decommit(run, arena_maxclass);
	arena->stats.ndecommit++;
	arena->stats.decommitted += (chunk_npages - arena_chunk_header_npages);
#endif
	arena->stats.committed += arena_chunk_header_npages;

	/* Insert the run into the runs_avail tree. */
	arena_avail_tree_insert(&arena->runs_avail,
	    &chunk->map[arena_chunk_header_npages]);

#ifdef MALLOC_DOUBLE_PURGE
	LinkedList_Init(&chunk->chunks_madvised_elem);
#endif
}

static void
arena_chunk_dealloc(arena_t *arena, arena_chunk_t *chunk)
{

	if (arena->spare) {
		if (arena->spare->ndirty > 0) {
			arena_chunk_tree_dirty_remove(
			    &chunk->arena->chunks_dirty, arena->spare);
			arena->ndirty -= arena->spare->ndirty;
			arena->stats.committed -= arena->spare->ndirty;
		}

#ifdef MALLOC_DOUBLE_PURGE
		/* This is safe to do even if arena->spare is not in the list. */
		LinkedList_Remove(&arena->spare->chunks_madvised_elem);
#endif

		chunk_dealloc((void *)arena->spare, chunksize, ARENA_CHUNK);
		arena->stats.mapped -= chunksize;
		arena->stats.committed -= arena_chunk_header_npages;
	}

	/*
	 * Remove run from runs_avail, so that the arena does not use it.
	 * Dirty page flushing only uses the chunks_dirty tree, so leaving this
	 * chunk in the chunks_* trees is sufficient for that purpose.
	 */
	arena_avail_tree_remove(&arena->runs_avail,
	    &chunk->map[arena_chunk_header_npages]);

	arena->spare = chunk;
}

static arena_run_t *
arena_run_alloc(arena_t *arena, arena_bin_t *bin, size_t size, bool large,
    bool zero)
{
	arena_run_t *run;
	arena_chunk_map_t *mapelm, key;

	MOZ_ASSERT(size <= arena_maxclass);
	MOZ_ASSERT((size & pagesize_mask) == 0);

	/* Search the arena's chunks for the lowest best fit. */
	key.bits = size | CHUNK_MAP_KEY;
	mapelm = arena_avail_tree_nsearch(&arena->runs_avail, &key);
	if (mapelm) {
		arena_chunk_t *chunk =
		    (arena_chunk_t*)CHUNK_ADDR2BASE(mapelm);
		size_t pageind = ((uintptr_t)mapelm -
		    (uintptr_t)chunk->map) /
		    sizeof(arena_chunk_map_t);

		run = (arena_run_t *)((uintptr_t)chunk + (pageind
		    << pagesize_2pow));
		arena_run_split(arena, run, size, large, zero);
		return (run);
	}

	if (arena->spare) {
		/* Use the spare. */
		arena_chunk_t *chunk = arena->spare;
		arena->spare = nullptr;
		run = (arena_run_t *)((uintptr_t)chunk +
		    (arena_chunk_header_npages << pagesize_2pow));
		/* Insert the run into the runs_avail tree. */
		arena_avail_tree_insert(&arena->runs_avail,
		    &chunk->map[arena_chunk_header_npages]);
		arena_run_split(arena, run, size, large, zero);
		return (run);
	}

	/*
	 * No usable runs.  Create a new chunk from which to allocate
	 * the run.
	 */
	{
		arena_chunk_t *chunk = (arena_chunk_t *)
		    chunk_alloc(chunksize, chunksize, false, true);
		if (!chunk)
			return nullptr;

		arena_chunk_init(arena, chunk);
		run = (arena_run_t *)((uintptr_t)chunk +
		    (arena_chunk_header_npages << pagesize_2pow));
	}
	/* Update page map. */
	arena_run_split(arena, run, size, large, zero);
	return (run);
}

static void
arena_purge(arena_t *arena, bool all)
{
	arena_chunk_t *chunk;
	size_t i, npages;
	/* If all is set purge all dirty pages. */
	size_t dirty_max = all ? 1 : opt_dirty_max;
#ifdef MOZ_DEBUG
	size_t ndirty = 0;
	rb_foreach_begin(arena_chunk_t, link_dirty, &arena->chunks_dirty,
	    chunk) {
		ndirty += chunk->ndirty;
	} rb_foreach_end(arena_chunk_t, link_dirty, &arena->chunks_dirty, chunk)
	MOZ_ASSERT(ndirty == arena->ndirty);
#endif
	MOZ_DIAGNOSTIC_ASSERT(all || (arena->ndirty > opt_dirty_max));

	arena->stats.npurge++;

	/*
	 * Iterate downward through chunks until enough dirty memory has been
	 * purged.  Terminate as soon as possible in order to minimize the
	 * number of system calls, even if a chunk has only been partially
	 * purged.
	 */
	while (arena->ndirty > (dirty_max >> 1)) {
#ifdef MALLOC_DOUBLE_PURGE
		bool madvised = false;
#endif
		chunk = arena_chunk_tree_dirty_last(&arena->chunks_dirty);
		MOZ_DIAGNOSTIC_ASSERT(chunk);

		for (i = chunk_npages - 1; chunk->ndirty > 0; i--) {
			MOZ_DIAGNOSTIC_ASSERT(i >= arena_chunk_header_npages);

			if (chunk->map[i].bits & CHUNK_MAP_DIRTY) {
#ifdef MALLOC_DECOMMIT
				const size_t free_operation = CHUNK_MAP_DECOMMITTED;
#else
				const size_t free_operation = CHUNK_MAP_MADVISED;
#endif
				MOZ_ASSERT((chunk->map[i].bits &
				            CHUNK_MAP_MADVISED_OR_DECOMMITTED) == 0);
				chunk->map[i].bits ^= free_operation | CHUNK_MAP_DIRTY;
				/* Find adjacent dirty run(s). */
				for (npages = 1;
				     i > arena_chunk_header_npages &&
				       (chunk->map[i - 1].bits & CHUNK_MAP_DIRTY);
				     npages++) {
					i--;
					MOZ_ASSERT((chunk->map[i].bits &
					            CHUNK_MAP_MADVISED_OR_DECOMMITTED) == 0);
					chunk->map[i].bits ^= free_operation | CHUNK_MAP_DIRTY;
				}
				chunk->ndirty -= npages;
				arena->ndirty -= npages;

#ifdef MALLOC_DECOMMIT
				pages_decommit((void *)((uintptr_t)
				    chunk + (i << pagesize_2pow)),
				    (npages << pagesize_2pow));
				arena->stats.ndecommit++;
				arena->stats.decommitted += npages;
#endif
				arena->stats.committed -= npages;

#ifndef MALLOC_DECOMMIT
				madvise((void *)((uintptr_t)chunk + (i <<
				    pagesize_2pow)), (npages << pagesize_2pow),
				    MADV_FREE);
#  ifdef MALLOC_DOUBLE_PURGE
				madvised = true;
#  endif
#endif
				arena->stats.nmadvise++;
				arena->stats.purged += npages;
				if (arena->ndirty <= (dirty_max >> 1))
					break;
			}
		}

		if (chunk->ndirty == 0) {
			arena_chunk_tree_dirty_remove(&arena->chunks_dirty,
			    chunk);
		}
#ifdef MALLOC_DOUBLE_PURGE
		if (madvised) {
			/* The chunk might already be in the list, but this
			 * makes sure it's at the front. */
			LinkedList_Remove(&chunk->chunks_madvised_elem);
			LinkedList_InsertHead(&arena->chunks_madvised, &chunk->chunks_madvised_elem);
		}
#endif
	}
}

static void
arena_run_dalloc(arena_t *arena, arena_run_t *run, bool dirty)
{
	arena_chunk_t *chunk;
	size_t size, run_ind, run_pages;

	chunk = (arena_chunk_t *)CHUNK_ADDR2BASE(run);
	run_ind = (size_t)(((uintptr_t)run - (uintptr_t)chunk)
	    >> pagesize_2pow);
	MOZ_DIAGNOSTIC_ASSERT(run_ind >= arena_chunk_header_npages);
	MOZ_DIAGNOSTIC_ASSERT(run_ind < chunk_npages);
	if ((chunk->map[run_ind].bits & CHUNK_MAP_LARGE) != 0)
		size = chunk->map[run_ind].bits & ~pagesize_mask;
	else
		size = run->bin->run_size;
	run_pages = (size >> pagesize_2pow);

	/* Mark pages as unallocated in the chunk map. */
	if (dirty) {
		size_t i;

		for (i = 0; i < run_pages; i++) {
			MOZ_DIAGNOSTIC_ASSERT((chunk->map[run_ind + i].bits & CHUNK_MAP_DIRTY)
			    == 0);
			chunk->map[run_ind + i].bits = CHUNK_MAP_DIRTY;
		}

		if (chunk->ndirty == 0) {
			arena_chunk_tree_dirty_insert(&arena->chunks_dirty,
			    chunk);
		}
		chunk->ndirty += run_pages;
		arena->ndirty += run_pages;
	} else {
		size_t i;

		for (i = 0; i < run_pages; i++) {
			chunk->map[run_ind + i].bits &= ~(CHUNK_MAP_LARGE |
			    CHUNK_MAP_ALLOCATED);
		}
	}
	chunk->map[run_ind].bits = size | (chunk->map[run_ind].bits &
	    pagesize_mask);
	chunk->map[run_ind+run_pages-1].bits = size |
	    (chunk->map[run_ind+run_pages-1].bits & pagesize_mask);

	/* Try to coalesce forward. */
	if (run_ind + run_pages < chunk_npages &&
	    (chunk->map[run_ind+run_pages].bits & CHUNK_MAP_ALLOCATED) == 0) {
		size_t nrun_size = chunk->map[run_ind+run_pages].bits &
		    ~pagesize_mask;

		/*
		 * Remove successor from runs_avail; the coalesced run is
		 * inserted later.
		 */
		arena_avail_tree_remove(&arena->runs_avail,
		    &chunk->map[run_ind+run_pages]);

		size += nrun_size;
		run_pages = size >> pagesize_2pow;

		MOZ_DIAGNOSTIC_ASSERT((chunk->map[run_ind+run_pages-1].bits & ~pagesize_mask)
		    == nrun_size);
		chunk->map[run_ind].bits = size | (chunk->map[run_ind].bits &
		    pagesize_mask);
		chunk->map[run_ind+run_pages-1].bits = size |
		    (chunk->map[run_ind+run_pages-1].bits & pagesize_mask);
	}

	/* Try to coalesce backward. */
	if (run_ind > arena_chunk_header_npages && (chunk->map[run_ind-1].bits &
	    CHUNK_MAP_ALLOCATED) == 0) {
		size_t prun_size = chunk->map[run_ind-1].bits & ~pagesize_mask;

		run_ind -= prun_size >> pagesize_2pow;

		/*
		 * Remove predecessor from runs_avail; the coalesced run is
		 * inserted later.
		 */
		arena_avail_tree_remove(&arena->runs_avail,
		    &chunk->map[run_ind]);

		size += prun_size;
		run_pages = size >> pagesize_2pow;

		MOZ_DIAGNOSTIC_ASSERT((chunk->map[run_ind].bits & ~pagesize_mask) ==
		    prun_size);
		chunk->map[run_ind].bits = size | (chunk->map[run_ind].bits &
		    pagesize_mask);
		chunk->map[run_ind+run_pages-1].bits = size |
		    (chunk->map[run_ind+run_pages-1].bits & pagesize_mask);
	}

	/* Insert into runs_avail, now that coalescing is complete. */
	arena_avail_tree_insert(&arena->runs_avail, &chunk->map[run_ind]);

	/* Deallocate chunk if it is now completely unused. */
	if ((chunk->map[arena_chunk_header_npages].bits & (~pagesize_mask |
	    CHUNK_MAP_ALLOCATED)) == arena_maxclass)
		arena_chunk_dealloc(arena, chunk);

	/* Enforce opt_dirty_max. */
	if (arena->ndirty > opt_dirty_max)
		arena_purge(arena, false);
}

static void
arena_run_trim_head(arena_t *arena, arena_chunk_t *chunk, arena_run_t *run,
    size_t oldsize, size_t newsize)
{
	size_t pageind = ((uintptr_t)run - (uintptr_t)chunk) >> pagesize_2pow;
	size_t head_npages = (oldsize - newsize) >> pagesize_2pow;

	MOZ_ASSERT(oldsize > newsize);

	/*
	 * Update the chunk map so that arena_run_dalloc() can treat the
	 * leading run as separately allocated.
	 */
	chunk->map[pageind].bits = (oldsize - newsize) | CHUNK_MAP_LARGE |
	    CHUNK_MAP_ALLOCATED;
	chunk->map[pageind+head_npages].bits = newsize | CHUNK_MAP_LARGE |
	    CHUNK_MAP_ALLOCATED;

	arena_run_dalloc(arena, run, false);
}

static void
arena_run_trim_tail(arena_t *arena, arena_chunk_t *chunk, arena_run_t *run,
    size_t oldsize, size_t newsize, bool dirty)
{
	size_t pageind = ((uintptr_t)run - (uintptr_t)chunk) >> pagesize_2pow;
	size_t npages = newsize >> pagesize_2pow;

	MOZ_ASSERT(oldsize > newsize);

	/*
	 * Update the chunk map so that arena_run_dalloc() can treat the
	 * trailing run as separately allocated.
	 */
	chunk->map[pageind].bits = newsize | CHUNK_MAP_LARGE |
	    CHUNK_MAP_ALLOCATED;
	chunk->map[pageind+npages].bits = (oldsize - newsize) | CHUNK_MAP_LARGE
	    | CHUNK_MAP_ALLOCATED;

	arena_run_dalloc(arena, (arena_run_t *)((uintptr_t)run + newsize),
	    dirty);
}

static arena_run_t *
arena_bin_nonfull_run_get(arena_t *arena, arena_bin_t *bin)
{
	arena_chunk_map_t *mapelm;
	arena_run_t *run;
	unsigned i, remainder;

	/* Look for a usable run. */
	mapelm = arena_run_tree_first(&bin->runs);
	if (mapelm) {
		/* run is guaranteed to have available space. */
		arena_run_tree_remove(&bin->runs, mapelm);
		run = (arena_run_t *)(mapelm->bits & ~pagesize_mask);
		bin->stats.reruns++;
		return (run);
	}
	/* No existing runs have any space available. */

	/* Allocate a new run. */
	run = arena_run_alloc(arena, bin, bin->run_size, false, false);
	if (!run)
		return nullptr;
	/*
	 * Don't initialize if a race in arena_run_alloc() allowed an existing
	 * run to become usable.
	 */
	if (run == bin->runcur)
		return (run);

	/* Initialize run internals. */
	run->bin = bin;

	for (i = 0; i < bin->regs_mask_nelms - 1; i++)
		run->regs_mask[i] = UINT_MAX;
	remainder = bin->nregs & ((1U << (SIZEOF_INT_2POW + 3)) - 1);
	if (remainder == 0)
		run->regs_mask[i] = UINT_MAX;
	else {
		/* The last element has spare bits that need to be unset. */
		run->regs_mask[i] = (UINT_MAX >> ((1U << (SIZEOF_INT_2POW + 3))
		    - remainder));
	}

	run->regs_minelm = 0;

	run->nfree = bin->nregs;
#if defined(MOZ_DEBUG) || defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
	run->magic = ARENA_RUN_MAGIC;
#endif

	bin->stats.nruns++;
	bin->stats.curruns++;
	if (bin->stats.curruns > bin->stats.highruns)
		bin->stats.highruns = bin->stats.curruns;
	return (run);
}

/* bin->runcur must have space available before this function is called. */
static inline void *
arena_bin_malloc_easy(arena_t *arena, arena_bin_t *bin, arena_run_t *run)
{
	void *ret;

	MOZ_DIAGNOSTIC_ASSERT(run->magic == ARENA_RUN_MAGIC);
	MOZ_DIAGNOSTIC_ASSERT(run->nfree > 0);

	ret = arena_run_reg_alloc(run, bin);
	MOZ_DIAGNOSTIC_ASSERT(ret);
	run->nfree--;

	return (ret);
}

/* Re-fill bin->runcur, then call arena_bin_malloc_easy(). */
static void *
arena_bin_malloc_hard(arena_t *arena, arena_bin_t *bin)
{

	bin->runcur = arena_bin_nonfull_run_get(arena, bin);
	if (!bin->runcur)
		return nullptr;
	MOZ_DIAGNOSTIC_ASSERT(bin->runcur->magic == ARENA_RUN_MAGIC);
	MOZ_DIAGNOSTIC_ASSERT(bin->runcur->nfree > 0);

	return (arena_bin_malloc_easy(arena, bin, bin->runcur));
}

/*
 * Calculate bin->run_size such that it meets the following constraints:
 *
 *   *) bin->run_size >= min_run_size
 *   *) bin->run_size <= arena_maxclass
 *   *) bin->run_size <= RUN_MAX_SMALL
 *   *) run header overhead <= RUN_MAX_OVRHD (or header overhead relaxed).
 *
 * bin->nregs, bin->regs_mask_nelms, and bin->reg0_offset are
 * also calculated here, since these settings are all interdependent.
 */
static size_t
arena_bin_run_size_calc(arena_bin_t *bin, size_t min_run_size)
{
	size_t try_run_size, good_run_size;
	unsigned good_nregs, good_mask_nelms, good_reg0_offset;
	unsigned try_nregs, try_mask_nelms, try_reg0_offset;

	MOZ_ASSERT(min_run_size >= pagesize);
	MOZ_ASSERT(min_run_size <= arena_maxclass);

	/*
	 * Calculate known-valid settings before entering the run_size
	 * expansion loop, so that the first part of the loop always copies
	 * valid settings.
	 *
	 * The do..while loop iteratively reduces the number of regions until
	 * the run header and the regions no longer overlap.  A closed formula
	 * would be quite messy, since there is an interdependency between the
	 * header's mask length and the number of regions.
	 */
	try_run_size = min_run_size;
	try_nregs = ((try_run_size - sizeof(arena_run_t)) / bin->reg_size)
	    + 1; /* Counter-act try_nregs-- in loop. */
	do {
		try_nregs--;
		try_mask_nelms = (try_nregs >> (SIZEOF_INT_2POW + 3)) +
		    ((try_nregs & ((1U << (SIZEOF_INT_2POW + 3)) - 1)) ? 1 : 0);
		try_reg0_offset = try_run_size - (try_nregs * bin->reg_size);
	} while (sizeof(arena_run_t) + (sizeof(unsigned) * (try_mask_nelms - 1))
	    > try_reg0_offset);

	/* run_size expansion loop. */
	do {
		/*
		 * Copy valid settings before trying more aggressive settings.
		 */
		good_run_size = try_run_size;
		good_nregs = try_nregs;
		good_mask_nelms = try_mask_nelms;
		good_reg0_offset = try_reg0_offset;

		/* Try more aggressive settings. */
		try_run_size += pagesize;
		try_nregs = ((try_run_size - sizeof(arena_run_t)) /
		    bin->reg_size) + 1; /* Counter-act try_nregs-- in loop. */
		do {
			try_nregs--;
			try_mask_nelms = (try_nregs >> (SIZEOF_INT_2POW + 3)) +
			    ((try_nregs & ((1U << (SIZEOF_INT_2POW + 3)) - 1)) ?
			    1 : 0);
			try_reg0_offset = try_run_size - (try_nregs *
			    bin->reg_size);
		} while (sizeof(arena_run_t) + (sizeof(unsigned) *
		    (try_mask_nelms - 1)) > try_reg0_offset);
	} while (try_run_size <= arena_maxclass
	    && RUN_MAX_OVRHD * (bin->reg_size << 3) > RUN_MAX_OVRHD_RELAX
	    && (try_reg0_offset << RUN_BFP) > RUN_MAX_OVRHD * try_run_size);

	MOZ_ASSERT(sizeof(arena_run_t) + (sizeof(unsigned) * (good_mask_nelms - 1))
	    <= good_reg0_offset);
	MOZ_ASSERT((good_mask_nelms << (SIZEOF_INT_2POW + 3)) >= good_nregs);

	/* Copy final settings. */
	bin->run_size = good_run_size;
	bin->nregs = good_nregs;
	bin->regs_mask_nelms = good_mask_nelms;
	bin->reg0_offset = good_reg0_offset;

	return (good_run_size);
}

static inline void *
arena_malloc_small(arena_t *arena, size_t size, bool zero)
{
	void *ret;
	arena_bin_t *bin;
	arena_run_t *run;

	if (size < small_min) {
		/* Tiny. */
		size = pow2_ceil(size);
		bin = &arena->bins[ffs((int)(size >> (TINY_MIN_2POW +
		    1)))];
		/*
		 * Bin calculation is always correct, but we may need
		 * to fix size for the purposes of assertions and/or
		 * stats accuracy.
		 */
		if (size < (1U << TINY_MIN_2POW))
			size = (1U << TINY_MIN_2POW);
	} else if (size <= small_max) {
		/* Quantum-spaced. */
		size = QUANTUM_CEILING(size);
		bin = &arena->bins[ntbins + (size >> opt_quantum_2pow)
		    - 1];
	} else {
		/* Sub-page. */
		size = pow2_ceil(size);
		bin = &arena->bins[ntbins + nqbins
		    + (ffs((int)(size >> opt_small_max_2pow)) - 2)];
	}
	MOZ_DIAGNOSTIC_ASSERT(size == bin->reg_size);

	malloc_spin_lock(&arena->lock);
	if ((run = bin->runcur) && run->nfree > 0)
		ret = arena_bin_malloc_easy(arena, bin, run);
	else
		ret = arena_bin_malloc_hard(arena, bin);

	if (!ret) {
		malloc_spin_unlock(&arena->lock);
		return nullptr;
	}

	bin->stats.nrequests++;
	arena->stats.nmalloc_small++;
	arena->stats.allocated_small += size;
	malloc_spin_unlock(&arena->lock);

	if (zero == false) {
		if (opt_junk)
			memset(ret, kAllocJunk, size);
		else if (opt_zero)
			memset(ret, 0, size);
	} else
		memset(ret, 0, size);

	return (ret);
}

static void *
arena_malloc_large(arena_t *arena, size_t size, bool zero)
{
	void *ret;

	/* Large allocation. */
	size = PAGE_CEILING(size);
	malloc_spin_lock(&arena->lock);
	ret = (void *)arena_run_alloc(arena, nullptr, size, true, zero);
	if (!ret) {
		malloc_spin_unlock(&arena->lock);
		return nullptr;
	}
	arena->stats.nmalloc_large++;
	arena->stats.allocated_large += size;
	malloc_spin_unlock(&arena->lock);

	if (zero == false) {
		if (opt_junk)
			memset(ret, kAllocJunk, size);
		else if (opt_zero)
			memset(ret, 0, size);
	}

	return (ret);
}

static inline void *
arena_malloc(arena_t *arena, size_t size, bool zero)
{

	MOZ_ASSERT(arena);
	MOZ_DIAGNOSTIC_ASSERT(arena->magic == ARENA_MAGIC);
	MOZ_ASSERT(size != 0);
	MOZ_ASSERT(QUANTUM_CEILING(size) <= arena_maxclass);

	if (size <= bin_maxclass) {
		return (arena_malloc_small(arena, size, zero));
	} else
		return (arena_malloc_large(arena, size, zero));
}

static inline void *
imalloc(size_t size)
{

	MOZ_ASSERT(size != 0);

	if (size <= arena_maxclass)
		return (arena_malloc(choose_arena(), size, false));
	else
		return (huge_malloc(size, false));
}

static inline void *
icalloc(size_t size)
{

	if (size <= arena_maxclass)
		return (arena_malloc(choose_arena(), size, true));
	else
		return (huge_malloc(size, true));
}

/* Only handles large allocations that require more than page alignment. */
static void *
arena_palloc(arena_t *arena, size_t alignment, size_t size, size_t alloc_size)
{
	void *ret;
	size_t offset;
	arena_chunk_t *chunk;

	MOZ_ASSERT((size & pagesize_mask) == 0);
	MOZ_ASSERT((alignment & pagesize_mask) == 0);

	malloc_spin_lock(&arena->lock);
	ret = (void *)arena_run_alloc(arena, nullptr, alloc_size, true, false);
	if (!ret) {
		malloc_spin_unlock(&arena->lock);
		return nullptr;
	}

	chunk = (arena_chunk_t *)CHUNK_ADDR2BASE(ret);

	offset = (uintptr_t)ret & (alignment - 1);
	MOZ_ASSERT((offset & pagesize_mask) == 0);
	MOZ_ASSERT(offset < alloc_size);
	if (offset == 0)
		arena_run_trim_tail(arena, chunk, (arena_run_t*)ret, alloc_size, size, false);
	else {
		size_t leadsize, trailsize;

		leadsize = alignment - offset;
		if (leadsize > 0) {
			arena_run_trim_head(arena, chunk, (arena_run_t*)ret, alloc_size,
			    alloc_size - leadsize);
			ret = (void *)((uintptr_t)ret + leadsize);
		}

		trailsize = alloc_size - leadsize - size;
		if (trailsize != 0) {
			/* Trim trailing space. */
			MOZ_ASSERT(trailsize < alloc_size);
			arena_run_trim_tail(arena, chunk, (arena_run_t*)ret, size + trailsize,
			    size, false);
		}
	}

	arena->stats.nmalloc_large++;
	arena->stats.allocated_large += size;
	malloc_spin_unlock(&arena->lock);

	if (opt_junk)
		memset(ret, kAllocJunk, size);
	else if (opt_zero)
		memset(ret, 0, size);
	return (ret);
}

static inline void *
ipalloc(size_t alignment, size_t size)
{
	void *ret;
	size_t ceil_size;

	/*
	 * Round size up to the nearest multiple of alignment.
	 *
	 * This done, we can take advantage of the fact that for each small
	 * size class, every object is aligned at the smallest power of two
	 * that is non-zero in the base two representation of the size.  For
	 * example:
	 *
	 *   Size |   Base 2 | Minimum alignment
	 *   -----+----------+------------------
	 *     96 |  1100000 |  32
	 *    144 | 10100000 |  32
	 *    192 | 11000000 |  64
	 *
	 * Depending on runtime settings, it is possible that arena_malloc()
	 * will further round up to a power of two, but that never causes
	 * correctness issues.
	 */
	ceil_size = ALIGNMENT_CEILING(size, alignment);
	/*
	 * (ceil_size < size) protects against the combination of maximal
	 * alignment and size greater than maximal alignment.
	 */
	if (ceil_size < size) {
		/* size_t overflow. */
		return nullptr;
	}

	if (ceil_size <= pagesize || (alignment <= pagesize
	    && ceil_size <= arena_maxclass))
		ret = arena_malloc(choose_arena(), ceil_size, false);
	else {
		size_t run_size;

		/*
		 * We can't achieve sub-page alignment, so round up alignment
		 * permanently; it makes later calculations simpler.
		 */
		alignment = PAGE_CEILING(alignment);
		ceil_size = PAGE_CEILING(size);
		/*
		 * (ceil_size < size) protects against very large sizes within
		 * pagesize of SIZE_T_MAX.
		 *
		 * (ceil_size + alignment < ceil_size) protects against the
		 * combination of maximal alignment and ceil_size large enough
		 * to cause overflow.  This is similar to the first overflow
		 * check above, but it needs to be repeated due to the new
		 * ceil_size value, which may now be *equal* to maximal
		 * alignment, whereas before we only detected overflow if the
		 * original size was *greater* than maximal alignment.
		 */
		if (ceil_size < size || ceil_size + alignment < ceil_size) {
			/* size_t overflow. */
			return nullptr;
		}

		/*
		 * Calculate the size of the over-size run that arena_palloc()
		 * would need to allocate in order to guarantee the alignment.
		 */
		if (ceil_size >= alignment)
			run_size = ceil_size + alignment - pagesize;
		else {
			/*
			 * It is possible that (alignment << 1) will cause
			 * overflow, but it doesn't matter because we also
			 * subtract pagesize, which in the case of overflow
			 * leaves us with a very large run_size.  That causes
			 * the first conditional below to fail, which means
			 * that the bogus run_size value never gets used for
			 * anything important.
			 */
			run_size = (alignment << 1) - pagesize;
		}

		if (run_size <= arena_maxclass) {
			ret = arena_palloc(choose_arena(), alignment, ceil_size,
			    run_size);
		} else if (alignment <= chunksize)
			ret = huge_malloc(ceil_size, false);
		else
			ret = huge_palloc(ceil_size, alignment, false);
	}

	MOZ_ASSERT(((uintptr_t)ret & (alignment - 1)) == 0);
	return (ret);
}

/* Return the size of the allocation pointed to by ptr. */
static size_t
arena_salloc(const void *ptr)
{
	size_t ret;
	arena_chunk_t *chunk;
	size_t pageind, mapbits;

	MOZ_ASSERT(ptr);
	MOZ_ASSERT(CHUNK_ADDR2BASE(ptr) != ptr);

	chunk = (arena_chunk_t *)CHUNK_ADDR2BASE(ptr);
	pageind = (((uintptr_t)ptr - (uintptr_t)chunk) >> pagesize_2pow);
	mapbits = chunk->map[pageind].bits;
	MOZ_DIAGNOSTIC_ASSERT((mapbits & CHUNK_MAP_ALLOCATED) != 0);
	if ((mapbits & CHUNK_MAP_LARGE) == 0) {
		arena_run_t *run = (arena_run_t *)(mapbits & ~pagesize_mask);
		MOZ_DIAGNOSTIC_ASSERT(run->magic == ARENA_RUN_MAGIC);
		ret = run->bin->reg_size;
	} else {
		ret = mapbits & ~pagesize_mask;
		MOZ_DIAGNOSTIC_ASSERT(ret != 0);
	}

	return (ret);
}

/*
 * Validate ptr before assuming that it points to an allocation.  Currently,
 * the following validation is performed:
 *
 * + Check that ptr is not nullptr.
 *
 * + Check that ptr lies within a mapped chunk.
 */
static inline size_t
isalloc_validate(const void *ptr)
{
	arena_chunk_t *chunk;

	chunk = (arena_chunk_t *)CHUNK_ADDR2BASE(ptr);
	if (!chunk)
		return (0);

	if (!malloc_rtree_get(chunk_rtree, (uintptr_t)chunk))
		return (0);

	if (chunk != ptr) {
		MOZ_DIAGNOSTIC_ASSERT(chunk->arena->magic == ARENA_MAGIC);
		return (arena_salloc(ptr));
	} else {
		size_t ret;
		extent_node_t *node;
		extent_node_t key;

		/* Chunk. */
		key.addr = (void *)chunk;
		malloc_mutex_lock(&huge_mtx);
		node = extent_tree_ad_search(&huge, &key);
		if (node)
			ret = node->size;
		else
			ret = 0;
		malloc_mutex_unlock(&huge_mtx);
		return (ret);
	}
}

static inline size_t
isalloc(const void *ptr)
{
	size_t ret;
	arena_chunk_t *chunk;

	MOZ_ASSERT(ptr);

	chunk = (arena_chunk_t *)CHUNK_ADDR2BASE(ptr);
	if (chunk != ptr) {
		/* Region. */
		MOZ_ASSERT(chunk->arena->magic == ARENA_MAGIC);

		ret = arena_salloc(ptr);
	} else {
		extent_node_t *node, key;

		/* Chunk (huge allocation). */

		malloc_mutex_lock(&huge_mtx);

		/* Extract from tree of huge allocations. */
		key.addr = const_cast<void*>(ptr);
		node = extent_tree_ad_search(&huge, &key);
		MOZ_DIAGNOSTIC_ASSERT(node);

		ret = node->size;

		malloc_mutex_unlock(&huge_mtx);
	}

	return (ret);
}

static inline void
arena_dalloc_small(arena_t *arena, arena_chunk_t *chunk, void *ptr,
    arena_chunk_map_t *mapelm)
{
	arena_run_t *run;
	arena_bin_t *bin;
	size_t size;

	run = (arena_run_t *)(mapelm->bits & ~pagesize_mask);
	MOZ_DIAGNOSTIC_ASSERT(run->magic == ARENA_RUN_MAGIC);
	bin = run->bin;
	size = bin->reg_size;

	memset(ptr, kAllocPoison, size);

	arena_run_reg_dalloc(run, bin, ptr, size);
	run->nfree++;

	if (run->nfree == bin->nregs) {
		/* Deallocate run. */
		if (run == bin->runcur)
			bin->runcur = nullptr;
		else if (bin->nregs != 1) {
			size_t run_pageind = (((uintptr_t)run -
			    (uintptr_t)chunk)) >> pagesize_2pow;
			arena_chunk_map_t *run_mapelm =
			    &chunk->map[run_pageind];
			/*
			 * This block's conditional is necessary because if the
			 * run only contains one region, then it never gets
			 * inserted into the non-full runs tree.
			 */
			MOZ_DIAGNOSTIC_ASSERT(arena_run_tree_search(&bin->runs, run_mapelm) ==
				run_mapelm);
			arena_run_tree_remove(&bin->runs, run_mapelm);
		}
#if defined(MOZ_DEBUG) || defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
		run->magic = 0;
#endif
		arena_run_dalloc(arena, run, true);
		bin->stats.curruns--;
	} else if (run->nfree == 1 && run != bin->runcur) {
		/*
		 * Make sure that bin->runcur always refers to the lowest
		 * non-full run, if one exists.
		 */
		if (!bin->runcur)
			bin->runcur = run;
		else if ((uintptr_t)run < (uintptr_t)bin->runcur) {
			/* Switch runcur. */
			if (bin->runcur->nfree > 0) {
				arena_chunk_t *runcur_chunk =
				    (arena_chunk_t*)CHUNK_ADDR2BASE(bin->runcur);
				size_t runcur_pageind =
				    (((uintptr_t)bin->runcur -
				    (uintptr_t)runcur_chunk)) >> pagesize_2pow;
				arena_chunk_map_t *runcur_mapelm =
				    &runcur_chunk->map[runcur_pageind];

				/* Insert runcur. */
				MOZ_DIAGNOSTIC_ASSERT(!arena_run_tree_search(&bin->runs,
				    runcur_mapelm));
				arena_run_tree_insert(&bin->runs,
				    runcur_mapelm);
			}
			bin->runcur = run;
		} else {
			size_t run_pageind = (((uintptr_t)run -
			    (uintptr_t)chunk)) >> pagesize_2pow;
			arena_chunk_map_t *run_mapelm =
			    &chunk->map[run_pageind];

			MOZ_DIAGNOSTIC_ASSERT(arena_run_tree_search(&bin->runs, run_mapelm) ==
			    nullptr);
			arena_run_tree_insert(&bin->runs, run_mapelm);
		}
	}
	arena->stats.allocated_small -= size;
	arena->stats.ndalloc_small++;
}

static void
arena_dalloc_large(arena_t *arena, arena_chunk_t *chunk, void *ptr)
{
	size_t pageind = ((uintptr_t)ptr - (uintptr_t)chunk) >>
	    pagesize_2pow;
	size_t size = chunk->map[pageind].bits & ~pagesize_mask;

	memset(ptr, kAllocPoison, size);
	arena->stats.allocated_large -= size;
	arena->stats.ndalloc_large++;

	arena_run_dalloc(arena, (arena_run_t *)ptr, true);
}

static inline void
arena_dalloc(void *ptr, size_t offset)
{
	arena_chunk_t *chunk;
	arena_t *arena;
	size_t pageind;
	arena_chunk_map_t *mapelm;

	MOZ_ASSERT(ptr);
	MOZ_ASSERT(offset != 0);
	MOZ_ASSERT(CHUNK_ADDR2OFFSET(ptr) == offset);

	chunk = (arena_chunk_t *) ((uintptr_t)ptr - offset);
	arena = chunk->arena;
	MOZ_ASSERT(arena);
	MOZ_DIAGNOSTIC_ASSERT(arena->magic == ARENA_MAGIC);

	malloc_spin_lock(&arena->lock);
	pageind = offset >> pagesize_2pow;
	mapelm = &chunk->map[pageind];
	MOZ_DIAGNOSTIC_ASSERT((mapelm->bits & CHUNK_MAP_ALLOCATED) != 0);
	if ((mapelm->bits & CHUNK_MAP_LARGE) == 0) {
		/* Small allocation. */
		arena_dalloc_small(arena, chunk, ptr, mapelm);
	} else {
		/* Large allocation. */
		arena_dalloc_large(arena, chunk, ptr);
	}
	malloc_spin_unlock(&arena->lock);
}

static inline void
idalloc(void *ptr)
{
	size_t offset;

	MOZ_ASSERT(ptr);

	offset = CHUNK_ADDR2OFFSET(ptr);
	if (offset != 0)
		arena_dalloc(ptr, offset);
	else
		huge_dalloc(ptr);
}

static void
arena_ralloc_large_shrink(arena_t *arena, arena_chunk_t *chunk, void *ptr,
    size_t size, size_t oldsize)
{

	MOZ_ASSERT(size < oldsize);

	/*
	 * Shrink the run, and make trailing pages available for other
	 * allocations.
	 */
	malloc_spin_lock(&arena->lock);
	arena_run_trim_tail(arena, chunk, (arena_run_t *)ptr, oldsize, size,
	    true);
	arena->stats.allocated_large -= oldsize - size;
	malloc_spin_unlock(&arena->lock);
}

static bool
arena_ralloc_large_grow(arena_t *arena, arena_chunk_t *chunk, void *ptr,
    size_t size, size_t oldsize)
{
	size_t pageind = ((uintptr_t)ptr - (uintptr_t)chunk) >> pagesize_2pow;
	size_t npages = oldsize >> pagesize_2pow;

	malloc_spin_lock(&arena->lock);
	MOZ_DIAGNOSTIC_ASSERT(oldsize == (chunk->map[pageind].bits & ~pagesize_mask));

	/* Try to extend the run. */
	MOZ_ASSERT(size > oldsize);
	if (pageind + npages < chunk_npages && (chunk->map[pageind+npages].bits
	    & CHUNK_MAP_ALLOCATED) == 0 && (chunk->map[pageind+npages].bits &
	    ~pagesize_mask) >= size - oldsize) {
		/*
		 * The next run is available and sufficiently large.  Split the
		 * following run, then merge the first part with the existing
		 * allocation.
		 */
		arena_run_split(arena, (arena_run_t *)((uintptr_t)chunk +
		    ((pageind+npages) << pagesize_2pow)), size - oldsize, true,
		    false);

		chunk->map[pageind].bits = size | CHUNK_MAP_LARGE |
		    CHUNK_MAP_ALLOCATED;
		chunk->map[pageind+npages].bits = CHUNK_MAP_LARGE |
		    CHUNK_MAP_ALLOCATED;

		arena->stats.allocated_large += size - oldsize;
		malloc_spin_unlock(&arena->lock);
		return (false);
	}
	malloc_spin_unlock(&arena->lock);

	return (true);
}

/*
 * Try to resize a large allocation, in order to avoid copying.  This will
 * always fail if growing an object, and the following run is already in use.
 */
static bool
arena_ralloc_large(void *ptr, size_t size, size_t oldsize)
{
	size_t psize;

	psize = PAGE_CEILING(size);
	if (psize == oldsize) {
		/* Same size class. */
		if (size < oldsize) {
			memset((void *)((uintptr_t)ptr + size), kAllocPoison, oldsize -
			    size);
		}
		return (false);
	} else {
		arena_chunk_t *chunk;
		arena_t *arena;

		chunk = (arena_chunk_t *)CHUNK_ADDR2BASE(ptr);
		arena = chunk->arena;
		MOZ_DIAGNOSTIC_ASSERT(arena->magic == ARENA_MAGIC);

		if (psize < oldsize) {
			/* Fill before shrinking in order avoid a race. */
			memset((void *)((uintptr_t)ptr + size), kAllocPoison,
			    oldsize - size);
			arena_ralloc_large_shrink(arena, chunk, ptr, psize,
			    oldsize);
			return (false);
		} else {
			bool ret = arena_ralloc_large_grow(arena, chunk, ptr,
			    psize, oldsize);
			if (ret == false && opt_zero) {
				memset((void *)((uintptr_t)ptr + oldsize), 0,
				    size - oldsize);
			}
			return (ret);
		}
	}
}

static void *
arena_ralloc(void *ptr, size_t size, size_t oldsize)
{
	void *ret;
	size_t copysize;

	/* Try to avoid moving the allocation. */
	if (size < small_min) {
		if (oldsize < small_min &&
		    ffs((int)(pow2_ceil(size) >> (TINY_MIN_2POW + 1)))
		    == ffs((int)(pow2_ceil(oldsize) >> (TINY_MIN_2POW + 1))))
			goto IN_PLACE; /* Same size class. */
	} else if (size <= small_max) {
		if (oldsize >= small_min && oldsize <= small_max &&
		    (QUANTUM_CEILING(size) >> opt_quantum_2pow)
		    == (QUANTUM_CEILING(oldsize) >> opt_quantum_2pow))
			goto IN_PLACE; /* Same size class. */
	} else if (size <= bin_maxclass) {
		if (oldsize > small_max && oldsize <= bin_maxclass &&
		    pow2_ceil(size) == pow2_ceil(oldsize))
			goto IN_PLACE; /* Same size class. */
	} else if (oldsize > bin_maxclass && oldsize <= arena_maxclass) {
		MOZ_ASSERT(size > bin_maxclass);
		if (arena_ralloc_large(ptr, size, oldsize) == false)
			return (ptr);
	}

	/*
	 * If we get here, then size and oldsize are different enough that we
	 * need to move the object.  In that case, fall back to allocating new
	 * space and copying.
	 */
	ret = arena_malloc(choose_arena(), size, false);
	if (!ret)
		return nullptr;

	/* Junk/zero-filling were already done by arena_malloc(). */
	copysize = (size < oldsize) ? size : oldsize;
#ifdef VM_COPY_MIN
	if (copysize >= VM_COPY_MIN)
		pages_copy(ret, ptr, copysize);
	else
#endif
		memcpy(ret, ptr, copysize);
	idalloc(ptr);
	return (ret);
IN_PLACE:
	if (size < oldsize)
		memset((void *)((uintptr_t)ptr + size), kAllocPoison, oldsize - size);
	else if (opt_zero && size > oldsize)
		memset((void *)((uintptr_t)ptr + oldsize), 0, size - oldsize);
	return (ptr);
}

static inline void *
iralloc(void *ptr, size_t size)
{
	size_t oldsize;

	MOZ_ASSERT(ptr);
	MOZ_ASSERT(size != 0);

	oldsize = isalloc(ptr);

	if (size <= arena_maxclass)
		return (arena_ralloc(ptr, size, oldsize));
	else
		return (huge_ralloc(ptr, size, oldsize));
}

static bool
arena_new(arena_t *arena)
{
	unsigned i;
	arena_bin_t *bin;
	size_t prev_run_size;

	if (malloc_spin_init(&arena->lock))
		return (true);

	memset(&arena->stats, 0, sizeof(arena_stats_t));

	/* Initialize chunks. */
	arena_chunk_tree_dirty_new(&arena->chunks_dirty);
#ifdef MALLOC_DOUBLE_PURGE
	LinkedList_Init(&arena->chunks_madvised);
#endif
	arena->spare = nullptr;

	arena->ndirty = 0;

	arena_avail_tree_new(&arena->runs_avail);

	/* Initialize bins. */
	prev_run_size = pagesize;

	/* (2^n)-spaced tiny bins. */
	for (i = 0; i < ntbins; i++) {
		bin = &arena->bins[i];
		bin->runcur = nullptr;
		arena_run_tree_new(&bin->runs);

		bin->reg_size = (1ULL << (TINY_MIN_2POW + i));

		prev_run_size = arena_bin_run_size_calc(bin, prev_run_size);

		memset(&bin->stats, 0, sizeof(malloc_bin_stats_t));
	}

	/* Quantum-spaced bins. */
	for (; i < ntbins + nqbins; i++) {
		bin = &arena->bins[i];
		bin->runcur = nullptr;
		arena_run_tree_new(&bin->runs);

		bin->reg_size = quantum * (i - ntbins + 1);

		prev_run_size = arena_bin_run_size_calc(bin, prev_run_size);

		memset(&bin->stats, 0, sizeof(malloc_bin_stats_t));
	}

	/* (2^n)-spaced sub-page bins. */
	for (; i < ntbins + nqbins + nsbins; i++) {
		bin = &arena->bins[i];
		bin->runcur = nullptr;
		arena_run_tree_new(&bin->runs);

		bin->reg_size = (small_max << (i - (ntbins + nqbins) + 1));

		prev_run_size = arena_bin_run_size_calc(bin, prev_run_size);

		memset(&bin->stats, 0, sizeof(malloc_bin_stats_t));
	}

#if defined(MOZ_DEBUG) || defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
	arena->magic = ARENA_MAGIC;
#endif

	return (false);
}

static inline arena_t *
arenas_fallback()
{
	/* Only reached if there is an OOM error. */

	/*
	 * OOM here is quite inconvenient to propagate, since dealing with it
	 * would require a check for failure in the fast path.  Instead, punt
	 * by using arenas[0].
	 * In practice, this is an extremely unlikely failure.
	 */
	_malloc_message(_getprogname(),
	    ": (malloc) Error initializing arena\n", "", "");
	if (opt_abort)
		moz_abort();

	return arenas[0];
}

/* Create a new arena and return it. */
static arena_t *
arenas_extend()
{
	/*
	 * The list of arenas is first allocated to contain at most 16 elements,
	 * and when the limit is reached, the list is grown such that it can
	 * contain 16 more elements.
	 */
	const size_t arenas_growth = 16;
	arena_t *ret;


	/* Allocate enough space for trailing bins. */
	ret = (arena_t *)base_alloc(sizeof(arena_t)
	    + (sizeof(arena_bin_t) * (ntbins + nqbins + nsbins - 1)));
	if (!ret || arena_new(ret)) {
		return arenas_fallback();
        }

	malloc_spin_lock(&arenas_lock);

	/* Allocate and initialize arenas. */
	if (narenas % arenas_growth == 0) {
		size_t max_arenas = ((narenas + arenas_growth) / arenas_growth) * arenas_growth;
		/*
		 * We're unfortunately leaking the previous allocation ;
		 * the base allocator doesn't know how to free things
		 */
		arena_t** new_arenas = (arena_t **)base_alloc(sizeof(arena_t *) * max_arenas);
		if (!new_arenas) {
			ret = arenas ? arenas_fallback() : nullptr;
			malloc_spin_unlock(&arenas_lock);
			return (ret);
		}
		memcpy(new_arenas, arenas, narenas * sizeof(arena_t *));
		/*
		 * Zero the array.  In practice, this should always be pre-zeroed,
		 * since it was just mmap()ed, but let's be sure.
		 */
		memset(new_arenas + narenas, 0, sizeof(arena_t *) * (max_arenas - narenas));
		arenas = new_arenas;
	}
	arenas[narenas++] = ret;

	malloc_spin_unlock(&arenas_lock);
	return (ret);
}

/*
 * End arena.
 */
/******************************************************************************/
/*
 * Begin general internal functions.
 */

static void *
huge_malloc(size_t size, bool zero)
{
	return huge_palloc(size, chunksize, zero);
}

static void *
huge_palloc(size_t size, size_t alignment, bool zero)
{
	void *ret;
	size_t csize;
	size_t psize;
	extent_node_t *node;

	/* Allocate one or more contiguous chunks for this request. */

	csize = CHUNK_CEILING(size);
	if (csize == 0) {
		/* size is large enough to cause size_t wrap-around. */
		return nullptr;
	}

	/* Allocate an extent node with which to track the chunk. */
	node = base_node_alloc();
	if (!node)
		return nullptr;

	ret = chunk_alloc(csize, alignment, false, zero);
	if (!ret) {
		base_node_dealloc(node);
		return nullptr;
	}

	/* Insert node into huge. */
	node->addr = ret;
	psize = PAGE_CEILING(size);
	node->size = psize;

	malloc_mutex_lock(&huge_mtx);
	extent_tree_ad_insert(&huge, node);
	huge_nmalloc++;

        /* Although we allocated space for csize bytes, we indicate that we've
         * allocated only psize bytes.
         *
         * If DECOMMIT is defined, this is a reasonable thing to do, since
         * we'll explicitly decommit the bytes in excess of psize.
         *
         * If DECOMMIT is not defined, then we're relying on the OS to be lazy
         * about how it allocates physical pages to mappings.  If we never
         * touch the pages in excess of psize, the OS won't allocate a physical
         * page, and we won't use more than psize bytes of physical memory.
         *
         * A correct program will only touch memory in excess of how much it
         * requested if it first calls malloc_usable_size and finds out how
         * much space it has to play with.  But because we set node->size =
         * psize above, malloc_usable_size will return psize, not csize, and
         * the program will (hopefully) never touch bytes in excess of psize.
         * Thus those bytes won't take up space in physical memory, and we can
         * reasonably claim we never "allocated" them in the first place. */
	huge_allocated += psize;
	huge_mapped += csize;
	malloc_mutex_unlock(&huge_mtx);

#ifdef MALLOC_DECOMMIT
	if (csize - psize > 0)
		pages_decommit((void *)((uintptr_t)ret + psize), csize - psize);
#endif

	if (zero == false) {
		if (opt_junk)
#  ifdef MALLOC_DECOMMIT
			memset(ret, kAllocJunk, psize);
#  else
			memset(ret, kAllocJunk, csize);
#  endif
		else if (opt_zero)
#  ifdef MALLOC_DECOMMIT
			memset(ret, 0, psize);
#  else
			memset(ret, 0, csize);
#  endif
	}

	return (ret);
}

static void *
huge_ralloc(void *ptr, size_t size, size_t oldsize)
{
	void *ret;
	size_t copysize;

	/* Avoid moving the allocation if the size class would not change. */

	if (oldsize > arena_maxclass &&
	    CHUNK_CEILING(size) == CHUNK_CEILING(oldsize)) {
		size_t psize = PAGE_CEILING(size);
		if (size < oldsize) {
			memset((void *)((uintptr_t)ptr + size), kAllocPoison, oldsize
			    - size);
		}
#ifdef MALLOC_DECOMMIT
		if (psize < oldsize) {
			extent_node_t *node, key;

			pages_decommit((void *)((uintptr_t)ptr + psize),
			    oldsize - psize);

			/* Update recorded size. */
			malloc_mutex_lock(&huge_mtx);
			key.addr = const_cast<void*>(ptr);
			node = extent_tree_ad_search(&huge, &key);
			MOZ_ASSERT(node);
			MOZ_ASSERT(node->size == oldsize);
			huge_allocated -= oldsize - psize;
			/* No need to change huge_mapped, because we didn't
			 * (un)map anything. */
			node->size = psize;
			malloc_mutex_unlock(&huge_mtx);
		} else if (psize > oldsize) {
			pages_commit((void *)((uintptr_t)ptr + oldsize),
			    psize - oldsize);
                }
#endif

                /* Although we don't have to commit or decommit anything if
                 * DECOMMIT is not defined and the size class didn't change, we
                 * do need to update the recorded size if the size increased,
                 * so malloc_usable_size doesn't return a value smaller than
                 * what was requested via realloc(). */

                if (psize > oldsize) {
                        /* Update recorded size. */
                        extent_node_t *node, key;
                        malloc_mutex_lock(&huge_mtx);
                        key.addr = const_cast<void*>(ptr);
                        node = extent_tree_ad_search(&huge, &key);
                        MOZ_ASSERT(node);
                        MOZ_ASSERT(node->size == oldsize);
                        huge_allocated += psize - oldsize;
			/* No need to change huge_mapped, because we didn't
			 * (un)map anything. */
                        node->size = psize;
                        malloc_mutex_unlock(&huge_mtx);
                }

		if (opt_zero && size > oldsize) {
			memset((void *)((uintptr_t)ptr + oldsize), 0, size
			    - oldsize);
		}
		return (ptr);
	}

	/*
	 * If we get here, then size and oldsize are different enough that we
	 * need to use a different size class.  In that case, fall back to
	 * allocating new space and copying.
	 */
	ret = huge_malloc(size, false);
	if (!ret)
		return nullptr;

	copysize = (size < oldsize) ? size : oldsize;
#ifdef VM_COPY_MIN
	if (copysize >= VM_COPY_MIN)
		pages_copy(ret, ptr, copysize);
	else
#endif
		memcpy(ret, ptr, copysize);
	idalloc(ptr);
	return (ret);
}

static void
huge_dalloc(void *ptr)
{
	extent_node_t *node, key;

	malloc_mutex_lock(&huge_mtx);

	/* Extract from tree of huge allocations. */
	key.addr = ptr;
	node = extent_tree_ad_search(&huge, &key);
	MOZ_ASSERT(node);
	MOZ_ASSERT(node->addr == ptr);
	extent_tree_ad_remove(&huge, node);

	huge_ndalloc++;
	huge_allocated -= node->size;
	huge_mapped -= CHUNK_CEILING(node->size);

	malloc_mutex_unlock(&huge_mtx);

	/* Unmap chunk. */
	chunk_dealloc(node->addr, CHUNK_CEILING(node->size), HUGE_CHUNK);

	base_node_dealloc(node);
}

/*
 * FreeBSD's pthreads implementation calls malloc(3), so the malloc
 * implementation has to take pains to avoid infinite recursion during
 * initialization.
 */
#if (defined(MOZ_MEMORY_WINDOWS) || defined(MOZ_MEMORY_DARWIN))
#define	malloc_init() false
#else
static inline bool
malloc_init(void)
{

	if (malloc_initialized == false)
		return (malloc_init_hard());

	return (false);
}
#endif

#if defined(MOZ_MEMORY_DARWIN)
extern "C" void register_zone(void);
#endif

#if !defined(MOZ_MEMORY_WINDOWS)
static
#endif
bool
malloc_init_hard(void)
{
	unsigned i;
	const char *opts;
	long result;

#ifndef MOZ_MEMORY_WINDOWS
	malloc_mutex_lock(&init_lock);
#endif

	if (malloc_initialized) {
		/*
		 * Another thread initialized the allocator before this one
		 * acquired init_lock.
		 */
#ifndef MOZ_MEMORY_WINDOWS
		malloc_mutex_unlock(&init_lock);
#endif
		return (false);
	}

#ifdef MOZ_MEMORY_WINDOWS
	/* get a thread local storage index */
	tlsIndex = TlsAlloc();
#elif defined(MOZ_MEMORY_DARWIN)
	pthread_key_create(&tlsIndex, nullptr);
#endif

	/* Get page size and number of CPUs */
#ifdef MOZ_MEMORY_WINDOWS
	{
		SYSTEM_INFO info;

		GetSystemInfo(&info);
		result = info.dwPageSize;

	}
#else
	result = sysconf(_SC_PAGESIZE);
	MOZ_ASSERT(result != -1);
#endif

	/* We assume that the page size is a power of 2. */
	MOZ_ASSERT(((result - 1) & result) == 0);
#ifdef MALLOC_STATIC_SIZES
	if (pagesize % (size_t) result) {
		_malloc_message(_getprogname(),
				"Compile-time page size does not divide the runtime one.\n",
				"", "");
		moz_abort();
	}
#else
	pagesize = (size_t) result;
	pagesize_mask = (size_t) result - 1;
	pagesize_2pow = ffs((int)result) - 1;
#endif

	/* Get runtime configuration. */
	if ((opts = getenv("MALLOC_OPTIONS"))) {
		for (i = 0; opts[i] != '\0'; i++) {
			unsigned j, nreps;
			bool nseen;

			/* Parse repetition count, if any. */
			for (nreps = 0, nseen = false;; i++, nseen = true) {
				switch (opts[i]) {
					case '0': case '1': case '2': case '3':
					case '4': case '5': case '6': case '7':
					case '8': case '9':
						nreps *= 10;
						nreps += opts[i] - '0';
						break;
					default:
						goto MALLOC_OUT;
				}
			}
MALLOC_OUT:
			if (nseen == false)
				nreps = 1;

			for (j = 0; j < nreps; j++) {
				switch (opts[i]) {
				case 'a':
					opt_abort = false;
					break;
				case 'A':
					opt_abort = true;
					break;
				case 'f':
					opt_dirty_max >>= 1;
					break;
				case 'F':
					if (opt_dirty_max == 0)
						opt_dirty_max = 1;
					else if ((opt_dirty_max << 1) != 0)
						opt_dirty_max <<= 1;
					break;
#ifdef MOZ_DEBUG
				case 'j':
					opt_junk = false;
					break;
				case 'J':
					opt_junk = true;
					break;
#endif
#ifndef MALLOC_STATIC_SIZES
				case 'k':
					/*
					 * Chunks always require at least one
					 * header page, so chunks can never be
					 * smaller than two pages.
					 */
					if (opt_chunk_2pow > pagesize_2pow + 1)
						opt_chunk_2pow--;
					break;
				case 'K':
					if (opt_chunk_2pow + 1 <
					    (sizeof(size_t) << 3))
						opt_chunk_2pow++;
					break;
#endif
#ifndef MALLOC_STATIC_SIZES
				case 'q':
					if (opt_quantum_2pow > QUANTUM_2POW_MIN)
						opt_quantum_2pow--;
					break;
				case 'Q':
					if (opt_quantum_2pow < pagesize_2pow -
					    1)
						opt_quantum_2pow++;
					break;
				case 's':
					if (opt_small_max_2pow >
					    QUANTUM_2POW_MIN)
						opt_small_max_2pow--;
					break;
				case 'S':
					if (opt_small_max_2pow < pagesize_2pow
					    - 1)
						opt_small_max_2pow++;
					break;
#endif
#ifdef MOZ_DEBUG
				case 'z':
					opt_zero = false;
					break;
				case 'Z':
					opt_zero = true;
					break;
#endif
				default: {
					char cbuf[2];

					cbuf[0] = opts[i];
					cbuf[1] = '\0';
					_malloc_message(_getprogname(),
					    ": (malloc) Unsupported character "
					    "in malloc options: '", cbuf,
					    "'\n");
				}
				}
			}
		}
	}

#ifndef MALLOC_STATIC_SIZES
	/* Set variables according to the value of opt_small_max_2pow. */
	if (opt_small_max_2pow < opt_quantum_2pow)
		opt_small_max_2pow = opt_quantum_2pow;
	small_max = (1U << opt_small_max_2pow);

	/* Set bin-related variables. */
	bin_maxclass = (pagesize >> 1);
	MOZ_ASSERT(opt_quantum_2pow >= TINY_MIN_2POW);
	ntbins = opt_quantum_2pow - TINY_MIN_2POW;
	MOZ_ASSERT(ntbins <= opt_quantum_2pow);
	nqbins = (small_max >> opt_quantum_2pow);
	nsbins = pagesize_2pow - opt_small_max_2pow - 1;

	/* Set variables according to the value of opt_quantum_2pow. */
	quantum = (1U << opt_quantum_2pow);
	quantum_mask = quantum - 1;
	if (ntbins > 0)
		small_min = (quantum >> 1) + 1;
	else
		small_min = 1;
	MOZ_ASSERT(small_min <= quantum);

	/* Set variables according to the value of opt_chunk_2pow. */
	chunksize = (1LU << opt_chunk_2pow);
	chunksize_mask = chunksize - 1;
	chunk_npages = (chunksize >> pagesize_2pow);

	arena_chunk_header_npages = calculate_arena_header_pages();
	arena_maxclass = calculate_arena_maxclass();

	recycle_limit = CHUNK_RECYCLE_LIMIT * chunksize;
#endif

	recycled_size = 0;

	/* Various sanity checks that regard configuration. */
	MOZ_ASSERT(quantum >= sizeof(void *));
	MOZ_ASSERT(quantum <= pagesize);
	MOZ_ASSERT(chunksize >= pagesize);
	MOZ_ASSERT(quantum * 4 <= chunksize);

	/* Initialize chunks data. */
	malloc_mutex_init(&chunks_mtx);
	extent_tree_szad_new(&chunks_szad_mmap);
	extent_tree_ad_new(&chunks_ad_mmap);

	/* Initialize huge allocation data. */
	malloc_mutex_init(&huge_mtx);
	extent_tree_ad_new(&huge);
	huge_nmalloc = 0;
	huge_ndalloc = 0;
	huge_allocated = 0;
	huge_mapped = 0;

	/* Initialize base allocation data structures. */
	base_mapped = 0;
	base_committed = 0;
	base_nodes = nullptr;
	malloc_mutex_init(&base_mtx);

	malloc_spin_init(&arenas_lock);

	/*
	 * Initialize one arena here.
	 */
	arenas_extend();
	if (!arenas || !arenas[0]) {
#ifndef MOZ_MEMORY_WINDOWS
		malloc_mutex_unlock(&init_lock);
#endif
		return (true);
	}
#ifndef NO_TLS
	/*
	 * Assign the initial arena to the initial thread, in order to avoid
	 * spurious creation of an extra arena if the application switches to
	 * threaded mode.
	 */
#ifdef MOZ_MEMORY_WINDOWS
	TlsSetValue(tlsIndex, arenas[0]);
#elif defined(MOZ_MEMORY_DARWIN)
	pthread_setspecific(tlsIndex, arenas[0]);
#else
	arenas_map = arenas[0];
#endif
#endif

	chunk_rtree = malloc_rtree_new((SIZEOF_PTR << 3) - opt_chunk_2pow);
	if (!chunk_rtree)
		return (true);

	malloc_initialized = true;

#if !defined(MOZ_MEMORY_WINDOWS) && !defined(MOZ_MEMORY_DARWIN)
	/* Prevent potential deadlock on malloc locks after fork. */
	pthread_atfork(_malloc_prefork, _malloc_postfork_parent, _malloc_postfork_child);
#endif

#if defined(MOZ_MEMORY_DARWIN)
	register_zone();
#endif

#ifndef MOZ_MEMORY_WINDOWS
	malloc_mutex_unlock(&init_lock);
#endif
	return (false);
}

/*
 * End general internal functions.
 */
/******************************************************************************/
/*
 * Begin malloc(3)-compatible functions.
 */

MOZ_MEMORY_API void *
malloc_impl(size_t size)
{
	void *ret;

	if (malloc_init()) {
		ret = nullptr;
		goto RETURN;
	}

	if (size == 0) {
		size = 1;
	}

	ret = imalloc(size);

RETURN:
	if (!ret) {
		errno = ENOMEM;
	}

	return (ret);
}

/*
 * In ELF systems the default visibility allows symbols to be preempted at
 * runtime. This in turn prevents the uses of memalign in this file from being
 * optimized. What we do in here is define two aliasing symbols (they point to
 * the same code): memalign and memalign_internal. The internal version has
 * hidden visibility and is used in every reference from this file.
 *
 * For more information on this technique, see section 2.2.7 (Avoid Using
 * Exported Symbols) in http://www.akkadia.org/drepper/dsohowto.pdf.
 */

#ifndef MOZ_REPLACE_MALLOC
#if defined(__GNUC__) && !defined(MOZ_MEMORY_DARWIN)
#define MOZ_MEMORY_ELF
#endif
#endif /* MOZ_REPLACE_MALLOC */

#ifdef MOZ_MEMORY_ELF
#define MEMALIGN memalign_internal
extern "C"
#else
#define MEMALIGN memalign_impl
MOZ_MEMORY_API
#endif
void *
MEMALIGN(size_t alignment, size_t size)
{
	void *ret;

	MOZ_ASSERT(((alignment - 1) & alignment) == 0);

	if (malloc_init()) {
		ret = nullptr;
		goto RETURN;
	}

	if (size == 0) {
		size = 1;
	}

	alignment = alignment < sizeof(void*) ? sizeof(void*) : alignment;
	ret = ipalloc(alignment, size);

RETURN:
	return (ret);
}

#ifdef MOZ_MEMORY_ELF
extern void *
memalign_impl(size_t alignment, size_t size) __attribute__((alias ("memalign_internal"), visibility ("default")));
#endif

MOZ_MEMORY_API int
posix_memalign_impl(void **memptr, size_t alignment, size_t size)
{
	void *result;

	/* Make sure that alignment is a large enough power of 2. */
	if (((alignment - 1) & alignment) != 0 || alignment < sizeof(void *)) {
		return (EINVAL);
	}

	/* The 0-->1 size promotion is done in the memalign() call below */

	result = MEMALIGN(alignment, size);

	if (!result)
		return (ENOMEM);

	*memptr = result;
	return (0);
}

MOZ_MEMORY_API void *
aligned_alloc_impl(size_t alignment, size_t size)
{
	if (size % alignment) {
		return nullptr;
	}
	return MEMALIGN(alignment, size);
}

MOZ_MEMORY_API void *
valloc_impl(size_t size)
{
	return (MEMALIGN(pagesize, size));
}

MOZ_MEMORY_API void *
calloc_impl(size_t num, size_t size)
{
	void *ret;
	size_t num_size;

	if (malloc_init()) {
		num_size = 0;
		ret = nullptr;
		goto RETURN;
	}

	num_size = num * size;
	if (num_size == 0) {
		num_size = 1;
	/*
	 * Try to avoid division here.  We know that it isn't possible to
	 * overflow during multiplication if neither operand uses any of the
	 * most significant half of the bits in a size_t.
	 */
	} else if (((num | size) & (SIZE_T_MAX << (sizeof(size_t) << 2)))
	    && (num_size / size != num)) {
		/* size_t overflow. */
		ret = nullptr;
		goto RETURN;
	}

	ret = icalloc(num_size);

RETURN:
	if (!ret) {
		errno = ENOMEM;
	}

	return (ret);
}

MOZ_MEMORY_API void *
realloc_impl(void *ptr, size_t size)
{
	void *ret;

	if (size == 0) {
		size = 1;
	}

	if (ptr) {
		MOZ_ASSERT(malloc_initialized);

		ret = iralloc(ptr, size);

		if (!ret) {
			errno = ENOMEM;
		}
	} else {
		if (malloc_init())
			ret = nullptr;
		else
			ret = imalloc(size);

		if (!ret) {
			errno = ENOMEM;
		}
	}

	return (ret);
}

MOZ_MEMORY_API void
free_impl(void *ptr)
{
	size_t offset;

	/*
	 * A version of idalloc that checks for nullptr pointer but only for
	 * huge allocations assuming that CHUNK_ADDR2OFFSET(nullptr) == 0.
	 */
	MOZ_ASSERT(CHUNK_ADDR2OFFSET(nullptr) == 0);
	offset = CHUNK_ADDR2OFFSET(ptr);
	if (offset != 0)
		arena_dalloc(ptr, offset);
	else if (ptr)
		huge_dalloc(ptr);
}

/*
 * End malloc(3)-compatible functions.
 */
/******************************************************************************/
/*
 * Begin non-standard functions.
 */

/* This was added by Mozilla for use by SQLite. */
MOZ_MEMORY_API size_t
malloc_good_size_impl(size_t size)
{
	/*
	 * This duplicates the logic in imalloc(), arena_malloc() and
	 * arena_malloc_small().
	 */
	if (size < small_min) {
		/* Small (tiny). */
		size = pow2_ceil(size);
		/*
		 * We omit the #ifdefs from arena_malloc_small() --
		 * it can be inaccurate with its size in some cases, but this
		 * function must be accurate.
		 */
		if (size < (1U << TINY_MIN_2POW))
			size = (1U << TINY_MIN_2POW);
	} else if (size <= small_max) {
		/* Small (quantum-spaced). */
		size = QUANTUM_CEILING(size);
	} else if (size <= bin_maxclass) {
		/* Small (sub-page). */
		size = pow2_ceil(size);
	} else if (size <= arena_maxclass) {
		/* Large. */
		size = PAGE_CEILING(size);
	} else {
		/*
		 * Huge.  We use PAGE_CEILING to get psize, instead of using
		 * CHUNK_CEILING to get csize.  This ensures that this
		 * malloc_usable_size(malloc(n)) always matches
		 * malloc_good_size(n).
		 */
		size = PAGE_CEILING(size);
	}
	return size;
}


MOZ_MEMORY_API size_t
malloc_usable_size_impl(MALLOC_USABLE_SIZE_CONST_PTR void *ptr)
{
	return (isalloc_validate(ptr));
}

MOZ_JEMALLOC_API void
jemalloc_stats_impl(jemalloc_stats_t *stats)
{
	size_t i, non_arena_mapped, chunk_header_size;

	MOZ_ASSERT(stats);

	/*
	 * Gather runtime settings.
	 */
	stats->opt_abort = opt_abort;
	stats->opt_junk = opt_junk;
	stats->opt_zero = opt_zero;
	stats->narenas = narenas;
	stats->quantum = quantum;
	stats->small_max = small_max;
	stats->large_max = arena_maxclass;
	stats->chunksize = chunksize;
	stats->dirty_max = opt_dirty_max;

	/*
	 * Gather current memory usage statistics.
	 */
	stats->mapped = 0;
	stats->allocated = 0;
        stats->waste = 0;
	stats->page_cache = 0;
        stats->bookkeeping = 0;
	stats->bin_unused = 0;

	non_arena_mapped = 0;

	/* Get huge mapped/allocated. */
	malloc_mutex_lock(&huge_mtx);
	non_arena_mapped += huge_mapped;
	stats->allocated += huge_allocated;
	MOZ_ASSERT(huge_mapped >= huge_allocated);
	malloc_mutex_unlock(&huge_mtx);

	/* Get base mapped/allocated. */
	malloc_mutex_lock(&base_mtx);
	non_arena_mapped += base_mapped;
	stats->bookkeeping += base_committed;
	MOZ_ASSERT(base_mapped >= base_committed);
	malloc_mutex_unlock(&base_mtx);

	malloc_spin_lock(&arenas_lock);
	/* Iterate over arenas. */
	for (i = 0; i < narenas; i++) {
		arena_t *arena = arenas[i];
		size_t arena_mapped, arena_allocated, arena_committed, arena_dirty, j,
		    arena_unused, arena_headers;
		arena_run_t* run;
		arena_chunk_map_t* mapelm;

		if (!arena) {
			continue;
		}

		arena_headers = 0;
		arena_unused = 0;

		malloc_spin_lock(&arena->lock);

		arena_mapped = arena->stats.mapped;

		/* "committed" counts dirty and allocated memory. */
		arena_committed = arena->stats.committed << pagesize_2pow;

		arena_allocated = arena->stats.allocated_small +
				  arena->stats.allocated_large;

		arena_dirty = arena->ndirty << pagesize_2pow;

		for (j = 0; j < ntbins + nqbins + nsbins; j++) {
			arena_bin_t* bin = &arena->bins[j];
			size_t bin_unused = 0;

			rb_foreach_begin(arena_chunk_map_t, link, &bin->runs, mapelm) {
				run = (arena_run_t *)(mapelm->bits & ~pagesize_mask);
				bin_unused += run->nfree * bin->reg_size;
			} rb_foreach_end(arena_chunk_map_t, link, &bin->runs, mapelm)

			if (bin->runcur) {
				bin_unused += bin->runcur->nfree * bin->reg_size;
			}

			arena_unused += bin_unused;
			arena_headers += bin->stats.curruns * bin->reg0_offset;
		}

		malloc_spin_unlock(&arena->lock);

		MOZ_ASSERT(arena_mapped >= arena_committed);
		MOZ_ASSERT(arena_committed >= arena_allocated + arena_dirty);

		/* "waste" is committed memory that is neither dirty nor
		 * allocated. */
		stats->mapped += arena_mapped;
		stats->allocated += arena_allocated;
		stats->page_cache += arena_dirty;
		stats->waste += arena_committed -
		    arena_allocated - arena_dirty - arena_unused - arena_headers;
		stats->bin_unused += arena_unused;
		stats->bookkeeping += arena_headers;
	}
	malloc_spin_unlock(&arenas_lock);

	/* Account for arena chunk headers in bookkeeping rather than waste. */
	chunk_header_size =
	    ((stats->mapped / stats->chunksize) * arena_chunk_header_npages) <<
	    pagesize_2pow;

	stats->mapped += non_arena_mapped;
	stats->bookkeeping += chunk_header_size;
	stats->waste -= chunk_header_size;

	MOZ_ASSERT(stats->mapped >= stats->allocated + stats->waste +
				stats->page_cache + stats->bookkeeping);
}

#ifdef MALLOC_DOUBLE_PURGE

/* Explicitly remove all of this chunk's MADV_FREE'd pages from memory. */
static void
hard_purge_chunk(arena_chunk_t *chunk)
{
	/* See similar logic in arena_purge(). */

	size_t i;
	for (i = arena_chunk_header_npages; i < chunk_npages; i++) {
		/* Find all adjacent pages with CHUNK_MAP_MADVISED set. */
		size_t npages;
		for (npages = 0;
		     chunk->map[i + npages].bits & CHUNK_MAP_MADVISED && i + npages < chunk_npages;
		     npages++) {
			/* Turn off the chunk's MADV_FREED bit and turn on its
			 * DECOMMITTED bit. */
			MOZ_DIAGNOSTIC_ASSERT(!(chunk->map[i + npages].bits & CHUNK_MAP_DECOMMITTED));
			chunk->map[i + npages].bits ^= CHUNK_MAP_MADVISED_OR_DECOMMITTED;
		}

		/* We could use mincore to find out which pages are actually
		 * present, but it's not clear that's better. */
		if (npages > 0) {
			pages_decommit(((char*)chunk) + (i << pagesize_2pow), npages << pagesize_2pow);
			pages_commit(((char*)chunk) + (i << pagesize_2pow), npages << pagesize_2pow);
		}
		i += npages;
	}
}

/* Explicitly remove all of this arena's MADV_FREE'd pages from memory. */
static void
hard_purge_arena(arena_t *arena)
{
	malloc_spin_lock(&arena->lock);

	while (!LinkedList_IsEmpty(&arena->chunks_madvised)) {
		arena_chunk_t *chunk =
			LinkedList_Get(arena->chunks_madvised.next,
				       arena_chunk_t, chunks_madvised_elem);
		hard_purge_chunk(chunk);
		LinkedList_Remove(&chunk->chunks_madvised_elem);
	}

	malloc_spin_unlock(&arena->lock);
}

MOZ_JEMALLOC_API void
jemalloc_purge_freed_pages_impl()
{
	size_t i;
	malloc_spin_lock(&arenas_lock);
	for (i = 0; i < narenas; i++) {
		arena_t *arena = arenas[i];
		if (arena)
			hard_purge_arena(arena);
	}
	malloc_spin_unlock(&arenas_lock);
}

#else /* !defined MALLOC_DOUBLE_PURGE */

MOZ_JEMALLOC_API void
jemalloc_purge_freed_pages_impl()
{
	/* Do nothing. */
}

#endif /* defined MALLOC_DOUBLE_PURGE */



#ifdef MOZ_MEMORY_WINDOWS
void*
_recalloc(void *ptr, size_t count, size_t size)
{
	size_t oldsize = ptr ? isalloc(ptr) : 0;
	size_t newsize = count * size;

	/*
	 * In order for all trailing bytes to be zeroed, the caller needs to
	 * use calloc(), followed by recalloc().  However, the current calloc()
	 * implementation only zeros the bytes requested, so if recalloc() is
	 * to work 100% correctly, calloc() will need to change to zero
	 * trailing bytes.
	 */

	ptr = realloc_impl(ptr, newsize);
	if (ptr && oldsize < newsize) {
		memset((void *)((uintptr_t)ptr + oldsize), 0, newsize -
		    oldsize);
	}

	return ptr;
}

/*
 * This impl of _expand doesn't ever actually expand or shrink blocks: it
 * simply replies that you may continue using a shrunk block.
 */
void*
_expand(void *ptr, size_t newsize)
{
	if (isalloc(ptr) >= newsize)
		return ptr;

	return nullptr;
}

size_t
_msize(void *ptr)
{

	return malloc_usable_size_impl(ptr);
}
#endif

MOZ_JEMALLOC_API void
jemalloc_free_dirty_pages_impl(void)
{
	size_t i;
	malloc_spin_lock(&arenas_lock);
	for (i = 0; i < narenas; i++) {
		arena_t *arena = arenas[i];

		if (arena) {
			malloc_spin_lock(&arena->lock);
			arena_purge(arena, true);
			malloc_spin_unlock(&arena->lock);
		}
	}
	malloc_spin_unlock(&arenas_lock);
}

/*
 * End non-standard functions.
 */
/******************************************************************************/
/*
 * Begin library-private functions, used by threading libraries for protection
 * of malloc during fork().  These functions are only called if the program is
 * running in threaded mode, so there is no need to check whether the program
 * is threaded here.
 */

#ifndef MOZ_MEMORY_DARWIN
static
#endif
void
_malloc_prefork(void)
{
	unsigned i;

	/* Acquire all mutexes in a safe order. */

	malloc_spin_lock(&arenas_lock);
	for (i = 0; i < narenas; i++) {
		if (arenas[i])
			malloc_spin_lock(&arenas[i]->lock);
	}

	malloc_mutex_lock(&base_mtx);

	malloc_mutex_lock(&huge_mtx);
}

#ifndef MOZ_MEMORY_DARWIN
static
#endif
void
_malloc_postfork_parent(void)
{
	unsigned i;

	/* Release all mutexes, now that fork() has completed. */

	malloc_mutex_unlock(&huge_mtx);

	malloc_mutex_unlock(&base_mtx);

	for (i = 0; i < narenas; i++) {
		if (arenas[i])
			malloc_spin_unlock(&arenas[i]->lock);
	}
	malloc_spin_unlock(&arenas_lock);
}

#ifndef MOZ_MEMORY_DARWIN
static
#endif
void
_malloc_postfork_child(void)
{
	unsigned i;

	/* Reinitialize all mutexes, now that fork() has completed. */

	malloc_mutex_init(&huge_mtx);

	malloc_mutex_init(&base_mtx);

	for (i = 0; i < narenas; i++) {
		if (arenas[i])
			malloc_spin_init(&arenas[i]->lock);
	}
	malloc_spin_init(&arenas_lock);
}

/*
 * End library-private functions.
 */
/******************************************************************************/

#ifdef HAVE_DLOPEN
#  include <dlfcn.h>
#endif

#if defined(MOZ_MEMORY_DARWIN)

__attribute__((constructor))
void
jemalloc_darwin_init(void)
{
	if (malloc_init_hard())
		moz_abort();
}

#endif

/*
 * is_malloc(malloc_impl) is some macro magic to detect if malloc_impl is
 * defined as "malloc" in mozmemory_wrap.h
 */
#define malloc_is_malloc 1
#define is_malloc_(a) malloc_is_ ## a
#define is_malloc(a) is_malloc_(a)

#if !defined(MOZ_MEMORY_DARWIN) && (is_malloc(malloc_impl) == 1)
#  if defined(__GLIBC__) && !defined(__UCLIBC__)
/*
 * glibc provides the RTLD_DEEPBIND flag for dlopen which can make it possible
 * to inconsistently reference libc's malloc(3)-compatible functions
 * (bug 493541).
 *
 * These definitions interpose hooks in glibc.  The functions are actually
 * passed an extra argument for the caller return address, which will be
 * ignored.
 */
extern "C" {
MOZ_EXPORT void (*__free_hook)(void *ptr) = free_impl;
MOZ_EXPORT void *(*__malloc_hook)(size_t size) = malloc_impl;
MOZ_EXPORT void *(*__realloc_hook)(void *ptr, size_t size) = realloc_impl;
MOZ_EXPORT void *(*__memalign_hook)(size_t alignment, size_t size) = MEMALIGN;
}

#  elif defined(RTLD_DEEPBIND)
/*
 * XXX On systems that support RTLD_GROUP or DF_1_GROUP, do their
 * implementations permit similar inconsistencies?  Should STV_SINGLETON
 * visibility be used for interposition where available?
 */
#    error "Interposing malloc is unsafe on this system without libc malloc hooks."
#  endif
#endif

#ifdef MOZ_MEMORY_WINDOWS
/*
 * In the new style jemalloc integration jemalloc is built as a separate
 * shared library.  Since we're no longer hooking into the CRT binary,
 * we need to initialize the heap at the first opportunity we get.
 * DLL_PROCESS_ATTACH in DllMain is that opportunity.
 */
BOOL APIENTRY DllMain(HINSTANCE hModule,
                      DWORD reason,
                      LPVOID lpReserved)
{
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      /* Don't force the system to page DllMain back in every time
       * we create/destroy a thread */
      DisableThreadLibraryCalls(hModule);
      /* Initialize the heap */
      malloc_init_hard();
      break;

    case DLL_PROCESS_DETACH:
      break;

  }

  return TRUE;
}
#endif
