/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "memory_hooks.h"

#include "nscore.h"

#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/FastBernoulliTrial.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/JSONWriter.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PlatformMutex.h"
#include "mozilla/ProfilerCounts.h"
#include "mozilla/ThreadLocal.h"

#include "GeckoProfiler.h"
#include "prenv.h"
#include "replace_malloc.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef XP_WIN
#  include <windows.h>
#  include <process.h>
#else
#  include <pthread.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

#ifdef ANDROID
#  include <android/log.h>
#endif

// The counters start out as a nullptr, and then get initialized only once. They
// are never destroyed, as it would cause race conditions for the memory hooks
// that use the counters. This helps guard against potentially expensive
// operations like using a mutex.
//
// In addition, this is a raw pointer and not a UniquePtr, as the counter
// machinery will try and de-register itself from the profiler. This could
// happen after the profiler and its PSMutex was already destroyed, resulting in
// a crash.
static ProfilerCounterTotal* sCounter;

// The gBernoulli value starts out as a nullptr, and only gets initialized once.
// It then lives for the entire lifetime of the process. It cannot be deleted
// without additional multi-threaded protections, since if we deleted it during
// profiler_stop then there could be a race between threads already in a
// memory hook that might try to access the value after or during deletion.
static mozilla::FastBernoulliTrial* gBernoulli;

namespace mozilla::profiler {

//---------------------------------------------------------------------------
// Utilities
//---------------------------------------------------------------------------

static malloc_table_t gMallocTable;

// This is only needed because of the |const void*| vs |void*| arg mismatch.
static size_t MallocSizeOf(const void* aPtr) {
  return gMallocTable.malloc_usable_size(const_cast<void*>(aPtr));
}

// The values for the Bernoulli trial are taken from DMD. According to DMD:
//
//   In testing, a probability of 0.003 resulted in ~25% of heap blocks getting
//   a stack trace and ~80% of heap bytes getting a stack trace. (This is
//   possible because big heap blocks are more likely to get a stack trace.)
//
//   The random number seeds are arbitrary and were obtained from random.org.
//
// However this value resulted in a lot of slowdown since the profiler stacks
// are pretty heavy to collect. The value was lowered to 10% of the original to
// 0.0003.
static void EnsureBernoulliIsInstalled() {
  if (!gBernoulli) {
    // This is only installed once. See the gBernoulli definition for more
    // information.
    gBernoulli =
        new FastBernoulliTrial(0.0003, 0x8e26eeee166bc8ca, 0x56820f304a9c9ae0);
  }
}

// This class provides infallible allocations (they abort on OOM) like
// mozalloc's InfallibleAllocPolicy, except that memory hooks are bypassed. This
// policy is used by the HashSet.
class InfallibleAllocWithoutHooksPolicy {
  static void ExitOnFailure(const void* aP) {
    if (!aP) {
      MOZ_CRASH("Profiler memory hooks out of memory; aborting");
    }
  }

 public:
  template <typename T>
  static T* maybe_pod_malloc(size_t aNumElems) {
    if (aNumElems & mozilla::tl::MulOverflowMask<sizeof(T)>::value) {
      return nullptr;
    }
    return (T*)gMallocTable.malloc(aNumElems * sizeof(T));
  }

  template <typename T>
  static T* maybe_pod_calloc(size_t aNumElems) {
    return (T*)gMallocTable.calloc(aNumElems, sizeof(T));
  }

  template <typename T>
  static T* maybe_pod_realloc(T* aPtr, size_t aOldSize, size_t aNewSize) {
    if (aNewSize & mozilla::tl::MulOverflowMask<sizeof(T)>::value) {
      return nullptr;
    }
    return (T*)gMallocTable.realloc(aPtr, aNewSize * sizeof(T));
  }

  template <typename T>
  static T* pod_malloc(size_t aNumElems) {
    T* p = maybe_pod_malloc<T>(aNumElems);
    ExitOnFailure(p);
    return p;
  }

  template <typename T>
  static T* pod_calloc(size_t aNumElems) {
    T* p = maybe_pod_calloc<T>(aNumElems);
    ExitOnFailure(p);
    return p;
  }

  template <typename T>
  static T* pod_realloc(T* aPtr, size_t aOldSize, size_t aNewSize) {
    T* p = maybe_pod_realloc(aPtr, aOldSize, aNewSize);
    ExitOnFailure(p);
    return p;
  }

