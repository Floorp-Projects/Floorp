/* -*- Mode: C; tab-width: 8; c-basic-offset: 8; indent-tabs-mode: t -*- */
/* vim:set softtabstop=8 shiftwidth=8 noet: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Portions of this file were originally under the following license:
//
// Copyright (C) 2006-2008 Jason Evans <jasone@FreeBSD.org>.
// All rights reserved.
// Copyright (C) 2007-2017 Mozilla Foundation.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice(s), this list of conditions and the following disclaimer as
//    the first lines of this file unmodified other than the possible
//    addition of one or more copyright notices.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice(s), this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the
//    distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
// EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// *****************************************************************************
//
// This allocator implementation is designed to provide scalable performance
// for multi-threaded programs on multi-processor systems.  The following
// features are included for this purpose:
//
//   + Multiple arenas are used if there are multiple CPUs, which reduces lock
//     contention and cache sloshing.
//
//   + Cache line sharing between arenas is avoided for internal data
//     structures.
//
//   + Memory is managed in chunks and runs (chunks can be split into runs),
//     rather than as individual pages.  This provides a constant-time
//     mechanism for associating allocations with particular arenas.
//
// Allocation requests are rounded up to the nearest size class, and no record
// of the original request size is maintained.  Allocations are broken into
// categories according to size class.  Assuming runtime defaults, 4 kB pages
// and a 16 byte quantum on a 32-bit system, the size classes in each category
// are as follows:
//
//   |=====================================|
//   | Category | Subcategory    |    Size |
//   |=====================================|
//   | Small    | Tiny           |       2 |
//   |          |                |       4 |
//   |          |                |       8 |
//   |          |----------------+---------|
//   |          | Quantum-spaced |      16 |
//   |          |                |      32 |
//   |          |                |      48 |
//   |          |                |     ... |
//   |          |                |     480 |
//   |          |                |     496 |
//   |          |                |     512 |
//   |          |----------------+---------|
//   |          | Sub-page       |    1 kB |
//   |          |                |    2 kB |
//   |=====================================|
//   | Large                     |    4 kB |
//   |                           |    8 kB |
//   |                           |   12 kB |
//   |                           |     ... |
//   |                           | 1012 kB |
//   |                           | 1016 kB |
//   |                           | 1020 kB |
//   |=====================================|
//   | Huge                      |    1 MB |
//   |                           |    2 MB |
//   |                           |    3 MB |
//   |                           |     ... |
//   |=====================================|
//
// NOTE: Due to Mozilla bug 691003, we cannot reserve less than one word for an
// allocation on Linux or Mac.  So on 32-bit *nix, the smallest bucket size is
// 4 bytes, and on 64-bit, the smallest bucket size is 8 bytes.
//
// A different mechanism is used for each category:
//
//   Small : Each size class is segregated into its own set of runs.  Each run
//           maintains a bitmap of which regions are free/allocated.
//
//   Large : Each allocation is backed by a dedicated run.  Metadata are stored
//           in the associated arena chunk header maps.
//
//   Huge : Each allocation is backed by a dedicated contiguous set of chunks.
//          Metadata are stored in a separate red-black tree.
//
// *****************************************************************************

#include "mozmemory_wrap.h"
#include "mozjemalloc.h"
#include "mozilla/Atomics.h"
#include "mozilla/Alignment.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DoublyLinkedList.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/Likely.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Sprintf.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/fallible.h"
#include "Utils.h"

// On Linux, we use madvise(MADV_DONTNEED) to release memory back to the
// operating system.  If we release 1MB of live pages with MADV_DONTNEED, our
// RSS will decrease by 1MB (almost) immediately.
//
// On Mac, we use madvise(MADV_FREE).  Unlike MADV_DONTNEED on Linux, MADV_FREE
// on Mac doesn't cause the OS to release the specified pages immediately; the
// OS keeps them in our process until the machine comes under memory pressure.
//
// It's therefore difficult to measure the process's RSS on Mac, since, in the
// absence of memory pressure, the contribution from the heap to RSS will not
// decrease due to our madvise calls.
//
// We therefore define MALLOC_DOUBLE_PURGE on Mac.  This causes jemalloc to
// track which pages have been MADV_FREE'd.  You can then call
// jemalloc_purge_freed_pages(), which will force the OS to release those
// MADV_FREE'd pages, making the process's RSS reflect its true memory usage.
//
// The jemalloc_purge_freed_pages definition in memory/build/mozmemory.h needs
// to be adjusted if MALLOC_DOUBLE_PURGE is ever enabled on Linux.

#ifdef XP_DARWIN
#define MALLOC_DOUBLE_PURGE
#endif

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

using namespace mozilla;

#ifdef XP_WIN

// Some defines from the CRT internal headers that we need here.
#define _CRT_SPINCOUNT 5000
#include <io.h>
#include <windows.h>
#include <intrin.h>

#define STDERR_FILENO 2

// Implement getenv without using malloc.
static char mozillaMallocOptionsBuf[64];

#define getenv xgetenv
static char*
getenv(const char* name)
{

  if (GetEnvironmentVariableA(
        name, mozillaMallocOptionsBuf, sizeof(mozillaMallocOptionsBuf)) > 0) {
    return mozillaMallocOptionsBuf;
  }

  return nullptr;
}

#if defined(_WIN64)
typedef long long ssize_t;
#else
typedef long ssize_t;
#endif

#define MALLOC_DECOMMIT
#endif

#ifndef XP_WIN
#ifndef XP_SOLARIS
#include <sys/cdefs.h>
#endif
#include <sys/mman.h>
#ifndef MADV_FREE
#define MADV_FREE MADV_DONTNEED
#endif
#ifndef MAP_NOSYNC
#define MAP_NOSYNC 0
#endif
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#if !defined(XP_SOLARIS) && !defined(ANDROID)
#include <sys/sysctl.h>
#endif
#include <sys/uio.h>

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef XP_DARWIN
#include <libkern/OSAtomic.h>
#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#include <malloc/malloc.h>
#endif

#endif

#include "mozilla/ThreadLocal.h"
#include "mozjemalloc_types.h"

// Some tools, such as /dev/dsp wrappers, LD_PRELOAD libraries that
// happen to override mmap() and call dlsym() from their overridden
// mmap(). The problem is that dlsym() calls malloc(), and this ends
// up in a dead lock in jemalloc.
// On these systems, we prefer to directly use the system call.
// We do that for Linux systems and kfreebsd with GNU userland.
// Note sanity checks are not done (alignment of offset, ...) because
// the uses of mmap are pretty limited, in jemalloc.
//
// On Alpha, glibc has a bug that prevents syscall() to work for system
// calls with 6 arguments.
#if (defined(XP_LINUX) && !defined(__alpha__)) ||                              \
  (defined(__FreeBSD_kernel__) && defined(__GLIBC__))
#include <sys/syscall.h>
#if defined(SYS_mmap) || defined(SYS_mmap2)
static inline void*
_mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset)
{
// S390 only passes one argument to the mmap system call, which is a
// pointer to a structure containing the arguments.
#ifdef __s390__
  struct
  {
    void* addr;
    size_t length;
    long prot;
    long flags;
    long fd;
    off_t offset;
  } args = { addr, length, prot, flags, fd, offset };
  return (void*)syscall(SYS_mmap, &args);
#else
#if defined(ANDROID) && defined(__aarch64__) && defined(SYS_mmap2)
// Android NDK defines SYS_mmap2 for AArch64 despite it not supporting mmap2.
#undef SYS_mmap2
#endif
#ifdef SYS_mmap2
  return (void*)syscall(SYS_mmap2, addr, length, prot, flags, fd, offset >> 12);
#else
  return (void*)syscall(SYS_mmap, addr, length, prot, flags, fd, offset);
#endif
#endif
}
#define mmap _mmap
#define munmap(a, l) syscall(SYS_munmap, a, l)
#endif
#endif

// Size of stack-allocated buffer passed to strerror_r().
#define STRERROR_BUF 64

// Minimum alignment of non-tiny allocations is 2^QUANTUM_2POW_MIN bytes.
#define QUANTUM_2POW_MIN 4

#include "rb.h"

// sizeof(int) == (1U << SIZEOF_INT_2POW).
#ifndef SIZEOF_INT_2POW
#define SIZEOF_INT_2POW 2
#endif

// Size and alignment of memory chunks that are allocated by the OS's virtual
// memory system.
#define CHUNK_2POW_DEFAULT 20
// Maximum number of dirty pages per arena.
#define DIRTY_MAX_DEFAULT (1U << 8)

static size_t opt_dirty_max = DIRTY_MAX_DEFAULT;

// Maximum size of L1 cache line.  This is used to avoid cache line aliasing,
// so over-estimates are okay (up to a point), but under-estimates will
// negatively affect performance.
#define CACHELINE_2POW 6
#define CACHELINE ((size_t)(1U << CACHELINE_2POW))

// Smallest size class to support.  On Windows the smallest allocation size
// must be 8 bytes on 32-bit, 16 bytes on 64-bit.  On Linux and Mac, even
// malloc(1) must reserve a word's worth of memory (see Mozilla bug 691003).
#ifdef XP_WIN
#define TINY_MIN_2POW (sizeof(void*) == 8 ? 4 : 3)
#else
#define TINY_MIN_2POW (sizeof(void*) == 8 ? 3 : 2)
#endif

// Maximum size class that is a multiple of the quantum, but not (necessarily)
// a power of 2.  Above this size, allocations are rounded up to the nearest
// power of 2.
#define SMALL_MAX_2POW_DEFAULT 9
#define SMALL_MAX_DEFAULT (1U << SMALL_MAX_2POW_DEFAULT)

// RUN_MAX_OVRHD indicates maximum desired run header overhead.  Runs are sized
// as small as possible such that this setting is still honored, without
// violating other constraints.  The goal is to make runs as small as possible
// without exceeding a per run external fragmentation threshold.
//
// We use binary fixed point math for overhead computations, where the binary
// point is implicitly RUN_BFP bits to the left.
//
// Note that it is possible to set RUN_MAX_OVRHD low enough that it cannot be
// honored for some/all object sizes, since there is one bit of header overhead
// per object (plus a constant).  This constraint is relaxed (ignored) for runs
// that are so small that the per-region overhead is greater than:
//
//   (RUN_MAX_OVRHD / (reg_size << (3+RUN_BFP))
#define RUN_BFP 12
//                                    \/   Implicit binary fixed point.
#define RUN_MAX_OVRHD 0x0000003dU
#define RUN_MAX_OVRHD_RELAX 0x00001800U

// When MALLOC_STATIC_PAGESIZE is defined, the page size is fixed at
// compile-time for better performance, as opposed to determined at
// runtime. Some platforms can have different page sizes at runtime
// depending on kernel configuration, so they are opted out by default.
// Debug builds are opted out too, for test coverage.
#ifndef MOZ_DEBUG
#if !defined(__ia64__) && !defined(__sparc__) && !defined(__mips__) &&         \
  !defined(__aarch64__)
#define MALLOC_STATIC_PAGESIZE 1
#endif
#endif

// Various quantum-related settings.
#define QUANTUM_DEFAULT (size_t(1) << QUANTUM_2POW_MIN)
static const size_t quantum = QUANTUM_DEFAULT;
static const size_t quantum_mask = QUANTUM_DEFAULT - 1;

// Various bin-related settings.
static const size_t small_min = (QUANTUM_DEFAULT >> 1) + 1;
static const size_t small_max = size_t(SMALL_MAX_DEFAULT);

// Number of (2^n)-spaced tiny bins.
static const unsigned ntbins = unsigned(QUANTUM_2POW_MIN - TINY_MIN_2POW);

// Number of quantum-spaced bins.
static const unsigned nqbins = unsigned(SMALL_MAX_DEFAULT >> QUANTUM_2POW_MIN);

#ifdef MALLOC_STATIC_PAGESIZE

// VM page size. It must divide the runtime CPU page size or the code
// will abort.
// Platform specific page size conditions copied from js/public/HeapAPI.h
#if (defined(SOLARIS) || defined(__FreeBSD__)) &&                              \
  (defined(__sparc) || defined(__sparcv9) || defined(__ia64))
#define pagesize_2pow (size_t(13))
#elif defined(__powerpc64__)
#define pagesize_2pow (size_t(16))
#else
#define pagesize_2pow (size_t(12))
#endif
#define pagesize (size_t(1) << pagesize_2pow)
#define pagesize_mask (pagesize - 1)

// Max size class for bins.
static const size_t bin_maxclass = pagesize >> 1;

// Number of (2^n)-spaced sub-page bins.
static const unsigned nsbins =
  unsigned(pagesize_2pow - SMALL_MAX_2POW_DEFAULT - 1);

#else // !MALLOC_STATIC_PAGESIZE

// VM page size.
static size_t pagesize;
static size_t pagesize_mask;
static size_t pagesize_2pow;

// Various bin-related settings.
static size_t bin_maxclass; // Max size class for bins.
static unsigned nsbins;     // Number of (2^n)-spaced sub-page bins.
#endif

// Various chunk-related settings.

// Compute the header size such that it is large enough to contain the page map
// and enough nodes for the worst case: one node per non-header page plus one
// extra for situations where we briefly have one more node allocated than we
// will need.
#define calculate_arena_header_size()                                          \
  (sizeof(arena_chunk_t) + sizeof(arena_chunk_map_t) * (chunk_npages - 1))

#define calculate_arena_header_pages()                                         \
  ((calculate_arena_header_size() >> pagesize_2pow) +                          \
   ((calculate_arena_header_size() & pagesize_mask) ? 1 : 0))

// Max size class for arenas.
#define calculate_arena_maxclass()                                             \
  (chunksize - (arena_chunk_header_npages << pagesize_2pow))

#define CHUNKSIZE_DEFAULT ((size_t)1 << CHUNK_2POW_DEFAULT)
static const size_t chunksize = CHUNKSIZE_DEFAULT;
static const size_t chunksize_mask = CHUNKSIZE_DEFAULT - 1;

#ifdef MALLOC_STATIC_PAGESIZE
static const size_t chunk_npages = CHUNKSIZE_DEFAULT >> pagesize_2pow;
#define arena_chunk_header_npages calculate_arena_header_pages()
#define arena_maxclass calculate_arena_maxclass()
#else
static size_t chunk_npages;
static size_t arena_chunk_header_npages;
static size_t arena_maxclass; // Max size class for arenas.
#endif

// Recycle at most 128 chunks. With 1 MiB chunks, this means we retain at most
// 6.25% of the process address space on a 32-bit OS for later use.
#define CHUNK_RECYCLE_LIMIT 128

static const size_t gRecycleLimit = CHUNK_RECYCLE_LIMIT * CHUNKSIZE_DEFAULT;

// The current amount of recycled bytes, updated atomically.
static Atomic<size_t, ReleaseAcquire> gRecycledSize;

// ***************************************************************************
// MALLOC_DECOMMIT and MALLOC_DOUBLE_PURGE are mutually exclusive.
#if defined(MALLOC_DECOMMIT) && defined(MALLOC_DOUBLE_PURGE)
#error MALLOC_DECOMMIT and MALLOC_DOUBLE_PURGE are mutually exclusive.
#endif

static void*
base_alloc(size_t aSize);

// Mutexes based on spinlocks.  We can't use normal pthread spinlocks in all
// places, because they require malloc()ed memory, which causes bootstrapping
// issues in some cases.
struct Mutex
{
#if defined(XP_WIN)
  CRITICAL_SECTION mMutex;
#elif defined(XP_DARWIN)
  OSSpinLock mMutex;
#else
  pthread_mutex_t mMutex;
#endif

  inline bool Init();

  inline void Lock();

  inline void Unlock();
};

struct MOZ_RAII MutexAutoLock
{
  explicit MutexAutoLock(Mutex& aMutex MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mMutex(aMutex)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    mMutex.Lock();
  }

  ~MutexAutoLock() { mMutex.Unlock(); }

private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER;
  Mutex& mMutex;
};

// Set to true once the allocator has been initialized.
static Atomic<bool> malloc_initialized(false);

#if defined(XP_WIN)
// No init lock for Windows.
#elif defined(XP_DARWIN)
static Mutex gInitLock = { OS_SPINLOCK_INIT };
#elif defined(XP_LINUX) && !defined(ANDROID)
static Mutex gInitLock = { PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP };
#else
static Mutex gInitLock = { PTHREAD_MUTEX_INITIALIZER };
#endif

// ***************************************************************************
// Statistics data structures.

struct malloc_bin_stats_t
{
  // Current number of runs in this bin.
  unsigned long curruns;
};

struct arena_stats_t
{
  // Number of bytes currently mapped.
  size_t mapped;

  // Current number of committed pages.
  size_t committed;

  // Per-size-category statistics.
  size_t allocated_small;

  size_t allocated_large;
};

// ***************************************************************************
// Extent data structures.

enum ChunkType
{
  UNKNOWN_CHUNK,
  ZEROED_CHUNK,   // chunk only contains zeroes.
  ARENA_CHUNK,    // used to back arena runs created by arena_t::AllocRun.
  HUGE_CHUNK,     // used to back huge allocations (e.g. huge_malloc).
  RECYCLED_CHUNK, // chunk has been stored for future use by chunk_recycle.
};

// Tree of extents.
struct extent_node_t
{
  // Linkage for the size/address-ordered tree.
  RedBlackTreeNode<extent_node_t> link_szad;

  // Linkage for the address-ordered tree.
  RedBlackTreeNode<extent_node_t> link_ad;

  // Pointer to the extent that this tree node is responsible for.
  void* addr;

  // Total region size.
  size_t size;

  // What type of chunk is there; used by chunk recycling code.
  ChunkType chunk_type;
};

template<typename T>
int
CompareAddr(T* aAddr1, T* aAddr2)
{
  uintptr_t addr1 = reinterpret_cast<uintptr_t>(aAddr1);
  uintptr_t addr2 = reinterpret_cast<uintptr_t>(aAddr2);

  return (addr1 > addr2) - (addr1 < addr2);
}

struct ExtentTreeSzTrait
{
  static RedBlackTreeNode<extent_node_t>& GetTreeNode(extent_node_t* aThis)
  {
    return aThis->link_szad;
  }

  static inline int Compare(extent_node_t* aNode, extent_node_t* aOther)
  {
    int ret = (aNode->size > aOther->size) - (aNode->size < aOther->size);
    return ret ? ret : CompareAddr(aNode->addr, aOther->addr);
  }
};

struct ExtentTreeTrait
{
  static RedBlackTreeNode<extent_node_t>& GetTreeNode(extent_node_t* aThis)
  {
    return aThis->link_ad;
  }

  static inline int Compare(extent_node_t* aNode, extent_node_t* aOther)
  {
    return CompareAddr(aNode->addr, aOther->addr);
  }
};

struct ExtentTreeBoundsTrait : public ExtentTreeTrait
{
  static inline int Compare(extent_node_t* aKey, extent_node_t* aNode)
  {
    uintptr_t key_addr = reinterpret_cast<uintptr_t>(aKey->addr);
    uintptr_t node_addr = reinterpret_cast<uintptr_t>(aNode->addr);
    size_t node_size = aNode->size;

    // Is aKey within aNode?
    if (node_addr <= key_addr && key_addr < node_addr + node_size) {
      return 0;
    }

    return (key_addr > node_addr) - (key_addr < node_addr);
  }
};

// ***************************************************************************
// Radix tree data structures.
//
// The number of bits passed to the template is the number of significant bits
// in an address to do a radix lookup with.
//
// An address is looked up by splitting it in kBitsPerLevel bit chunks, except
// the most significant bits, where the bit chunk is kBitsAtLevel1 which can be
// different if Bits is not a multiple of kBitsPerLevel.
//
// With e.g. sizeof(void*)=4, Bits=16 and kBitsPerLevel=8, an address is split
// like the following:
// 0x12345678 -> mRoot[0x12][0x34]
template<size_t Bits>
class AddressRadixTree
{
// Size of each radix tree node (as a power of 2).
// This impacts tree depth.
#ifdef HAVE_64BIT_BUILD
  static const size_t kNodeSize2Pow = CACHELINE_2POW;
#else
  static const size_t kNodeSize2Pow = 14;
#endif
  static const size_t kBitsPerLevel = kNodeSize2Pow - LOG2(sizeof(void*));
  static const size_t kBitsAtLevel1 =
    (Bits % kBitsPerLevel) ? Bits % kBitsPerLevel : kBitsPerLevel;
  static const size_t kHeight = (Bits + kBitsPerLevel - 1) / kBitsPerLevel;
  static_assert(kBitsAtLevel1 + (kHeight - 1) * kBitsPerLevel == Bits,
                "AddressRadixTree parameters don't work out");

  Mutex mLock;
  void** mRoot;

public:
  bool Init();

  inline void* Get(void* aAddr);

  // Returns whether the value was properly set.
  inline bool Set(void* aAddr, void* aValue);

  inline bool Unset(void* aAddr) { return Set(aAddr, nullptr); }

private:
  inline void** GetSlot(void* aAddr, bool aCreate = false);
};

// ***************************************************************************
// Arena data structures.

struct arena_t;
struct arena_bin_t;

// Each element of the chunk map corresponds to one page within the chunk.
struct arena_chunk_map_t
{
  // Linkage for run trees.  There are two disjoint uses:
  //
  // 1) arena_t's tree or available runs.
  // 2) arena_run_t conceptually uses this linkage for in-use non-full
  //    runs, rather than directly embedding linkage.
  RedBlackTreeNode<arena_chunk_map_t> link;

  // Run address (or size) and various flags are stored together.  The bit
  // layout looks like (assuming 32-bit system):
  //
  //   ???????? ???????? ????---- -mckdzla
  //
  // ? : Unallocated: Run address for first/last pages, unset for internal
  //                  pages.
  //     Small: Run address.
  //     Large: Run size for first page, unset for trailing pages.
  // - : Unused.
  // m : MADV_FREE/MADV_DONTNEED'ed?
  // c : decommitted?
  // k : key?
  // d : dirty?
  // z : zeroed?
  // l : large?
  // a : allocated?
  //
  // Following are example bit patterns for the three types of runs.
  //
  // r : run address
  // s : run size
  // x : don't care
  // - : 0
  // [cdzla] : bit set
  //
  //   Unallocated:
  //     ssssssss ssssssss ssss---- --c-----
  //     xxxxxxxx xxxxxxxx xxxx---- ----d---
  //     ssssssss ssssssss ssss---- -----z--
  //
  //   Small:
  //     rrrrrrrr rrrrrrrr rrrr---- -------a
  //     rrrrrrrr rrrrrrrr rrrr---- -------a
  //     rrrrrrrr rrrrrrrr rrrr---- -------a
  //
  //   Large:
  //     ssssssss ssssssss ssss---- ------la
  //     -------- -------- -------- ------la
  //     -------- -------- -------- ------la
  size_t bits;

// Note that CHUNK_MAP_DECOMMITTED's meaning varies depending on whether
// MALLOC_DECOMMIT and MALLOC_DOUBLE_PURGE are defined.
//
// If MALLOC_DECOMMIT is defined, a page which is CHUNK_MAP_DECOMMITTED must be
// re-committed with pages_commit() before it may be touched.  If
// MALLOC_DECOMMIT is defined, MALLOC_DOUBLE_PURGE may not be defined.
//
// If neither MALLOC_DECOMMIT nor MALLOC_DOUBLE_PURGE is defined, pages which
// are madvised (with either MADV_DONTNEED or MADV_FREE) are marked with
// CHUNK_MAP_MADVISED.
//
// Otherwise, if MALLOC_DECOMMIT is not defined and MALLOC_DOUBLE_PURGE is
// defined, then a page which is madvised is marked as CHUNK_MAP_MADVISED.
// When it's finally freed with jemalloc_purge_freed_pages, the page is marked
// as CHUNK_MAP_DECOMMITTED.
#define CHUNK_MAP_MADVISED ((size_t)0x40U)
#define CHUNK_MAP_DECOMMITTED ((size_t)0x20U)
#define CHUNK_MAP_MADVISED_OR_DECOMMITTED                                      \
  (CHUNK_MAP_MADVISED | CHUNK_MAP_DECOMMITTED)
#define CHUNK_MAP_KEY ((size_t)0x10U)
#define CHUNK_MAP_DIRTY ((size_t)0x08U)
#define CHUNK_MAP_ZEROED ((size_t)0x04U)
#define CHUNK_MAP_LARGE ((size_t)0x02U)
#define CHUNK_MAP_ALLOCATED ((size_t)0x01U)
};

struct ArenaChunkMapLink
{
  static RedBlackTreeNode<arena_chunk_map_t>& GetTreeNode(
    arena_chunk_map_t* aThis)
  {
    return aThis->link;
  }
};

struct ArenaRunTreeTrait : public ArenaChunkMapLink
{
  static inline int Compare(arena_chunk_map_t* aNode, arena_chunk_map_t* aOther)
  {
    MOZ_ASSERT(aNode);
    MOZ_ASSERT(aOther);
    return CompareAddr(aNode, aOther);
  }
};

struct ArenaAvailTreeTrait : public ArenaChunkMapLink
{
  static inline int Compare(arena_chunk_map_t* aNode, arena_chunk_map_t* aOther)
  {
    size_t size1 = aNode->bits & ~pagesize_mask;
    size_t size2 = aOther->bits & ~pagesize_mask;
    int ret = (size1 > size2) - (size1 < size2);
    return ret ? ret
               : CompareAddr((aNode->bits & CHUNK_MAP_KEY) ? nullptr : aNode,
                             aOther);
  }
};

// Arena chunk header.
struct arena_chunk_t
{
  // Arena that owns the chunk.
  arena_t* arena;

  // Linkage for the arena's tree of dirty chunks.
  RedBlackTreeNode<arena_chunk_t> link_dirty;

#ifdef MALLOC_DOUBLE_PURGE
  // If we're double-purging, we maintain a linked list of chunks which
  // have pages which have been madvise(MADV_FREE)'d but not explicitly
  // purged.
  //
  // We're currently lazy and don't remove a chunk from this list when
  // all its madvised pages are recommitted.
  DoublyLinkedListElement<arena_chunk_t> chunks_madvised_elem;
#endif

  // Number of dirty pages.
  size_t ndirty;

  // Map of pages within chunk that keeps track of free/large/small.
  arena_chunk_map_t map[1]; // Dynamically sized.
};

struct ArenaDirtyChunkTrait
{
  static RedBlackTreeNode<arena_chunk_t>& GetTreeNode(arena_chunk_t* aThis)
  {
    return aThis->link_dirty;
  }

  static inline int Compare(arena_chunk_t* aNode, arena_chunk_t* aOther)
  {
    MOZ_ASSERT(aNode);
    MOZ_ASSERT(aOther);
    return CompareAddr(aNode, aOther);
  }
};

#ifdef MALLOC_DOUBLE_PURGE
namespace mozilla {

template<>
struct GetDoublyLinkedListElement<arena_chunk_t>
{
  static DoublyLinkedListElement<arena_chunk_t>& Get(arena_chunk_t* aThis)
  {
    return aThis->chunks_madvised_elem;
  }
};
}
#endif

struct arena_run_t
{
#if defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
  uint32_t magic;
#define ARENA_RUN_MAGIC 0x384adf93
#endif

  // Bin this run is associated with.
  arena_bin_t* bin;

  // Index of first element that might have a free region.
  unsigned regs_minelm;

  // Number of free regions in run.
  unsigned nfree;

  // Bitmask of in-use regions (0: in use, 1: free).
  unsigned regs_mask[1]; // Dynamically sized.
};

struct arena_bin_t
{
  // Current run being used to service allocations of this bin's size
  // class.
  arena_run_t* runcur;

  // Tree of non-full runs.  This tree is used when looking for an
  // existing run when runcur is no longer usable.  We choose the
  // non-full run that is lowest in memory; this policy tends to keep
  // objects packed well, and it can also help reduce the number of
  // almost-empty chunks.
  RedBlackTree<arena_chunk_map_t, ArenaRunTreeTrait> runs;

  // Size of regions in a run for this bin's size class.
  size_t reg_size;

  // Total size of a run for this bin's size class.
  size_t run_size;

  // Total number of regions in a run for this bin's size class.
  uint32_t nregs;

  // Number of elements in a run's regs_mask for this bin's size class.
  uint32_t regs_mask_nelms;

  // Offset of first region in a run for this bin's size class.
  uint32_t reg0_offset;

  // Bin statistics.
  malloc_bin_stats_t stats;
};

struct arena_t
{
#if defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
  uint32_t mMagic;
#define ARENA_MAGIC 0x947d3d24
#endif

  arena_id_t mId;
  // Linkage for the tree of arenas by id.
  RedBlackTreeNode<arena_t> mLink;

  // All operations on this arena require that lock be locked.
  Mutex mLock;

  arena_stats_t mStats;

private:
  // Tree of dirty-page-containing chunks this arena manages.
  RedBlackTree<arena_chunk_t, ArenaDirtyChunkTrait> mChunksDirty;

#ifdef MALLOC_DOUBLE_PURGE
  // Head of a linked list of MADV_FREE'd-page-containing chunks this
  // arena manages.
  DoublyLinkedList<arena_chunk_t> mChunksMAdvised;
#endif

  // In order to avoid rapid chunk allocation/deallocation when an arena
  // oscillates right on the cusp of needing a new chunk, cache the most
  // recently freed chunk.  The spare is left in the arena's chunk trees
  // until it is deleted.
  //
  // There is one spare chunk per arena, rather than one spare total, in
  // order to avoid interactions between multiple threads that could make
  // a single spare inadequate.
  arena_chunk_t* mSpare;

public:
  // Current count of pages within unused runs that are potentially
  // dirty, and for which madvise(... MADV_FREE) has not been called.  By
  // tracking this, we can institute a limit on how much dirty unused
  // memory is mapped for each arena.
  size_t mNumDirty;

  // Maximum value allowed for mNumDirty.
  size_t mMaxDirty;

private:
  // Size/address-ordered tree of this arena's available runs.  This tree
  // is used for first-best-fit run allocation.
  RedBlackTree<arena_chunk_map_t, ArenaAvailTreeTrait> mRunsAvail;

public:
  // mBins is used to store rings of free regions of the following sizes,
  // assuming a 16-byte quantum, 4kB pagesize, and default MALLOC_OPTIONS.
  //
  //   mBins[i] | size |
  //   --------+------+
  //        0  |    2 |
  //        1  |    4 |
  //        2  |    8 |
  //   --------+------+
  //        3  |   16 |
  //        4  |   32 |
  //        5  |   48 |
  //        6  |   64 |
  //           :      :
  //           :      :
  //       33  |  496 |
  //       34  |  512 |
  //   --------+------+
  //       35  | 1024 |
  //       36  | 2048 |
  //   --------+------+
  arena_bin_t mBins[1]; // Dynamically sized.

  arena_t();

private:
  void InitChunk(arena_chunk_t* aChunk, bool aZeroed);

  void DeallocChunk(arena_chunk_t* aChunk);

  arena_run_t* AllocRun(arena_bin_t* aBin,
                        size_t aSize,
                        bool aLarge,
                        bool aZero);

  void DallocRun(arena_run_t* aRun, bool aDirty);

  MOZ_MUST_USE bool SplitRun(arena_run_t* aRun,
                             size_t aSize,
                             bool aLarge,
                             bool aZero);

  void TrimRunHead(arena_chunk_t* aChunk,
                   arena_run_t* aRun,
                   size_t aOldSize,
                   size_t aNewSize);

  void TrimRunTail(arena_chunk_t* aChunk,
                   arena_run_t* aRun,
                   size_t aOldSize,
                   size_t aNewSize,
                   bool dirty);

  inline void* MallocBinEasy(arena_bin_t* aBin, arena_run_t* aRun);

  void* MallocBinHard(arena_bin_t* aBin);

  arena_run_t* GetNonFullBinRun(arena_bin_t* aBin);

  inline void* MallocSmall(size_t aSize, bool aZero);

  void* MallocLarge(size_t aSize, bool aZero);

public:
  inline void* Malloc(size_t aSize, bool aZero);

  void* Palloc(size_t aAlignment, size_t aSize, size_t aAllocSize);

  inline void DallocSmall(arena_chunk_t* aChunk,
                          void* aPtr,
                          arena_chunk_map_t* aMapElm);

  void DallocLarge(arena_chunk_t* aChunk, void* aPtr);

  void RallocShrinkLarge(arena_chunk_t* aChunk,
                         void* aPtr,
                         size_t aSize,
                         size_t aOldSize);

  bool RallocGrowLarge(arena_chunk_t* aChunk,
                       void* aPtr,
                       size_t aSize,
                       size_t aOldSize);

  void Purge(bool aAll);

  void HardPurge();

  void* operator new(size_t aCount) = delete;

  void* operator new(size_t aCount, const fallible_t&)
#if !defined(_MSC_VER) || defined(_CPPUNWIND)
    noexcept
#endif
  {
    MOZ_ASSERT(aCount == sizeof(arena_t));
    // Allocate enough space for trailing bins.
    return base_alloc(aCount +
                      (sizeof(arena_bin_t) * (ntbins + nqbins + nsbins - 1)));
  }

  void operator delete(void*) = delete;
};

struct ArenaTreeTrait
{
  static RedBlackTreeNode<arena_t>& GetTreeNode(arena_t* aThis)
  {
    return aThis->mLink;
  }

  static inline int Compare(arena_t* aNode, arena_t* aOther)
  {
    MOZ_ASSERT(aNode);
    MOZ_ASSERT(aOther);
    return (aNode->mId > aOther->mId) - (aNode->mId < aOther->mId);
  }
};

// Bookkeeping for all the arenas used by the allocator.
// Arenas are separated in two categories:
// - "private" arenas, used through the moz_arena_* API
// - all the other arenas: the default arena, and thread-local arenas,
//   used by the standard API.
class ArenaCollection
{
public:
  bool Init()
  {
    mArenas.Init();
    mPrivateArenas.Init();
    mDefaultArena = mLock.Init() ? CreateArena(/* IsPrivate = */ false) : nullptr;
    if (mDefaultArena) {
      // arena_t constructor sets this to a lower value for thread local
      // arenas; Reset to the default value for the main arena.
      mDefaultArena->mMaxDirty = opt_dirty_max;
    }
    return bool(mDefaultArena);
  }

  inline arena_t* GetById(arena_id_t aArenaId, bool aIsPrivate);

  arena_t* CreateArena(bool aIsPrivate);

  void DisposeArena(arena_t* aArena)
  {
    MutexAutoLock lock(mLock);
    (mPrivateArenas.Search(aArena) ? mPrivateArenas : mArenas).Remove(aArena);
    // The arena is leaked, and remaining allocations in it still are alive
    // until they are freed. After that, the arena will be empty but still
    // taking have at least a chunk taking address space. TODO: bug 1364359.
  }

  using Tree = RedBlackTree<arena_t, ArenaTreeTrait>;

  struct Iterator : Tree::Iterator
  {
    explicit Iterator(Tree* aTree, Tree* aSecondTree)
      : Tree::Iterator(aTree)
      , mNextTree(aSecondTree)
    {
    }

    Item<Iterator> begin()
    {
      return Item<Iterator>(this, *Tree::Iterator::begin());
    }

    Item<Iterator> end()
    {
      return Item<Iterator>(this, nullptr);
    }

    Tree::TreeNode* Next()
    {
      Tree::TreeNode* result = Tree::Iterator::Next();
      if (!result && mNextTree) {
	new (this) Iterator(mNextTree, nullptr);
	result = reinterpret_cast<Tree::TreeNode*>(*Tree::Iterator::begin());
      }
      return result;
    }

  private:
    Tree* mNextTree;
  };

  Iterator iter() { return Iterator(&mArenas, &mPrivateArenas); }

  inline arena_t* GetDefault() { return mDefaultArena; }

  Mutex mLock;

