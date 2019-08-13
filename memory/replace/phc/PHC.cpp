/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// PHC is a probabilistic heap checker. A tiny fraction of randomly chosen heap
// allocations are subject to some expensive checking via the use of OS page
// access protection. A failed check triggers a crash, whereupon useful
// information about the failure is put into the crash report. The cost and
// coverage for each user is minimal, but spread over the entire user base the
// coverage becomes significant.
//
// The idea comes from Chromium, where it is called GWP-ASAN. (Firefox uses PHC
// as the name because GWP-ASAN is long, awkward, and doesn't have any
// particular meaning.)
//
// In the current implementation up to 64 allocations per process can become
// PHC allocations. These allocations must be page-sized or smaller. Each PHC
// allocation gets its own page, and when the allocation is freed its page is
// marked inaccessible until the page is reused for another allocation. This
// means that a use-after-free defect (which includes double-frees) will be
// caught if the use occurs before the page is reused for another allocation.
// The crash report will contain stack traces for the allocation site, the free
// site, and the use-after-free site, which is often enough to diagnose the
// defect.
//
// The design space for the randomization strategy is large. The current
// implementation has a large random delay before it starts operating, and a
// small random delay between each PHC allocation attempt. Each freed PHC
// allocation is quarantined for a medium random delay before being reused, in
// order to increase the chance of catching UAFs.
//
// The basic cost of PHC's operation is as follows.
//
// - The memory cost is 64 * 4 KiB = 256 KiB per process (assuming 4 KiB
//   pages) plus some metadata (including stack traces) for each page.
//
// - Every allocation requires a size check and a decrement-and-check of an
//   atomic counter. When the counter reaches zero a PHC allocation can occur,
//   which involves marking a page as accessible and getting a stack trace for
//   the allocation site. Otherwise, mozjemalloc performs the allocation.
//
// - Every deallocation requires a range check on the pointer to see if it
//   involves a PHC allocation. (The choice to only do PHC allocations that are
//   a page or smaller enables this range check, because the 64 pages are
//   contiguous. Allowing larger allocations would make this more complicated,
//   and we definitely don't want something as slow as a hash table lookup on
//   every deallocation.) PHC deallocations involve marking a page as
//   inaccessible and getting a stack trace for the deallocation site.
//
// In the future, we may add guard pages between the used pages in order
// to detect buffer overflows/underflows. This would change the memory cost to
// (64 * 2 + 1) * 4 KiB = 516 KiB per process and complicate the machinery
// somewhat.
//
// Note that calls to realloc(), free(), malloc_usable_size(), and
// IsPHCAllocation() will succeed if the given pointer falls anywhere within a
// page allocation's page, even if that is beyond the bounds of the page
// allocation's usable size. For example:
//
//   void* p = malloc(64);
//   free(p + 128);     // p+128 is within p's page --> same outcome as free(p)

#include "PHC.h"

#include <stdlib.h>
#include <time.h>

#include <algorithm>

#ifdef XP_WIN
#  include <process.h>
#else
#  include <sys/mman.h>
#  include <sys/types.h>
#  include <pthread.h>
#  include <unistd.h>
#endif

#include "replace_malloc.h"
#include "FdPrintf.h"
#include "Mutex.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Maybe.h"
#include "mozilla/StackWalk.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/XorShift128PlusRNG.h"

using namespace mozilla;

//---------------------------------------------------------------------------
// Utilities
//---------------------------------------------------------------------------

#ifdef ANDROID
// Android doesn't have pthread_atfork defined in pthread.h.
extern "C" MOZ_EXPORT int pthread_atfork(void (*)(void), void (*)(void),
                                         void (*)(void));
#endif

#ifndef DISALLOW_COPY_AND_ASSIGN
#  define DISALLOW_COPY_AND_ASSIGN(T) \
    T(const T&);                      \
    void operator=(const T&)
#endif

static malloc_table_t sMallocTable;

// This class provides infallible operations for the small number of heap
// allocations that PHC does for itself. It would be nice if we could use the
// InfallibleAllocPolicy from mozalloc, but PHC cannot use mozalloc.
class InfallibleAllocPolicy {
 public:
  static void AbortOnFailure(const void* aP) {
    if (!aP) {
      MOZ_CRASH("PHC failed to allocate");
    }
  }

  template <class T>
  static T* new_() {
    void* p = sMallocTable.malloc(sizeof(T));
    AbortOnFailure(p);
    return new (p) T;
  }
};

//---------------------------------------------------------------------------
// Stack traces
//---------------------------------------------------------------------------

// This code is similar to the equivalent code within DMD.

class StackTrace : public phc::StackTrace {
 public:
  StackTrace() : phc::StackTrace() {}

  void Clear() { mLength = 0; }

  void Fill();

 private:
  static void StackWalkCallback(uint32_t aFrameNumber, void* aPc, void* aSp,
                                void* aClosure) {
    StackTrace* st = (StackTrace*)aClosure;
    MOZ_ASSERT(st->mLength < kMaxFrames);
    st->mPcs[st->mLength] = aPc;
    st->mLength++;
    MOZ_ASSERT(st->mLength == aFrameNumber);
  }
};

// WARNING WARNING WARNING: this function must only be called when GMut::sMutex
// is *not* locked, otherwise we might get deadlocks.
//
// How? On Windows, MozStackWalk() can lock a mutex, M, from the shared library
// loader. Another thread might call malloc() while holding M locked (when
// loading a shared library) and try to lock GMut::sMutex, causing a deadlock.
// So GMut::sMutex can't be locked during the call to MozStackWalk(). (For
// details, see https://bugzilla.mozilla.org/show_bug.cgi?id=374829#c8. On
// Linux, something similar can happen; see bug 824340. So we just disallow it
// on all platforms.)
//
// In DMD, to avoid this problem we temporarily unlock the equivalent mutex for
// the MozStackWalk() call. But that's grotty, and things are a bit different
// here, so we just require that stack traces be obtained before locking
// GMut::sMutex.
//
// Unfortunately, there is no reliable way at compile-time or run-time to ensure
// this pre-condition. Hence this large comment.
//
void StackTrace::Fill() {
  mLength = 0;

#if defined(XP_WIN) && defined(_M_IX86)
  // This avoids MozStackWalk(), which causes unusably slow startup on Win32
  // when it is called during static initialization (see bug 1241684).
  //
  // This code is cribbed from the Gecko Profiler, which also uses
  // FramePointerStackWalk() on Win32: Registers::SyncPopulate() for the
  // frame pointer, and GetStackTop() for the stack end.
  CONTEXT context;
  RtlCaptureContext(&context);
  void** fp = reinterpret_cast<void**>(context.Ebp);

  PNT_TIB pTib = reinterpret_cast<PNT_TIB>(NtCurrentTeb());
  void* stackEnd = static_cast<void*>(pTib->StackBase);
  FramePointerStackWalk(StackWalkCallback, /* aSkipFrames = */ 0, kMaxFrames,
                        this, fp, stackEnd);
#elif defined(XP_MACOSX)
  // This avoids MozStackWalk(), which has become unusably slow on Mac due to
  // changes in libunwind.
  //
  // This code is cribbed from the Gecko Profiler, which also uses
  // FramePointerStackWalk() on Mac: Registers::SyncPopulate() for the frame
  // pointer, and GetStackTop() for the stack end.
  void** fp;
  asm(
      // Dereference %rbp to get previous %rbp
      "movq (%%rbp), %0\n\t"
      : "=r"(fp));
  void* stackEnd = pthread_get_stackaddr_np(pthread_self());
  FramePointerStackWalk(StackWalkCallback, /* skipFrames = */ 0, kMaxFrames,
                        this, fp, stackEnd);
#else
  MozStackWalk(StackWalkCallback, /* aSkipFrames = */ 0, kMaxFrames, this);
#endif
}