  template <typename T>
  static void free_(T* aPtr, size_t aSize = 0) {
    gMallocTable.free(aPtr);
  }

  static void reportAllocOverflow() { ExitOnFailure(nullptr); }
  bool checkSimulatedOOM() const { return true; }
};

// We can't use mozilla::Mutex because it causes re-entry into the memory hooks.
// Define a custom implementation here.
class Mutex : private ::mozilla::detail::MutexImpl {
 public:
  Mutex() : ::mozilla::detail::MutexImpl() {}

  void Lock() { ::mozilla::detail::MutexImpl::lock(); }
  void Unlock() { ::mozilla::detail::MutexImpl::unlock(); }
};

class MutexAutoLock {
  MutexAutoLock(const MutexAutoLock&) = delete;
  void operator=(const MutexAutoLock&) = delete;

  Mutex& mMutex;

 public:
  explicit MutexAutoLock(Mutex& aMutex) : mMutex(aMutex) { mMutex.Lock(); }
  ~MutexAutoLock() { mMutex.Unlock(); }
};

//---------------------------------------------------------------------------
// Tracked allocations
//---------------------------------------------------------------------------

// The allocation tracker is shared between multiple threads, and is the
// coordinator for knowing when allocations have been tracked. The mutable
// internal state is protected by a mutex, and managed by the methods.
//
// The tracker knows about all the allocations that we have added to the
// profiler. This way, whenever any given piece of memory is freed, we can see
// if it was previously tracked, and we can track its deallocation.

class AllocationTracker {
  // This type tracks all of the allocations that we have captured. This way, we
  // can see if a deallocation is inside of this set. We want to provide a
  // balanced view into the allocations and deallocations.
  typedef mozilla::HashSet<const void*, mozilla::DefaultHasher<const void*>,
                           InfallibleAllocWithoutHooksPolicy>
      AllocationSet;

 public:
  AllocationTracker() : mAllocations(), mMutex() {}

  void AddMemoryAddress(const void* memoryAddress) {
    MutexAutoLock lock(mMutex);
    if (!mAllocations.put(memoryAddress)) {
      MOZ_CRASH("Out of memory while tracking native allocations.");
    };
  }

  void Reset() {
    MutexAutoLock lock(mMutex);
    mAllocations.clearAndCompact();
  }

  // Returns true when the memory address is found and removed, otherwise that
  // memory address is not being tracked and it returns false.
  bool RemoveMemoryAddressIfFound(const void* memoryAddress) {
    MutexAutoLock lock(mMutex);

    auto ptr = mAllocations.lookup(memoryAddress);
    if (ptr) {
      // The memory was present. It no longer needs to be tracked.
      mAllocations.remove(ptr);
      return true;
    }

    return false;
  }

 private:
  AllocationSet mAllocations;
  Mutex mMutex;
};

static AllocationTracker* gAllocationTracker;

static void EnsureAllocationTrackerIsInstalled() {
  if (!gAllocationTracker) {
    // This is only installed once.
    gAllocationTracker = new AllocationTracker();
  }
}

//---------------------------------------------------------------------------
// Per-thread blocking of intercepts
//---------------------------------------------------------------------------

// On MacOS, and Linux the first __thread/thread_local access calls malloc,
// which leads to an infinite loop. So we use pthread-based TLS instead, which
// somehow doesn't have this problem.
#if !defined(XP_DARWIN) && !defined(XP_LINUX)
#  define PROFILER_THREAD_LOCAL(T) MOZ_THREAD_LOCAL(T)
#else
#  define PROFILER_THREAD_LOCAL(T) \
    ::mozilla::detail::ThreadLocal<T, ::mozilla::detail::ThreadLocalKeyStorage>
#endif

class ThreadIntercept {
  // When set to true, malloc does not intercept additional allocations. This is
  // needed because collecting stacks creates new allocations. When blocked,
  // these allocations are then ignored by the memory hook.
  static PROFILER_THREAD_LOCAL(bool) tlsIsBlocked;

  // This is a quick flag to check and see if the allocations feature is enabled
  // or disabled.
  static mozilla::Atomic<bool, mozilla::Relaxed> sAllocationsFeatureEnabled;