private:
  arena_t* mDefaultArena;
  arena_id_t mLastArenaId;
  Tree mArenas;
  Tree mPrivateArenas;
};

static ArenaCollection gArenas;

// ******
// Chunks.
static AddressRadixTree<(sizeof(void*) << 3) - CHUNK_2POW_DEFAULT> gChunkRTree;

// Protects chunk-related data structures.
static Mutex chunks_mtx;

// Trees of chunks that were previously allocated (trees differ only in node
// ordering).  These are used when allocating chunks, in an attempt to re-use
// address space.  Depending on function, different tree orderings are needed,
// which is why there are two trees with the same contents.
static RedBlackTree<extent_node_t, ExtentTreeSzTrait> gChunksBySize;
static RedBlackTree<extent_node_t, ExtentTreeTrait> gChunksByAddress;

// Protects huge allocation-related data structures.
static Mutex huge_mtx;

// Tree of chunks that are stand-alone huge allocations.
static RedBlackTree<extent_node_t, ExtentTreeTrait> huge;

// Huge allocation statistics.
static size_t huge_allocated;
static size_t huge_mapped;

// **************************
// base (internal allocation).

// Current pages that are being used for internal memory allocations.  These
// pages are carved up in cacheline-size quanta, so that there is no chance of
// false cache line sharing.

static void* base_pages;
static void* base_next_addr;
static void* base_next_decommitted;
static void* base_past_addr; // Addr immediately past base_pages.
static extent_node_t* base_nodes;
static Mutex base_mtx;
static size_t base_mapped;
static size_t base_committed;

// ******
// Arenas.

// The arena associated with the current thread (per jemalloc_thread_local_arena)
// On OSX, __thread/thread_local circles back calling malloc to allocate storage
// on first access on each thread, which leads to an infinite loop, but
// pthread-based TLS somehow doesn't have this problem.
#if !defined(XP_DARWIN)
static MOZ_THREAD_LOCAL(arena_t*) thread_arena;
#else
static detail::ThreadLocal<arena_t*, detail::ThreadLocalKeyStorage>
  thread_arena;
#endif

// *****************************
// Runtime configuration options.

const uint8_t kAllocJunk = 0xe4;
const uint8_t kAllocPoison = 0xe5;

#ifdef MOZ_DEBUG
static bool opt_junk = true;
static bool opt_zero = false;
#else
static const bool opt_junk = false;
static const bool opt_zero = false;
#endif

// ***************************************************************************
// Begin forward declarations.

static void*
chunk_alloc(size_t aSize,
            size_t aAlignment,
            bool aBase,
            bool* aZeroed = nullptr);
static void
chunk_dealloc(void* aChunk, size_t aSize, ChunkType aType);
static void
chunk_ensure_zero(void* aPtr, size_t aSize, bool aZeroed);
static void*
huge_malloc(size_t size, bool zero);
static void*
huge_palloc(size_t aSize, size_t aAlignment, bool aZero);
static void*
huge_ralloc(void* aPtr, size_t aSize, size_t aOldSize);
static void
huge_dalloc(void* aPtr);
#ifdef XP_WIN
extern "C"
#else
static
#endif
  bool
  malloc_init_hard();

#ifdef XP_DARWIN
#define FORK_HOOK extern "C"
#else
#define FORK_HOOK static
#endif
FORK_HOOK void
_malloc_prefork(void);
FORK_HOOK void
_malloc_postfork_parent(void);
FORK_HOOK void
_malloc_postfork_child(void);

// End forward declarations.
// ***************************************************************************

// FreeBSD's pthreads implementation calls malloc(3), so the malloc
// implementation has to take pains to avoid infinite recursion during
// initialization.
#if defined(XP_WIN)
#define malloc_init() true
#else
// Returns whether the allocator was successfully initialized.
static inline bool
malloc_init()
{

  if (malloc_initialized == false) {
    return malloc_init_hard();
  }

  return true;
}
#endif

static void
_malloc_message(const char* p)
{
#if !defined(XP_WIN)
#define _write write
#endif
  // Pretend to check _write() errors to suppress gcc warnings about
  // warn_unused_result annotations in some versions of glibc headers.
  if (_write(STDERR_FILENO, p, (unsigned int)strlen(p)) < 0) {
    return;
  }
}

template<typename... Args>
static void
_malloc_message(const char* p, Args... args)
{
  _malloc_message(p);
  _malloc_message(args...);
}

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/TaggedAnonymousMemory.h"
  // Note: MozTaggedAnonymousMmap() could call an LD_PRELOADed mmap
  // instead of the one defined here; use only MozTagAnonymousMemory().

#ifdef ANDROID
// Android's pthread.h does not declare pthread_atfork() until SDK 21.
extern "C" MOZ_EXPORT int
pthread_atfork(void (*)(void), void (*)(void), void (*)(void));
#endif

// ***************************************************************************
// Begin mutex.  We can't use normal pthread mutexes in all places, because
// they require malloc()ed memory, which causes bootstrapping issues in some
// cases. We also can't use constructors, because for statics, they would fire
// after the first use of malloc, resetting the locks.

// Initializes a mutex. Returns whether initialization succeeded.
bool
Mutex::Init()
{
#if defined(XP_WIN)
  if (!InitializeCriticalSectionAndSpinCount(&mMutex, _CRT_SPINCOUNT)) {
    return false;
  }
#elif defined(XP_DARWIN)
  mMutex = OS_SPINLOCK_INIT;
#elif defined(XP_LINUX) && !defined(ANDROID)
  pthread_mutexattr_t attr;
  if (pthread_mutexattr_init(&attr) != 0) {
    return false;
  }
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ADAPTIVE_NP);
  if (pthread_mutex_init(&mMutex, &attr) != 0) {
    pthread_mutexattr_destroy(&attr);
    return false;
  }
  pthread_mutexattr_destroy(&attr);
#else
  if (pthread_mutex_init(&mMutex, nullptr) != 0) {
    return false;
  }
#endif
  return true;
}