//---------------------------------------------------------------------------
// Logging
//---------------------------------------------------------------------------

// Change this to 1 to enable some PHC logging. Useful for debugging.
#define PHC_LOGGING 0

#if PHC_LOGGING

static size_t GetPid() { return size_t(getpid()); }

static size_t GetTid() {
#  if defined(XP_WIN)
  return size_t(GetCurrentThreadId());
#  else
  return size_t(pthread_self());
#  endif
}

#  if defined(XP_WIN)
#    define LOG_STDERR \
      reinterpret_cast<intptr_t>(GetStdHandle(STD_ERROR_HANDLE))
#  else
#    define LOG_STDERR 2
#  endif
#  define LOG(fmt, ...)                                                \
    FdPrintf(LOG_STDERR, "PHC[%zu,%zu,~%zu] " fmt, GetPid(), GetTid(), \
             size_t(GAtomic::Now()), __VA_ARGS__)

#else

#  define LOG(fmt, ...)

#endif  // PHC_LOGGING

//---------------------------------------------------------------------------
// Global state
//---------------------------------------------------------------------------

// Throughout this entire file time is measured as the number of sub-page
// allocations performed (by PHC and mozjemalloc combined). `Time` is 64-bit
// because we could have more than 2**32 allocations in a long-running session.
// `Delay` is 32-bit because the delays used within PHC are always much smaller
// than 2**32.
using Time = uint64_t;   // A moment in time.
using Delay = uint32_t;  // A time duration.

// PHC only runs if the page size is 4 KiB; anything more is uncommon and would
// use too much memory. So we hardwire this size.
static const size_t kPageSize = 4096;

// The maximum number of live page allocations.
static const size_t kMaxPageAllocs = 64;

// The total size of the pages.
static const size_t kAllPagesSize = kPageSize * kMaxPageAllocs;

// The junk value used to fill new allocation in debug builds. It's same value
// as the one used by mozjemalloc. PHC applies it unconditionally in debug
// builds. Unlike mozjemalloc, PHC doesn't consult the MALLOC_OPTIONS
// environment variable to possibly change that behaviour.
//
// Also note that, unlike mozjemalloc, PHC doesn't have a poison value for freed
// allocations because freed allocations are protected by OS page protection.
const uint8_t kAllocJunk = 0xe4;

// The maximum time.
static const Time kMaxTime = ~(Time(0));

// The average delay before doing any page allocations at the start of a
// process. Note that roughly 1 million allocations occur in the main process
// while starting the browser.
static const Delay kAvgFirstAllocDelay = 512 * 1024;

// The average delay until the next attempted page allocation, once we get past
// the first delay.
static const Delay kAvgAllocDelay = 2 * 1024;

// The average delay before reusing a freed page. Should be significantly larger
// than kAvgAllocDelay, otherwise there's not much point in having it.
static const Delay kAvgPageReuseDelay = 32 * 1024;

// Truncate aRnd to the range (1 .. AvgDelay*2). If aRnd is random, this
// results in an average value of aAvgDelay + 0.5, which is close enough to
// aAvgDelay. aAvgDelay must be a power-of-two (otherwise it will crash) for
// speed.
template <Delay AvgDelay>
constexpr Delay Rnd64ToDelay(uint64_t aRnd) {
  static_assert(IsPowerOfTwo(AvgDelay), "must be a power of two");

  return aRnd % (AvgDelay * 2) + 1;
}

// Shared, atomic, mutable global state.
class GAtomic {
 public:
  static void Init(Delay aFirstDelay) {
    sAllocDelay = aFirstDelay;

    LOG("Initial sAllocDelay <- %zu\n", size_t(aFirstDelay));
  }

  static Time Now() { return sNow; }

  static void IncrementNow() { sNow++; }

  // Decrements the delay and returns the decremented value.
  static int32_t DecrementDelay() { return --sAllocDelay; }

  static void SetAllocDelay(Delay aAllocDelay) { sAllocDelay = aAllocDelay; }

 private:
  // The current time. Relaxed semantics because it's primarily used for
  // determining if an allocation can be recycled yet and therefore it doesn't
  // need to be exact.
  static Atomic<Time, Relaxed> sNow;

  // Delay until the next attempt at a page allocation. See the comment in
  // MaybePageAlloc() for an explanation of why it is a signed integer, and why
  // it uses ReleaseAcquire semantics.
  static Atomic<Delay, ReleaseAcquire> sAllocDelay;
};

Atomic<Time, Relaxed> GAtomic::sNow;
Atomic<Delay, ReleaseAcquire> GAtomic::sAllocDelay;

// Shared, immutable global state. Initialized by replace_init() and never
// changed after that. replace_init() runs early enough that no synchronization
// is needed.
class GConst {
 private:
  // The bounds of the allocated pages.
  const uintptr_t mPagesStart;
  const uintptr_t mPagesLimit;

  uintptr_t AllocPages() {
    // Allocate the pages so that they are inaccessible. They are never freed,
    // because it would happen at process termination when it would be of little
    // use.
    void* pages =
#ifdef XP_WIN
        VirtualAlloc(nullptr, kAllPagesSize, MEM_RESERVE, PAGE_NOACCESS);
#else
        mmap(nullptr, kAllPagesSize, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1,
             0);
#endif
    if (!pages) {
      MOZ_CRASH();
    }

    return reinterpret_cast<uintptr_t>(pages);
  }