  ThreadIntercept() = default;

  // Only allow consumers to access this information if they run
  // ThreadIntercept::MaybeGet and ask through the non-static version.
  static bool IsBlocked_() {
    // When the native allocations feature is turned on, memory hooks run on
    // every single allocation. For a subset of these allocations, the stack
    // gets sampled by running profiler_get_backtrace(), which locks the
    // profiler mutex.
    //
    // An issue arises when allocating while the profiler is locked FOR THE
    // CURRENT THREAD. In this situation, if profiler_get_backtrace() were to be
    // run, it would try to re-acquire this lock and deadlock. In order to guard
    // against this deadlock, we check to see if the mutex is already locked by
    // this thread.
    return tlsIsBlocked.get() || profiler_is_locked_on_current_thread();
  }

 public:
  static void Init() { tlsIsBlocked.infallibleInit(); }

  // The ThreadIntercept object can only be created through this method, the
  // constructor is private. This is so that we only check to see if the native
  // allocations are turned on once, and not multiple times through the calls.
  // The feature maybe be toggled between different stages of the memory hook.
  static Maybe<ThreadIntercept> MaybeGet() {
    if (sAllocationsFeatureEnabled && !ThreadIntercept::IsBlocked_()) {
      // Only return thread intercepts when the native allocations feature is
      // enabled and we aren't blocked.
      return Some(ThreadIntercept());
    }
    return Nothing();
  }

  void Block() {
    MOZ_ASSERT(!tlsIsBlocked.get());
    tlsIsBlocked.set(true);
  }

  void Unblock() {
    MOZ_ASSERT(tlsIsBlocked.get());
    tlsIsBlocked.set(false);
  }

  bool IsBlocked() const { return ThreadIntercept::IsBlocked_(); }

  static void EnableAllocationFeature() { sAllocationsFeatureEnabled = true; }

  static void DisableAllocationFeature() { sAllocationsFeatureEnabled = false; }
};

PROFILER_THREAD_LOCAL(bool) ThreadIntercept::tlsIsBlocked;

mozilla::Atomic<bool, mozilla::Relaxed>
    ThreadIntercept::sAllocationsFeatureEnabled(false);

// An object of this class must be created (on the stack) before running any
// code that might allocate.
class AutoBlockIntercepts {
  ThreadIntercept& mThreadIntercept;

 public:
  // Disallow copy and assign.
  AutoBlockIntercepts(const AutoBlockIntercepts&) = delete;
  void operator=(const AutoBlockIntercepts&) = delete;