void
Mutex::Lock()
{
#if defined(XP_WIN)
  EnterCriticalSection(&mMutex);
#elif defined(XP_DARWIN)
  OSSpinLockLock(&mMutex);
#else
  pthread_mutex_lock(&mMutex);
#endif
}

void
Mutex::Unlock()
{
#if defined(XP_WIN)
  LeaveCriticalSection(&mMutex);
#elif defined(XP_DARWIN)
  OSSpinLockUnlock(&mMutex);
#else
  pthread_mutex_unlock(&mMutex);
#endif
}

// End mutex.
// ***************************************************************************
// Begin Utility functions/macros.

// Return the chunk address for allocation address a.
static inline arena_chunk_t*
GetChunkForPtr(const void* aPtr)
{
  return (arena_chunk_t*)(uintptr_t(aPtr) & ~chunksize_mask);
}

// Return the chunk offset of address a.
static inline size_t
GetChunkOffsetForPtr(const void* aPtr)
{
  return (size_t)(uintptr_t(aPtr) & chunksize_mask);
}

// Return the smallest chunk multiple that is >= s.
#define CHUNK_CEILING(s) (((s) + chunksize_mask) & ~chunksize_mask)

// Return the smallest cacheline multiple that is >= s.
#define CACHELINE_CEILING(s) (((s) + (CACHELINE - 1)) & ~(CACHELINE - 1))

// Return the smallest quantum multiple that is >= a.
#define QUANTUM_CEILING(a) (((a) + quantum_mask) & ~quantum_mask)

// Return the smallest pagesize multiple that is >= s.
#define PAGE_CEILING(s) (((s) + pagesize_mask) & ~pagesize_mask)

static inline const char*
_getprogname(void)
{

  return "<jemalloc>";
}

// ***************************************************************************

static inline void
pages_decommit(void* aAddr, size_t aSize)
{
#ifdef XP_WIN
  // The region starting at addr may have been allocated in multiple calls
  // to VirtualAlloc and recycled, so decommitting the entire region in one
  // go may not be valid. However, since we allocate at least a chunk at a
  // time, we may touch any region in chunksized increments.
  size_t pages_size = std::min(aSize, chunksize - GetChunkOffsetForPtr(aAddr));
  while (aSize > 0) {
    if (!VirtualFree(aAddr, pages_size, MEM_DECOMMIT)) {
      MOZ_CRASH();
    }
    aAddr = (void*)((uintptr_t)aAddr + pages_size);
    aSize -= pages_size;
    pages_size = std::min(aSize, chunksize);
  }
#else
  if (mmap(
        aAddr, aSize, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0) ==
      MAP_FAILED) {
    MOZ_CRASH();
  }
  MozTagAnonymousMemory(aAddr, aSize, "jemalloc-decommitted");
#endif
}

// Commit pages. Returns whether pages were committed.
MOZ_MUST_USE static inline bool
pages_commit(void* aAddr, size_t aSize)
{
#ifdef XP_WIN
  // The region starting at addr may have been allocated in multiple calls
  // to VirtualAlloc and recycled, so committing the entire region in one
  // go may not be valid. However, since we allocate at least a chunk at a
  // time, we may touch any region in chunksized increments.
  size_t pages_size = std::min(aSize, chunksize - GetChunkOffsetForPtr(aAddr));
  while (aSize > 0) {
    if (!VirtualAlloc(aAddr, pages_size, MEM_COMMIT, PAGE_READWRITE)) {
      return false;
    }
    aAddr = (void*)((uintptr_t)aAddr + pages_size);
    aSize -= pages_size;
    pages_size = std::min(aSize, chunksize);
  }
#else
  if (mmap(aAddr,
           aSize,
           PROT_READ | PROT_WRITE,
           MAP_FIXED | MAP_PRIVATE | MAP_ANON,
           -1,
           0) == MAP_FAILED) {
    return false;
  }
  MozTagAnonymousMemory(aAddr, aSize, "jemalloc");
#endif
  return true;
}

static bool
base_pages_alloc(size_t minsize)
{
  size_t csize;
  size_t pminsize;

  MOZ_ASSERT(minsize != 0);
  csize = CHUNK_CEILING(minsize);
  base_pages = chunk_alloc(csize, chunksize, true);
  if (!base_pages) {
    return true;
  }
  base_next_addr = base_pages;
  base_past_addr = (void*)((uintptr_t)base_pages + csize);
  // Leave enough pages for minsize committed, since otherwise they would
  // have to be immediately recommitted.
  pminsize = PAGE_CEILING(minsize);
  base_next_decommitted = (void*)((uintptr_t)base_pages + pminsize);
#if defined(MALLOC_DECOMMIT)
  if (pminsize < csize) {
    pages_decommit(base_next_decommitted, csize - pminsize);
  }
#endif
  base_mapped += csize;
  base_committed += pminsize;

  return false;
}

static void*
base_alloc(size_t aSize)
{
  void* ret;
  size_t csize;

  // Round size up to nearest multiple of the cacheline size.
  csize = CACHELINE_CEILING(aSize);

  MutexAutoLock lock(base_mtx);
  // Make sure there's enough space for the allocation.
  if ((uintptr_t)base_next_addr + csize > (uintptr_t)base_past_addr) {
    if (base_pages_alloc(csize)) {
      return nullptr;
    }
  }
  // Allocate.
  ret = base_next_addr;
  base_next_addr = (void*)((uintptr_t)base_next_addr + csize);
  // Make sure enough pages are committed for the new allocation.
  if ((uintptr_t)base_next_addr > (uintptr_t)base_next_decommitted) {
    void* pbase_next_addr = (void*)(PAGE_CEILING((uintptr_t)base_next_addr));

#ifdef MALLOC_DECOMMIT
    if (!pages_commit(base_next_decommitted,
                      (uintptr_t)pbase_next_addr -
                        (uintptr_t)base_next_decommitted)) {
      return nullptr;
    }
#endif
    base_next_decommitted = pbase_next_addr;
    base_committed +=
      (uintptr_t)pbase_next_addr - (uintptr_t)base_next_decommitted;
  }

  return ret;
}

static void*
base_calloc(size_t aNumber, size_t aSize)
{
  void* ret = base_alloc(aNumber * aSize);
  if (ret) {
    memset(ret, 0, aNumber * aSize);
  }
  return ret;
}

static extent_node_t*
base_node_alloc(void)
{
  extent_node_t* ret;

  base_mtx.Lock();
  if (base_nodes) {
    ret = base_nodes;
    base_nodes = *(extent_node_t**)ret;
    base_mtx.Unlock();
  } else {
    base_mtx.Unlock();
    ret = (extent_node_t*)base_alloc(sizeof(extent_node_t));
  }

  return ret;
}

static void
base_node_dealloc(extent_node_t* aNode)
{
  MutexAutoLock lock(base_mtx);
  *(extent_node_t**)aNode = base_nodes;
  base_nodes = aNode;
}

struct BaseNodeFreePolicy
{
  void operator()(extent_node_t* aPtr) { base_node_dealloc(aPtr); }
};

using UniqueBaseNode = UniquePtr<extent_node_t, BaseNodeFreePolicy>;

// End Utility functions/macros.
// ***************************************************************************
// Begin chunk management functions.

#ifdef XP_WIN