 public:
  GConst()
      : mPagesStart(AllocPages()), mPagesLimit(mPagesStart + kAllPagesSize) {
    LOG("AllocPages at %p..%p\n", (void*)mPagesStart, (void*)mPagesLimit);
  }

  // Detect if a pointer is to a page allocation, and if so, which one. This
  // function must be fast because it is called for every call to free(),
  // realloc(), malloc_usable_size(), and jemalloc_ptr_info().
  Maybe<uintptr_t> PageIndex(const void* aPtr) {
    auto ptr = reinterpret_cast<uintptr_t>(aPtr);
    if (!(mPagesStart <= ptr && ptr < mPagesLimit)) {
      return Nothing();
    }

    size_t i = (ptr - mPagesStart) / kPageSize;
    MOZ_ASSERT(i < kMaxPageAllocs);
    return Some(i);
  }

  // Get the address of a page referred to via an index.
  void* PagePtr(size_t aIndex) {
    MOZ_ASSERT(aIndex < kMaxPageAllocs);
    return reinterpret_cast<void*>(mPagesStart + kPageSize * aIndex);
  }
};

static GConst* gConst;

// On MacOS, the first __thread/thread_local access calls malloc, which leads
// to an infinite loop. So we use pthread-based TLS instead, which somehow
// doesn't have this problem.
#if !defined(XP_DARWIN)
#  define PHC_THREAD_LOCAL(T) MOZ_THREAD_LOCAL(T)
#else
#  define PHC_THREAD_LOCAL(T) \
    detail::ThreadLocal<T, detail::ThreadLocalKeyStorage>
#endif

// Thread-local state.
class GTls {
  DISALLOW_COPY_AND_ASSIGN(GTls);

  // When true, PHC does as little as possible.
  //
  // (a) It does not allocate any new page allocations.
  //
  // (b) It avoids doing any operations that might call malloc/free/etc., which
  //     would cause re-entry into PHC. (In practice, MozStackWalk() is the
  //     only such operation.) Note that calls to the functions in sMallocTable
  //     are ok.
  //
  // For example, replace_malloc() will just fall back to mozjemalloc. However,
  // operations involving existing allocations are more complex, because those
  // existing allocations may be page allocations. For example, if
  // replace_free() is passed a page allocation on a PHC-disabled thread, it
  // will free the page allocation in the usual way, but it will get a dummy
  // freeStack in order to avoid calling MozStackWalk(), as per (b) above.
  //
  // This single disabling mechanism has two distinct uses.
  //
  // - It's used to prevent re-entry into PHC, which can cause correctness
  //   problems. For example, consider this sequence.
  //
  //   1. enter replace_free()
  //   2. which calls PageFree()
  //   3. which calls MozStackWalk()
  //   4. which locks a mutex M, and then calls malloc
  //   5. enter replace_malloc()
  //   6. which calls MaybePageAlloc()
  //   7. which calls MozStackWalk()
  //   8. which (re)locks a mutex M --> deadlock
  //
  //   We avoid this sequence by "disabling" the thread in PageFree() (at step
  //   2), which causes MaybePageAlloc() to fail, avoiding the call to
  //   MozStackWalk() (at step 7).
  //
  //   In practice, realloc or free of a PHC allocation is unlikely on a thread
  //   that is disabled because of this use: MozStackWalk() will probably only
  //   realloc/free allocations that it allocated itself, but those won't be
  //   page allocations because PHC is disabled before calling MozStackWalk().
  //
  //   (Note that MaybePageAlloc() could safely do a page allocation so long as
  //   it avoided calling MozStackWalk() by getting a dummy allocStack. But it
  //   wouldn't be useful, and it would prevent the second use below.)
  //
  // - It's used to prevent PHC allocations in some tests that rely on
  //   mozjemalloc's exact allocation behaviour, which PHC does not replicate
  //   exactly. (Note that (b) isn't necessary for this use -- MozStackWalk()
  //   could be safely called -- but it is necessary for the first use above.)
  //
  static PHC_THREAD_LOCAL(bool) tlsIsDisabled;

 public:
  static void Init() {
    if (!tlsIsDisabled.init()) {
      MOZ_CRASH();
    }
  }

  static void DisableOnCurrentThread() {
    MOZ_ASSERT(!GTls::tlsIsDisabled.get());
    tlsIsDisabled.set(true);
  }

  static void EnableOnCurrentThread() {
    MOZ_ASSERT(GTls::tlsIsDisabled.get());
    tlsIsDisabled.set(false);
  }

  static bool IsDisabledOnCurrentThread() { return tlsIsDisabled.get(); }
};

PHC_THREAD_LOCAL(bool) GTls::tlsIsDisabled;

class AutoDisableOnCurrentThread {
  DISALLOW_COPY_AND_ASSIGN(AutoDisableOnCurrentThread);

 public:
  explicit AutoDisableOnCurrentThread() { GTls::DisableOnCurrentThread(); }
  ~AutoDisableOnCurrentThread() { GTls::EnableOnCurrentThread(); }
};

// This type is used as a proof-of-lock token, to make it clear which functions
// require sMutex to be locked.
using GMutLock = const MutexAutoLock&;

// Shared, mutable global state. Protected by sMutex; all accessing functions
// take a GMutLock as proof that sMutex is held.
class GMut {
  enum class PageState {
    NeverAllocated = 0,
    InUse = 1,
    Freed = 2,
  };

  // Metadata for each page.
  class PageInfo {
   public:
    PageInfo()
        : mState(PageState::NeverAllocated),
          mArenaId(),
          mUsableSize(0),
          mAllocStack(),
          mFreeStack(),
          mReuseTime(0) {}

    // The current page state.
    PageState mState;

    // The arena that the allocation is nominally from. This isn't meaningful
    // within PHC, which has no arenas. But it is necessary for reallocation of
    // page allocations as normal allocations, such as in this code:
    //
    //   p = moz_arena_malloc(arenaId, 4096);
    //   realloc(p, 8192);
    //
    // The realloc is more than one page, and thus too large for PHC to handle.
    // Therefore, if PHC handles the first allocation, it must ask mozjemalloc
    // to allocate the 8192 bytes in the correct arena, and to do that, it must
    // call sMallocTable.moz_arena_malloc with the correct arenaId under the
    // covers. Therefore it must record that arenaId.
    //
    // This field is also needed for jemalloc_ptr_info() to work, because it
    // also returns the arena ID (but only in debug builds).
    //
    // - NeverAllocated: must be 0.
    // - InUse: can be any valid arena ID value.
    // - Freed: can be any valid arena ID value.
    Maybe<arena_id_t> mArenaId;