  explicit AutoBlockIntercepts(ThreadIntercept& aThreadIntercept)
      : mThreadIntercept(aThreadIntercept) {
    mThreadIntercept.Block();
  }
  ~AutoBlockIntercepts() {
    MOZ_ASSERT(mThreadIntercept.IsBlocked());
    mThreadIntercept.Unblock();
  }
};

//---------------------------------------------------------------------------
// malloc/free callbacks
//---------------------------------------------------------------------------

static void AllocCallback(void* aPtr, size_t aReqSize) {
  if (!aPtr) {
    return;
  }

  // The first part of this function does not allocate.
  size_t actualSize = gMallocTable.malloc_usable_size(aPtr);
  if (actualSize > 0) {
    sCounter->Add(actualSize);
  }

  auto threadIntercept = ThreadIntercept::MaybeGet();
  if (threadIntercept.isNothing()) {
    // Either the native allocations feature is not turned on, or we  may be
    // recursing into a memory hook, return. We'll still collect counter
    // information about this allocation, but no stack.
    return;
  }

  // The next part of the function requires allocations, so block the memory
  // hooks from recursing on any new allocations coming in.
  AutoBlockIntercepts block(threadIntercept.ref());
  AUTO_PROFILER_LABEL("AllocCallback", PROFILER);

  // Perform a bernoulli trial, which will return true or false based on its
  // configured probability. It takes into account the byte size so that
  // larger allocations are weighted heavier than smaller allocations.
  MOZ_ASSERT(gBernoulli,
             "gBernoulli must be properly installed for the memory hooks.");
  if (
      // First perform the Bernoulli trial.
      gBernoulli->trial(actualSize) &&
      // Second, attempt to add a marker if the Bernoulli trial passed.
      profiler_add_native_allocation_marker(
          static_cast<int64_t>(actualSize),
          reinterpret_cast<uintptr_t>(aPtr))) {
    MOZ_ASSERT(gAllocationTracker,
               "gAllocationTracker must be properly installed for the memory "
               "hooks.");
    // Only track the memory if the allocation marker was actually added to the
    // profiler.
    gAllocationTracker->AddMemoryAddress(aPtr);
  }

  // We're ignoring aReqSize here
}

static void FreeCallback(void* aPtr) {
  if (!aPtr) {
    return;
  }

  // The first part of this function does not allocate.
  size_t unsignedSize = MallocSizeOf(aPtr);
  int64_t signedSize = -(static_cast<int64_t>(unsignedSize));
  sCounter->Add(signedSize);

  auto threadIntercept = ThreadIntercept::MaybeGet();
  if (threadIntercept.isNothing()) {
    // Either the native allocations feature is not turned on, or we  may be
    // recursing into a memory hook, return. We'll still collect counter
    // information about this allocation, but no stack.
    return;
  }

  // The next part of the function requires allocations, so block the memory
  // hooks from recursing on any new allocations coming in.
  AutoBlockIntercepts block(threadIntercept.ref());
  AUTO_PROFILER_LABEL("FreeCallback", PROFILER);

  // Perform a bernoulli trial, which will return true or false based on its
  // configured probability. It takes into account the byte size so that
  // larger allocations are weighted heavier than smaller allocations.
  MOZ_ASSERT(
      gAllocationTracker,
      "gAllocationTracker must be properly installed for the memory hooks.");
  if (gAllocationTracker->RemoveMemoryAddressIfFound(aPtr)) {
    // This size here is negative, indicating a deallocation.
    profiler_add_native_allocation_marker(signedSize,
                                          reinterpret_cast<uintptr_t>(aPtr));
  }
}

}  // namespace mozilla::profiler

//---------------------------------------------------------------------------
// malloc/free interception
//---------------------------------------------------------------------------

using namespace mozilla::profiler;

static void* replace_malloc(size_t aSize) {
  // This must be a call to malloc from outside.  Intercept it.
  void* ptr = gMallocTable.malloc(aSize);
  AllocCallback(ptr, aSize);
  return ptr;
}

static void* replace_calloc(size_t aCount, size_t aSize) {
  void* ptr = gMallocTable.calloc(aCount, aSize);
  AllocCallback(ptr, aCount * aSize);
  return ptr;
}

static void* replace_realloc(void* aOldPtr, size_t aSize) {
  // If |aOldPtr| is nullptr, the call is equivalent to |malloc(aSize)|.
  if (!aOldPtr) {
    return replace_malloc(aSize);
  }

  FreeCallback(aOldPtr);
  void* ptr = gMallocTable.realloc(aOldPtr, aSize);
  if (ptr) {
    AllocCallback(ptr, aSize);
  } else {
    // If realloc fails, we undo the prior operations by re-inserting the old
    // pointer into the live block table. We don't have to do anything with the
    // dead block list because the dead block hasn't yet been inserted. The
    // block will end up looking like it was allocated for the first time here,
    // which is untrue, and the slop bytes will be zero, which may be untrue.
    // But this case is rare and doing better isn't worth the effort.
    AllocCallback(aOldPtr, gMallocTable.malloc_usable_size(aOldPtr));
  }
  return ptr;
}

static void* replace_memalign(size_t aAlignment, size_t aSize) {
  void* ptr = gMallocTable.memalign(aAlignment, aSize);
  AllocCallback(ptr, aSize);
  return ptr;
}

static void replace_free(void* aPtr) {
  FreeCallback(aPtr);
  gMallocTable.free(aPtr);
}

static void* replace_moz_arena_malloc(arena_id_t aArena, size_t aSize) {
  void* ptr = gMallocTable.moz_arena_malloc(aArena, aSize);
  AllocCallback(ptr, aSize);
  return ptr;
}

static void* replace_moz_arena_calloc(arena_id_t aArena, size_t aCount,
                                      size_t aSize) {
  void* ptr = gMallocTable.moz_arena_calloc(aArena, aCount, aSize);
  AllocCallback(ptr, aCount * aSize);
  return ptr;
}

