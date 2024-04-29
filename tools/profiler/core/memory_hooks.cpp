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
#include "mozilla/ThreadSafety.h"

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

// Returns true or or false depending on whether the marker was actually added
// or not.
static bool profiler_add_native_allocation_marker(int64_t aSize,
                                                  uintptr_t aMemoryAddress) {
  if (!profiler_thread_is_being_profiled_for_markers(
          profiler_main_thread_id())) {
    return false;
  }

  // Because native allocations may be intercepted anywhere, blocking while
  // locking the profiler mutex here could end up causing a deadlock if another
  // mutex is taken, which the profiler may indirectly need elsewhere.
  // See bug 1642726 for such a scenario.
  // So instead we bail out if the mutex is already locked. Native allocations
  // are statistically sampled anyway, so missing a few because of this is
  // acceptable.
  if (profiler_is_locked_on_current_thread()) {
    return false;
  }

  struct NativeAllocationMarker {
    static constexpr mozilla::Span<const char> MarkerTypeName() {
      return mozilla::MakeStringSpan("Native allocation");
    }
    static void StreamJSONMarkerData(
        mozilla::baseprofiler::SpliceableJSONWriter& aWriter, int64_t aSize,
        uintptr_t aMemoryAddress, ProfilerThreadId aThreadId) {
      aWriter.IntProperty("size", aSize);
      aWriter.IntProperty("memoryAddress",
                          static_cast<int64_t>(aMemoryAddress));
      // Tech note: If `ToNumber()` returns a uint64_t, the conversion to
      // int64_t is "implementation-defined" before C++20. This is acceptable
      // here, because this is a one-way conversion to a unique identifier
      // that's used to visually separate data by thread on the front-end.
      aWriter.IntProperty("threadId",
                          static_cast<int64_t>(aThreadId.ToNumber()));
    }
    static mozilla::MarkerSchema MarkerTypeDisplay() {
      return mozilla::MarkerSchema::SpecialFrontendLocation{};
    }
  };

  profiler_add_marker("Native allocation", geckoprofiler::category::OTHER,
                      {MarkerThreadId::MainThread(), MarkerStack::Capture()},
                      NativeAllocationMarker{}, aSize, aMemoryAddress,
                      profiler_current_thread_id());
  return true;
}

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
class MOZ_CAPABILITY("mutex") Mutex : private ::mozilla::detail::MutexImpl {
 public:
  Mutex() = default;

  void Lock() MOZ_CAPABILITY_ACQUIRE() { ::mozilla::detail::MutexImpl::lock(); }
  void Unlock() MOZ_CAPABILITY_RELEASE() {
    ::mozilla::detail::MutexImpl::unlock();
  }
};

class MOZ_SCOPED_CAPABILITY MutexAutoLock {
  MutexAutoLock(const MutexAutoLock&) = delete;
  void operator=(const MutexAutoLock&) = delete;

  Mutex& mMutex;

 public:
  explicit MutexAutoLock(Mutex& aMutex) MOZ_CAPABILITY_ACQUIRE(aMutex)
      : mMutex(aMutex) {
    mMutex.Lock();
  }
  ~MutexAutoLock() MOZ_CAPABILITY_RELEASE() { mMutex.Unlock(); }
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
  AllocationTracker() = default;

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
  Mutex mMutex MOZ_UNANNOTATED;
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

// This class is used to determine if allocations on this thread should be
// intercepted or not.
// Creating a ThreadIntercept object on the stack will implicitly block nested
// ones. There are other reasons to block: The feature is off, or we're inside a
// profiler function that is locking a mutex.
class MOZ_RAII ThreadIntercept {
  // When set to true, malloc does not intercept additional allocations. This is
  // needed because collecting stacks creates new allocations. When blocked,
  // these allocations are then ignored by the memory hook.
  static PROFILER_THREAD_LOCAL(bool) tlsIsBlocked;

  // This is a quick flag to check and see if the allocations feature is enabled
  // or disabled.
  static mozilla::Atomic<bool, mozilla::Relaxed> sAllocationsFeatureEnabled;

  // True if this ThreadIntercept has set tlsIsBlocked.
  bool mIsBlockingTLS;

  // True if interception is blocked for any reason.
  bool mIsBlocked;

 public:
  static void Init() {
    tlsIsBlocked.infallibleInit();
    // infallibleInit should zero-initialize, which corresponds to `false`.
    MOZ_ASSERT(!tlsIsBlocked.get());
  }

  ThreadIntercept() {
    // If the allocation interception feature is enabled, and the TLS is not
    // blocked yet, we will block the TLS now, and unblock on destruction.
    mIsBlockingTLS = sAllocationsFeatureEnabled && !tlsIsBlocked.get();
    if (mIsBlockingTLS) {
      MOZ_ASSERT(!tlsIsBlocked.get());
      tlsIsBlocked.set(true);
      // Since this is the top-level ThreadIntercept, interceptions are not
      // blocked unless the profiler itself holds a locked mutex, in which case
      // we don't want to intercept allocations that originate from such a
      // profiler call.
      mIsBlocked = profiler_is_locked_on_current_thread();
    } else {
      // The feature is off, or the TLS was already blocked, then we block this
      // interception.
      mIsBlocked = true;
    }
  }

  ~ThreadIntercept() {
    if (mIsBlockingTLS) {
      MOZ_ASSERT(tlsIsBlocked.get());
      tlsIsBlocked.set(false);
    }
  }

  // Is this ThreadIntercept effectively blocked? (Feature is off, or this
  // ThreadIntercept is nested, or we're inside a locked-Profiler function.)
  bool IsBlocked() const { return mIsBlocked; }

  static void EnableAllocationFeature() { sAllocationsFeatureEnabled = true; }

  static void DisableAllocationFeature() { sAllocationsFeatureEnabled = false; }
};

PROFILER_THREAD_LOCAL(bool) ThreadIntercept::tlsIsBlocked;

mozilla::Atomic<bool, mozilla::Relaxed>
    ThreadIntercept::sAllocationsFeatureEnabled(false);

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

  ThreadIntercept threadIntercept;
  if (threadIntercept.IsBlocked()) {
    // Either the native allocations feature is not turned on, or we may be
    // recursing into a memory hook, return. We'll still collect counter
    // information about this allocation, but no stack.
    return;
  }

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

  ThreadIntercept threadIntercept;
  if (threadIntercept.IsBlocked()) {
    // Either the native allocations feature is not turned on, or we may be
    // recursing into a memory hook, return. We'll still collect counter
    // information about this allocation, but no stack.
    return;
  }

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

static void replace_moz_set_max_dirty_page_modifier(int32_t aModifier) {
  return gMallocTable.moz_set_max_dirty_page_modifier(aModifier);
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