    // The usable size, which could be bigger than the requested size.
    // - NeverAllocated: must be 0.
    // - InUse: must be > 0.
    // - Freed: must be > 0.
    size_t mUsableSize;

    // The allocation stack.
    // - NeverAllocated: Nothing.
    // - InUse: Some.
    // - Freed: Some.
    Maybe<StackTrace> mAllocStack;

    // The free stack.
    // - NeverAllocated: Nothing.
    // - InUse: Some.
    // - Freed: Some.
    Maybe<StackTrace> mFreeStack;

    // The time at which the page is available for reuse, as measured against
    // GAtomic::sNow. When the page is in use this value will be kMaxTime.
    // - NeverAllocated: must be 0.
    // - InUse: must be kMaxTime.
    // - Freed: must be > 0 and < kMaxTime.
    Time mReuseTime;
  };

 public:
  // The mutex that protects the other members.
  static Mutex sMutex;

  GMut() : mRNG(RandomSeed<0>(), RandomSeed<1>()), mPages() { sMutex.Init(); }

  uint64_t Random64(GMutLock) { return mRNG.next(); }

  bool IsPageInUse(GMutLock, uintptr_t aIndex) {
    return mPages[aIndex].mState == PageState::InUse;
  }

  // Is the page free? And if so, has enough time passed that we can use it?
  bool IsPageAllocatable(GMutLock, uintptr_t aIndex, Time aNow) {
    const PageInfo& page = mPages[aIndex];
    return page.mState != PageState::InUse && aNow >= page.mReuseTime;
  }

  Maybe<arena_id_t> PageArena(GMutLock aLock, uintptr_t aIndex) {
    const PageInfo& page = mPages[aIndex];
    AssertPageInUse(aLock, page);

    return page.mArenaId;
  }

  size_t PageUsableSize(GMutLock aLock, uintptr_t aIndex) {
    const PageInfo& page = mPages[aIndex];
    AssertPageInUse(aLock, page);

    return page.mUsableSize;
  }

  void SetPageInUse(GMutLock aLock, uintptr_t aIndex,
                    const Maybe<arena_id_t>& aArenaId, size_t aUsableSize,
                    const StackTrace& aAllocStack) {
    MOZ_ASSERT(aUsableSize == sMallocTable.malloc_good_size(aUsableSize));

    PageInfo& page = mPages[aIndex];
    AssertPageNotInUse(aLock, page);

    page.mState = PageState::InUse;
    page.mArenaId = aArenaId;
    page.mUsableSize = aUsableSize;
    page.mAllocStack = Some(aAllocStack);
    page.mFreeStack = Nothing();
    page.mReuseTime = kMaxTime;
  }

  void ResizePageInUse(GMutLock aLock, uintptr_t aIndex,
                       const Maybe<arena_id_t>& aArenaId, size_t aNewUsableSize,
                       const StackTrace& aAllocStack) {
    MOZ_ASSERT(aNewUsableSize == sMallocTable.malloc_good_size(aNewUsableSize));

    PageInfo& page = mPages[aIndex];
    AssertPageInUse(aLock, page);

    // page.mState is not changed.
    if (aArenaId.isSome()) {
      // Crash if the arenas don't match.
      MOZ_RELEASE_ASSERT(page.mArenaId == aArenaId);
    }
    page.mUsableSize = aNewUsableSize;
    // We could just keep the original alloc stack, but the realloc stack is
    // more recent and therefore seems more useful.
    page.mAllocStack = Some(aAllocStack);
    // page.mFreeStack is not changed.
    // page.mReuseTime is not changed.
  };

  void SetPageFreed(GMutLock aLock, uintptr_t aIndex,
                    const Maybe<arena_id_t>& aArenaId,
                    const StackTrace& aFreeStack, Delay aReuseDelay) {
    PageInfo& page = mPages[aIndex];
    AssertPageInUse(aLock, page);

    page.mState = PageState::Freed;

    // page.mArenaId is left unchanged, for jemalloc_ptr_info() calls that
    // occur after freeing (e.g. in the PtrInfo test in TestJemalloc.cpp).
    if (aArenaId.isSome()) {
      // Crash if the arenas don't match.
      MOZ_RELEASE_ASSERT(page.mArenaId == aArenaId);
    }

    // page.musableSize is left unchanged, for reporting on UAF, and for
    // jemalloc_ptr_info() calls that occur after freeing (e.g. in the PtrInfo
    // test in TestJemalloc.cpp).

    // page.mAllocStack is left unchanged, for reporting on UAF.

    page.mFreeStack = Some(aFreeStack);
    page.mReuseTime = GAtomic::Now() + aReuseDelay;
  }

  void EnsureInUse(GMutLock, void* aPtr, uintptr_t aIndex) {
    const PageInfo& page = mPages[aIndex];
    MOZ_RELEASE_ASSERT(page.mState != PageState::NeverAllocated);
    if (page.mState == PageState::Freed) {
      LOG("EnsureInUse(%p), failure\n", aPtr);
      // An operation on a freed page? This is a particular kind of
      // use-after-free. Deliberately touch the page in question, in order to
      // cause a crash that triggers the usual PHC machinery. But unlock sMutex
      // first, because that self-same PHC machinery needs to re-lock it, and
      // the crash causes non-local control flow so sMutex won't be unlocked
      // the normal way in the caller.
      sMutex.Unlock();
      *static_cast<char*>(aPtr) = 0;
      MOZ_CRASH("unreachable");
    }
  }

  void FillAddrInfo(GMutLock, uintptr_t aIndex, const void* aBaseAddr,
                    phc::AddrInfo& aOut) {
    const PageInfo& page = mPages[aIndex];
    switch (page.mState) {
      case PageState::NeverAllocated:
        aOut.mKind = phc::AddrInfo::Kind::NeverAllocatedPage;
        break;

      case PageState::InUse:
        aOut.mKind = phc::AddrInfo::Kind::InUsePage;
        break;

      case PageState::Freed:
        aOut.mKind = phc::AddrInfo::Kind::FreedPage;
        break;

      default:
        MOZ_CRASH();
    }
    aOut.mBaseAddr = const_cast<const void*>(gConst->PagePtr(aIndex));
    aOut.mUsableSize = page.mUsableSize;
    aOut.mAllocStack = page.mAllocStack;
    aOut.mFreeStack = page.mFreeStack;
  }