static void* replace_moz_arena_realloc(arena_id_t aArena, void* aPtr,
                                       size_t aSize) {
  void* ptr = gMallocTable.moz_arena_realloc(aArena, aPtr, aSize);
  AllocCallback(ptr, aSize);
  return ptr;
}

static void replace_moz_arena_free(arena_id_t aArena, void* aPtr) {
  FreeCallback(aPtr);
  gMallocTable.moz_arena_free(aArena, aPtr);
}

static void* replace_moz_arena_memalign(arena_id_t aArena, size_t aAlignment,
                                        size_t aSize) {
  void* ptr = gMallocTable.moz_arena_memalign(aArena, aAlignment, aSize);
  AllocCallback(ptr, aSize);
  return ptr;
}

// we have to replace these or jemalloc will assume we don't implement any
// of the arena replacements!
static arena_id_t replace_moz_create_arena_with_params(
    arena_params_t* aParams) {
  return gMallocTable.moz_create_arena_with_params(aParams);
}

static void replace_moz_dispose_arena(arena_id_t aArenaId) {
  return gMallocTable.moz_dispose_arena(aArenaId);
}

// Must come after all the replace_* funcs
void replace_init(malloc_table_t* aMallocTable, ReplaceMallocBridge** aBridge) {
  gMallocTable = *aMallocTable;
#define MALLOC_FUNCS (MALLOC_FUNCS_MALLOC_BASE | MALLOC_FUNCS_ARENA)
#define MALLOC_DECL(name, ...) aMallocTable->name = replace_##name;
#include "malloc_decls.h"
}

void profiler_replace_remove() {}

namespace mozilla::profiler {
//---------------------------------------------------------------------------
// Initialization
//---------------------------------------------------------------------------

BaseProfilerCount* install_memory_hooks() {
  if (!sCounter) {
    sCounter = new ProfilerCounterTotal("malloc", "Memory",
                                        "Amount of allocated memory");
    // Also initialize the ThreadIntercept, even if native allocation tracking
    // won't be turned on. This way the TLS will be initialized.
    ThreadIntercept::Init();
  } else {
    sCounter->Clear();
  }
  jemalloc_replace_dynamic(replace_init);
  return sCounter;
}

// Remove the hooks, but leave the sCounter machinery. Deleting the counter
// would race with any existing memory hooks that are currently running. Rather
// than adding overhead here of mutexes it's cheaper for the performance to just
// leak these values.
void remove_memory_hooks() { jemalloc_replace_dynamic(nullptr); }

void enable_native_allocations() {
  // The bloat log tracks allocations and deallocations. This can conflict
  // with the memory hook machinery, as the bloat log creates its own
  // allocations. This means we can re-enter inside the bloat log machinery. At
  // this time, the bloat log does not know about cannot handle the native
  // allocation feature.
  //
  // At the time of this writing, we hit this assertion:
  // IsIdle(oldState) || IsRead(oldState) in Checker::StartReadOp()
  //
  //    #01: GetBloatEntry(char const*, unsigned int)
  //    #02: NS_LogCtor
  //    #03: profiler_get_backtrace()
  //    #04: profiler_add_native_allocation_marker(long long)
  //    #05: mozilla::profiler::AllocCallback(void*, unsigned long)
  //    #06: replace_calloc(unsigned long, unsigned long)
  //    #07: PLDHashTable::ChangeTable(int)
  //    #08: PLDHashTable::Add(void const*, std::nothrow_t const&)
  //    #09: nsBaseHashtable<nsDepCharHashKey, nsAutoPtr<BloatEntry>, ...
  //    #10: GetBloatEntry(char const*, unsigned int)
  //    #11: NS_LogCtor
  //    #12: profiler_get_backtrace()
  //    ...
  MOZ_ASSERT(!PR_GetEnv("XPCOM_MEM_BLOAT_LOG"),
             "The bloat log feature is not compatible with the native "
             "allocations instrumentation.");

  EnsureBernoulliIsInstalled();
  EnsureAllocationTrackerIsInstalled();
  ThreadIntercept::EnableAllocationFeature();
}

// This is safe to call even if native allocations hasn't been enabled.
void disable_native_allocations() {
  ThreadIntercept::DisableAllocationFeature();
  if (gAllocationTracker) {
    gAllocationTracker->Reset();
  }
}

}  // namespace mozilla::profiler