static void*
pages_map(void* aAddr, size_t aSize)
{
  void* ret = nullptr;
  ret = VirtualAlloc(aAddr, aSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  return ret;
}

static void
pages_unmap(void* aAddr, size_t aSize)
{
  if (VirtualFree(aAddr, 0, MEM_RELEASE) == 0) {
    _malloc_message(_getprogname(), ": (malloc) Error in VirtualFree()\n");
  }
}
#else

static void
pages_unmap(void* aAddr, size_t aSize)
{
  if (munmap(aAddr, aSize) == -1) {
    char buf[STRERROR_BUF];

    if (strerror_r(errno, buf, sizeof(buf)) == 0) {
      _malloc_message(
        _getprogname(), ": (malloc) Error in munmap(): ", buf, "\n");
    }
  }
}

static void*
pages_map(void* aAddr, size_t aSize)
{
  void* ret;
#if defined(__ia64__) ||                                                       \
  (defined(__sparc__) && defined(__arch64__) && defined(__linux__))
  // The JS engine assumes that all allocated pointers have their high 17 bits clear,
  // which ia64's mmap doesn't support directly. However, we can emulate it by passing
  // mmap an "addr" parameter with those bits clear. The mmap will return that address,
  // or the nearest available memory above that address, providing a near-guarantee
  // that those bits are clear. If they are not, we return nullptr below to indicate
  // out-of-memory.
  //
  // The addr is chosen as 0x0000070000000000, which still allows about 120TB of virtual
  // address space.
  //
  // See Bug 589735 for more information.
  bool check_placement = true;
  if (!aAddr) {
    aAddr = (void*)0x0000070000000000;
    check_placement = false;
  }
#endif

#if defined(__sparc__) && defined(__arch64__) && defined(__linux__)
  const uintptr_t start = 0x0000070000000000ULL;
  const uintptr_t end = 0x0000800000000000ULL;

  // Copied from js/src/gc/Memory.cpp and adapted for this source
  uintptr_t hint;
  void* region = MAP_FAILED;
  for (hint = start; region == MAP_FAILED && hint + aSize <= end;
       hint += chunksize) {
    region = mmap((void*)hint,
                  aSize,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANON,
                  -1,
                  0);
    if (region != MAP_FAILED) {
      if (((size_t)region + (aSize - 1)) & 0xffff800000000000) {
        if (munmap(region, aSize)) {
          MOZ_ASSERT(errno == ENOMEM);
        }
        region = MAP_FAILED;
      }
    }
  }
  ret = region;
#else
  // We don't use MAP_FIXED here, because it can cause the *replacement*
  // of existing mappings, and we only want to create new mappings.
  ret =
    mmap(aAddr, aSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  MOZ_ASSERT(ret);
#endif
  if (ret == MAP_FAILED) {
    ret = nullptr;
  }
#if defined(__ia64__) ||                                                       \
  (defined(__sparc__) && defined(__arch64__) && defined(__linux__))
  // If the allocated memory doesn't have its upper 17 bits clear, consider it
  // as out of memory.
  else if ((long long)ret & 0xffff800000000000) {
    munmap(ret, aSize);
    ret = nullptr;
  }
  // If the caller requested a specific memory location, verify that's what mmap returned.
  else if (check_placement && ret != aAddr) {
#else
  else if (aAddr && ret != aAddr) {
#endif
    // We succeeded in mapping memory, but not in the right place.
    pages_unmap(ret, aSize);
    ret = nullptr;
  }
  if (ret) {
    MozTagAnonymousMemory(ret, aSize, "jemalloc");
  }

#if defined(__ia64__) ||                                                       \
  (defined(__sparc__) && defined(__arch64__) && defined(__linux__))
  MOZ_ASSERT(!ret || (!check_placement && ret) ||
             (check_placement && ret == aAddr));
#else
  MOZ_ASSERT(!ret || (!aAddr && ret != aAddr) || (aAddr && ret == aAddr));
#endif
  return ret;
}
#endif

#ifdef XP_DARWIN
#define VM_COPY_MIN (pagesize << 5)
static inline void
pages_copy(void* dest, const void* src, size_t n)
{

  MOZ_ASSERT((void*)((uintptr_t)dest & ~pagesize_mask) == dest);
  MOZ_ASSERT(n >= VM_COPY_MIN);
  MOZ_ASSERT((void*)((uintptr_t)src & ~pagesize_mask) == src);

  vm_copy(
    mach_task_self(), (vm_address_t)src, (vm_size_t)n, (vm_address_t)dest);
}
#endif

template<size_t Bits>
bool
AddressRadixTree<Bits>::Init()
{
  mLock.Init();
  mRoot = (void**)base_calloc(1 << kBitsAtLevel1, sizeof(void*));
  return mRoot;
}

template<size_t Bits>
void**
AddressRadixTree<Bits>::GetSlot(void* aKey, bool aCreate)
{
  uintptr_t key = reinterpret_cast<uintptr_t>(aKey);
  uintptr_t subkey;
  unsigned i, lshift, height, bits;
  void** node;
  void** child;

  for (i = lshift = 0, height = kHeight, node = mRoot; i < height - 1;
       i++, lshift += bits, node = child) {
    bits = i ? kBitsPerLevel : kBitsAtLevel1;
    subkey = (key << lshift) >> ((sizeof(void*) << 3) - bits);
    child = (void**)node[subkey];
    if (!child && aCreate) {
      child = (void**)base_calloc(1 << kBitsPerLevel, sizeof(void*));
      if (child) {
        node[subkey] = child;
      }
    }
    if (!child) {
      return nullptr;
    }
  }

  // node is a leaf, so it contains values rather than node
  // pointers.
  bits = i ? kBitsPerLevel : kBitsAtLevel1;
  subkey = (key << lshift) >> ((sizeof(void*) << 3) - bits);
  return &node[subkey];
}

template<size_t Bits>
void*
AddressRadixTree<Bits>::Get(void* aKey)
{
  void* ret = nullptr;

  void** slot = GetSlot(aKey);

  if (slot) {
    ret = *slot;
  }
#ifdef MOZ_DEBUG
  MutexAutoLock lock(mLock);

  // Suppose that it were possible for a jemalloc-allocated chunk to be
  // munmap()ped, followed by a different allocator in another thread re-using
  // overlapping virtual memory, all without invalidating the cached rtree
  // value.  The result would be a false positive (the rtree would claim that
  // jemalloc owns memory that it had actually discarded).  I don't think this
  // scenario is possible, but the following assertion is a prudent sanity
  // check.
  if (!slot) {
    // In case a slot has been created in the meantime.
    slot = GetSlot(aKey);
  }
  if (slot) {
    // The MutexAutoLock above should act as a memory barrier, forcing
    // the compiler to emit a new read instruction for *slot.
    MOZ_ASSERT(ret == *slot);
  } else {
    MOZ_ASSERT(ret == nullptr);
  }
#endif
  return ret;
}

template<size_t Bits>
bool
AddressRadixTree<Bits>::Set(void* aKey, void* aValue)
{
  MutexAutoLock lock(mLock);
  void** slot = GetSlot(aKey, /* create = */ true);
  if (slot) {
    *slot = aValue;
  }
  return slot;
}

// pages_trim, chunk_alloc_mmap_slow and chunk_alloc_mmap were cherry-picked
// from upstream jemalloc 3.4.1 to fix Mozilla bug 956501.

// Return the offset between a and the nearest aligned address at or below a.
#define ALIGNMENT_ADDR2OFFSET(a, alignment)                                    \
  ((size_t)((uintptr_t)(a) & (alignment - 1)))

// Return the smallest alignment multiple that is >= s.
#define ALIGNMENT_CEILING(s, alignment)                                        \
  (((s) + (alignment - 1)) & (~(alignment - 1)))

static void*
pages_trim(void* addr, size_t alloc_size, size_t leadsize, size_t size)
{
  void* ret = (void*)((uintptr_t)addr + leadsize);

  MOZ_ASSERT(alloc_size >= leadsize + size);
#ifdef XP_WIN
  {
    void* new_addr;

    pages_unmap(addr, alloc_size);
    new_addr = pages_map(ret, size);
    if (new_addr == ret) {
      return ret;
    }
    if (new_addr) {
      pages_unmap(new_addr, size);
    }
    return nullptr;
  }
#else
  {
    size_t trailsize = alloc_size - leadsize - size;

    if (leadsize != 0) {
      pages_unmap(addr, leadsize);
    }
    if (trailsize != 0) {
      pages_unmap((void*)((uintptr_t)ret + size), trailsize);
    }
    return ret;
  }
#endif
}

static void*
chunk_alloc_mmap_slow(size_t size, size_t alignment)
{
  void *ret, *pages;
  size_t alloc_size, leadsize;

  alloc_size = size + alignment - pagesize;
  // Beware size_t wrap-around.
  if (alloc_size < size) {
    return nullptr;
  }
  do {
    pages = pages_map(nullptr, alloc_size);
    if (!pages) {
      return nullptr;
    }
    leadsize =
      ALIGNMENT_CEILING((uintptr_t)pages, alignment) - (uintptr_t)pages;
    ret = pages_trim(pages, alloc_size, leadsize, size);
  } while (!ret);

  MOZ_ASSERT(ret);
  return ret;
}

static void*
chunk_alloc_mmap(size_t size, size_t alignment)
{
  void* ret;
  size_t offset;

  // Ideally, there would be a way to specify alignment to mmap() (like
  // NetBSD has), but in the absence of such a feature, we have to work
  // hard to efficiently create aligned mappings. The reliable, but
  // slow method is to create a mapping that is over-sized, then trim the
  // excess. However, that always results in one or two calls to
  // pages_unmap().
  //
  // Optimistically try mapping precisely the right amount before falling
  // back to the slow method, with the expectation that the optimistic
  // approach works most of the time.
  ret = pages_map(nullptr, size);
  if (!ret) {
    return nullptr;
  }
  offset = ALIGNMENT_ADDR2OFFSET(ret, alignment);
  if (offset != 0) {
    pages_unmap(ret, size);
    return chunk_alloc_mmap_slow(size, alignment);
  }

  MOZ_ASSERT(ret);
  return ret;
}

// Purge and release the pages in the chunk of length `length` at `addr` to
// the OS.
// Returns whether the pages are guaranteed to be full of zeroes when the
// function returns.
// The force_zero argument explicitly requests that the memory is guaranteed
// to be full of zeroes when the function returns.
static bool
pages_purge(void* addr, size_t length, bool force_zero)
{
#ifdef MALLOC_DECOMMIT
  pages_decommit(addr, length);
  return true;
#else
#ifndef XP_LINUX
  if (force_zero) {
    memset(addr, 0, length);
  }
#endif
#ifdef XP_WIN
  // The region starting at addr may have been allocated in multiple calls
  // to VirtualAlloc and recycled, so resetting the entire region in one
  // go may not be valid. However, since we allocate at least a chunk at a
  // time, we may touch any region in chunksized increments.
  size_t pages_size = std::min(length, chunksize - GetChunkOffsetForPtr(addr));
  while (length > 0) {
    VirtualAlloc(addr, pages_size, MEM_RESET, PAGE_READWRITE);
    addr = (void*)((uintptr_t)addr + pages_size);
    length -= pages_size;
    pages_size = std::min(length, chunksize);
  }
  return force_zero;
#else
#ifdef XP_LINUX
#define JEMALLOC_MADV_PURGE MADV_DONTNEED
#define JEMALLOC_MADV_ZEROS true
#else // FreeBSD and Darwin.
#define JEMALLOC_MADV_PURGE MADV_FREE
#define JEMALLOC_MADV_ZEROS force_zero
#endif
  int err = madvise(addr, length, JEMALLOC_MADV_PURGE);
  return JEMALLOC_MADV_ZEROS && err == 0;
#undef JEMALLOC_MADV_PURGE
#undef JEMALLOC_MADV_ZEROS
#endif
#endif
}

static void*
chunk_recycle(size_t aSize, size_t aAlignment, bool* aZeroed)
{
  extent_node_t key;

  size_t alloc_size = aSize + aAlignment - chunksize;
  // Beware size_t wrap-around.
  if (alloc_size < aSize) {
    return nullptr;
  }
  key.addr = nullptr;
  key.size = alloc_size;
  chunks_mtx.Lock();
  extent_node_t* node = gChunksBySize.SearchOrNext(&key);
  if (!node) {
    chunks_mtx.Unlock();
    return nullptr;
  }
  size_t leadsize = ALIGNMENT_CEILING((uintptr_t)node->addr, aAlignment) -
                    (uintptr_t)node->addr;
  MOZ_ASSERT(node->size >= leadsize + aSize);
  size_t trailsize = node->size - leadsize - aSize;
  void* ret = (void*)((uintptr_t)node->addr + leadsize);
  ChunkType chunk_type = node->chunk_type;
  if (aZeroed) {
    *aZeroed = (chunk_type == ZEROED_CHUNK);
  }
  // Remove node from the tree.
  gChunksBySize.Remove(node);
  gChunksByAddress.Remove(node);
  if (leadsize != 0) {
    // Insert the leading space as a smaller chunk.
    node->size = leadsize;
    gChunksBySize.Insert(node);
    gChunksByAddress.Insert(node);
    node = nullptr;
  }
  if (trailsize != 0) {
    // Insert the trailing space as a smaller chunk.
    if (!node) {
      // An additional node is required, but
      // base_node_alloc() can cause a new base chunk to be
      // allocated.  Drop chunks_mtx in order to avoid
      // deadlock, and if node allocation fails, deallocate
      // the result before returning an error.
      chunks_mtx.Unlock();
      node = base_node_alloc();
      if (!node) {
        chunk_dealloc(ret, aSize, chunk_type);
        return nullptr;
      }
      chunks_mtx.Lock();
    }
    node->addr = (void*)((uintptr_t)(ret) + aSize);
    node->size = trailsize;
    node->chunk_type = chunk_type;
    gChunksBySize.Insert(node);
    gChunksByAddress.Insert(node);
    node = nullptr;
  }

  gRecycledSize -= aSize;

  chunks_mtx.Unlock();

  if (node) {
    base_node_dealloc(node);
  }
#ifdef MALLOC_DECOMMIT
  if (!pages_commit(ret, aSize)) {
    return nullptr;
  }
  // pages_commit is guaranteed to zero the chunk.
  if (aZeroed) {
    *aZeroed = true;
  }
#endif
  return ret;
}

#ifdef XP_WIN
// On Windows, calls to VirtualAlloc and VirtualFree must be matched, making it
// awkward to recycle allocations of varying sizes. Therefore we only allow
// recycling when the size equals the chunksize, unless deallocation is entirely
// disabled.
#define CAN_RECYCLE(size) (size == chunksize)
#else
#define CAN_RECYCLE(size) true
#endif

// Allocates `size` bytes of system memory aligned for `alignment`.
// `base` indicates whether the memory will be used for the base allocator
// (e.g. base_alloc).
// `zeroed` is an outvalue that returns whether the allocated memory is
// guaranteed to be full of zeroes. It can be omitted when the caller doesn't
// care about the result.
static void*
chunk_alloc(size_t aSize, size_t aAlignment, bool aBase, bool* aZeroed)
{
  void* ret = nullptr;

  MOZ_ASSERT(aSize != 0);
  MOZ_ASSERT((aSize & chunksize_mask) == 0);
  MOZ_ASSERT(aAlignment != 0);
  MOZ_ASSERT((aAlignment & chunksize_mask) == 0);

  // Base allocations can't be fulfilled by recycling because of
  // possible deadlock or infinite recursion.
  if (CAN_RECYCLE(aSize) && !aBase) {
    ret = chunk_recycle(aSize, aAlignment, aZeroed);
  }
  if (!ret) {
    ret = chunk_alloc_mmap(aSize, aAlignment);
    if (aZeroed) {
      *aZeroed = true;
    }
  }
  if (ret && !aBase) {
    if (!gChunkRTree.Set(ret, ret)) {
      chunk_dealloc(ret, aSize, UNKNOWN_CHUNK);
      return nullptr;
    }
  }

  MOZ_ASSERT(GetChunkOffsetForPtr(ret) == 0);
  return ret;
}

static void
chunk_ensure_zero(void* aPtr, size_t aSize, bool aZeroed)
{
  if (aZeroed == false) {
    memset(aPtr, 0, aSize);
  }
#ifdef MOZ_DEBUG
  else {
    size_t i;
    size_t* p = (size_t*)(uintptr_t)aPtr;

    for (i = 0; i < aSize / sizeof(size_t); i++) {
      MOZ_ASSERT(p[i] == 0);
    }
  }
#endif
}

static void
chunk_record(void* aChunk, size_t aSize, ChunkType aType)
{
  extent_node_t key;

  if (aType != ZEROED_CHUNK) {
    if (pages_purge(aChunk, aSize, aType == HUGE_CHUNK)) {
      aType = ZEROED_CHUNK;
    }
  }

  // Allocate a node before acquiring chunks_mtx even though it might not
  // be needed, because base_node_alloc() may cause a new base chunk to
  // be allocated, which could cause deadlock if chunks_mtx were already
  // held.
  UniqueBaseNode xnode(base_node_alloc());
  // Use xprev to implement conditional deferred deallocation of prev.
  UniqueBaseNode xprev;

  // RAII deallocates xnode and xprev defined above after unlocking
  // in order to avoid potential dead-locks
  MutexAutoLock lock(chunks_mtx);
  key.addr = (void*)((uintptr_t)aChunk + aSize);
  extent_node_t* node = gChunksByAddress.SearchOrNext(&key);
  // Try to coalesce forward.
  if (node && node->addr == key.addr) {
    // Coalesce chunk with the following address range.  This does
    // not change the position within gChunksByAddress, so only
    // remove/insert from/into gChunksBySize.
    gChunksBySize.Remove(node);
    node->addr = aChunk;
    node->size += aSize;
    if (node->chunk_type != aType) {
      node->chunk_type = RECYCLED_CHUNK;
    }
    gChunksBySize.Insert(node);
  } else {
    // Coalescing forward failed, so insert a new node.
    if (!xnode) {
      // base_node_alloc() failed, which is an exceedingly
      // unlikely failure.  Leak chunk; its pages have
      // already been purged, so this is only a virtual
      // memory leak.
      return;
    }
    node = xnode.release();
    node->addr = aChunk;
    node->size = aSize;
    node->chunk_type = aType;
    gChunksByAddress.Insert(node);
    gChunksBySize.Insert(node);
  }

  // Try to coalesce backward.
  extent_node_t* prev = gChunksByAddress.Prev(node);
  if (prev && (void*)((uintptr_t)prev->addr + prev->size) == aChunk) {
    // Coalesce chunk with the previous address range.  This does
    // not change the position within gChunksByAddress, so only
    // remove/insert node from/into gChunksBySize.
    gChunksBySize.Remove(prev);
    gChunksByAddress.Remove(prev);

    gChunksBySize.Remove(node);
    node->addr = prev->addr;
    node->size += prev->size;
    if (node->chunk_type != prev->chunk_type) {
      node->chunk_type = RECYCLED_CHUNK;
    }
    gChunksBySize.Insert(node);

    xprev.reset(prev);
  }

  gRecycledSize += aSize;
}

static void
chunk_dealloc(void* aChunk, size_t aSize, ChunkType aType)
{
  MOZ_ASSERT(aChunk);
  MOZ_ASSERT(GetChunkOffsetForPtr(aChunk) == 0);
  MOZ_ASSERT(aSize != 0);
  MOZ_ASSERT((aSize & chunksize_mask) == 0);

  gChunkRTree.Unset(aChunk);

  if (CAN_RECYCLE(aSize)) {
    size_t recycled_so_far = gRecycledSize;
    // In case some race condition put us above the limit.
    if (recycled_so_far < gRecycleLimit) {
      size_t recycle_remaining = gRecycleLimit - recycled_so_far;
      size_t to_recycle;
      if (aSize > recycle_remaining) {
        to_recycle = recycle_remaining;
        // Drop pages that would overflow the recycle limit
        pages_trim(aChunk, aSize, 0, to_recycle);
      } else {
        to_recycle = aSize;
      }
      chunk_record(aChunk, to_recycle, aType);
      return;
    }
  }

  pages_unmap(aChunk, aSize);
}

#undef CAN_RECYCLE

// End chunk management functions.
// ***************************************************************************
// Begin arena.

static inline arena_t*
thread_local_arena(bool enabled)
{
  arena_t* arena;

  if (enabled) {
    // The arena will essentially be leaked if this function is
    // called with `false`, but it doesn't matter at the moment.
    // because in practice nothing actually calls this function
    // with `false`, except maybe at shutdown.
    arena = gArenas.CreateArena(/* IsPrivate = */ false);
  } else {
    arena = gArenas.GetDefault();
  }
  thread_arena.set(arena);
  return arena;
}

template<>
inline void
MozJemalloc::jemalloc_thread_local_arena(bool aEnabled)
{
  if (malloc_init()) {
    thread_local_arena(aEnabled);
  }
}

// Choose an arena based on a per-thread value.
static inline arena_t*
choose_arena(size_t size)
{
  arena_t* ret = nullptr;

  // We can only use TLS if this is a PIC library, since for the static
  // library version, libc's malloc is used by TLS allocation, which
  // introduces a bootstrapping issue.

  // Only use a thread local arena for small sizes.
  if (size <= small_max) {
    ret = thread_arena.get();
  }

  if (!ret) {
    ret = thread_local_arena(false);
  }
  MOZ_DIAGNOSTIC_ASSERT(ret);
  return ret;
}

static inline void*
arena_run_reg_alloc(arena_run_t* run, arena_bin_t* bin)
{
  void* ret;
  unsigned i, mask, bit, regind;

  MOZ_DIAGNOSTIC_ASSERT(run->magic == ARENA_RUN_MAGIC);
  MOZ_ASSERT(run->regs_minelm < bin->regs_mask_nelms);

  // Move the first check outside the loop, so that run->regs_minelm can
  // be updated unconditionally, without the possibility of updating it
  // multiple times.
  i = run->regs_minelm;
  mask = run->regs_mask[i];
  if (mask != 0) {
    // Usable allocation found.
    bit = CountTrailingZeroes32(mask);

    regind = ((i << (SIZEOF_INT_2POW + 3)) + bit);
    MOZ_ASSERT(regind < bin->nregs);
    ret =
      (void*)(((uintptr_t)run) + bin->reg0_offset + (bin->reg_size * regind));

    // Clear bit.
    mask ^= (1U << bit);
    run->regs_mask[i] = mask;

    return ret;
  }

  for (i++; i < bin->regs_mask_nelms; i++) {
    mask = run->regs_mask[i];
    if (mask != 0) {
      // Usable allocation found.
      bit = CountTrailingZeroes32(mask);

      regind = ((i << (SIZEOF_INT_2POW + 3)) + bit);
      MOZ_ASSERT(regind < bin->nregs);
      ret =
        (void*)(((uintptr_t)run) + bin->reg0_offset + (bin->reg_size * regind));

      // Clear bit.
      mask ^= (1U << bit);
      run->regs_mask[i] = mask;

      // Make a note that nothing before this element
      // contains a free region.
      run->regs_minelm = i; // Low payoff: + (mask == 0);

      return ret;
    }
  }
  // Not reached.
  MOZ_DIAGNOSTIC_ASSERT(0);
  return nullptr;
}

static inline void
arena_run_reg_dalloc(arena_run_t* run, arena_bin_t* bin, void* ptr, size_t size)
{
// To divide by a number D that is not a power of two we multiply
// by (2^21 / D) and then right shift by 21 positions.
//
//   X / D
//
// becomes
//
//   (X * size_invs[(D >> QUANTUM_2POW_MIN) - 3]) >> SIZE_INV_SHIFT

#define SIZE_INV_SHIFT 21
#define SIZE_INV(s) (((1U << SIZE_INV_SHIFT) / (s << QUANTUM_2POW_MIN)) + 1)
  // clang-format off
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
  // clang-format on
  unsigned diff, regind, elm, bit;

  MOZ_DIAGNOSTIC_ASSERT(run->magic == ARENA_RUN_MAGIC);
  MOZ_ASSERT(((sizeof(size_invs)) / sizeof(unsigned)) + 3 >=
             (SMALL_MAX_DEFAULT >> QUANTUM_2POW_MIN));

  // Avoid doing division with a variable divisor if possible.  Using
  // actual division here can reduce allocator throughput by over 20%!
  diff = (unsigned)((uintptr_t)ptr - (uintptr_t)run - bin->reg0_offset);
  if ((size & (size - 1)) == 0) {
    // log2_table allows fast division of a power of two in the
    // [1..128] range.
    //
    // (x / divisor) becomes (x >> log2_table[divisor - 1]).
    // clang-format off
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
    // clang-format on

    if (size <= 128) {
      regind = (diff >> log2_table[size - 1]);
    } else if (size <= 32768) {
      regind = diff >> (8 + log2_table[(size >> 8) - 1]);
    } else {
      // The run size is too large for us to use the lookup
      // table.  Use real division.
      regind = diff / size;
    }
  } else if (size <=
             ((sizeof(size_invs) / sizeof(unsigned)) << QUANTUM_2POW_MIN) + 2) {
    regind = size_invs[(size >> QUANTUM_2POW_MIN) - 3] * diff;
    regind >>= SIZE_INV_SHIFT;
  } else {
    // size_invs isn't large enough to handle this size class, so
    // calculate regind using actual division.  This only happens
    // if the user increases small_max via the 'S' runtime
    // configuration option.
    regind = diff / size;
  };
  MOZ_DIAGNOSTIC_ASSERT(diff == regind * size);
  MOZ_DIAGNOSTIC_ASSERT(regind < bin->nregs);

  elm = regind >> (SIZEOF_INT_2POW + 3);
  if (elm < run->regs_minelm) {
    run->regs_minelm = elm;
  }
  bit = regind - (elm << (SIZEOF_INT_2POW + 3));
  MOZ_DIAGNOSTIC_ASSERT((run->regs_mask[elm] & (1U << bit)) == 0);
  run->regs_mask[elm] |= (1U << bit);
#undef SIZE_INV
#undef SIZE_INV_SHIFT
}

bool
arena_t::SplitRun(arena_run_t* aRun, size_t aSize, bool aLarge, bool aZero)
{
  arena_chunk_t* chunk;
  size_t old_ndirty, run_ind, total_pages, need_pages, rem_pages, i;

  chunk = GetChunkForPtr(aRun);
  old_ndirty = chunk->ndirty;
  run_ind = (unsigned)((uintptr_t(aRun) - uintptr_t(chunk)) >> pagesize_2pow);
  total_pages = (chunk->map[run_ind].bits & ~pagesize_mask) >> pagesize_2pow;
  need_pages = (aSize >> pagesize_2pow);
  MOZ_ASSERT(need_pages > 0);
  MOZ_ASSERT(need_pages <= total_pages);
  rem_pages = total_pages - need_pages;

  for (i = 0; i < need_pages; i++) {
    // Commit decommitted pages if necessary.  If a decommitted
    // page is encountered, commit all needed adjacent decommitted
    // pages in one operation, in order to reduce system call
    // overhead.
    if (chunk->map[run_ind + i].bits & CHUNK_MAP_MADVISED_OR_DECOMMITTED) {
      size_t j;

      // Advance i+j to just past the index of the last page
      // to commit.  Clear CHUNK_MAP_DECOMMITTED and
      // CHUNK_MAP_MADVISED along the way.
      for (j = 0; i + j < need_pages && (chunk->map[run_ind + i + j].bits &
                                         CHUNK_MAP_MADVISED_OR_DECOMMITTED);
           j++) {
        // DECOMMITTED and MADVISED are mutually exclusive.
        MOZ_ASSERT(!(chunk->map[run_ind + i + j].bits & CHUNK_MAP_DECOMMITTED &&
                     chunk->map[run_ind + i + j].bits & CHUNK_MAP_MADVISED));

        chunk->map[run_ind + i + j].bits &= ~CHUNK_MAP_MADVISED_OR_DECOMMITTED;
      }

#ifdef MALLOC_DECOMMIT
      bool committed = pages_commit(
        (void*)(uintptr_t(chunk) + ((run_ind + i) << pagesize_2pow)),
        j << pagesize_2pow);
      // pages_commit zeroes pages, so mark them as such if it succeeded.
      // That's checked further below to avoid manually zeroing the pages.
      for (size_t k = 0; k < j; k++) {
        chunk->map[run_ind + i + k].bits |=
          committed ? CHUNK_MAP_ZEROED : CHUNK_MAP_DECOMMITTED;
      }
      if (!committed) {
        return false;
      }
#endif

      mStats.committed += j;
    }
  }

  mRunsAvail.Remove(&chunk->map[run_ind]);

  // Keep track of trailing unused pages for later use.
  if (rem_pages > 0) {
    chunk->map[run_ind + need_pages].bits =
      (rem_pages << pagesize_2pow) |
      (chunk->map[run_ind + need_pages].bits & pagesize_mask);
    chunk->map[run_ind + total_pages - 1].bits =
      (rem_pages << pagesize_2pow) |
      (chunk->map[run_ind + total_pages - 1].bits & pagesize_mask);
    mRunsAvail.Insert(&chunk->map[run_ind + need_pages]);
  }

  for (i = 0; i < need_pages; i++) {
    // Zero if necessary.
    if (aZero) {
      if ((chunk->map[run_ind + i].bits & CHUNK_MAP_ZEROED) == 0) {
        memset((void*)(uintptr_t(chunk) + ((run_ind + i) << pagesize_2pow)),
               0,
               pagesize);
        // CHUNK_MAP_ZEROED is cleared below.
      }
    }

    // Update dirty page accounting.
    if (chunk->map[run_ind + i].bits & CHUNK_MAP_DIRTY) {
      chunk->ndirty--;
      mNumDirty--;
      // CHUNK_MAP_DIRTY is cleared below.
    }

    // Initialize the chunk map.
    if (aLarge) {
      chunk->map[run_ind + i].bits = CHUNK_MAP_LARGE | CHUNK_MAP_ALLOCATED;
    } else {
      chunk->map[run_ind + i].bits = size_t(aRun) | CHUNK_MAP_ALLOCATED;
    }
  }

  // Set the run size only in the first element for large runs.  This is
  // primarily a debugging aid, since the lack of size info for trailing
  // pages only matters if the application tries to operate on an
  // interior pointer.
  if (aLarge) {
    chunk->map[run_ind].bits |= aSize;
  }

  if (chunk->ndirty == 0 && old_ndirty > 0) {
    mChunksDirty.Remove(chunk);
  }
  return true;
}

void
arena_t::InitChunk(arena_chunk_t* aChunk, bool aZeroed)
{
  size_t i;
  // WARNING: The following relies on !aZeroed meaning "used to be an arena
  // chunk".
  // When the chunk we're initializating as an arena chunk is zeroed, we
  // mark all runs are decommitted and zeroed.
  // When it is not, which we can assume means it's a recycled arena chunk,
  // all it can contain is an arena chunk header (which we're overwriting),
  // and zeroed or poisoned memory (because a recycled arena chunk will
  // have been emptied before being recycled). In that case, we can get
  // away with reusing the chunk as-is, marking all runs as madvised.

  size_t flags =
    aZeroed ? CHUNK_MAP_DECOMMITTED | CHUNK_MAP_ZEROED : CHUNK_MAP_MADVISED;

  mStats.mapped += chunksize;

  aChunk->arena = this;

  // Claim that no pages are in use, since the header is merely overhead.
  aChunk->ndirty = 0;

  // Initialize the map to contain one maximal free untouched run.
#ifdef MALLOC_DECOMMIT
  arena_run_t* run =
    (arena_run_t*)(uintptr_t(aChunk) +
                   (arena_chunk_header_npages << pagesize_2pow));
#endif

  for (i = 0; i < arena_chunk_header_npages; i++) {
    aChunk->map[i].bits = 0;
  }
  aChunk->map[i].bits = arena_maxclass | flags;
  for (i++; i < chunk_npages - 1; i++) {
    aChunk->map[i].bits = flags;
  }
  aChunk->map[chunk_npages - 1].bits = arena_maxclass | flags;

#ifdef MALLOC_DECOMMIT
  // Start out decommitted, in order to force a closer correspondence
  // between dirty pages and committed untouched pages.
  pages_decommit(run, arena_maxclass);
#endif
  mStats.committed += arena_chunk_header_npages;

  // Insert the run into the tree of available runs.
  mRunsAvail.Insert(&aChunk->map[arena_chunk_header_npages]);

#ifdef MALLOC_DOUBLE_PURGE
  new (&aChunk->chunks_madvised_elem) DoublyLinkedListElement<arena_chunk_t>();
#endif
}

void
arena_t::DeallocChunk(arena_chunk_t* aChunk)
{
  if (mSpare) {
    if (mSpare->ndirty > 0) {
      aChunk->arena->mChunksDirty.Remove(mSpare);
      mNumDirty -= mSpare->ndirty;
      mStats.committed -= mSpare->ndirty;
    }

#ifdef MALLOC_DOUBLE_PURGE
    if (mChunksMAdvised.ElementProbablyInList(mSpare)) {
      mChunksMAdvised.remove(mSpare);
    }
#endif

    chunk_dealloc((void*)mSpare, chunksize, ARENA_CHUNK);
    mStats.mapped -= chunksize;
    mStats.committed -= arena_chunk_header_npages;
  }

  // Remove run from the tree of available runs, so that the arena does not use it.
  // Dirty page flushing only uses the tree of dirty chunks, so leaving this
  // chunk in the chunks_* trees is sufficient for that purpose.
  mRunsAvail.Remove(&aChunk->map[arena_chunk_header_npages]);

  mSpare = aChunk;
}

arena_run_t*
arena_t::AllocRun(arena_bin_t* aBin, size_t aSize, bool aLarge, bool aZero)
{
  arena_run_t* run;
  arena_chunk_map_t* mapelm;
  arena_chunk_map_t key;

  MOZ_ASSERT(aSize <= arena_maxclass);
  MOZ_ASSERT((aSize & pagesize_mask) == 0);

  // Search the arena's chunks for the lowest best fit.
  key.bits = aSize | CHUNK_MAP_KEY;
  mapelm = mRunsAvail.SearchOrNext(&key);
  if (mapelm) {
    arena_chunk_t* chunk = GetChunkForPtr(mapelm);
    size_t pageind =
      (uintptr_t(mapelm) - uintptr_t(chunk->map)) / sizeof(arena_chunk_map_t);

    run = (arena_run_t*)(uintptr_t(chunk) + (pageind << pagesize_2pow));
  } else if (mSpare) {
    // Use the spare.
    arena_chunk_t* chunk = mSpare;
    mSpare = nullptr;
    run = (arena_run_t*)(uintptr_t(chunk) +
                         (arena_chunk_header_npages << pagesize_2pow));
    // Insert the run into the tree of available runs.
    mRunsAvail.Insert(&chunk->map[arena_chunk_header_npages]);
  } else {
    // No usable runs.  Create a new chunk from which to allocate
    // the run.
    bool zeroed;
    arena_chunk_t* chunk =
      (arena_chunk_t*)chunk_alloc(chunksize, chunksize, false, &zeroed);
    if (!chunk) {
      return nullptr;
    }

    InitChunk(chunk, zeroed);
    run = (arena_run_t*)(uintptr_t(chunk) +
                         (arena_chunk_header_npages << pagesize_2pow));
  }
  // Update page map.
  return SplitRun(run, aSize, aLarge, aZero) ? run : nullptr;
}

void
arena_t::Purge(bool aAll)
{
  arena_chunk_t* chunk;
  size_t i, npages;
  // If all is set purge all dirty pages.
  size_t dirty_max = aAll ? 1 : mMaxDirty;
#ifdef MOZ_DEBUG
  size_t ndirty = 0;
  for (auto chunk : mChunksDirty.iter()) {
    ndirty += chunk->ndirty;
  }
  MOZ_ASSERT(ndirty == mNumDirty);
#endif
  MOZ_DIAGNOSTIC_ASSERT(aAll || (mNumDirty > mMaxDirty));

  // Iterate downward through chunks until enough dirty memory has been
  // purged.  Terminate as soon as possible in order to minimize the
  // number of system calls, even if a chunk has only been partially
  // purged.
  while (mNumDirty > (dirty_max >> 1)) {
#ifdef MALLOC_DOUBLE_PURGE
    bool madvised = false;
#endif
    chunk = mChunksDirty.Last();
    MOZ_DIAGNOSTIC_ASSERT(chunk);

    for (i = chunk_npages - 1; chunk->ndirty > 0; i--) {
      MOZ_DIAGNOSTIC_ASSERT(i >= arena_chunk_header_npages);

      if (chunk->map[i].bits & CHUNK_MAP_DIRTY) {
#ifdef MALLOC_DECOMMIT
        const size_t free_operation = CHUNK_MAP_DECOMMITTED;
#else
        const size_t free_operation = CHUNK_MAP_MADVISED;
#endif
        MOZ_ASSERT((chunk->map[i].bits & CHUNK_MAP_MADVISED_OR_DECOMMITTED) ==
                   0);
        chunk->map[i].bits ^= free_operation | CHUNK_MAP_DIRTY;
        // Find adjacent dirty run(s).
        for (npages = 1; i > arena_chunk_header_npages &&
                         (chunk->map[i - 1].bits & CHUNK_MAP_DIRTY);
             npages++) {
          i--;
          MOZ_ASSERT((chunk->map[i].bits & CHUNK_MAP_MADVISED_OR_DECOMMITTED) ==
                     0);
          chunk->map[i].bits ^= free_operation | CHUNK_MAP_DIRTY;
        }
        chunk->ndirty -= npages;
        mNumDirty -= npages;

#ifdef MALLOC_DECOMMIT
        pages_decommit((void*)(uintptr_t(chunk) + (i << pagesize_2pow)),
                       (npages << pagesize_2pow));
#endif
        mStats.committed -= npages;

#ifndef MALLOC_DECOMMIT
        madvise((void*)(uintptr_t(chunk) + (i << pagesize_2pow)),
                (npages << pagesize_2pow),
                MADV_FREE);
#ifdef MALLOC_DOUBLE_PURGE
        madvised = true;
#endif
#endif
        if (mNumDirty <= (dirty_max >> 1)) {
          break;
        }
      }
    }

    if (chunk->ndirty == 0) {
      mChunksDirty.Remove(chunk);
    }
#ifdef MALLOC_DOUBLE_PURGE
    if (madvised) {
      // The chunk might already be in the list, but this
      // makes sure it's at the front.
      if (mChunksMAdvised.ElementProbablyInList(chunk)) {
        mChunksMAdvised.remove(chunk);
      }
      mChunksMAdvised.pushFront(chunk);
    }
#endif
  }
}

void
arena_t::DallocRun(arena_run_t* aRun, bool aDirty)
{
  arena_chunk_t* chunk;
  size_t size, run_ind, run_pages;

  chunk = GetChunkForPtr(aRun);
  run_ind = (size_t)((uintptr_t(aRun) - uintptr_t(chunk)) >> pagesize_2pow);
  MOZ_DIAGNOSTIC_ASSERT(run_ind >= arena_chunk_header_npages);
  MOZ_DIAGNOSTIC_ASSERT(run_ind < chunk_npages);
  if ((chunk->map[run_ind].bits & CHUNK_MAP_LARGE) != 0) {
    size = chunk->map[run_ind].bits & ~pagesize_mask;
  } else {
    size = aRun->bin->run_size;
  }
  run_pages = (size >> pagesize_2pow);

  // Mark pages as unallocated in the chunk map.
  if (aDirty) {
    size_t i;

    for (i = 0; i < run_pages; i++) {
      MOZ_DIAGNOSTIC_ASSERT((chunk->map[run_ind + i].bits & CHUNK_MAP_DIRTY) ==
                            0);
      chunk->map[run_ind + i].bits = CHUNK_MAP_DIRTY;
    }

    if (chunk->ndirty == 0) {
      mChunksDirty.Insert(chunk);
    }
    chunk->ndirty += run_pages;
    mNumDirty += run_pages;
  } else {
    size_t i;

    for (i = 0; i < run_pages; i++) {
      chunk->map[run_ind + i].bits &= ~(CHUNK_MAP_LARGE | CHUNK_MAP_ALLOCATED);
    }
  }
  chunk->map[run_ind].bits = size | (chunk->map[run_ind].bits & pagesize_mask);
  chunk->map[run_ind + run_pages - 1].bits =
    size | (chunk->map[run_ind + run_pages - 1].bits & pagesize_mask);

  // Try to coalesce forward.
  if (run_ind + run_pages < chunk_npages &&
      (chunk->map[run_ind + run_pages].bits & CHUNK_MAP_ALLOCATED) == 0) {
    size_t nrun_size = chunk->map[run_ind + run_pages].bits & ~pagesize_mask;

    // Remove successor from tree of available runs; the coalesced run is
    // inserted later.
    mRunsAvail.Remove(&chunk->map[run_ind + run_pages]);

    size += nrun_size;
    run_pages = size >> pagesize_2pow;

    MOZ_DIAGNOSTIC_ASSERT(
      (chunk->map[run_ind + run_pages - 1].bits & ~pagesize_mask) == nrun_size);
    chunk->map[run_ind].bits =
      size | (chunk->map[run_ind].bits & pagesize_mask);
    chunk->map[run_ind + run_pages - 1].bits =
      size | (chunk->map[run_ind + run_pages - 1].bits & pagesize_mask);
  }

  // Try to coalesce backward.
  if (run_ind > arena_chunk_header_npages &&
      (chunk->map[run_ind - 1].bits & CHUNK_MAP_ALLOCATED) == 0) {
    size_t prun_size = chunk->map[run_ind - 1].bits & ~pagesize_mask;

    run_ind -= prun_size >> pagesize_2pow;

    // Remove predecessor from tree of available runs; the coalesced run is
    // inserted later.
    mRunsAvail.Remove(&chunk->map[run_ind]);

    size += prun_size;
    run_pages = size >> pagesize_2pow;

    MOZ_DIAGNOSTIC_ASSERT((chunk->map[run_ind].bits & ~pagesize_mask) ==
                          prun_size);
    chunk->map[run_ind].bits =
      size | (chunk->map[run_ind].bits & pagesize_mask);
    chunk->map[run_ind + run_pages - 1].bits =
      size | (chunk->map[run_ind + run_pages - 1].bits & pagesize_mask);
  }

  // Insert into tree of available runs, now that coalescing is complete.
  mRunsAvail.Insert(&chunk->map[run_ind]);

  // Deallocate chunk if it is now completely unused.
  if ((chunk->map[arena_chunk_header_npages].bits &
       (~pagesize_mask | CHUNK_MAP_ALLOCATED)) == arena_maxclass) {
    DeallocChunk(chunk);
  }

  // Enforce mMaxDirty.
  if (mNumDirty > mMaxDirty) {
    Purge(false);
  }
}

void
arena_t::TrimRunHead(arena_chunk_t* aChunk,
                     arena_run_t* aRun,
                     size_t aOldSize,
                     size_t aNewSize)
{
  size_t pageind = (uintptr_t(aRun) - uintptr_t(aChunk)) >> pagesize_2pow;
  size_t head_npages = (aOldSize - aNewSize) >> pagesize_2pow;

  MOZ_ASSERT(aOldSize > aNewSize);

  // Update the chunk map so that arena_t::RunDalloc() can treat the
  // leading run as separately allocated.
  aChunk->map[pageind].bits =
    (aOldSize - aNewSize) | CHUNK_MAP_LARGE | CHUNK_MAP_ALLOCATED;
  aChunk->map[pageind + head_npages].bits =
    aNewSize | CHUNK_MAP_LARGE | CHUNK_MAP_ALLOCATED;

  DallocRun(aRun, false);
}

void
arena_t::TrimRunTail(arena_chunk_t* aChunk,
                     arena_run_t* aRun,
                     size_t aOldSize,
                     size_t aNewSize,
                     bool aDirty)
{
  size_t pageind = (uintptr_t(aRun) - uintptr_t(aChunk)) >> pagesize_2pow;
  size_t npages = aNewSize >> pagesize_2pow;

  MOZ_ASSERT(aOldSize > aNewSize);

  // Update the chunk map so that arena_t::RunDalloc() can treat the
  // trailing run as separately allocated.
  aChunk->map[pageind].bits = aNewSize | CHUNK_MAP_LARGE | CHUNK_MAP_ALLOCATED;
  aChunk->map[pageind + npages].bits =
    (aOldSize - aNewSize) | CHUNK_MAP_LARGE | CHUNK_MAP_ALLOCATED;

  DallocRun((arena_run_t*)(uintptr_t(aRun) + aNewSize), aDirty);
}

arena_run_t*
arena_t::GetNonFullBinRun(arena_bin_t* aBin)
{
  arena_chunk_map_t* mapelm;
  arena_run_t* run;
  unsigned i, remainder;

  // Look for a usable run.
  mapelm = aBin->runs.First();
  if (mapelm) {
    // run is guaranteed to have available space.
    aBin->runs.Remove(mapelm);
    run = (arena_run_t*)(mapelm->bits & ~pagesize_mask);
    return run;
  }
  // No existing runs have any space available.

  // Allocate a new run.
  run = AllocRun(aBin, aBin->run_size, false, false);
  if (!run) {
    return nullptr;
  }
  // Don't initialize if a race in arena_t::RunAlloc() allowed an existing
  // run to become usable.
  if (run == aBin->runcur) {
    return run;
  }

  // Initialize run internals.
  run->bin = aBin;

  for (i = 0; i < aBin->regs_mask_nelms - 1; i++) {
    run->regs_mask[i] = UINT_MAX;
  }
  remainder = aBin->nregs & ((1U << (SIZEOF_INT_2POW + 3)) - 1);
  if (remainder == 0) {
    run->regs_mask[i] = UINT_MAX;
  } else {
    // The last element has spare bits that need to be unset.
    run->regs_mask[i] =
      (UINT_MAX >> ((1U << (SIZEOF_INT_2POW + 3)) - remainder));
  }

  run->regs_minelm = 0;

  run->nfree = aBin->nregs;
#if defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
  run->magic = ARENA_RUN_MAGIC;
#endif

  aBin->stats.curruns++;
  return run;
}

// bin->runcur must have space available before this function is called.
void*
arena_t::MallocBinEasy(arena_bin_t* aBin, arena_run_t* aRun)
{
  void* ret;

  MOZ_DIAGNOSTIC_ASSERT(aRun->magic == ARENA_RUN_MAGIC);
  MOZ_DIAGNOSTIC_ASSERT(aRun->nfree > 0);

  ret = arena_run_reg_alloc(aRun, aBin);
  MOZ_DIAGNOSTIC_ASSERT(ret);
  aRun->nfree--;

  return ret;
}

// Re-fill aBin->runcur, then call arena_t::MallocBinEasy().
void*
arena_t::MallocBinHard(arena_bin_t* aBin)
{
  aBin->runcur = GetNonFullBinRun(aBin);
  if (!aBin->runcur) {
    return nullptr;
  }
  MOZ_DIAGNOSTIC_ASSERT(aBin->runcur->magic == ARENA_RUN_MAGIC);
  MOZ_DIAGNOSTIC_ASSERT(aBin->runcur->nfree > 0);

  return MallocBinEasy(aBin, aBin->runcur);
}

// Calculate bin->run_size such that it meets the following constraints:
//
//   *) bin->run_size >= min_run_size
//   *) bin->run_size <= arena_maxclass
//   *) bin->run_size <= RUN_MAX_SMALL
//   *) run header overhead <= RUN_MAX_OVRHD (or header overhead relaxed).
//
// bin->nregs, bin->regs_mask_nelms, and bin->reg0_offset are
// also calculated here, since these settings are all interdependent.
static size_t
arena_bin_run_size_calc(arena_bin_t* bin, size_t min_run_size)
{
  size_t try_run_size, good_run_size;
  unsigned good_nregs, good_mask_nelms, good_reg0_offset;
  unsigned try_nregs, try_mask_nelms, try_reg0_offset;

  MOZ_ASSERT(min_run_size >= pagesize);
  MOZ_ASSERT(min_run_size <= arena_maxclass);

  // Calculate known-valid settings before entering the run_size
  // expansion loop, so that the first part of the loop always copies
  // valid settings.
  //
  // The do..while loop iteratively reduces the number of regions until
  // the run header and the regions no longer overlap.  A closed formula
  // would be quite messy, since there is an interdependency between the
  // header's mask length and the number of regions.
  try_run_size = min_run_size;
  try_nregs = ((try_run_size - sizeof(arena_run_t)) / bin->reg_size) +
              1; // Counter-act try_nregs-- in loop.
  do {
    try_nregs--;
    try_mask_nelms =
      (try_nregs >> (SIZEOF_INT_2POW + 3)) +
      ((try_nregs & ((1U << (SIZEOF_INT_2POW + 3)) - 1)) ? 1 : 0);
    try_reg0_offset = try_run_size - (try_nregs * bin->reg_size);
  } while (sizeof(arena_run_t) + (sizeof(unsigned) * (try_mask_nelms - 1)) >
           try_reg0_offset);

  // run_size expansion loop.
  do {
    // Copy valid settings before trying more aggressive settings.
    good_run_size = try_run_size;
    good_nregs = try_nregs;
    good_mask_nelms = try_mask_nelms;
    good_reg0_offset = try_reg0_offset;

    // Try more aggressive settings.
    try_run_size += pagesize;
    try_nregs = ((try_run_size - sizeof(arena_run_t)) / bin->reg_size) +
                1; // Counter-act try_nregs-- in loop.
    do {
      try_nregs--;
      try_mask_nelms =
        (try_nregs >> (SIZEOF_INT_2POW + 3)) +
        ((try_nregs & ((1U << (SIZEOF_INT_2POW + 3)) - 1)) ? 1 : 0);
      try_reg0_offset = try_run_size - (try_nregs * bin->reg_size);
    } while (sizeof(arena_run_t) + (sizeof(unsigned) * (try_mask_nelms - 1)) >
             try_reg0_offset);
  } while (try_run_size <= arena_maxclass &&
           RUN_MAX_OVRHD * (bin->reg_size << 3) > RUN_MAX_OVRHD_RELAX &&
           (try_reg0_offset << RUN_BFP) > RUN_MAX_OVRHD * try_run_size);

  MOZ_ASSERT(sizeof(arena_run_t) + (sizeof(unsigned) * (good_mask_nelms - 1)) <=
             good_reg0_offset);
  MOZ_ASSERT((good_mask_nelms << (SIZEOF_INT_2POW + 3)) >= good_nregs);

  // Copy final settings.
  bin->run_size = good_run_size;
  bin->nregs = good_nregs;
  bin->regs_mask_nelms = good_mask_nelms;
  bin->reg0_offset = good_reg0_offset;

  return good_run_size;
}

void*
arena_t::MallocSmall(size_t aSize, bool aZero)
{
  void* ret;
  arena_bin_t* bin;
  arena_run_t* run;

  if (aSize < small_min) {
    // Tiny.
    aSize = RoundUpPow2(aSize);
    if (aSize < (1U << TINY_MIN_2POW)) {
      aSize = 1U << TINY_MIN_2POW;
    }
    bin = &mBins[FloorLog2(aSize >> TINY_MIN_2POW)];
  } else if (aSize <= small_max) {
    // Quantum-spaced.
    aSize = QUANTUM_CEILING(aSize);
    bin = &mBins[ntbins + (aSize >> QUANTUM_2POW_MIN) - 1];
  } else {
    // Sub-page.
    aSize = RoundUpPow2(aSize);
    bin = &mBins[ntbins + nqbins +
                 (FloorLog2(aSize >> SMALL_MAX_2POW_DEFAULT) - 1)];
  }
  MOZ_DIAGNOSTIC_ASSERT(aSize == bin->reg_size);

  {
    MutexAutoLock lock(mLock);
    if ((run = bin->runcur) && run->nfree > 0) {
      ret = MallocBinEasy(bin, run);
    } else {
      ret = MallocBinHard(bin);
    }

    if (!ret) {
      return nullptr;
    }

    mStats.allocated_small += aSize;
  }

  if (aZero == false) {
    if (opt_junk) {
      memset(ret, kAllocJunk, aSize);
    } else if (opt_zero) {
      memset(ret, 0, aSize);
    }
  } else {
    memset(ret, 0, aSize);
  }

  return ret;
}

void*
arena_t::MallocLarge(size_t aSize, bool aZero)
{
  void* ret;

  // Large allocation.
  aSize = PAGE_CEILING(aSize);

  {
    MutexAutoLock lock(mLock);
    ret = AllocRun(nullptr, aSize, true, aZero);
    if (!ret) {
      return nullptr;
    }
    mStats.allocated_large += aSize;
  }

  if (aZero == false) {
    if (opt_junk) {
      memset(ret, kAllocJunk, aSize);
    } else if (opt_zero) {
      memset(ret, 0, aSize);
    }
  }

  return ret;
}

void*
arena_t::Malloc(size_t aSize, bool aZero)
{
  MOZ_DIAGNOSTIC_ASSERT(mMagic == ARENA_MAGIC);
  MOZ_ASSERT(aSize != 0);
  MOZ_ASSERT(QUANTUM_CEILING(aSize) <= arena_maxclass);

  return (aSize <= bin_maxclass) ? MallocSmall(aSize, aZero)
                                 : MallocLarge(aSize, aZero);
}

static inline void*
imalloc(size_t aSize, bool aZero, arena_t* aArena)
{
  MOZ_ASSERT(aSize != 0);

  if (aSize <= arena_maxclass) {
    aArena = aArena ? aArena : choose_arena(aSize);
    return aArena->Malloc(aSize, aZero);
  }
  return huge_malloc(aSize, aZero);
}

// Only handles large allocations that require more than page alignment.
void*
arena_t::Palloc(size_t aAlignment, size_t aSize, size_t aAllocSize)
{
  void* ret;
  size_t offset;
  arena_chunk_t* chunk;

  MOZ_ASSERT((aSize & pagesize_mask) == 0);
  MOZ_ASSERT((aAlignment & pagesize_mask) == 0);

  {
    MutexAutoLock lock(mLock);
    ret = AllocRun(nullptr, aAllocSize, true, false);
    if (!ret) {
      return nullptr;
    }

    chunk = GetChunkForPtr(ret);

    offset = uintptr_t(ret) & (aAlignment - 1);
    MOZ_ASSERT((offset & pagesize_mask) == 0);
    MOZ_ASSERT(offset < aAllocSize);
    if (offset == 0) {
      TrimRunTail(chunk, (arena_run_t*)ret, aAllocSize, aSize, false);
    } else {
      size_t leadsize, trailsize;

      leadsize = aAlignment - offset;
      if (leadsize > 0) {
        TrimRunHead(
          chunk, (arena_run_t*)ret, aAllocSize, aAllocSize - leadsize);
        ret = (void*)(uintptr_t(ret) + leadsize);
      }

      trailsize = aAllocSize - leadsize - aSize;
      if (trailsize != 0) {
        // Trim trailing space.
        MOZ_ASSERT(trailsize < aAllocSize);
        TrimRunTail(chunk, (arena_run_t*)ret, aSize + trailsize, aSize, false);
      }
    }

    mStats.allocated_large += aSize;
  }

  if (opt_junk) {
    memset(ret, kAllocJunk, aSize);
  } else if (opt_zero) {
    memset(ret, 0, aSize);
  }
  return ret;
}

static inline void*
ipalloc(size_t aAlignment, size_t aSize, arena_t* aArena)
{
  void* ret;
  size_t ceil_size;

  // Round size up to the nearest multiple of alignment.
  //
  // This done, we can take advantage of the fact that for each small
  // size class, every object is aligned at the smallest power of two
  // that is non-zero in the base two representation of the size.  For
  // example:
  //
  //   Size |   Base 2 | Minimum alignment
  //   -----+----------+------------------
  //     96 |  1100000 |  32
  //    144 | 10100000 |  32
  //    192 | 11000000 |  64
  //
  // Depending on runtime settings, it is possible that arena_malloc()
  // will further round up to a power of two, but that never causes
  // correctness issues.
  ceil_size = ALIGNMENT_CEILING(aSize, aAlignment);

  // (ceil_size < aSize) protects against the combination of maximal
  // alignment and size greater than maximal alignment.
  if (ceil_size < aSize) {
    // size_t overflow.
    return nullptr;
  }

  if (ceil_size <= pagesize ||
      (aAlignment <= pagesize && ceil_size <= arena_maxclass)) {
    aArena = aArena ? aArena : choose_arena(aSize);
    ret = aArena->Malloc(ceil_size, false);
  } else {
    size_t run_size;

    // We can't achieve sub-page alignment, so round up alignment
    // permanently; it makes later calculations simpler.
    aAlignment = PAGE_CEILING(aAlignment);
    ceil_size = PAGE_CEILING(aSize);

    // (ceil_size < aSize) protects against very large sizes within
    // pagesize of SIZE_T_MAX.
    //
    // (ceil_size + aAlignment < ceil_size) protects against the
    // combination of maximal alignment and ceil_size large enough
    // to cause overflow.  This is similar to the first overflow
    // check above, but it needs to be repeated due to the new
    // ceil_size value, which may now be *equal* to maximal
    // alignment, whereas before we only detected overflow if the
    // original size was *greater* than maximal alignment.
    if (ceil_size < aSize || ceil_size + aAlignment < ceil_size) {
      // size_t overflow.
      return nullptr;
    }

    // Calculate the size of the over-size run that arena_palloc()
    // would need to allocate in order to guarantee the alignment.
    if (ceil_size >= aAlignment) {
      run_size = ceil_size + aAlignment - pagesize;
    } else {
      // It is possible that (aAlignment << 1) will cause
      // overflow, but it doesn't matter because we also
      // subtract pagesize, which in the case of overflow
      // leaves us with a very large run_size.  That causes
      // the first conditional below to fail, which means
      // that the bogus run_size value never gets used for
      // anything important.
      run_size = (aAlignment << 1) - pagesize;
    }

    if (run_size <= arena_maxclass) {
      aArena = aArena ? aArena : choose_arena(aSize);
      ret = aArena->Palloc(aAlignment, ceil_size, run_size);
    } else if (aAlignment <= chunksize) {
      ret = huge_malloc(ceil_size, false);
    } else {
      ret = huge_palloc(ceil_size, aAlignment, false);
    }
  }

  MOZ_ASSERT((uintptr_t(ret) & (aAlignment - 1)) == 0);
  return ret;
}

// Return the size of the allocation pointed to by ptr.
static size_t
arena_salloc(const void* ptr)
{
  size_t ret;
  arena_chunk_t* chunk;
  size_t pageind, mapbits;

  MOZ_ASSERT(ptr);
  MOZ_ASSERT(GetChunkOffsetForPtr(ptr) != 0);

  chunk = GetChunkForPtr(ptr);
  pageind = (((uintptr_t)ptr - (uintptr_t)chunk) >> pagesize_2pow);
  mapbits = chunk->map[pageind].bits;
  MOZ_DIAGNOSTIC_ASSERT((mapbits & CHUNK_MAP_ALLOCATED) != 0);
  if ((mapbits & CHUNK_MAP_LARGE) == 0) {
    arena_run_t* run = (arena_run_t*)(mapbits & ~pagesize_mask);
    MOZ_DIAGNOSTIC_ASSERT(run->magic == ARENA_RUN_MAGIC);
    ret = run->bin->reg_size;
  } else {
    ret = mapbits & ~pagesize_mask;
    MOZ_DIAGNOSTIC_ASSERT(ret != 0);
  }

  return ret;
}

// Validate ptr before assuming that it points to an allocation.  Currently,
// the following validation is performed:
//
// + Check that ptr is not nullptr.
//
// + Check that ptr lies within a mapped chunk.
static inline size_t
isalloc_validate(const void* aPtr)
{
  // If the allocator is not initialized, the pointer can't belong to it.
  if (malloc_initialized == false) {
    return 0;
  }

  auto chunk = GetChunkForPtr(aPtr);
  if (!chunk) {
    return 0;
  }

  if (!gChunkRTree.Get(chunk)) {
    return 0;
  }

  if (chunk != aPtr) {
    MOZ_DIAGNOSTIC_ASSERT(chunk->arena->mMagic == ARENA_MAGIC);
    return arena_salloc(aPtr);
  }

  extent_node_t key;

  // Chunk.
  key.addr = (void*)chunk;
  MutexAutoLock lock(huge_mtx);
  extent_node_t* node = huge.Search(&key);
  if (node) {
    return node->size;
  }
  return 0;
}

static inline size_t
isalloc(const void* aPtr)
{
  MOZ_ASSERT(aPtr);

  auto chunk = GetChunkForPtr(aPtr);
  if (chunk != aPtr) {
    // Region.
    MOZ_DIAGNOSTIC_ASSERT(chunk->arena->mMagic == ARENA_MAGIC);

    return arena_salloc(aPtr);
  }

  extent_node_t key;

  // Chunk (huge allocation).
  MutexAutoLock lock(huge_mtx);

  // Extract from tree of huge allocations.
  key.addr = const_cast<void*>(aPtr);
  extent_node_t* node = huge.Search(&key);
  MOZ_DIAGNOSTIC_ASSERT(node);

  return node->size;
}

template<>
inline void
MozJemalloc::jemalloc_ptr_info(const void* aPtr, jemalloc_ptr_info_t* aInfo)
{
  arena_chunk_t* chunk = GetChunkForPtr(aPtr);

  // Is the pointer null, or within one chunk's size of null?
  // Alternatively, if the allocator is not initialized yet, the pointer
  // can't be known.
  if (!chunk || !malloc_initialized) {
    *aInfo = { TagUnknown, nullptr, 0 };
    return;
  }

  // Look for huge allocations before looking for |chunk| in gChunkRTree.
  // This is necessary because |chunk| won't be in gChunkRTree if it's
  // the second or subsequent chunk in a huge allocation.
  extent_node_t* node;
  extent_node_t key;
  {
    MutexAutoLock lock(huge_mtx);
    key.addr = const_cast<void*>(aPtr);
    node =
      reinterpret_cast<RedBlackTree<extent_node_t, ExtentTreeBoundsTrait>*>(
        &huge)
        ->Search(&key);
    if (node) {
      *aInfo = { TagLiveHuge, node->addr, node->size };
      return;
    }
  }

  // It's not a huge allocation. Check if we have a known chunk.
  if (!gChunkRTree.Get(chunk)) {
    *aInfo = { TagUnknown, nullptr, 0 };
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(chunk->arena->mMagic == ARENA_MAGIC);

  // Get the page number within the chunk.
  size_t pageind = (((uintptr_t)aPtr - (uintptr_t)chunk) >> pagesize_2pow);
  if (pageind < arena_chunk_header_npages) {
    // Within the chunk header.
    *aInfo = { TagUnknown, nullptr, 0 };
    return;
  }

  size_t mapbits = chunk->map[pageind].bits;

  if (!(mapbits & CHUNK_MAP_ALLOCATED)) {
    PtrInfoTag tag = TagFreedPageDirty;
    if (mapbits & CHUNK_MAP_DIRTY) {
      tag = TagFreedPageDirty;
    } else if (mapbits & CHUNK_MAP_DECOMMITTED) {
      tag = TagFreedPageDecommitted;
    } else if (mapbits & CHUNK_MAP_MADVISED) {
      tag = TagFreedPageMadvised;
    } else if (mapbits & CHUNK_MAP_ZEROED) {
      tag = TagFreedPageZeroed;
    } else {
      MOZ_CRASH();
    }

    void* pageaddr = (void*)(uintptr_t(aPtr) & ~pagesize_mask);
    *aInfo = { tag, pageaddr, pagesize };
    return;
  }

  if (mapbits & CHUNK_MAP_LARGE) {
    // It's a large allocation. Only the first page of a large
    // allocation contains its size, so if the address is not in
    // the first page, scan back to find the allocation size.
    size_t size;
    while (true) {
      size = mapbits & ~pagesize_mask;
      if (size != 0) {
        break;
      }

      // The following two return paths shouldn't occur in
      // practice unless there is heap corruption.
      pageind--;
      MOZ_DIAGNOSTIC_ASSERT(pageind >= arena_chunk_header_npages);
      if (pageind < arena_chunk_header_npages) {
        *aInfo = { TagUnknown, nullptr, 0 };
        return;
      }

      mapbits = chunk->map[pageind].bits;
      MOZ_DIAGNOSTIC_ASSERT(mapbits & CHUNK_MAP_LARGE);
      if (!(mapbits & CHUNK_MAP_LARGE)) {
        *aInfo = { TagUnknown, nullptr, 0 };
        return;
      }
    }

    void* addr = ((char*)chunk) + (pageind << pagesize_2pow);
    *aInfo = { TagLiveLarge, addr, size };
    return;
  }

  // It must be a small allocation.
  auto run = (arena_run_t*)(mapbits & ~pagesize_mask);
  MOZ_DIAGNOSTIC_ASSERT(run->magic == ARENA_RUN_MAGIC);

  // The allocation size is stored in the run metadata.
  size_t size = run->bin->reg_size;

  // Address of the first possible pointer in the run after its headers.
  uintptr_t reg0_addr = (uintptr_t)run + run->bin->reg0_offset;
  if (aPtr < (void*)reg0_addr) {
    // In the run header.
    *aInfo = { TagUnknown, nullptr, 0 };
    return;
  }

  // Position in the run.
  unsigned regind = ((uintptr_t)aPtr - reg0_addr) / size;

  // Pointer to the allocation's base address.
  void* addr = (void*)(reg0_addr + regind * size);

  // Check if the allocation has been freed.
  unsigned elm = regind >> (SIZEOF_INT_2POW + 3);
  unsigned bit = regind - (elm << (SIZEOF_INT_2POW + 3));
  PtrInfoTag tag =
    ((run->regs_mask[elm] & (1U << bit))) ? TagFreedSmall : TagLiveSmall;

  *aInfo = { tag, addr, size };
}

namespace Debug {
// Helper for debuggers. We don't want it to be inlined and optimized out.
MOZ_NEVER_INLINE jemalloc_ptr_info_t*
jemalloc_ptr_info(const void* aPtr)
{
  static jemalloc_ptr_info_t info;
  MozJemalloc::jemalloc_ptr_info(aPtr, &info);
  return &info;
}
}

void
arena_t::DallocSmall(arena_chunk_t* aChunk,
                     void* aPtr,
                     arena_chunk_map_t* aMapElm)
{
  arena_run_t* run;
  arena_bin_t* bin;
  size_t size;

  run = (arena_run_t*)(aMapElm->bits & ~pagesize_mask);
  MOZ_DIAGNOSTIC_ASSERT(run->magic == ARENA_RUN_MAGIC);
  bin = run->bin;
  size = bin->reg_size;
  MOZ_DIAGNOSTIC_ASSERT(uintptr_t(aPtr) >= uintptr_t(run) + bin->reg0_offset);
  MOZ_DIAGNOSTIC_ASSERT(
    (uintptr_t(aPtr) - (uintptr_t(run) + bin->reg0_offset)) % size == 0);

  memset(aPtr, kAllocPoison, size);

  arena_run_reg_dalloc(run, bin, aPtr, size);
  run->nfree++;

  if (run->nfree == bin->nregs) {
    // Deallocate run.
    if (run == bin->runcur) {
      bin->runcur = nullptr;
    } else if (bin->nregs != 1) {
      size_t run_pageind =
        (uintptr_t(run) - uintptr_t(aChunk)) >> pagesize_2pow;
      arena_chunk_map_t* run_mapelm = &aChunk->map[run_pageind];

      // This block's conditional is necessary because if the
      // run only contains one region, then it never gets
      // inserted into the non-full runs tree.
      MOZ_DIAGNOSTIC_ASSERT(bin->runs.Search(run_mapelm) == run_mapelm);
      bin->runs.Remove(run_mapelm);
    }
#if defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
    run->magic = 0;
#endif
    DallocRun(run, true);
    bin->stats.curruns--;
  } else if (run->nfree == 1 && run != bin->runcur) {
    // Make sure that bin->runcur always refers to the lowest
    // non-full run, if one exists.
    if (!bin->runcur) {
      bin->runcur = run;
    } else if (uintptr_t(run) < uintptr_t(bin->runcur)) {
      // Switch runcur.
      if (bin->runcur->nfree > 0) {
        arena_chunk_t* runcur_chunk = GetChunkForPtr(bin->runcur);
        size_t runcur_pageind =
          (uintptr_t(bin->runcur) - uintptr_t(runcur_chunk)) >> pagesize_2pow;
        arena_chunk_map_t* runcur_mapelm = &runcur_chunk->map[runcur_pageind];

        // Insert runcur.
        MOZ_DIAGNOSTIC_ASSERT(!bin->runs.Search(runcur_mapelm));
        bin->runs.Insert(runcur_mapelm);
      }
      bin->runcur = run;
    } else {
      size_t run_pageind =
        (uintptr_t(run) - uintptr_t(aChunk)) >> pagesize_2pow;
      arena_chunk_map_t* run_mapelm = &aChunk->map[run_pageind];

      MOZ_DIAGNOSTIC_ASSERT(bin->runs.Search(run_mapelm) == nullptr);
      bin->runs.Insert(run_mapelm);
    }
  }
  mStats.allocated_small -= size;
}

void
arena_t::DallocLarge(arena_chunk_t* aChunk, void* aPtr)
{
  MOZ_DIAGNOSTIC_ASSERT((uintptr_t(aPtr) & pagesize_mask) == 0);
  size_t pageind = (uintptr_t(aPtr) - uintptr_t(aChunk)) >> pagesize_2pow;
  size_t size = aChunk->map[pageind].bits & ~pagesize_mask;

  memset(aPtr, kAllocPoison, size);
  mStats.allocated_large -= size;

  DallocRun((arena_run_t*)aPtr, true);
}

static inline void
arena_dalloc(void* aPtr, size_t aOffset)
{
  MOZ_ASSERT(aPtr);
  MOZ_ASSERT(aOffset != 0);
  MOZ_ASSERT(GetChunkOffsetForPtr(aPtr) == aOffset);

  auto chunk = (arena_chunk_t*)((uintptr_t)aPtr - aOffset);
  auto arena = chunk->arena;
  MOZ_ASSERT(arena);
  MOZ_DIAGNOSTIC_ASSERT(arena->mMagic == ARENA_MAGIC);

  MutexAutoLock lock(arena->mLock);
  size_t pageind = aOffset >> pagesize_2pow;
  arena_chunk_map_t* mapelm = &chunk->map[pageind];
  MOZ_DIAGNOSTIC_ASSERT((mapelm->bits & CHUNK_MAP_ALLOCATED) != 0);
  if ((mapelm->bits & CHUNK_MAP_LARGE) == 0) {
    // Small allocation.
    arena->DallocSmall(chunk, aPtr, mapelm);
  } else {
    // Large allocation.
    arena->DallocLarge(chunk, aPtr);
  }
}

static inline void
idalloc(void* ptr)
{
  size_t offset;

  MOZ_ASSERT(ptr);

  offset = GetChunkOffsetForPtr(ptr);
  if (offset != 0) {
    arena_dalloc(ptr, offset);
  } else {
    huge_dalloc(ptr);
  }
}

void
arena_t::RallocShrinkLarge(arena_chunk_t* aChunk,
                           void* aPtr,
                           size_t aSize,
                           size_t aOldSize)
{
  MOZ_ASSERT(aSize < aOldSize);

  // Shrink the run, and make trailing pages available for other
  // allocations.
  MutexAutoLock lock(mLock);
  TrimRunTail(aChunk, (arena_run_t*)aPtr, aOldSize, aSize, true);
  mStats.allocated_large -= aOldSize - aSize;
}

// Returns whether reallocation was successful.
bool
arena_t::RallocGrowLarge(arena_chunk_t* aChunk,
                         void* aPtr,
                         size_t aSize,
                         size_t aOldSize)
{
  size_t pageind = (uintptr_t(aPtr) - uintptr_t(aChunk)) >> pagesize_2pow;
  size_t npages = aOldSize >> pagesize_2pow;

  MutexAutoLock lock(mLock);
  MOZ_DIAGNOSTIC_ASSERT(aOldSize ==
                        (aChunk->map[pageind].bits & ~pagesize_mask));

  // Try to extend the run.
  MOZ_ASSERT(aSize > aOldSize);
  if (pageind + npages < chunk_npages &&
      (aChunk->map[pageind + npages].bits & CHUNK_MAP_ALLOCATED) == 0 &&
      (aChunk->map[pageind + npages].bits & ~pagesize_mask) >=
        aSize - aOldSize) {
    // The next run is available and sufficiently large.  Split the
    // following run, then merge the first part with the existing
    // allocation.
    if (!SplitRun((arena_run_t*)(uintptr_t(aChunk) +
                                 ((pageind + npages) << pagesize_2pow)),
                  aSize - aOldSize,
                  true,
                  false)) {
      return false;
    }

    aChunk->map[pageind].bits = aSize | CHUNK_MAP_LARGE | CHUNK_MAP_ALLOCATED;
    aChunk->map[pageind + npages].bits = CHUNK_MAP_LARGE | CHUNK_MAP_ALLOCATED;

    mStats.allocated_large += aSize - aOldSize;
    return true;
  }

  return false;
}

// Try to resize a large allocation, in order to avoid copying.  This will
// always fail if growing an object, and the following run is already in use.
// Returns whether reallocation was successful.
static bool
arena_ralloc_large(void* aPtr, size_t aSize, size_t aOldSize)
{
  size_t psize;

  psize = PAGE_CEILING(aSize);
  if (psize == aOldSize) {
    // Same size class.
    if (aSize < aOldSize) {
      memset((void*)((uintptr_t)aPtr + aSize), kAllocPoison, aOldSize - aSize);
    }
    return true;
  }

  arena_chunk_t* chunk = GetChunkForPtr(aPtr);
  arena_t* arena = chunk->arena;
  MOZ_DIAGNOSTIC_ASSERT(arena->mMagic == ARENA_MAGIC);

  if (psize < aOldSize) {
    // Fill before shrinking in order avoid a race.
    memset((void*)((uintptr_t)aPtr + aSize), kAllocPoison, aOldSize - aSize);
    arena->RallocShrinkLarge(chunk, aPtr, psize, aOldSize);
    return true;
  }

  bool ret = arena->RallocGrowLarge(chunk, aPtr, psize, aOldSize);
  if (ret && opt_zero) {
    memset((void*)((uintptr_t)aPtr + aOldSize), 0, aSize - aOldSize);
  }
  return ret;
}

static void*
arena_ralloc(void* aPtr, size_t aSize, size_t aOldSize, arena_t* aArena)
{
  void* ret;
  size_t copysize;

  // Try to avoid moving the allocation.
  if (aSize < small_min) {
    if (aOldSize < small_min &&
        (RoundUpPow2(aSize) >> (TINY_MIN_2POW + 1) ==
         RoundUpPow2(aOldSize) >> (TINY_MIN_2POW + 1))) {
      goto IN_PLACE; // Same size class.
    }
  } else if (aSize <= small_max) {
    if (aOldSize >= small_min && aOldSize <= small_max &&
        (QUANTUM_CEILING(aSize) >> QUANTUM_2POW_MIN) ==
          (QUANTUM_CEILING(aOldSize) >> QUANTUM_2POW_MIN)) {
      goto IN_PLACE; // Same size class.
    }
  } else if (aSize <= bin_maxclass) {
    if (aOldSize > small_max && aOldSize <= bin_maxclass &&
        RoundUpPow2(aSize) == RoundUpPow2(aOldSize)) {
      goto IN_PLACE; // Same size class.
    }
  } else if (aOldSize > bin_maxclass && aOldSize <= arena_maxclass) {
    MOZ_ASSERT(aSize > bin_maxclass);
    if (arena_ralloc_large(aPtr, aSize, aOldSize)) {
      return aPtr;
    }
  }

  // If we get here, then aSize and aOldSize are different enough that we
  // need to move the object.  In that case, fall back to allocating new
  // space and copying.
  aArena = aArena ? aArena : choose_arena(aSize);
  ret = aArena->Malloc(aSize, false);
  if (!ret) {
    return nullptr;
  }

  // Junk/zero-filling were already done by arena_t::Malloc().
  copysize = (aSize < aOldSize) ? aSize : aOldSize;
#ifdef VM_COPY_MIN
  if (copysize >= VM_COPY_MIN) {
    pages_copy(ret, aPtr, copysize);
  } else
#endif
  {
    memcpy(ret, aPtr, copysize);
  }
  idalloc(aPtr);
  return ret;
IN_PLACE:
  if (aSize < aOldSize) {
    memset((void*)(uintptr_t(aPtr) + aSize), kAllocPoison, aOldSize - aSize);
  } else if (opt_zero && aSize > aOldSize) {
    memset((void*)(uintptr_t(aPtr) + aOldSize), 0, aSize - aOldSize);
  }
  return aPtr;
}

static inline void*
iralloc(void* aPtr, size_t aSize, arena_t* aArena)
{
  size_t oldsize;

  MOZ_ASSERT(aPtr);
  MOZ_ASSERT(aSize != 0);

  oldsize = isalloc(aPtr);

  return (aSize <= arena_maxclass) ? arena_ralloc(aPtr, aSize, oldsize, aArena)
                                   : huge_ralloc(aPtr, aSize, oldsize);
}

arena_t::arena_t()
{
  unsigned i;
  arena_bin_t* bin;
  size_t prev_run_size;

  MOZ_RELEASE_ASSERT(mLock.Init());

  memset(&mLink, 0, sizeof(mLink));
  memset(&mStats, 0, sizeof(arena_stats_t));

  // Initialize chunks.
  mChunksDirty.Init();
#ifdef MALLOC_DOUBLE_PURGE
  new (&mChunksMAdvised) DoublyLinkedList<arena_chunk_t>();
#endif
  mSpare = nullptr;

  mNumDirty = 0;
  // Reduce the maximum amount of dirty pages we allow to be kept on
  // thread local arenas. TODO: make this more flexible.
  mMaxDirty = opt_dirty_max >> 3;

  mRunsAvail.Init();

  // Initialize bins.
  prev_run_size = pagesize;

  // (2^n)-spaced tiny bins.
  for (i = 0; i < ntbins; i++) {
    bin = &mBins[i];
    bin->runcur = nullptr;
    bin->runs.Init();

    bin->reg_size = (1ULL << (TINY_MIN_2POW + i));

    prev_run_size = arena_bin_run_size_calc(bin, prev_run_size);

    memset(&bin->stats, 0, sizeof(malloc_bin_stats_t));
  }

  // Quantum-spaced bins.
  for (; i < ntbins + nqbins; i++) {
    bin = &mBins[i];
    bin->runcur = nullptr;
    bin->runs.Init();

    bin->reg_size = quantum * (i - ntbins + 1);

    prev_run_size = arena_bin_run_size_calc(bin, prev_run_size);

    memset(&bin->stats, 0, sizeof(malloc_bin_stats_t));
  }

  // (2^n)-spaced sub-page bins.
  for (; i < ntbins + nqbins + nsbins; i++) {
    bin = &mBins[i];
    bin->runcur = nullptr;
    bin->runs.Init();

    bin->reg_size = (small_max << (i - (ntbins + nqbins) + 1));

    prev_run_size = arena_bin_run_size_calc(bin, prev_run_size);

    memset(&bin->stats, 0, sizeof(malloc_bin_stats_t));
  }

#if defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
  mMagic = ARENA_MAGIC;
#endif
}

arena_t*
ArenaCollection::CreateArena(bool aIsPrivate)
{
  fallible_t fallible;
  arena_t* ret = new (fallible) arena_t();
  if (!ret) {
    // Only reached if there is an OOM error.

    // OOM here is quite inconvenient to propagate, since dealing with it
    // would require a check for failure in the fast path.  Instead, punt
    // by using the first arena.
    // In practice, this is an extremely unlikely failure.
    _malloc_message(_getprogname(), ": (malloc) Error initializing arena\n");

    return mDefaultArena;
  }

  MutexAutoLock lock(mLock);

  // TODO: Use random Ids.
  ret->mId = mLastArenaId++;
  (aIsPrivate ? mPrivateArenas : mArenas).Insert(ret);
  return ret;
}

// End arena.
// ***************************************************************************
// Begin general internal functions.

static void*
huge_malloc(size_t size, bool zero)
{
  return huge_palloc(size, chunksize, zero);
}

static void*
huge_palloc(size_t aSize, size_t aAlignment, bool aZero)
{
  void* ret;
  size_t csize;
  size_t psize;
  extent_node_t* node;
  bool zeroed;

  // Allocate one or more contiguous chunks for this request.
  csize = CHUNK_CEILING(aSize);
  if (csize == 0) {
    // size is large enough to cause size_t wrap-around.
    return nullptr;
  }

  // Allocate an extent node with which to track the chunk.
  node = base_node_alloc();
  if (!node) {
    return nullptr;
  }

  ret = chunk_alloc(csize, aAlignment, false, &zeroed);
  if (!ret) {
    base_node_dealloc(node);
    return nullptr;
  }
  if (aZero) {
    chunk_ensure_zero(ret, csize, zeroed);
  }

  // Insert node into huge.
  node->addr = ret;
  psize = PAGE_CEILING(aSize);
  node->size = psize;

  {
    MutexAutoLock lock(huge_mtx);
    huge.Insert(node);

    // Although we allocated space for csize bytes, we indicate that we've
    // allocated only psize bytes.
    //
    // If DECOMMIT is defined, this is a reasonable thing to do, since
    // we'll explicitly decommit the bytes in excess of psize.
    //
    // If DECOMMIT is not defined, then we're relying on the OS to be lazy
    // about how it allocates physical pages to mappings.  If we never
    // touch the pages in excess of psize, the OS won't allocate a physical
    // page, and we won't use more than psize bytes of physical memory.
    //
    // A correct program will only touch memory in excess of how much it
    // requested if it first calls malloc_usable_size and finds out how
    // much space it has to play with.  But because we set node->size =
    // psize above, malloc_usable_size will return psize, not csize, and
    // the program will (hopefully) never touch bytes in excess of psize.
    // Thus those bytes won't take up space in physical memory, and we can
    // reasonably claim we never "allocated" them in the first place.
    huge_allocated += psize;
    huge_mapped += csize;
  }

#ifdef MALLOC_DECOMMIT
  if (csize - psize > 0) {
    pages_decommit((void*)((uintptr_t)ret + psize), csize - psize);
  }
#endif

  if (aZero == false) {
    if (opt_junk) {
#ifdef MALLOC_DECOMMIT
      memset(ret, kAllocJunk, psize);
#else
      memset(ret, kAllocJunk, csize);
#endif
    } else if (opt_zero) {
#ifdef MALLOC_DECOMMIT
      memset(ret, 0, psize);
#else
      memset(ret, 0, csize);
#endif
    }
  }

  return ret;
}

static void*
huge_ralloc(void* aPtr, size_t aSize, size_t aOldSize)
{
  void* ret;
  size_t copysize;

  // Avoid moving the allocation if the size class would not change.
  if (aOldSize > arena_maxclass &&
      CHUNK_CEILING(aSize) == CHUNK_CEILING(aOldSize)) {
    size_t psize = PAGE_CEILING(aSize);
    if (aSize < aOldSize) {
      memset((void*)((uintptr_t)aPtr + aSize), kAllocPoison, aOldSize - aSize);
    }
#ifdef MALLOC_DECOMMIT
    if (psize < aOldSize) {
      extent_node_t key;

      pages_decommit((void*)((uintptr_t)aPtr + psize), aOldSize - psize);

      // Update recorded size.
      MutexAutoLock lock(huge_mtx);
      key.addr = const_cast<void*>(aPtr);
      extent_node_t* node = huge.Search(&key);
      MOZ_ASSERT(node);
      MOZ_ASSERT(node->size == aOldSize);
      huge_allocated -= aOldSize - psize;
      // No need to change huge_mapped, because we didn't (un)map anything.
      node->size = psize;
    } else if (psize > aOldSize) {
      if (!pages_commit((void*)((uintptr_t)aPtr + aOldSize),
                        psize - aOldSize)) {
        return nullptr;
      }
    }
#endif

    // Although we don't have to commit or decommit anything if
    // DECOMMIT is not defined and the size class didn't change, we
    // do need to update the recorded size if the size increased,
    // so malloc_usable_size doesn't return a value smaller than
    // what was requested via realloc().
    if (psize > aOldSize) {
      // Update recorded size.
      extent_node_t key;
      MutexAutoLock lock(huge_mtx);
      key.addr = const_cast<void*>(aPtr);
      extent_node_t* node = huge.Search(&key);
      MOZ_ASSERT(node);
      MOZ_ASSERT(node->size == aOldSize);
      huge_allocated += psize - aOldSize;
      // No need to change huge_mapped, because we didn't
      // (un)map anything.
      node->size = psize;
    }

    if (opt_zero && aSize > aOldSize) {
      memset((void*)((uintptr_t)aPtr + aOldSize), 0, aSize - aOldSize);
    }
    return aPtr;
  }

  // If we get here, then aSize and aOldSize are different enough that we
  // need to use a different size class.  In that case, fall back to
  // allocating new space and copying.
  ret = huge_malloc(aSize, false);
  if (!ret) {
    return nullptr;
  }

  copysize = (aSize < aOldSize) ? aSize : aOldSize;
#ifdef VM_COPY_MIN
  if (copysize >= VM_COPY_MIN) {
    pages_copy(ret, aPtr, copysize);
  } else
#endif
  {
    memcpy(ret, aPtr, copysize);
  }
  idalloc(aPtr);
  return ret;
}

static void
huge_dalloc(void* aPtr)
{
  extent_node_t* node;
  {
    extent_node_t key;
    MutexAutoLock lock(huge_mtx);

    // Extract from tree of huge allocations.
    key.addr = aPtr;
    node = huge.Search(&key);
    MOZ_ASSERT(node);
    MOZ_ASSERT(node->addr == aPtr);
    huge.Remove(node);

    huge_allocated -= node->size;
    huge_mapped -= CHUNK_CEILING(node->size);
  }

  // Unmap chunk.
  chunk_dealloc(node->addr, CHUNK_CEILING(node->size), HUGE_CHUNK);

  base_node_dealloc(node);
}

static size_t
GetKernelPageSize()
{
  static size_t kernel_page_size = ([]() {
#ifdef XP_WIN
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    long result = sysconf(_SC_PAGESIZE);
    MOZ_ASSERT(result != -1);
    return result;
#endif
  })();
  return kernel_page_size;
}

// Returns whether the allocator was successfully initialized.
#if !defined(XP_WIN)
static
#endif
  bool
  malloc_init_hard()
{
  unsigned i;
  const char* opts;
  long result;

#ifndef XP_WIN
  MutexAutoLock lock(gInitLock);
#endif

  if (malloc_initialized) {
    // Another thread initialized the allocator before this one
    // acquired gInitLock.
    return true;
  }

  if (!thread_arena.init()) {
    return true;
  }

  // Get page size and number of CPUs
  result = GetKernelPageSize();
  // We assume that the page size is a power of 2.
  MOZ_ASSERT(((result - 1) & result) == 0);
#ifdef MALLOC_STATIC_PAGESIZE
  if (pagesize % (size_t)result) {
    _malloc_message(
      _getprogname(),
      "Compile-time page size does not divide the runtime one.\n");
    MOZ_CRASH();
  }
#else
  pagesize = (size_t)result;
  pagesize_mask = (size_t)result - 1;
  pagesize_2pow = FloorLog2(result);
  MOZ_RELEASE_ASSERT(1ULL << pagesize_2pow == pagesize,
                     "Page size is not a power of two");
#endif

  // Get runtime configuration.
  if ((opts = getenv("MALLOC_OPTIONS"))) {
    for (i = 0; opts[i] != '\0'; i++) {
      unsigned j, nreps;
      bool nseen;

      // Parse repetition count, if any.
      for (nreps = 0, nseen = false;; i++, nseen = true) {
        switch (opts[i]) {
          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
            nreps *= 10;
            nreps += opts[i] - '0';
            break;
          default:
            goto MALLOC_OUT;
        }
      }
    MALLOC_OUT:
      if (nseen == false) {
        nreps = 1;
      }

      for (j = 0; j < nreps; j++) {
        switch (opts[i]) {
          case 'f':
            opt_dirty_max >>= 1;
            break;
          case 'F':
            if (opt_dirty_max == 0) {
              opt_dirty_max = 1;
            } else if ((opt_dirty_max << 1) != 0) {
              opt_dirty_max <<= 1;
            }
            break;
#ifdef MOZ_DEBUG
          case 'j':
            opt_junk = false;
            break;
          case 'J':
            opt_junk = true;
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
                            "in malloc options: '",
                            cbuf,
                            "'\n");
          }
        }
      }
    }
  }

#ifndef MALLOC_STATIC_PAGESIZE
  // Set bin-related variables.
  bin_maxclass = (pagesize >> 1);
  nsbins = pagesize_2pow - SMALL_MAX_2POW_DEFAULT - 1;

  chunk_npages = (chunksize >> pagesize_2pow);

  arena_chunk_header_npages = calculate_arena_header_pages();
  arena_maxclass = calculate_arena_maxclass();
#endif

  gRecycledSize = 0;

  // Various sanity checks that regard configuration.
  MOZ_ASSERT(quantum >= sizeof(void*));
  MOZ_ASSERT(quantum <= pagesize);
  MOZ_ASSERT(chunksize >= pagesize);
  MOZ_ASSERT(quantum * 4 <= chunksize);

  // Initialize chunks data.
  chunks_mtx.Init();
  gChunksBySize.Init();
  gChunksByAddress.Init();

  // Initialize huge allocation data.
  huge_mtx.Init();
  huge.Init();
  huge_allocated = 0;
  huge_mapped = 0;

  // Initialize base allocation data structures.
  base_mapped = 0;
  base_committed = 0;
  base_nodes = nullptr;
  base_mtx.Init();

  // Initialize arenas collection here.
  if (!gArenas.Init()) {
    return false;
  }

  // Assign the default arena to the initial thread.
  thread_arena.set(gArenas.GetDefault());

  if (!gChunkRTree.Init()) {
    return false;
  }

  malloc_initialized = true;

  // Dummy call so that the function is not removed by dead-code elimination
  Debug::jemalloc_ptr_info(nullptr);

#if !defined(XP_WIN) && !defined(XP_DARWIN)
  // Prevent potential deadlock on malloc locks after fork.
  pthread_atfork(
    _malloc_prefork, _malloc_postfork_parent, _malloc_postfork_child);
#endif

  return true;
}

// End general internal functions.
// ***************************************************************************
// Begin malloc(3)-compatible functions.

// The BaseAllocator class is a helper class that implements the base allocator
// functions (malloc, calloc, realloc, free, memalign) for a given arena,
// or an appropriately chosen arena (per choose_arena()) when none is given.
struct BaseAllocator
{
#define MALLOC_DECL(name, return_type, ...)                                    \
  inline return_type name(__VA_ARGS__);

#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC_BASE
#include "malloc_decls.h"

  explicit BaseAllocator(arena_t* aArena)
    : mArena(aArena)
  {
  }

private:
  arena_t* mArena;
};

#define MALLOC_DECL(name, return_type, ...)                                    \
  template<>                                                                   \
  inline return_type MozJemalloc::name(ARGS_HELPER(TYPED_ARGS, ##__VA_ARGS__)) \
  {                                                                            \
    BaseAllocator allocator(nullptr);                                          \
    return allocator.name(ARGS_HELPER(ARGS, ##__VA_ARGS__));                   \
  }
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC_BASE
#include "malloc_decls.h"

inline void*
BaseAllocator::malloc(size_t aSize)
{
  void* ret;

  if (!malloc_init()) {
    ret = nullptr;
    goto RETURN;
  }

  if (aSize == 0) {
    aSize = 1;
  }

  ret = imalloc(aSize, /* zero = */ false, mArena);

RETURN:
  if (!ret) {
    errno = ENOMEM;
  }

  return ret;
}

inline void*
BaseAllocator::memalign(size_t aAlignment, size_t aSize)
{
  void* ret;

  MOZ_ASSERT(((aAlignment - 1) & aAlignment) == 0);

  if (!malloc_init()) {
    return nullptr;
  }

  if (aSize == 0) {
    aSize = 1;
  }

  aAlignment = aAlignment < sizeof(void*) ? sizeof(void*) : aAlignment;
  ret = ipalloc(aAlignment, aSize, mArena);

  return ret;
}

inline void*
BaseAllocator::calloc(size_t aNum, size_t aSize)
{
  void* ret;

  if (malloc_init()) {
    CheckedInt<size_t> checkedSize = CheckedInt<size_t>(aNum) * aSize;
    if (checkedSize.isValid()) {
      size_t allocSize = checkedSize.value();
      if (allocSize == 0) {
        allocSize = 1;
      }
      ret = imalloc(allocSize, /* zero = */ true, mArena);
    } else {
      ret = nullptr;
    }
  } else {
    ret = nullptr;
  }

  if (!ret) {
    errno = ENOMEM;
  }

  return ret;
}

inline void*
BaseAllocator::realloc(void* aPtr, size_t aSize)
{
  void* ret;

  if (aSize == 0) {
    aSize = 1;
  }

  if (aPtr) {
    MOZ_RELEASE_ASSERT(malloc_initialized);

    ret = iralloc(aPtr, aSize, mArena);

    if (!ret) {
      errno = ENOMEM;
    }
  } else {
    if (!malloc_init()) {
      ret = nullptr;
    } else {
      ret = imalloc(aSize, /* zero = */ false, mArena);
    }

    if (!ret) {
      errno = ENOMEM;
    }
  }

  return ret;
}

inline void
BaseAllocator::free(void* aPtr)
{
  size_t offset;

  // A version of idalloc that checks for nullptr pointer.
  offset = GetChunkOffsetForPtr(aPtr);
  if (offset != 0) {
    MOZ_RELEASE_ASSERT(malloc_initialized);
    arena_dalloc(aPtr, offset);
  } else if (aPtr) {
    MOZ_RELEASE_ASSERT(malloc_initialized);
    huge_dalloc(aPtr);
  }
}

template<void* (*memalign)(size_t, size_t)>
struct AlignedAllocator
{
  static inline int posix_memalign(void** aMemPtr,
                                   size_t aAlignment,
                                   size_t aSize)
  {
    void* result;

    // alignment must be a power of two and a multiple of sizeof(void*)
    if (((aAlignment - 1) & aAlignment) != 0 || aAlignment < sizeof(void*)) {
      return EINVAL;
    }

    // The 0-->1 size promotion is done in the memalign() call below
    result = memalign(aAlignment, aSize);

    if (!result) {
      return ENOMEM;
    }

    *aMemPtr = result;
    return 0;
  }

  static inline void* aligned_alloc(size_t aAlignment, size_t aSize)
  {
    if (aSize % aAlignment) {
      return nullptr;
    }
    return memalign(aAlignment, aSize);
  }

  static inline void* valloc(size_t aSize)
  {
    return memalign(GetKernelPageSize(), aSize);
  }
};

template<>
inline int
MozJemalloc::posix_memalign(void** aMemPtr, size_t aAlignment, size_t aSize)
{
  return AlignedAllocator<memalign>::posix_memalign(aMemPtr, aAlignment, aSize);
}

template<>
inline void*
MozJemalloc::aligned_alloc(size_t aAlignment, size_t aSize)
{
  return AlignedAllocator<memalign>::aligned_alloc(aAlignment, aSize);
}

template<>
inline void*
MozJemalloc::valloc(size_t aSize)
{
  return AlignedAllocator<memalign>::valloc(aSize);
}

// End malloc(3)-compatible functions.
// ***************************************************************************
// Begin non-standard functions.

// This was added by Mozilla for use by SQLite.
template<>
inline size_t
MozJemalloc::malloc_good_size(size_t aSize)
{
  // This duplicates the logic in imalloc(), arena_malloc() and
  // arena_t::MallocSmall().
  if (aSize < small_min) {
    // Small (tiny).
    aSize = RoundUpPow2(aSize);

    // We omit the #ifdefs from arena_t::MallocSmall() --
    // it can be inaccurate with its size in some cases, but this
    // function must be accurate.
    if (aSize < (1U << TINY_MIN_2POW)) {
      aSize = (1U << TINY_MIN_2POW);
    }
  } else if (aSize <= small_max) {
    // Small (quantum-spaced).
    aSize = QUANTUM_CEILING(aSize);
  } else if (aSize <= bin_maxclass) {
    // Small (sub-page).
    aSize = RoundUpPow2(aSize);
  } else if (aSize <= arena_maxclass) {
    // Large.
    aSize = PAGE_CEILING(aSize);
  } else {
    // Huge.  We use PAGE_CEILING to get psize, instead of using
    // CHUNK_CEILING to get csize.  This ensures that this
    // malloc_usable_size(malloc(n)) always matches
    // malloc_good_size(n).
    aSize = PAGE_CEILING(aSize);
  }
  return aSize;
}

template<>
inline size_t
MozJemalloc::malloc_usable_size(usable_ptr_t aPtr)
{
  return isalloc_validate(aPtr);
}

template<>
inline void
MozJemalloc::jemalloc_stats(jemalloc_stats_t* aStats)
{
  size_t non_arena_mapped, chunk_header_size;

  if (!aStats) {
    return;
  }
  if (!malloc_initialized) {
    memset(aStats, 0, sizeof(*aStats));
    return;
  }

  // Gather runtime settings.
  aStats->opt_junk = opt_junk;
  aStats->opt_zero = opt_zero;
  aStats->quantum = quantum;
  aStats->small_max = small_max;
  aStats->large_max = arena_maxclass;
  aStats->chunksize = chunksize;
  aStats->page_size = pagesize;
  aStats->dirty_max = opt_dirty_max;

  // Gather current memory usage statistics.
  aStats->narenas = 0;
  aStats->mapped = 0;
  aStats->allocated = 0;
  aStats->waste = 0;
  aStats->page_cache = 0;
  aStats->bookkeeping = 0;
  aStats->bin_unused = 0;

  non_arena_mapped = 0;

  // Get huge mapped/allocated.
  {
    MutexAutoLock lock(huge_mtx);
    non_arena_mapped += huge_mapped;
    aStats->allocated += huge_allocated;
    MOZ_ASSERT(huge_mapped >= huge_allocated);
  }

  // Get base mapped/allocated.
  {
    MutexAutoLock lock(base_mtx);
    non_arena_mapped += base_mapped;
    aStats->bookkeeping += base_committed;
    MOZ_ASSERT(base_mapped >= base_committed);
  }

  gArenas.mLock.Lock();
  // Iterate over arenas.
  for (auto arena : gArenas.iter()) {
    size_t arena_mapped, arena_allocated, arena_committed, arena_dirty, j,
      arena_unused, arena_headers;
    arena_run_t* run;

    arena_headers = 0;
    arena_unused = 0;

    {
      MutexAutoLock lock(arena->mLock);

      arena_mapped = arena->mStats.mapped;

      // "committed" counts dirty and allocated memory.
      arena_committed = arena->mStats.committed << pagesize_2pow;

      arena_allocated =
        arena->mStats.allocated_small + arena->mStats.allocated_large;

      arena_dirty = arena->mNumDirty << pagesize_2pow;

      for (j = 0; j < ntbins + nqbins + nsbins; j++) {
        arena_bin_t* bin = &arena->mBins[j];
        size_t bin_unused = 0;

        for (auto mapelm : bin->runs.iter()) {
          run = (arena_run_t*)(mapelm->bits & ~pagesize_mask);
          bin_unused += run->nfree * bin->reg_size;
        }

        if (bin->runcur) {
          bin_unused += bin->runcur->nfree * bin->reg_size;
        }

        arena_unused += bin_unused;
        arena_headers += bin->stats.curruns * bin->reg0_offset;
      }
    }

    MOZ_ASSERT(arena_mapped >= arena_committed);
    MOZ_ASSERT(arena_committed >= arena_allocated + arena_dirty);

    // "waste" is committed memory that is neither dirty nor
    // allocated.
    aStats->mapped += arena_mapped;
    aStats->allocated += arena_allocated;
    aStats->page_cache += arena_dirty;
    aStats->waste += arena_committed - arena_allocated - arena_dirty -
                     arena_unused - arena_headers;
    aStats->bin_unused += arena_unused;
    aStats->bookkeeping += arena_headers;
    aStats->narenas++;
  }
  gArenas.mLock.Unlock();

  // Account for arena chunk headers in bookkeeping rather than waste.
  chunk_header_size =
    ((aStats->mapped / aStats->chunksize) * arena_chunk_header_npages)
    << pagesize_2pow;

  aStats->mapped += non_arena_mapped;
  aStats->bookkeeping += chunk_header_size;
  aStats->waste -= chunk_header_size;

  MOZ_ASSERT(aStats->mapped >= aStats->allocated + aStats->waste +
                                 aStats->page_cache + aStats->bookkeeping);
}

#ifdef MALLOC_DOUBLE_PURGE

// Explicitly remove all of this chunk's MADV_FREE'd pages from memory.
static void
hard_purge_chunk(arena_chunk_t* aChunk)
{
  // See similar logic in arena_t::Purge().
  for (size_t i = arena_chunk_header_npages; i < chunk_npages; i++) {
    // Find all adjacent pages with CHUNK_MAP_MADVISED set.
    size_t npages;
    for (npages = 0; aChunk->map[i + npages].bits & CHUNK_MAP_MADVISED &&
                     i + npages < chunk_npages;
         npages++) {
      // Turn off the chunk's MADV_FREED bit and turn on its
      // DECOMMITTED bit.
      MOZ_DIAGNOSTIC_ASSERT(
        !(aChunk->map[i + npages].bits & CHUNK_MAP_DECOMMITTED));
      aChunk->map[i + npages].bits ^= CHUNK_MAP_MADVISED_OR_DECOMMITTED;
    }

    // We could use mincore to find out which pages are actually
    // present, but it's not clear that's better.
    if (npages > 0) {
      pages_decommit(((char*)aChunk) + (i << pagesize_2pow),
                     npages << pagesize_2pow);
      Unused << pages_commit(((char*)aChunk) + (i << pagesize_2pow),
                             npages << pagesize_2pow);
    }
    i += npages;
  }
}

// Explicitly remove all of this arena's MADV_FREE'd pages from memory.
void
arena_t::HardPurge()
{
  MutexAutoLock lock(mLock);

  while (!mChunksMAdvised.isEmpty()) {
    arena_chunk_t* chunk = mChunksMAdvised.popFront();
    hard_purge_chunk(chunk);
  }
}

template<>
inline void
MozJemalloc::jemalloc_purge_freed_pages()
{
  if (malloc_initialized) {
    MutexAutoLock lock(gArenas.mLock);
    for (auto arena : gArenas.iter()) {
      arena->HardPurge();
    }
  }
}

#else // !defined MALLOC_DOUBLE_PURGE

template<>
inline void
MozJemalloc::jemalloc_purge_freed_pages()
{
  // Do nothing.
}

#endif // defined MALLOC_DOUBLE_PURGE

template<>
inline void
MozJemalloc::jemalloc_free_dirty_pages(void)
{
  if (malloc_initialized) {
    MutexAutoLock lock(gArenas.mLock);
    for (auto arena : gArenas.iter()) {
      MutexAutoLock arena_lock(arena->mLock);
      arena->Purge(true);
    }
  }
}

inline arena_t*
ArenaCollection::GetById(arena_id_t aArenaId, bool aIsPrivate)
{
  if (!malloc_initialized) {
    return nullptr;
  }
  // Use AlignedStorage2 to avoid running the arena_t constructor, while
  // we only need it as a placeholder for mId.
  mozilla::AlignedStorage2<arena_t> key;
  key.addr()->mId = aArenaId;
  MutexAutoLock lock(mLock);
  arena_t* result = (aIsPrivate ? mPrivateArenas : mArenas).Search(key.addr());
  MOZ_RELEASE_ASSERT(result);
  return result;
}

#ifdef NIGHTLY_BUILD
template<>
inline arena_id_t
MozJemalloc::moz_create_arena()
{
  if (malloc_init()) {
    arena_t* arena = gArenas.CreateArena(/* IsPrivate = */ true);
    return arena->mId;
  }
  return 0;
}

template<>
inline void
MozJemalloc::moz_dispose_arena(arena_id_t aArenaId)
{
  arena_t* arena = gArenas.GetById(aArenaId, /* IsPrivate = */ true);
  if (arena) {
    gArenas.DisposeArena(arena);
  }
}

#define MALLOC_DECL(name, return_type, ...)                                    \
  template<>                                                                   \
  inline return_type MozJemalloc::moz_arena_##name(                            \
    arena_id_t aArenaId, ARGS_HELPER(TYPED_ARGS, ##__VA_ARGS__))               \
  {                                                                            \
    BaseAllocator allocator(gArenas.GetById(aArenaId, /* IsPrivate = */ true));\
    return allocator.name(ARGS_HELPER(ARGS, ##__VA_ARGS__));                   \
  }
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC_BASE
#include "malloc_decls.h"

#else

#define MALLOC_DECL(name, return_type, ...)                                    \
  template<>                                                                   \
  inline return_type MozJemalloc::name(ARGS_HELPER(TYPED_ARGS, ##__VA_ARGS__)) \
  {                                                                            \
    return DummyArenaAllocator<MozJemalloc>::name(                             \
      ARGS_HELPER(ARGS, ##__VA_ARGS__));                                       \
  }
#define MALLOC_FUNCS MALLOC_FUNCS_ARENA
#include "malloc_decls.h"

#endif

// End non-standard functions.
// ***************************************************************************
// Begin library-private functions, used by threading libraries for protection
// of malloc during fork().  These functions are only called if the program is
// running in threaded mode, so there is no need to check whether the program
// is threaded here.
#ifndef XP_DARWIN
static
#endif
  void
  _malloc_prefork(void)
{
  // Acquire all mutexes in a safe order.
  gArenas.mLock.Lock();

  for (auto arena : gArenas.iter()) {
    arena->mLock.Lock();
  }

  base_mtx.Lock();

  huge_mtx.Lock();
}

#ifndef XP_DARWIN
static
#endif
  void
  _malloc_postfork_parent(void)
{
  // Release all mutexes, now that fork() has completed.
  huge_mtx.Unlock();

  base_mtx.Unlock();

  for (auto arena : gArenas.iter()) {
    arena->mLock.Unlock();
  }

  gArenas.mLock.Unlock();
}

#ifndef XP_DARWIN
static
#endif
  void
  _malloc_postfork_child(void)
{
  // Reinitialize all mutexes, now that fork() has completed.
  huge_mtx.Init();

  base_mtx.Init();

  for (auto arena : gArenas.iter()) {
    arena->mLock.Init();
  }

  gArenas.mLock.Init();
}

// End library-private functions.
// ***************************************************************************
#ifdef MOZ_REPLACE_MALLOC
// Windows doesn't come with weak imports as they are possible with
// LD_PRELOAD or DYLD_INSERT_LIBRARIES on Linux/OSX. On this platform,
// the replacement functions are defined as variable pointers to the
// function resolved with GetProcAddress() instead of weak definitions
// of functions. On Android, the same needs to happen as well, because
// the Android linker doesn't handle weak linking with non LD_PRELOADed
// libraries, but LD_PRELOADing is not very convenient on Android, with
// the zygote.
#ifdef XP_DARWIN
#define MOZ_REPLACE_WEAK __attribute__((weak_import))
#elif defined(XP_WIN) || defined(MOZ_WIDGET_ANDROID)
#define MOZ_NO_REPLACE_FUNC_DECL
#elif defined(__GNUC__)
#define MOZ_REPLACE_WEAK __attribute__((weak))
#endif

#include "replace_malloc.h"

#define MALLOC_DECL(name, return_type, ...) MozJemalloc::name,

static const malloc_table_t malloc_table = {
#include "malloc_decls.h"
};

static malloc_table_t replace_malloc_table;

#ifdef MOZ_NO_REPLACE_FUNC_DECL
#define MALLOC_DECL(name, return_type, ...)                                    \
  typedef return_type(name##_impl_t)(__VA_ARGS__);                             \
  name##_impl_t* replace_##name = nullptr;
#define MALLOC_FUNCS (MALLOC_FUNCS_INIT | MALLOC_FUNCS_BRIDGE)
#include "malloc_decls.h"
#endif

#ifdef XP_WIN
typedef HMODULE replace_malloc_handle_t;

static replace_malloc_handle_t
replace_malloc_handle()
{
  char replace_malloc_lib[1024];
  if (GetEnvironmentVariableA("MOZ_REPLACE_MALLOC_LIB",
                              replace_malloc_lib,
                              sizeof(replace_malloc_lib)) > 0) {
    return LoadLibraryA(replace_malloc_lib);
  }
  return nullptr;
}

#define REPLACE_MALLOC_GET_FUNC(handle, name)                                  \
  (name##_impl_t*)GetProcAddress(handle, "replace_" #name)

#elif defined(ANDROID)
#include <dlfcn.h>

typedef void* replace_malloc_handle_t;

static replace_malloc_handle_t
replace_malloc_handle()
{
  const char* replace_malloc_lib = getenv("MOZ_REPLACE_MALLOC_LIB");
  if (replace_malloc_lib && *replace_malloc_lib) {
    return dlopen(replace_malloc_lib, RTLD_LAZY);
  }
  return nullptr;
}

#define REPLACE_MALLOC_GET_FUNC(handle, name)                                  \
  (name##_impl_t*)dlsym(handle, "replace_" #name)

#else

typedef bool replace_malloc_handle_t;

static replace_malloc_handle_t
replace_malloc_handle()
{
  return true;
}

#define REPLACE_MALLOC_GET_FUNC(handle, name) replace_##name

#endif

static void
replace_malloc_init_funcs();

// Below is the malloc implementation overriding jemalloc and calling the
// replacement functions if they exist.
static int replace_malloc_initialized = 0;
static void
init()
{
  replace_malloc_init_funcs();
  // Set this *before* calling replace_init, otherwise if replace_init calls
  // malloc() we'll get an infinite loop.
  replace_malloc_initialized = 1;
  if (replace_init) {
    replace_init(&malloc_table);
  }
}

#define MALLOC_DECL(name, return_type, ...)                                    \
  template<>                                                                   \
  inline return_type ReplaceMalloc::name(                                      \
    ARGS_HELPER(TYPED_ARGS, ##__VA_ARGS__))                                    \
  {                                                                            \
    if (MOZ_UNLIKELY(!replace_malloc_initialized)) {                           \
      init();                                                                  \
    }                                                                          \
    return replace_malloc_table.name(ARGS_HELPER(ARGS, ##__VA_ARGS__));        \
  }
#include "malloc_decls.h"

MOZ_JEMALLOC_API struct ReplaceMallocBridge*
get_bridge(void)
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized)) {
    init();
  }
  if (MOZ_LIKELY(!replace_get_bridge)) {
    return nullptr;
  }
  return replace_get_bridge();
}

// posix_memalign, aligned_alloc, memalign and valloc all implement some kind
// of aligned memory allocation. For convenience, a replace-malloc library can
// skip defining replace_posix_memalign, replace_aligned_alloc and
// replace_valloc, and default implementations will be automatically derived
// from replace_memalign.
static void
replace_malloc_init_funcs()
{
  replace_malloc_handle_t handle = replace_malloc_handle();
  if (handle) {
#ifdef MOZ_NO_REPLACE_FUNC_DECL
#define MALLOC_DECL(name, ...)                                                 \
  replace_##name = REPLACE_MALLOC_GET_FUNC(handle, name);

#define MALLOC_FUNCS (MALLOC_FUNCS_INIT | MALLOC_FUNCS_BRIDGE)
#include "malloc_decls.h"
#endif

#define MALLOC_DECL(name, ...)                                                 \
  replace_malloc_table.name = REPLACE_MALLOC_GET_FUNC(handle, name);
#include "malloc_decls.h"
  }

  if (!replace_malloc_table.posix_memalign && replace_malloc_table.memalign) {
    replace_malloc_table.posix_memalign =
      AlignedAllocator<ReplaceMalloc::memalign>::posix_memalign;
  }
  if (!replace_malloc_table.aligned_alloc && replace_malloc_table.memalign) {
    replace_malloc_table.aligned_alloc =
      AlignedAllocator<ReplaceMalloc::memalign>::aligned_alloc;
  }
  if (!replace_malloc_table.valloc && replace_malloc_table.memalign) {
    replace_malloc_table.valloc =
      AlignedAllocator<ReplaceMalloc::memalign>::valloc;
  }
  if (!replace_malloc_table.moz_create_arena && replace_malloc_table.malloc) {
#define MALLOC_DECL(name, ...)                                                 \
  replace_malloc_table.name = DummyArenaAllocator<ReplaceMalloc>::name;
#define MALLOC_FUNCS MALLOC_FUNCS_ARENA
#include "malloc_decls.h"
  }

#define MALLOC_DECL(name, ...)                                                 \
  if (!replace_malloc_table.name) {                                            \
    replace_malloc_table.name = MozJemalloc::name;                             \
  }
#include "malloc_decls.h"
}

#endif // MOZ_REPLACE_MALLOC
  // ***************************************************************************
  // Definition of all the _impl functions

#define GENERIC_MALLOC_DECL2(name, name_impl, return_type, ...)                \
  return_type name_impl(ARGS_HELPER(TYPED_ARGS, ##__VA_ARGS__))                \
  {                                                                            \
    return DefaultMalloc::name(ARGS_HELPER(ARGS, ##__VA_ARGS__));              \
  }

#define GENERIC_MALLOC_DECL(name, return_type, ...)                            \
  GENERIC_MALLOC_DECL2(name, name##_impl, return_type, ##__VA_ARGS__)

#define MALLOC_DECL(...)                                                       \
  MOZ_MEMORY_API MACRO_CALL(GENERIC_MALLOC_DECL, (__VA_ARGS__))
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC
#include "malloc_decls.h"

#undef GENERIC_MALLOC_DECL
#define GENERIC_MALLOC_DECL(name, return_type, ...)                            \
  GENERIC_MALLOC_DECL2(name, name, return_type, ##__VA_ARGS__)

#define MALLOC_DECL(...)                                                       \
  MOZ_JEMALLOC_API MACRO_CALL(GENERIC_MALLOC_DECL, (__VA_ARGS__))
#define MALLOC_FUNCS (MALLOC_FUNCS_JEMALLOC | MALLOC_FUNCS_ARENA)
#include "malloc_decls.h"
  // ***************************************************************************

#ifdef HAVE_DLOPEN
#include <dlfcn.h>
#endif

#if defined(__GLIBC__) && !defined(__UCLIBC__)
// glibc provides the RTLD_DEEPBIND flag for dlopen which can make it possible
// to inconsistently reference libc's malloc(3)-compatible functions
// (bug 493541).
//
// These definitions interpose hooks in glibc.  The functions are actually
// passed an extra argument for the caller return address, which will be
// ignored.

extern "C" {
MOZ_EXPORT void (*__free_hook)(void*) = free_impl;
MOZ_EXPORT void* (*__malloc_hook)(size_t) = malloc_impl;
MOZ_EXPORT void* (*__realloc_hook)(void*, size_t) = realloc_impl;
MOZ_EXPORT void* (*__memalign_hook)(size_t, size_t) = memalign_impl;
}

#elif defined(RTLD_DEEPBIND)
// XXX On systems that support RTLD_GROUP or DF_1_GROUP, do their
// implementations permit similar inconsistencies?  Should STV_SINGLETON
// visibility be used for interposition where available?
#error "Interposing malloc is unsafe on this system without libc malloc hooks."
#endif

#ifdef XP_WIN
void*
_recalloc(void* aPtr, size_t aCount, size_t aSize)
{
  size_t oldsize = aPtr ? isalloc(aPtr) : 0;
  CheckedInt<size_t> checkedSize = CheckedInt<size_t>(aCount) * aSize;

  if (!checkedSize.isValid()) {
    return nullptr;
  }

  size_t newsize = checkedSize.value();

  // In order for all trailing bytes to be zeroed, the caller needs to
  // use calloc(), followed by recalloc().  However, the current calloc()
  // implementation only zeros the bytes requested, so if recalloc() is
  // to work 100% correctly, calloc() will need to change to zero
  // trailing bytes.
  aPtr = DefaultMalloc::realloc(aPtr, newsize);
  if (aPtr && oldsize < newsize) {
    memset((void*)((uintptr_t)aPtr + oldsize), 0, newsize - oldsize);
  }

  return aPtr;
}

// This impl of _expand doesn't ever actually expand or shrink blocks: it
// simply replies that you may continue using a shrunk block.
void*
_expand(void* aPtr, size_t newsize)
{
  if (isalloc(aPtr) >= newsize) {
    return aPtr;
  }

  return nullptr;
}

size_t
_msize(void* aPtr)
{
  return DefaultMalloc::malloc_usable_size(aPtr);
}

// In the new style jemalloc integration jemalloc is built as a separate
// shared library.  Since we're no longer hooking into the CRT binary,
// we need to initialize the heap at the first opportunity we get.
// DLL_PROCESS_ATTACH in DllMain is that opportunity.
BOOL APIENTRY
DllMain(HINSTANCE hModule, DWORD reason, LPVOID lpReserved)
{
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      // Don't force the system to page DllMain back in every time
      // we create/destroy a thread
      DisableThreadLibraryCalls(hModule);
      // Initialize the heap
      malloc_init_hard();
      break;

    case DLL_PROCESS_DETACH:
      break;
  }

  return TRUE;
}
#endif