  void FillJemallocPtrInfo(GMutLock, const void* aPtr, uintptr_t aIndex,
                           jemalloc_ptr_info_t* aInfo) {
    const PageInfo& page = mPages[aIndex];
    switch (page.mState) {
      case PageState::NeverAllocated:
        break;

      case PageState::InUse: {
        // Only return TagLiveAlloc if the pointer is within the bounds of the
        // allocation's usable size.
        char* pagePtr = static_cast<char*>(gConst->PagePtr(aIndex));
        if (aPtr < pagePtr + page.mUsableSize) {
          *aInfo = {TagLiveAlloc, pagePtr, page.mUsableSize,
                    page.mArenaId.valueOr(0)};
          return;
        }
        break;
      }

      case PageState::Freed: {
        // Only return TagFreedAlloc if the pointer is within the bounds of the
        // former allocation's usable size.
        char* pagePtr = static_cast<char*>(gConst->PagePtr(aIndex));
        if (aPtr < pagePtr + page.mUsableSize) {
          *aInfo = {TagFreedAlloc, gConst->PagePtr(aIndex), page.mUsableSize,
                    page.mArenaId.valueOr(0)};
          return;
        }
        break;
      }

      default:
        MOZ_CRASH();
    }

    *aInfo = {TagUnknown, nullptr, 0, 0};
  }

  static void prefork() { sMutex.Lock(); }
  static void postfork() { sMutex.Unlock(); }

 private:
  template <int N>
  uint64_t RandomSeed() {
    // An older version of this code used RandomUint64() here, but on Mac that
    // function uses arc4random(), which can allocate, which would cause
    // re-entry, which would be bad. So we just use time() and a local variable
    // address. These are mediocre sources of entropy, but good enough for PHC.
    static_assert(N == 0 || N == 1, "must be 0 or 1");
    uint64_t seed;
    if (N == 0) {
      time_t t = time(nullptr);
      seed = t ^ (t << 32);
    } else {
      seed = uintptr_t(&seed) ^ (uintptr_t(&seed) << 32);
    }
    return seed;
  }

  void AssertPageInUse(GMutLock, const PageInfo& aPage) {
    MOZ_ASSERT(aPage.mState == PageState::InUse);
    // There is nothing to assert about aPage.mArenaId.
    MOZ_ASSERT(aPage.mUsableSize > 0);
    MOZ_ASSERT(aPage.mAllocStack.isSome());
    MOZ_ASSERT(aPage.mFreeStack.isNothing());
    MOZ_ASSERT(aPage.mReuseTime == kMaxTime);
  }

  void AssertPageNotInUse(GMutLock, const PageInfo& aPage) {
    // We can assert a lot about `NeverAllocated` pages, but not much about
    // `Freed` pages.
#ifdef DEBUG
    bool isFresh = aPage.mState == PageState::NeverAllocated;
    MOZ_ASSERT(isFresh || aPage.mState == PageState::Freed);
    MOZ_ASSERT_IF(isFresh, aPage.mArenaId == Nothing());
    MOZ_ASSERT(isFresh == (aPage.mUsableSize == 0));
    MOZ_ASSERT(isFresh == (aPage.mAllocStack.isNothing()));
    MOZ_ASSERT(isFresh == (aPage.mFreeStack.isNothing()));
    MOZ_ASSERT(aPage.mReuseTime != kMaxTime);
#endif
  }

  // RNG for deciding which allocations to treat specially. It doesn't need to
  // be high quality.
  //
  // This is a raw pointer for the reason explained in the comment above
  // GMut's constructor. Don't change it to UniquePtr or anything like that.
  non_crypto::XorShift128PlusRNG mRNG;

  PageInfo mPages[kMaxPageAllocs];
};

Mutex GMut::sMutex;

static GMut* gMut;

//---------------------------------------------------------------------------
// Page allocation operations
//---------------------------------------------------------------------------

// Attempt a page allocation if the time and the size are right. Allocated
// memory is zeroed if aZero is true. On failure, the caller should attempt a
// normal allocation via sMallocTable. Can be called in a context where
// GMut::sMutex is locked.
static void* MaybePageAlloc(const Maybe<arena_id_t>& aArenaId, size_t aReqSize,
                            bool aZero) {
  if (aReqSize > kPageSize) {
    return nullptr;
  }

  GAtomic::IncrementNow();

  // Decrement the delay. If it's zero, we do a page allocation and reset the
  // delay to a random number. Because the assignment to the random number isn't
  // atomic w.r.t. the decrement, we might have a sequence like this:
  //
  //     Thread 1                      Thread 2           Thread 3
  //     --------                      --------           --------
  // (a) newDelay = --sAllocDelay (-> 0)
  // (b)                               --sAllocDelay (-> -1)
  // (c) (newDelay != 0) fails
  // (d)                                                  --sAllocDelay (-> -2)
  // (e) sAllocDelay = new_random_number()
  //
  // It's critical that sAllocDelay has ReleaseAcquire semantics, because that
  // guarantees that exactly one thread will see sAllocDelay have the value 0.
  // (Relaxed semantics wouldn't guarantee that.)
  //
  // It's also nice that sAllocDelay is signed, given that we can decrement to
  // below zero. (Strictly speaking, an unsigned integer would also work due
  // to wrapping, but a signed integer is conceptually cleaner.)
  //
  // Finally, note that the decrements that occur between (a) and (e) above are
  // effectively ignored, because (e) clobbers them. This shouldn't be a
  // problem; it effectively just adds a little more randomness to
  // new_random_number(). An early version of this code tried to account for
  // these decrements by doing `sAllocDelay += new_random_number()`. However, if
  // new_random_value() is small, the number of decrements between (a) and (e)
  // can easily exceed it, whereupon sAllocDelay ends up negative after
  // `sAllocDelay += new_random_number()`, and the zero-check never succeeds
  // again. (At least, not until sAllocDelay wraps around on overflow, which
  // would take a very long time indeed.)
  //
  int32_t newDelay = GAtomic::DecrementDelay();
  if (newDelay != 0) {
    return nullptr;
  }

  if (GTls::IsDisabledOnCurrentThread()) {
    return nullptr;
  }

  // Disable on this thread *before* getting the stack trace.
  AutoDisableOnCurrentThread disable;

  // Get the stack trace *before* locking the mutex. If we return nullptr then
  // it was a waste, but it's not so frequent, and doing a stack walk while
  // the mutex is locked is problematic (see the big comment on
  // StackTrace::Fill() for details).
  StackTrace allocStack;
  allocStack.Fill();

  MutexAutoLock lock(GMut::sMutex);

  Time now = GAtomic::Now();
  Delay newAllocDelay = Rnd64ToDelay<kAvgAllocDelay>(gMut->Random64(lock));

  // We start at a random page alloc and wrap around, to ensure pages get even
  // amounts of use.
  void* ptr = nullptr;
  for (uintptr_t n = 0, i = size_t(gMut->Random64(lock)) % kMaxPageAllocs;
       n < kMaxPageAllocs; n++, i = (i + 1) % kMaxPageAllocs) {
    if (gMut->IsPageAllocatable(lock, i, now)) {
      void* pagePtr = gConst->PagePtr(i);
      bool ok =
#ifdef XP_WIN
          !!VirtualAlloc(pagePtr, kPageSize, MEM_COMMIT, PAGE_READWRITE);
#else
          mprotect(pagePtr, kPageSize, PROT_READ | PROT_WRITE) == 0;
#endif
      size_t usableSize = sMallocTable.malloc_good_size(aReqSize);
      if (ok) {
        gMut->SetPageInUse(lock, i, aArenaId, usableSize, allocStack);
        ptr = pagePtr;
        if (aZero) {
          memset(ptr, 0, usableSize);
        } else {
#ifdef DEBUG
          memset(ptr, kAllocJunk, usableSize);
#endif
        }
      }
      LOG("PageAlloc(%zu) -> %p[%zu] (%zu) (z%zu), sAllocDelay <- %zu\n",
          aReqSize, ptr, i, usableSize, size_t(aZero), size_t(newAllocDelay));
      break;
    }
  }

  if (!ptr) {
    // No pages are available, or VirtualAlloc/mprotect failed.
    LOG("No PageAlloc(%zu), sAllocDelay <- %zu\n", aReqSize,
        size_t(newAllocDelay));
  }

  // Set the new alloc delay.
  GAtomic::SetAllocDelay(newAllocDelay);

  return ptr;
}

static void FreePage(GMutLock aLock, size_t aIndex,
                     const Maybe<arena_id_t>& aArenaId,
                     const StackTrace& aFreeStack, Delay aReuseDelay) {
  void* pagePtr = gConst->PagePtr(aIndex);
#ifdef XP_WIN
  if (!VirtualFree(pagePtr, kPageSize, MEM_DECOMMIT)) {
    return;
  }
#else
  if (!mmap(pagePtr, kPageSize, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON,
            -1, 0)) {
    return;
  }
#endif

  gMut->SetPageFreed(aLock, aIndex, aArenaId, aFreeStack, aReuseDelay);
}

//---------------------------------------------------------------------------
// replace-malloc machinery
//---------------------------------------------------------------------------

// This handles malloc, moz_arena_malloc, and realloc-with-a-nullptr.
MOZ_ALWAYS_INLINE static void* PageMalloc(const Maybe<arena_id_t>& aArenaId,
                                          size_t aReqSize) {
  void* ptr = MaybePageAlloc(aArenaId, aReqSize, /* aZero */ false);
  return ptr ? ptr
             : (aArenaId.isSome()
                    ? sMallocTable.moz_arena_malloc(*aArenaId, aReqSize)
                    : sMallocTable.malloc(aReqSize));
}

static void* replace_malloc(size_t aReqSize) {
  return PageMalloc(Nothing(), aReqSize);
}

// This handles both calloc and moz_arena_calloc.
MOZ_ALWAYS_INLINE static void* PageCalloc(const Maybe<arena_id_t>& aArenaId,
                                          size_t aNum, size_t aReqSize) {
  CheckedInt<size_t> checkedSize = CheckedInt<size_t>(aNum) * aReqSize;
  if (!checkedSize.isValid()) {
    return nullptr;
  }

  void* ptr = MaybePageAlloc(aArenaId, checkedSize.value(), /* aZero */ true);
  return ptr ? ptr
             : (aArenaId.isSome()
                    ? sMallocTable.moz_arena_calloc(*aArenaId, aNum, aReqSize)
                    : sMallocTable.calloc(aNum, aReqSize));
}

static void* replace_calloc(size_t aNum, size_t aReqSize) {
  return PageCalloc(Nothing(), aNum, aReqSize);
}

// This function handles both realloc and moz_arena_realloc.
//
// As always, realloc is complicated, and doubly so when there are two
// different kinds of allocations in play. Here are the possible transitions,
// and what we do in practice.
//
// - normal-to-normal: This is straightforward and obviously necessary.
//
// - normal-to-page: This is disallowed because it would require getting the
//   arenaId of the normal allocation, which isn't possible in non-DEBUG builds
//   for security reasons.
//
// - page-to-page: This is done whenever possible, i.e. whenever the new size
//   is less than or equal to 4 KiB. This choice counterbalances the
//   disallowing of normal-to-page allocations, in order to avoid biasing
//   towards or away from page allocations. It always occurs in-place.
//
// - page-to-normal: this is done only when necessary, i.e. only when the new
//   size is greater than 4 KiB. This choice naturally flows from the
//   prior choice on page-to-page transitions.
//
// In summary: realloc doesn't change the allocation kind unless it must.
//
MOZ_ALWAYS_INLINE static void* PageRealloc(const Maybe<arena_id_t>& aArenaId,
                                           void* aOldPtr, size_t aNewSize) {
  if (!aOldPtr) {
    // Null pointer. Treat like malloc(aNewSize).
    return PageMalloc(aArenaId, aNewSize);
  }

  Maybe<uintptr_t> i = gConst->PageIndex(aOldPtr);
  if (i.isNothing()) {
    // A normal-to-normal transition.
    return aArenaId.isSome()
               ? sMallocTable.moz_arena_realloc(*aArenaId, aOldPtr, aNewSize)
               : sMallocTable.realloc(aOldPtr, aNewSize);
  }

  // A page-to-something transition.

  // Note that `disable` has no effect unless it is emplaced below.
  Maybe<AutoDisableOnCurrentThread> disable;
  // Get the stack trace *before* locking the mutex.
  StackTrace stack;
  if (GTls::IsDisabledOnCurrentThread()) {
    // PHC is disabled on this thread. Leave the stack empty.
  } else {
    // Disable on this thread *before* getting the stack trace.
    disable.emplace();
    stack.Fill();
  }

  MutexAutoLock lock(GMut::sMutex);

  // Check for realloc() of a freed block.
  gMut->EnsureInUse(lock, aOldPtr, *i);

  if (aNewSize <= kPageSize) {
    // A page-to-page transition. Just keep using the page allocation. We do
    // this even if the thread is disabled, because it doesn't create a new
    // page allocation. Note that ResizePageInUse() checks aArenaId.
    size_t newUsableSize = sMallocTable.malloc_good_size(aNewSize);
    gMut->ResizePageInUse(lock, *i, aArenaId, newUsableSize, stack);
    LOG("PageRealloc-Reuse(%p, %zu)\n", aOldPtr, aNewSize);
    return aOldPtr;
  }

  // A page-to-normal transition (with the new size greater than page-sized).
  // (Note that aArenaId is checked below.)
  void* newPtr;
  if (aArenaId.isSome()) {
    newPtr = sMallocTable.moz_arena_malloc(*aArenaId, aNewSize);
  } else {
    Maybe<arena_id_t> oldArenaId = gMut->PageArena(lock, *i);
    newPtr = (oldArenaId.isSome()
                  ? sMallocTable.moz_arena_malloc(*oldArenaId, aNewSize)
                  : sMallocTable.malloc(aNewSize));
  }
  if (!newPtr) {
    return nullptr;
  }

  MOZ_ASSERT(aNewSize > kPageSize);

  Delay reuseDelay = Rnd64ToDelay<kAvgPageReuseDelay>(gMut->Random64(lock));

  // Copy the usable size rather than the requested size, because the user
  // might have used malloc_usable_size() and filled up the usable size. Note
  // that FreePage() checks aArenaId (via SetPageFreed()).
  size_t oldUsableSize = gMut->PageUsableSize(lock, *i);
  memcpy(newPtr, aOldPtr, std::min(oldUsableSize, aNewSize));
  FreePage(lock, *i, aArenaId, stack, reuseDelay);
  LOG("PageRealloc-Free(%p[%zu], %zu) -> %p, %zu delay, reuse at ~%zu\n",
      aOldPtr, *i, aNewSize, newPtr, size_t(reuseDelay),
      size_t(GAtomic::Now()) + reuseDelay);

  return newPtr;
}

static void* replace_realloc(void* aOldPtr, size_t aNewSize) {
  return PageRealloc(Nothing(), aOldPtr, aNewSize);
}

// This handles both free and moz_arena_free.
MOZ_ALWAYS_INLINE static void PageFree(const Maybe<arena_id_t>& aArenaId,
                                       void* aPtr) {
  Maybe<uintptr_t> i = gConst->PageIndex(aPtr);
  if (i.isNothing()) {
    // Not a page allocation.
    return aArenaId.isSome() ? sMallocTable.moz_arena_free(*aArenaId, aPtr)
                             : sMallocTable.free(aPtr);
  }

  // Note that `disable` has no effect unless it is emplaced below.
  Maybe<AutoDisableOnCurrentThread> disable;
  // Get the stack trace *before* locking the mutex.
  StackTrace freeStack;
  if (GTls::IsDisabledOnCurrentThread()) {
    // PHC is disabled on this thread. Leave the stack empty.
  } else {
    // Disable on this thread *before* getting the stack trace.
    disable.emplace();
    freeStack.Fill();
  }

  MutexAutoLock lock(GMut::sMutex);

  // Check for a double-free.
  gMut->EnsureInUse(lock, aPtr, *i);

  // Note that FreePage() checks aArenaId (via SetPageFreed()).
  Delay reuseDelay = Rnd64ToDelay<kAvgPageReuseDelay>(gMut->Random64(lock));
  FreePage(lock, *i, aArenaId, freeStack, reuseDelay);

  LOG("PageFree(%p[%zu]), %zu delay, reuse at ~%zu\n", aPtr, *i,
      size_t(reuseDelay), size_t(GAtomic::Now()) + reuseDelay);
}

static void replace_free(void* aPtr) { return PageFree(Nothing(), aPtr); }

// This handles memalign and moz_arena_memalign.
MOZ_ALWAYS_INLINE static void* PageMemalign(const Maybe<arena_id_t>& aArenaId,
                                            size_t aAlignment,
                                            size_t aReqSize) {
  // PHC always allocates on a page boundary, so if the alignment required is
  // no greater than that it'll happen automatically. Otherwise, we can't
  // satisfy it, so fall back to mozjemalloc.
  MOZ_ASSERT(IsPowerOfTwo(aAlignment));
  void* ptr = nullptr;
  if (aAlignment <= kPageSize) {
    ptr = MaybePageAlloc(aArenaId, aReqSize, /* aZero */ false);
  }
  return ptr ? ptr
             : (aArenaId.isSome()
                    ? sMallocTable.moz_arena_memalign(*aArenaId, aAlignment,
                                                      aReqSize)
                    : sMallocTable.memalign(aAlignment, aReqSize));
}

static void* replace_memalign(size_t aAlignment, size_t aReqSize) {
  return PageMemalign(Nothing(), aAlignment, aReqSize);
}

static size_t replace_malloc_usable_size(usable_ptr_t aPtr) {
  Maybe<uintptr_t> i = gConst->PageIndex(aPtr);
  if (i.isNothing()) {
    // Not a page allocation. Measure it normally.
    return sMallocTable.malloc_usable_size(aPtr);
  }

  MutexAutoLock lock(GMut::sMutex);

  // Check for malloc_usable_size() of a freed block.
  gMut->EnsureInUse(lock, const_cast<void*>(aPtr), *i);

  return gMut->PageUsableSize(lock, *i);
}

void replace_jemalloc_stats(jemalloc_stats_t* aStats) {
  sMallocTable.jemalloc_stats(aStats);

  // Add all the pages to `mapped`.
  size_t mapped = kAllPagesSize;
  aStats->mapped += mapped;

  size_t allocated = 0;
  {
    MutexAutoLock lock(GMut::sMutex);

    // Add usable space of in-use allocations to `allocated`.
    for (size_t i = 0; i < kMaxPageAllocs; i++) {
      if (gMut->IsPageInUse(lock, i)) {
        allocated += gMut->PageUsableSize(lock, i);
      }
    }
  }
  aStats->allocated += allocated;

  // Waste is the gap between `allocated` and `mapped`.
  size_t waste = mapped - allocated;
  aStats->waste += waste;

  // aStats.page_cache and aStats.bin_unused are left unchanged because PHC
  // doesn't have anything corresponding to those.

  // gConst and gMut are normal heap allocations, so they're measured by
  // mozjemalloc as `allocated`. Move them into `bookkeeping`.
  size_t bookkeeping = sMallocTable.malloc_usable_size(gConst) +
                       sMallocTable.malloc_usable_size(gMut);
  aStats->allocated -= bookkeeping;
  aStats->bookkeeping += bookkeeping;
}

void replace_jemalloc_ptr_info(const void* aPtr, jemalloc_ptr_info_t* aInfo) {
  // We need to implement this properly, because various code locations do
  // things like checking that allocations are in the expected arena.
  Maybe<uintptr_t> i = gConst->PageIndex(aPtr);
  if (i.isNothing()) {
    // Not a page allocation.
    return sMallocTable.jemalloc_ptr_info(aPtr, aInfo);
  }

  MutexAutoLock lock(GMut::sMutex);

  gMut->FillJemallocPtrInfo(lock, aPtr, *i, aInfo);
#if DEBUG
  LOG("JemallocPtrInfo(%p[%zu]) -> {%zu, %p, %zu, %zu}\n", aPtr, *i,
      size_t(aInfo->tag), aInfo->addr, aInfo->size, aInfo->arenaId);
#else
  LOG("JemallocPtrInfo(%p[%zu]) -> {%zu, %p, %zu}\n", aPtr, *i,
      size_t(aInfo->tag), aInfo->addr, aInfo->size);
#endif
}

arena_id_t replace_moz_create_arena_with_params(arena_params_t* aParams) {
  // No need to do anything special here.
  return sMallocTable.moz_create_arena_with_params(aParams);
}

void replace_moz_dispose_arena(arena_id_t aArenaId) {
  // No need to do anything special here.
  return sMallocTable.moz_dispose_arena(aArenaId);
}

void* replace_moz_arena_malloc(arena_id_t aArenaId, size_t aReqSize) {
  return PageMalloc(Some(aArenaId), aReqSize);
}

void* replace_moz_arena_calloc(arena_id_t aArenaId, size_t aNum,
                               size_t aReqSize) {
  return PageCalloc(Some(aArenaId), aNum, aReqSize);
}

void* replace_moz_arena_realloc(arena_id_t aArenaId, void* aOldPtr,
                                size_t aNewSize) {
  return PageRealloc(Some(aArenaId), aOldPtr, aNewSize);
}

void replace_moz_arena_free(arena_id_t aArenaId, void* aPtr) {
  return PageFree(Some(aArenaId), aPtr);
}

void* replace_moz_arena_memalign(arena_id_t aArenaId, size_t aAlignment,
                                 size_t aReqSize) {
  return PageMemalign(Some(aArenaId), aAlignment, aReqSize);
}

class PHCBridge : public ReplaceMallocBridge {
  virtual bool IsPHCAllocation(const void* aPtr, phc::AddrInfo* aOut) override {
    Maybe<uintptr_t> i = gConst->PageIndex(aPtr);
    if (i.isNothing()) {
      return false;
    }

    if (aOut) {
      MutexAutoLock lock(GMut::sMutex);
      gMut->FillAddrInfo(lock, *i, aPtr, *aOut);
      LOG("IsPHCAllocation: %zu, %p, %zu, %zu, %zu\n", size_t(aOut->mKind),
          aOut->mBaseAddr, aOut->mUsableSize, aOut->mAllocStack.mLength,
          aOut->mFreeStack.mLength);
    }
    return true;
  }

  virtual void DisablePHCOnCurrentThread() override {
    GTls::DisableOnCurrentThread();
    LOG("DisablePHCOnCurrentThread: %zu\n", 0ul);
  }

  virtual void ReenablePHCOnCurrentThread() override {
    GTls::EnableOnCurrentThread();
    LOG("ReenablePHCOnCurrentThread: %zu\n", 0ul);
  }

  virtual bool IsPHCEnabledOnCurrentThread() override {
    bool enabled = !GTls::IsDisabledOnCurrentThread();
    LOG("IsPHCEnabledOnCurrentThread: %zu\n", size_t(enabled));
    return enabled;
  }
};

// WARNING: this function runs *very* early -- before all static initializers
// have run. For this reason, non-scalar globals (gConst, gMut) are allocated
// dynamically (so we can guarantee their construction in this function) rather
// than statically. GAtomic and GTls contain simple static data that doesn't
// involve static initializers so they don't need to be allocated dynamically.
void replace_init(malloc_table_t* aMallocTable, ReplaceMallocBridge** aBridge) {
  // Don't run PHC if the page size isn't 4 KiB.
  jemalloc_stats_t stats;
  aMallocTable->jemalloc_stats(&stats);
  if (stats.page_size != kPageSize) {
    return;
  }

  sMallocTable = *aMallocTable;

  // The choices of which functions to replace are complex enough that we set
  // them individually instead of using MALLOC_FUNCS/malloc_decls.h.

  aMallocTable->malloc = replace_malloc;
  aMallocTable->calloc = replace_calloc;
  aMallocTable->realloc = replace_realloc;
  aMallocTable->free = replace_free;
  aMallocTable->memalign = replace_memalign;

  // posix_memalign, aligned_alloc & valloc: unset, which means they fall back
  // to replace_memalign.
  aMallocTable->malloc_usable_size = replace_malloc_usable_size;
  // default malloc_good_size: the default suffices.

  aMallocTable->jemalloc_stats = replace_jemalloc_stats;
  // jemalloc_purge_freed_pages: the default suffices.
  // jemalloc_free_dirty_pages: the default suffices.
  // jemalloc_thread_local_arena: the default suffices.
  aMallocTable->jemalloc_ptr_info = replace_jemalloc_ptr_info;

  aMallocTable->moz_create_arena_with_params =
      replace_moz_create_arena_with_params;
  aMallocTable->moz_dispose_arena = replace_moz_dispose_arena;
  aMallocTable->moz_arena_malloc = replace_moz_arena_malloc;
  aMallocTable->moz_arena_calloc = replace_moz_arena_calloc;
  aMallocTable->moz_arena_realloc = replace_moz_arena_realloc;
  aMallocTable->moz_arena_free = replace_moz_arena_free;
  aMallocTable->moz_arena_memalign = replace_moz_arena_memalign;

  static PHCBridge bridge;
  *aBridge = &bridge;

#ifndef XP_WIN
  // Avoid deadlocks when forking by acquiring our state lock prior to forking
  // and releasing it after forking. See |LogAlloc|'s |replace_init| for
  // in-depth details.
  //
  // Note: This must run after attempting an allocation so as to give the
  // system malloc a chance to insert its own atfork handler.
  sMallocTable.malloc(-1);
  pthread_atfork(GMut::prefork, GMut::postfork, GMut::postfork);
#endif

  // gConst and gMut are never freed. They live for the life of the process.
  gConst = InfallibleAllocPolicy::new_<GConst>();
  GTls::Init();
  gMut = InfallibleAllocPolicy::new_<GMut>();
  {
    MutexAutoLock lock(GMut::sMutex);
    Delay firstAllocDelay =
        Rnd64ToDelay<kAvgFirstAllocDelay>(gMut->Random64(lock));
    GAtomic::Init(firstAllocDelay);
  }
}
