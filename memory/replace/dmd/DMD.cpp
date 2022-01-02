/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(MOZ_PROFILING)
#  error "DMD requires MOZ_PROFILING"
#endif

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

#include "nscore.h"

#include "mozilla/Assertions.h"
#include "mozilla/FastBernoulliTrial.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/HashTable.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/JSONWriter.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"
#include "mozilla/StackWalk.h"
#include "mozilla/ThreadLocal.h"

// CodeAddressService is defined entirely in the header, so this does not make
// DMD depend on XPCOM's object file.
#include "CodeAddressService.h"

// replace_malloc.h needs to be included before replace_malloc_bridge.h,
// which DMD.h includes, so DMD.h needs to be included after replace_malloc.h.
#include "replace_malloc.h"
#include "DMD.h"

namespace mozilla {
namespace dmd {

class DMDBridge : public ReplaceMallocBridge {
  virtual DMDFuncs* GetDMDFuncs() override;
};

static DMDBridge* gDMDBridge;
static DMDFuncs gDMDFuncs;

DMDFuncs* DMDBridge::GetDMDFuncs() { return &gDMDFuncs; }

MOZ_FORMAT_PRINTF(1, 2)
inline void StatusMsg(const char* aFmt, ...) {
  va_list ap;
  va_start(ap, aFmt);
  gDMDFuncs.StatusMsg(aFmt, ap);
  va_end(ap);
}

//---------------------------------------------------------------------------
// Utilities
//---------------------------------------------------------------------------

#ifndef DISALLOW_COPY_AND_ASSIGN
#  define DISALLOW_COPY_AND_ASSIGN(T) \
    T(const T&);                      \
    void operator=(const T&)
#endif

static malloc_table_t gMallocTable;

// This provides infallible allocations (they abort on OOM).  We use it for all
// of DMD's own allocations, which fall into the following three cases.
//
// - Direct allocations (the easy case).
//
// - Indirect allocations in mozilla::{Vector,HashSet,HashMap} -- this class
//   serves as their AllocPolicy.
//
// - Other indirect allocations (e.g. MozStackWalk) -- see the comments on
//   Thread::mBlockIntercepts and in replace_malloc for how these work.
//
// It would be nice if we could use the InfallibleAllocPolicy from mozalloc,
// but DMD cannot use mozalloc.
//
class InfallibleAllocPolicy {
  static void ExitOnFailure(const void* aP);

 public:
  template <typename T>
  static T* maybe_pod_malloc(size_t aNumElems) {
    if (aNumElems & mozilla::tl::MulOverflowMask<sizeof(T)>::value)
      return nullptr;
    return (T*)gMallocTable.malloc(aNumElems * sizeof(T));
  }

  template <typename T>
  static T* maybe_pod_calloc(size_t aNumElems) {
    return (T*)gMallocTable.calloc(aNumElems, sizeof(T));
  }

  template <typename T>
  static T* maybe_pod_realloc(T* aPtr, size_t aOldSize, size_t aNewSize) {
    if (aNewSize & mozilla::tl::MulOverflowMask<sizeof(T)>::value)
      return nullptr;
    return (T*)gMallocTable.realloc(aPtr, aNewSize * sizeof(T));
  }

  static void* malloc_(size_t aSize) {
    void* p = gMallocTable.malloc(aSize);
    ExitOnFailure(p);
    return p;
  }

  template <typename T>
  static T* pod_malloc(size_t aNumElems) {
    T* p = maybe_pod_malloc<T>(aNumElems);
    ExitOnFailure(p);
    return p;
  }

  static void* calloc_(size_t aCount, size_t aSize) {
    void* p = gMallocTable.calloc(aCount, aSize);
    ExitOnFailure(p);
    return p;
  }

  template <typename T>
  static T* pod_calloc(size_t aNumElems) {
    T* p = maybe_pod_calloc<T>(aNumElems);
    ExitOnFailure(p);
    return p;
  }

  static void* realloc_(void* aPtr, size_t aNewSize) {
    void* p = gMallocTable.realloc(aPtr, aNewSize);
    ExitOnFailure(p);
    return p;
  }

  template <typename T>
  static T* pod_realloc(T* aPtr, size_t aOldSize, size_t aNewSize) {
    T* p = maybe_pod_realloc(aPtr, aOldSize, aNewSize);
    ExitOnFailure(p);
    return p;
  }

  static void* memalign_(size_t aAlignment, size_t aSize) {
    void* p = gMallocTable.memalign(aAlignment, aSize);
    ExitOnFailure(p);
    return p;
  }

  template <typename T>
  static void free_(T* aPtr, size_t aSize = 0) {
    gMallocTable.free(aPtr);
  }

  static char* strdup_(const char* aStr) {
    char* s = (char*)InfallibleAllocPolicy::malloc_(strlen(aStr) + 1);
    strcpy(s, aStr);
    return s;
  }

  template <class T>
  static T* new_() {
    void* mem = malloc_(sizeof(T));
    return new (mem) T;
  }

  template <class T, typename P1>
  static T* new_(const P1& aP1) {
    void* mem = malloc_(sizeof(T));
    return new (mem) T(aP1);
  }

  template <class T>
  static void delete_(T* aPtr) {
    if (aPtr) {
      aPtr->~T();
      InfallibleAllocPolicy::free_(aPtr);
    }
  }

  static void reportAllocOverflow() { ExitOnFailure(nullptr); }
  bool checkSimulatedOOM() const { return true; }
};

// This is only needed because of the |const void*| vs |void*| arg mismatch.
static size_t MallocSizeOf(const void* aPtr) {
  return gMallocTable.malloc_usable_size(const_cast<void*>(aPtr));
}

void DMDFuncs::StatusMsg(const char* aFmt, va_list aAp) {
#ifdef ANDROID
  __android_log_vprint(ANDROID_LOG_INFO, "DMD", aFmt, aAp);
#else
  // The +64 is easily enough for the "DMD[<pid>] " prefix and the NUL.
  char* fmt = (char*)InfallibleAllocPolicy::malloc_(strlen(aFmt) + 64);
  sprintf(fmt, "DMD[%d] %s", getpid(), aFmt);
  vfprintf(stderr, fmt, aAp);
  InfallibleAllocPolicy::free_(fmt);
#endif
}

/* static */
void InfallibleAllocPolicy::ExitOnFailure(const void* aP) {
  if (!aP) {
    MOZ_CRASH("DMD out of memory; aborting");
  }
}

static double Percent(size_t part, size_t whole) {
  return (whole == 0) ? 0 : 100 * (double)part / whole;
}

// Commifies the number.
static char* Show(size_t n, char* buf, size_t buflen) {
  int nc = 0, i = 0, lasti = buflen - 2;
  buf[lasti + 1] = '\0';
  if (n == 0) {
    buf[lasti - i] = '0';
    i++;
  } else {
    while (n > 0) {
      if (((i - nc) % 3) == 0 && i != 0) {
        buf[lasti - i] = ',';
        i++;
        nc++;
      }
      buf[lasti - i] = static_cast<char>((n % 10) + '0');
      i++;
      n /= 10;
    }
  }
  int firstCharIndex = lasti - i + 1;

  MOZ_ASSERT(firstCharIndex >= 0);
  return &buf[firstCharIndex];
}

//---------------------------------------------------------------------------
// Options (Part 1)
//---------------------------------------------------------------------------

class Options {
  template <typename T>
  struct NumOption {
    const T mDefault;
    const T mMax;
    T mActual;
    NumOption(T aDefault, T aMax)
        : mDefault(aDefault), mMax(aMax), mActual(aDefault) {}
  };

  // DMD has several modes. These modes affect what data is recorded and
  // written to the output file, and the written data affects the
  // post-processing that dmd.py can do.
  //
  // Users specify the mode as soon as DMD starts. This leads to minimal memory
  // usage and log file size. It has the disadvantage that is inflexible -- if
  // you want to change modes you have to re-run DMD. But in practice changing
  // modes seems to be rare, so it's not much of a problem.
  //
  // An alternative possibility would be to always record and output *all* the
  // information needed for all modes. This would let you choose the mode when
  // running dmd.py, and so you could do multiple kinds of profiling on a
  // single DMD run. But if you are only interested in one of the simpler
  // modes, you'd pay the price of (a) increased memory usage and (b) *very*
  // large log files.
  //
  // Finally, another alternative possibility would be to do mode selection
  // partly at DMD startup or recording, and then partly in dmd.py. This would
  // give some extra flexibility at moderate memory and file size cost. But
  // certain mode pairs wouldn't work, which would be confusing.
  //
  enum class Mode {
    // For each live block, this mode outputs: size (usable and slop) and
    // (possibly) and allocation stack. This mode is good for live heap
    // profiling.
    Live,

    // Like "Live", but for each live block it also outputs: zero or more
    // report stacks. This mode is good for identifying where memory reporters
    // should be added. This is the default mode.
    DarkMatter,

    // Like "Live", but also outputs the same data for dead blocks. This mode
    // does cumulative heap profiling, which is good for identifying where large
    // amounts of short-lived allocations ("heap churn") occur.
    Cumulative,

    // Like "Live", but this mode also outputs for each live block the address
    // of the block and the values contained in the blocks. This mode is useful
    // for investigating leaks, by helping to figure out which blocks refer to
    // other blocks. This mode force-enables full stacks coverage.
    Scan
  };

  // With full stacks, every heap block gets a stack trace recorded for it.
  // This is complete but slow.
  //
  // With partial stacks, not all heap blocks will get a stack trace recorded.
  // A Bernoulli trial (see mfbt/FastBernoulliTrial.h for details) is performed
  // for each heap block to decide if it gets one. Because bigger heap blocks
  // are more likely to get a stack trace, even though most heap *blocks* won't
  // get a stack trace, most heap *bytes* will.
  enum class Stacks { Full, Partial };

  char* mDMDEnvVar;  // a saved copy, for later printing

  Mode mMode;
  Stacks mStacks;
  bool mShowDumpStats;

  void BadArg(const char* aArg);
  static const char* ValueIfMatch(const char* aArg, const char* aOptionName);
  static bool GetLong(const char* aArg, const char* aOptionName, long aMin,
                      long aMax, long* aValue);
  static bool GetBool(const char* aArg, const char* aOptionName, bool* aValue);

 public:
  explicit Options(const char* aDMDEnvVar);

  bool IsLiveMode() const { return mMode == Mode::Live; }
  bool IsDarkMatterMode() const { return mMode == Mode::DarkMatter; }
  bool IsCumulativeMode() const { return mMode == Mode::Cumulative; }
  bool IsScanMode() const { return mMode == Mode::Scan; }

  const char* ModeString() const;

  const char* DMDEnvVar() const { return mDMDEnvVar; }

  bool DoFullStacks() const { return mStacks == Stacks::Full; }
  size_t ShowDumpStats() const { return mShowDumpStats; }
};

static Options* gOptions;

//---------------------------------------------------------------------------
// The global lock
//---------------------------------------------------------------------------

// MutexBase implements the platform-specific parts of a mutex.

#ifdef XP_WIN

class MutexBase {
  CRITICAL_SECTION mCS;

  DISALLOW_COPY_AND_ASSIGN(MutexBase);

 public:
  MutexBase() { InitializeCriticalSection(&mCS); }
  ~MutexBase() { DeleteCriticalSection(&mCS); }

  void Lock() { EnterCriticalSection(&mCS); }
  void Unlock() { LeaveCriticalSection(&mCS); }
};

#else

class MutexBase {
  pthread_mutex_t mMutex;

  MutexBase(const MutexBase&) = delete;

  const MutexBase& operator=(const MutexBase&) = delete;

 public:
  MutexBase() { pthread_mutex_init(&mMutex, nullptr); }

  void Lock() { pthread_mutex_lock(&mMutex); }
  void Unlock() { pthread_mutex_unlock(&mMutex); }
};

#endif

class Mutex : private MutexBase {
  bool mIsLocked;

  Mutex(const Mutex&) = delete;

  const Mutex& operator=(const Mutex&) = delete;

 public:
  Mutex() : mIsLocked(false) {}

  void Lock() {
    MutexBase::Lock();
    MOZ_ASSERT(!mIsLocked);
    mIsLocked = true;
  }

  void Unlock() {
    MOZ_ASSERT(mIsLocked);
    mIsLocked = false;
    MutexBase::Unlock();
  }

  bool IsLocked() { return mIsLocked; }
};

// This lock must be held while manipulating global state such as
// gStackTraceTable, gLiveBlockTable, gDeadBlockTable. Note that gOptions is
// *not* protected by this lock because it is only written to by Options(),
// which is only invoked at start-up and in ResetEverything(), which is only
// used by SmokeDMD.cpp.
static Mutex* gStateLock = nullptr;

class AutoLockState {
  AutoLockState(const AutoLockState&) = delete;

  const AutoLockState& operator=(const AutoLockState&) = delete;

 public:
  AutoLockState() { gStateLock->Lock(); }
  ~AutoLockState() { gStateLock->Unlock(); }
};

class AutoUnlockState {
  AutoUnlockState(const AutoUnlockState&) = delete;

  const AutoUnlockState& operator=(const AutoUnlockState&) = delete;

 public:
  AutoUnlockState() { gStateLock->Unlock(); }
  ~AutoUnlockState() { gStateLock->Lock(); }
};

//---------------------------------------------------------------------------
// Per-thread blocking of intercepts
//---------------------------------------------------------------------------

// On MacOS, the first __thread/thread_local access calls malloc, which leads
// to an infinite loop. So we use pthread-based TLS instead, which somehow
// doesn't have this problem.
#if !defined(XP_DARWIN)
#  define DMD_THREAD_LOCAL(T) MOZ_THREAD_LOCAL(T)
#else
#  define DMD_THREAD_LOCAL(T) \
    detail::ThreadLocal<T, detail::ThreadLocalKeyStorage>
#endif

class Thread {
  // Required for allocation via InfallibleAllocPolicy::new_.
  friend class InfallibleAllocPolicy;

  // When true, this blocks intercepts, which allows malloc interception
  // functions to themselves call malloc.  (Nb: for direct calls to malloc we
  // can just use InfallibleAllocPolicy::{malloc_,new_}, but we sometimes
  // indirectly call vanilla malloc via functions like MozStackWalk.)
  bool mBlockIntercepts;

  Thread() : mBlockIntercepts(false) {}

  Thread(const Thread&) = delete;

  const Thread& operator=(const Thread&) = delete;

  static DMD_THREAD_LOCAL(Thread*) tlsThread;

 public:
  static void Init() {
    if (!tlsThread.init()) {
      MOZ_CRASH();
    }
  }

  static Thread* Fetch() {
    Thread* t = tlsThread.get();
    if (MOZ_UNLIKELY(!t)) {
      // This memory is never freed, even if the thread dies. It's a leak, but
      // only a tiny one.
      t = InfallibleAllocPolicy::new_<Thread>();
      tlsThread.set(t);
    }

    return t;
  }

  bool BlockIntercepts() {
    MOZ_ASSERT(!mBlockIntercepts);
    return mBlockIntercepts = true;
  }

  bool UnblockIntercepts() {
    MOZ_ASSERT(mBlockIntercepts);
    return mBlockIntercepts = false;
  }

  bool InterceptsAreBlocked() const { return mBlockIntercepts; }
};

DMD_THREAD_LOCAL(Thread*) Thread::tlsThread;

// An object of this class must be created (on the stack) before running any
// code that might allocate.
class AutoBlockIntercepts {
  Thread* const mT;

  AutoBlockIntercepts(const AutoBlockIntercepts&) = delete;

  const AutoBlockIntercepts& operator=(const AutoBlockIntercepts&) = delete;

 public:
  explicit AutoBlockIntercepts(Thread* aT) : mT(aT) { mT->BlockIntercepts(); }
  ~AutoBlockIntercepts() {
    MOZ_ASSERT(mT->InterceptsAreBlocked());
    mT->UnblockIntercepts();
  }
};

//---------------------------------------------------------------------------
// Location service
//---------------------------------------------------------------------------

struct DescribeCodeAddressLock {
  static void Unlock() { gStateLock->Unlock(); }
  static void Lock() { gStateLock->Lock(); }
  static bool IsLocked() { return gStateLock->IsLocked(); }
};

typedef CodeAddressService<InfallibleAllocPolicy, DescribeCodeAddressLock>
    CodeAddressService;

//---------------------------------------------------------------------------
// Stack traces
//---------------------------------------------------------------------------

class StackTrace {
 public:
  static const uint32_t MaxFrames = 24;

 private:
  uint32_t mLength;             // The number of PCs.
  const void* mPcs[MaxFrames];  // The PCs themselves.

 public:
  StackTrace() : mLength(0) {}
  StackTrace(const StackTrace& aOther) : mLength(aOther.mLength) {
    PodCopy(mPcs, aOther.mPcs, mLength);
  }

  uint32_t Length() const { return mLength; }
  const void* Pc(uint32_t i) const {
    MOZ_ASSERT(i < mLength);
    return mPcs[i];
  }

  uint32_t Size() const { return mLength * sizeof(mPcs[0]); }

  // The stack trace returned by this function is interned in gStackTraceTable,
  // and so is immortal and unmovable.
  static const StackTrace* Get(Thread* aT);

  // Hash policy.

  typedef StackTrace* Lookup;

  static mozilla::HashNumber hash(const StackTrace* const& aSt) {
    return mozilla::HashBytes(aSt->mPcs, aSt->Size());
  }

  static bool match(const StackTrace* const& aA, const StackTrace* const& aB) {
    return aA->mLength == aB->mLength &&
           memcmp(aA->mPcs, aB->mPcs, aA->Size()) == 0;
  }

 private:
  static void StackWalkCallback(uint32_t aFrameNumber, void* aPc, void* aSp,
                                void* aClosure) {
    StackTrace* st = (StackTrace*)aClosure;
    MOZ_ASSERT(st->mLength < MaxFrames);
    st->mPcs[st->mLength] = aPc;
    st->mLength++;
    MOZ_ASSERT(st->mLength == aFrameNumber);
  }
};

typedef mozilla::HashSet<StackTrace*, StackTrace, InfallibleAllocPolicy>
    StackTraceTable;
static StackTraceTable* gStackTraceTable = nullptr;

typedef mozilla::HashSet<const StackTrace*,
                         mozilla::DefaultHasher<const StackTrace*>,
                         InfallibleAllocPolicy>
    StackTraceSet;

typedef mozilla::HashSet<const void*, mozilla::DefaultHasher<const void*>,
                         InfallibleAllocPolicy>
    PointerSet;
typedef mozilla::HashMap<const void*, uint32_t,
                         mozilla::DefaultHasher<const void*>,
                         InfallibleAllocPolicy>
    PointerIdMap;

// We won't GC the stack trace table until it this many elements.
static uint32_t gGCStackTraceTableWhenSizeExceeds = 4 * 1024;

/* static */ const StackTrace* StackTrace::Get(Thread* aT) {
  MOZ_ASSERT(gStateLock->IsLocked());
  MOZ_ASSERT(aT->InterceptsAreBlocked());

  // On Windows, MozStackWalk can acquire a lock from the shared library
  // loader.  Another thread might call malloc while holding that lock (when
  // loading a shared library).  So we can't be in gStateLock during the call
  // to MozStackWalk.  For details, see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=374829#c8
  // On Linux, something similar can happen;  see bug 824340.
  // So let's just release it on all platforms.
  StackTrace tmp;
  {
    AutoUnlockState unlock;
    // In each of the following cases, skipFrames is chosen so that the
    // first frame in each stack trace is a replace_* function (or as close as
    // possible, given the vagaries of inlining on different platforms).
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
    FramePointerStackWalk(StackWalkCallback, MaxFrames, &tmp, fp, stackEnd);
#elif defined(XP_MACOSX)
    // This avoids MozStackWalk(), which has become unusably slow on Mac due to
    // changes in libunwind.
    //
    // This code is cribbed from the Gecko Profiler, which also uses
    // FramePointerStackWalk() on Mac: Registers::SyncPopulate() for the frame
    // pointer, and GetStackTop() for the stack end.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wframe-address"
    void** fp = reinterpret_cast<void**>(__builtin_frame_address(1));
#  pragma GCC diagnostic pop
    void* stackEnd = pthread_get_stackaddr_np(pthread_self());
    FramePointerStackWalk(StackWalkCallback, MaxFrames, &tmp, fp, stackEnd);
#else
    MozStackWalk(StackWalkCallback, nullptr, MaxFrames, &tmp);
#endif
  }

  StackTraceTable::AddPtr p = gStackTraceTable->lookupForAdd(&tmp);
  if (!p) {
    StackTrace* stnew = InfallibleAllocPolicy::new_<StackTrace>(tmp);
    MOZ_ALWAYS_TRUE(gStackTraceTable->add(p, stnew));
  }
  return *p;
}

//---------------------------------------------------------------------------
// Heap blocks
//---------------------------------------------------------------------------

// This class combines a 2-byte-aligned pointer (i.e. one whose bottom bit
// is zero) with a 1-bit tag.
//
// |T| is the pointer type, e.g. |int*|, not the pointed-to type.  This makes
// is easier to have const pointers, e.g. |TaggedPtr<const int*>|.
template <typename T>
class TaggedPtr {
  union {
    T mPtr;
    uintptr_t mUint;
  };

  static const uintptr_t kTagMask = uintptr_t(0x1);
  static const uintptr_t kPtrMask = ~kTagMask;

  static bool IsTwoByteAligned(T aPtr) {
    return (uintptr_t(aPtr) & kTagMask) == 0;
  }

 public:
  TaggedPtr() : mPtr(nullptr) {}

  TaggedPtr(T aPtr, bool aBool) : mPtr(aPtr) {
    MOZ_ASSERT(IsTwoByteAligned(aPtr));
    uintptr_t tag = uintptr_t(aBool);
    MOZ_ASSERT(tag <= kTagMask);
    mUint |= (tag & kTagMask);
  }

  void Set(T aPtr, bool aBool) {
    MOZ_ASSERT(IsTwoByteAligned(aPtr));
    mPtr = aPtr;
    uintptr_t tag = uintptr_t(aBool);
    MOZ_ASSERT(tag <= kTagMask);
    mUint |= (tag & kTagMask);
  }

  T Ptr() const { return reinterpret_cast<T>(mUint & kPtrMask); }

  bool Tag() const { return bool(mUint & kTagMask); }
};

// A live heap block. Stores both basic data and data about reports, if we're
// in DarkMatter mode.
class LiveBlock {
  const void* mPtr;
  const size_t mReqSize;  // size requested

  // The stack trace where this block was allocated, or nullptr if we didn't
  // record one.
  const StackTrace* const mAllocStackTrace;

  // This array has two elements because we record at most two reports of a
  // block.
  // - Ptr: |mReportStackTrace| - stack trace where this block was reported.
  //   nullptr if not reported.
  // - Tag bit 0: |mReportedOnAlloc| - was the block reported immediately on
  //   allocation?  If so, DMD must not clear the report at the end of
  //   Analyze(). Only relevant if |mReportStackTrace| is non-nullptr.
  //
  // |mPtr| is used as the key in LiveBlockTable, so it's ok for this member
  // to be |mutable|.
  //
  // Only used in DarkMatter mode.
  mutable TaggedPtr<const StackTrace*> mReportStackTrace_mReportedOnAlloc[2];

 public:
  LiveBlock(const void* aPtr, size_t aReqSize,
            const StackTrace* aAllocStackTrace)
      : mPtr(aPtr),
        mReqSize(aReqSize),
        mAllocStackTrace(aAllocStackTrace),
        mReportStackTrace_mReportedOnAlloc()  // all fields get zeroed
  {}

  const void* Address() const { return mPtr; }

  size_t ReqSize() const { return mReqSize; }

  size_t SlopSize() const { return MallocSizeOf(mPtr) - mReqSize; }

  const StackTrace* AllocStackTrace() const { return mAllocStackTrace; }

  const StackTrace* ReportStackTrace1() const {
    MOZ_ASSERT(gOptions->IsDarkMatterMode());
    return mReportStackTrace_mReportedOnAlloc[0].Ptr();
  }

  const StackTrace* ReportStackTrace2() const {
    MOZ_ASSERT(gOptions->IsDarkMatterMode());
    return mReportStackTrace_mReportedOnAlloc[1].Ptr();
  }

  bool ReportedOnAlloc1() const {
    MOZ_ASSERT(gOptions->IsDarkMatterMode());
    return mReportStackTrace_mReportedOnAlloc[0].Tag();
  }

  bool ReportedOnAlloc2() const {
    MOZ_ASSERT(gOptions->IsDarkMatterMode());
    return mReportStackTrace_mReportedOnAlloc[1].Tag();
  }

  void AddStackTracesToTable(StackTraceSet& aStackTraces) const {
    if (AllocStackTrace()) {
      MOZ_ALWAYS_TRUE(aStackTraces.put(AllocStackTrace()));
    }
    if (gOptions->IsDarkMatterMode()) {
      if (ReportStackTrace1()) {
        MOZ_ALWAYS_TRUE(aStackTraces.put(ReportStackTrace1()));
      }
      if (ReportStackTrace2()) {
        MOZ_ALWAYS_TRUE(aStackTraces.put(ReportStackTrace2()));
      }
    }
  }

  uint32_t NumReports() const {
    MOZ_ASSERT(gOptions->IsDarkMatterMode());
    if (ReportStackTrace2()) {
      MOZ_ASSERT(ReportStackTrace1());
      return 2;
    }
    if (ReportStackTrace1()) {
      return 1;
    }
    return 0;
  }

  // This is |const| thanks to the |mutable| fields above.
  void Report(Thread* aT, bool aReportedOnAlloc) const {
    MOZ_ASSERT(gOptions->IsDarkMatterMode());
    // We don't bother recording reports after the 2nd one.
    uint32_t numReports = NumReports();
    if (numReports < 2) {
      mReportStackTrace_mReportedOnAlloc[numReports].Set(StackTrace::Get(aT),
                                                         aReportedOnAlloc);
    }
  }

  void UnreportIfNotReportedOnAlloc() const {
    MOZ_ASSERT(gOptions->IsDarkMatterMode());
    if (!ReportedOnAlloc1() && !ReportedOnAlloc2()) {
      mReportStackTrace_mReportedOnAlloc[0].Set(nullptr, 0);
      mReportStackTrace_mReportedOnAlloc[1].Set(nullptr, 0);

    } else if (!ReportedOnAlloc1() && ReportedOnAlloc2()) {
      // Shift the 2nd report down to the 1st one.
      mReportStackTrace_mReportedOnAlloc[0] =
          mReportStackTrace_mReportedOnAlloc[1];
      mReportStackTrace_mReportedOnAlloc[1].Set(nullptr, 0);

    } else if (ReportedOnAlloc1() && !ReportedOnAlloc2()) {
      mReportStackTrace_mReportedOnAlloc[1].Set(nullptr, 0);
    }
  }

  // Hash policy.

  typedef const void* Lookup;

  static mozilla::HashNumber hash(const void* const& aPtr) {
    return mozilla::HashGeneric(aPtr);
  }

  static bool match(const LiveBlock& aB, const void* const& aPtr) {
    return aB.mPtr == aPtr;
  }
};

// A table of live blocks where the lookup key is the block address.
typedef mozilla::HashSet<LiveBlock, LiveBlock, InfallibleAllocPolicy>
    LiveBlockTable;
static LiveBlockTable* gLiveBlockTable = nullptr;

class AggregatedLiveBlockHashPolicy {
 public:
  typedef const LiveBlock* const Lookup;

  static mozilla::HashNumber hash(const LiveBlock* const& aB) {
    return gOptions->IsDarkMatterMode()
               ? mozilla::HashGeneric(
                     aB->ReqSize(), aB->SlopSize(), aB->AllocStackTrace(),
                     aB->ReportedOnAlloc1(), aB->ReportedOnAlloc2())
               : mozilla::HashGeneric(aB->ReqSize(), aB->SlopSize(),
                                      aB->AllocStackTrace());
  }

  static bool match(const LiveBlock* const& aA, const LiveBlock* const& aB) {
    return gOptions->IsDarkMatterMode()
               ? aA->ReqSize() == aB->ReqSize() &&
                     aA->SlopSize() == aB->SlopSize() &&
                     aA->AllocStackTrace() == aB->AllocStackTrace() &&
                     aA->ReportStackTrace1() == aB->ReportStackTrace1() &&
                     aA->ReportStackTrace2() == aB->ReportStackTrace2()
               : aA->ReqSize() == aB->ReqSize() &&
                     aA->SlopSize() == aB->SlopSize() &&
                     aA->AllocStackTrace() == aB->AllocStackTrace();
  }
};

// A table of live blocks where the lookup key is everything but the block
// address. For aggregating similar live blocks at output time.
typedef mozilla::HashMap<const LiveBlock*, size_t,
                         AggregatedLiveBlockHashPolicy, InfallibleAllocPolicy>
    AggregatedLiveBlockTable;

// A freed heap block.
class DeadBlock {
  const size_t mReqSize;   // size requested
  const size_t mSlopSize;  // slop above size requested

  // The stack trace where this block was allocated.
  const StackTrace* const mAllocStackTrace;

 public:
  DeadBlock() : mReqSize(0), mSlopSize(0), mAllocStackTrace(nullptr) {}

  explicit DeadBlock(const LiveBlock& aLb)
      : mReqSize(aLb.ReqSize()),
        mSlopSize(aLb.SlopSize()),
        mAllocStackTrace(aLb.AllocStackTrace()) {}

  ~DeadBlock() {}

  size_t ReqSize() const { return mReqSize; }
  size_t SlopSize() const { return mSlopSize; }

  const StackTrace* AllocStackTrace() const { return mAllocStackTrace; }

  void AddStackTracesToTable(StackTraceSet& aStackTraces) const {
    if (AllocStackTrace()) {
      MOZ_ALWAYS_TRUE(aStackTraces.put(AllocStackTrace()));
    }
  }

  // Hash policy.

  typedef DeadBlock Lookup;

  static mozilla::HashNumber hash(const DeadBlock& aB) {
    return mozilla::HashGeneric(aB.ReqSize(), aB.SlopSize(),
                                aB.AllocStackTrace());
  }

  static bool match(const DeadBlock& aA, const DeadBlock& aB) {
    return aA.ReqSize() == aB.ReqSize() && aA.SlopSize() == aB.SlopSize() &&
           aA.AllocStackTrace() == aB.AllocStackTrace();
  }
};

// For each unique DeadBlock value we store a count of how many actual dead
// blocks have that value.
typedef mozilla::HashMap<DeadBlock, size_t, DeadBlock, InfallibleAllocPolicy>
    DeadBlockTable;
static DeadBlockTable* gDeadBlockTable = nullptr;

// Add the dead block to the dead block table, if that's appropriate.
void MaybeAddToDeadBlockTable(const DeadBlock& aDb) {
  if (gOptions->IsCumulativeMode() && aDb.AllocStackTrace()) {
    AutoLockState lock;
    if (DeadBlockTable::AddPtr p = gDeadBlockTable->lookupForAdd(aDb)) {
      p->value() += 1;
    } else {
      MOZ_ALWAYS_TRUE(gDeadBlockTable->add(p, aDb, 1));
    }
  }
}

// Add a pointer to each live stack trace into the given StackTraceSet.  (A
// stack trace is live if it's used by one of the live blocks.)
static void GatherUsedStackTraces(StackTraceSet& aStackTraces) {
  MOZ_ASSERT(gStateLock->IsLocked());
  MOZ_ASSERT(Thread::Fetch()->InterceptsAreBlocked());

  aStackTraces.clear();
  MOZ_ALWAYS_TRUE(aStackTraces.reserve(512));

  for (auto iter = gLiveBlockTable->iter(); !iter.done(); iter.next()) {
    iter.get().AddStackTracesToTable(aStackTraces);
  }

  for (auto iter = gDeadBlockTable->iter(); !iter.done(); iter.next()) {
    iter.get().key().AddStackTracesToTable(aStackTraces);
  }
}

// Delete stack traces that we aren't using, and compact our hashtable.
static void GCStackTraces() {
  MOZ_ASSERT(gStateLock->IsLocked());
  MOZ_ASSERT(Thread::Fetch()->InterceptsAreBlocked());

  StackTraceSet usedStackTraces;
  GatherUsedStackTraces(usedStackTraces);

  // Delete all unused stack traces from gStackTraceTable.  The ModIterator
  // destructor will automatically rehash and compact the table.
  for (auto iter = gStackTraceTable->modIter(); !iter.done(); iter.next()) {
    StackTrace* const& st = iter.get();
    if (!usedStackTraces.has(st)) {
      iter.remove();
      InfallibleAllocPolicy::delete_(st);
    }
  }

  // Schedule a GC when we have twice as many stack traces as we had right after
  // this GC finished.
  gGCStackTraceTableWhenSizeExceeds = 2 * gStackTraceTable->count();
}

//---------------------------------------------------------------------------
// malloc/free callbacks
//---------------------------------------------------------------------------

static FastBernoulliTrial* gBernoulli;

// In testing, a probability of 0.003 resulted in ~25% of heap blocks getting
// a stack trace and ~80% of heap bytes getting a stack trace. (This is
// possible because big heap blocks are more likely to get a stack trace.)
//
// We deliberately choose not to give the user control over this probability
// (other than effectively setting it to 1 via --stacks=full) because it's
// quite inscrutable and generally the user just wants "faster and imprecise"
// or "slower and precise".
//
// The random number seeds are arbitrary and were obtained from random.org. If
// you change them you'll need to change the tests as well, because their
// expected output is based on the particular sequence of trial results that we
// get with these seeds.
static void ResetBernoulli() {
  new (gBernoulli)
      FastBernoulliTrial(0.003, 0x8e26eeee166bc8ca, 0x56820f304a9c9ae0);
}

static void AllocCallback(void* aPtr, size_t aReqSize, Thread* aT) {
  if (!aPtr) {
    return;
  }

  AutoLockState lock;
  AutoBlockIntercepts block(aT);

  size_t actualSize = gMallocTable.malloc_usable_size(aPtr);

  // We may or may not record the allocation stack trace, depending on the
  // options and the outcome of a Bernoulli trial.
  bool getTrace = gOptions->DoFullStacks() || gBernoulli->trial(actualSize);
  LiveBlock b(aPtr, aReqSize, getTrace ? StackTrace::Get(aT) : nullptr);
  LiveBlockTable::AddPtr p = gLiveBlockTable->lookupForAdd(aPtr);
  if (!p) {
    // Most common case: there wasn't a record already.
    MOZ_ALWAYS_TRUE(gLiveBlockTable->add(p, b));
  } else {
    // Edge-case: there was a record for the same address. We'll assume the
    // allocator is not giving out a pointer to an existing allocation, so
    // this means the previously recorded allocation was freed while we were
    // blocking interceptions. This can happen while processing the data in
    // e.g. AnalyzeImpl.
    if (gOptions->IsCumulativeMode()) {
      // Copy it out so it can be added to the dead block list later.
      DeadBlock db(*p);
      MaybeAddToDeadBlockTable(db);
    }
    gLiveBlockTable->remove(p);
    MOZ_ALWAYS_TRUE(gLiveBlockTable->putNew(aPtr, b));
  }
}

static void FreeCallback(void* aPtr, Thread* aT, DeadBlock* aDeadBlock) {
  if (!aPtr) {
    return;
  }

  AutoLockState lock;
  AutoBlockIntercepts block(aT);

  if (LiveBlockTable::Ptr lb = gLiveBlockTable->lookup(aPtr)) {
    if (gOptions->IsCumulativeMode()) {
      // Copy it out so it can be added to the dead block list later.
      new (aDeadBlock) DeadBlock(*lb);
    }
    gLiveBlockTable->remove(lb);
  } else {
    // We have no record of the block. It must be a bogus pointer, or one that
    // DMD wasn't able to see allocated. This should be extremely rare.
  }

  if (gStackTraceTable->count() > gGCStackTraceTableWhenSizeExceeds) {
    GCStackTraces();
  }
}

//---------------------------------------------------------------------------
// malloc/free interception
//---------------------------------------------------------------------------

static bool Init(malloc_table_t* aMallocTable);

}  // namespace dmd
}  // namespace mozilla

static void* replace_malloc(size_t aSize) {
  using namespace mozilla::dmd;

  Thread* t = Thread::Fetch();
  if (t->InterceptsAreBlocked()) {
    // Intercepts are blocked, which means this must be a call to malloc
    // triggered indirectly by DMD (e.g. via MozStackWalk).  Be infallible.
    return InfallibleAllocPolicy::malloc_(aSize);
  }

  // This must be a call to malloc from outside DMD.  Intercept it.
  void* ptr = gMallocTable.malloc(aSize);
  AllocCallback(ptr, aSize, t);
  return ptr;
}

static void* replace_calloc(size_t aCount, size_t aSize) {
  using namespace mozilla::dmd;

  Thread* t = Thread::Fetch();
  if (t->InterceptsAreBlocked()) {
    return InfallibleAllocPolicy::calloc_(aCount, aSize);
  }

  // |aCount * aSize| could overflow, but if that happens then
  // |gMallocTable.calloc()| will return nullptr and |AllocCallback()| will
  // return immediately without using the overflowed value.
  void* ptr = gMallocTable.calloc(aCount, aSize);
  AllocCallback(ptr, aCount * aSize, t);
  return ptr;
}

static void* replace_realloc(void* aOldPtr, size_t aSize) {
  using namespace mozilla::dmd;

  Thread* t = Thread::Fetch();
  if (t->InterceptsAreBlocked()) {
    return InfallibleAllocPolicy::realloc_(aOldPtr, aSize);
  }

  // If |aOldPtr| is nullptr, the call is equivalent to |malloc(aSize)|.
  if (!aOldPtr) {
    return replace_malloc(aSize);
  }

  // Be very careful here!  Must remove the block from the table before doing
  // the realloc to avoid races, just like in replace_free().
  // Nb: This does an unnecessary hashtable remove+add if the block doesn't
  // move, but doing better isn't worth the effort.
  DeadBlock db;
  FreeCallback(aOldPtr, t, &db);
  void* ptr = gMallocTable.realloc(aOldPtr, aSize);
  if (ptr) {
    AllocCallback(ptr, aSize, t);
    MaybeAddToDeadBlockTable(db);
  } else {
    // If realloc fails, we undo the prior operations by re-inserting the old
    // pointer into the live block table. We don't have to do anything with the
    // dead block list because the dead block hasn't yet been inserted. The
    // block will end up looking like it was allocated for the first time here,
    // which is untrue, and the slop bytes will be zero, which may be untrue.
    // But this case is rare and doing better isn't worth the effort.
    AllocCallback(aOldPtr, gMallocTable.malloc_usable_size(aOldPtr), t);
  }
  return ptr;
}

static void* replace_memalign(size_t aAlignment, size_t aSize) {
  using namespace mozilla::dmd;

  Thread* t = Thread::Fetch();
  if (t->InterceptsAreBlocked()) {
    return InfallibleAllocPolicy::memalign_(aAlignment, aSize);
  }

  void* ptr = gMallocTable.memalign(aAlignment, aSize);
  AllocCallback(ptr, aSize, t);
  return ptr;
}

static void replace_free(void* aPtr) {
  using namespace mozilla::dmd;

  Thread* t = Thread::Fetch();
  if (t->InterceptsAreBlocked()) {
    return InfallibleAllocPolicy::free_(aPtr);
  }

  // Do the actual free after updating the table.  Otherwise, another thread
  // could call malloc and get the freed block and update the table, and then
  // our update here would remove the newly-malloc'd block.
  DeadBlock db;
  FreeCallback(aPtr, t, &db);
  MaybeAddToDeadBlockTable(db);
  gMallocTable.free(aPtr);
}

void replace_init(malloc_table_t* aMallocTable, ReplaceMallocBridge** aBridge) {
  if (mozilla::dmd::Init(aMallocTable)) {
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC_BASE
#define MALLOC_DECL(name, ...) aMallocTable->name = replace_##name;
#include "malloc_decls.h"
    *aBridge = mozilla::dmd::gDMDBridge;
  }
}

namespace mozilla {
namespace dmd {

//---------------------------------------------------------------------------
// Options (Part 2)
//---------------------------------------------------------------------------

// Given an |aOptionName| like "foo", succeed if |aArg| has the form "foo=blah"
// (where "blah" is non-empty) and return the pointer to "blah".  |aArg| can
// have leading space chars (but not other whitespace).
const char* Options::ValueIfMatch(const char* aArg, const char* aOptionName) {
  MOZ_ASSERT(!isspace(*aArg));  // any leading whitespace should not remain
  size_t optionLen = strlen(aOptionName);
  if (strncmp(aArg, aOptionName, optionLen) == 0 && aArg[optionLen] == '=' &&
      aArg[optionLen + 1]) {
    return aArg + optionLen + 1;
  }
  return nullptr;
}

// Extracts a |long| value for an option from an argument.  It must be within
// the range |aMin..aMax| (inclusive).
bool Options::GetLong(const char* aArg, const char* aOptionName, long aMin,
                      long aMax, long* aValue) {
  if (const char* optionValue = ValueIfMatch(aArg, aOptionName)) {
    char* endPtr;
    *aValue = strtol(optionValue, &endPtr, /* base */ 10);
    if (!*endPtr && aMin <= *aValue && *aValue <= aMax && *aValue != LONG_MIN &&
        *aValue != LONG_MAX) {
      return true;
    }
  }
  return false;
}

// Extracts a |bool| value for an option -- encoded as "yes" or "no" -- from an
// argument.
bool Options::GetBool(const char* aArg, const char* aOptionName, bool* aValue) {
  if (const char* optionValue = ValueIfMatch(aArg, aOptionName)) {
    if (strcmp(optionValue, "yes") == 0) {
      *aValue = true;
      return true;
    }
    if (strcmp(optionValue, "no") == 0) {
      *aValue = false;
      return true;
    }
  }
  return false;
}

Options::Options(const char* aDMDEnvVar)
    : mDMDEnvVar(aDMDEnvVar ? InfallibleAllocPolicy::strdup_(aDMDEnvVar)
                            : nullptr),
      mMode(Mode::DarkMatter),
      mStacks(Stacks::Partial),
      mShowDumpStats(false) {
  char* e = mDMDEnvVar;
  if (e && strcmp(e, "1") != 0) {
    bool isEnd = false;
    while (!isEnd) {
      // Consume leading whitespace.
      while (isspace(*e)) {
        e++;
      }

      // Save the start of the arg.
      const char* arg = e;

      // Find the first char after the arg, and temporarily change it to '\0'
      // to isolate the arg.
      while (!isspace(*e) && *e != '\0') {
        e++;
      }
      char replacedChar = *e;
      isEnd = replacedChar == '\0';
      *e = '\0';

      // Handle arg
      bool myBool;
      if (strcmp(arg, "--mode=live") == 0) {
        mMode = Mode::Live;
      } else if (strcmp(arg, "--mode=dark-matter") == 0) {
        mMode = Mode::DarkMatter;
      } else if (strcmp(arg, "--mode=cumulative") == 0) {
        mMode = Mode::Cumulative;
      } else if (strcmp(arg, "--mode=scan") == 0) {
        mMode = Mode::Scan;

      } else if (strcmp(arg, "--stacks=full") == 0) {
        mStacks = Stacks::Full;
      } else if (strcmp(arg, "--stacks=partial") == 0) {
        mStacks = Stacks::Partial;

      } else if (GetBool(arg, "--show-dump-stats", &myBool)) {
        mShowDumpStats = myBool;

      } else if (strcmp(arg, "") == 0) {
        // This can only happen if there is trailing whitespace.  Ignore.
        MOZ_ASSERT(isEnd);

      } else {
        BadArg(arg);
      }

      // Undo the temporary isolation.
      *e = replacedChar;
    }
  }

  if (mMode == Mode::Scan) {
    mStacks = Stacks::Full;
  }
}

void Options::BadArg(const char* aArg) {
  StatusMsg("\n");
  StatusMsg("Bad entry in the $DMD environment variable: '%s'.\n", aArg);
  StatusMsg("See the output of |mach help run| for the allowed options.\n");
  exit(1);
}

const char* Options::ModeString() const {
  switch (mMode) {
    case Mode::Live:
      return "live";
    case Mode::DarkMatter:
      return "dark-matter";
    case Mode::Cumulative:
      return "cumulative";
    case Mode::Scan:
      return "scan";
    default:
      MOZ_ASSERT(false);
      return "(unknown DMD mode)";
  }
}

//---------------------------------------------------------------------------
// DMD start-up
//---------------------------------------------------------------------------

#ifndef XP_WIN
static void prefork() {
  if (gStateLock) {
    gStateLock->Lock();
  }
}

static void postfork() {
  if (gStateLock) {
    gStateLock->Unlock();
  }
}
#endif

// WARNING: this function runs *very* early -- before all static initializers
// have run.  For this reason, non-scalar globals such as gStateLock and
// gStackTraceTable are allocated dynamically (so we can guarantee their
// construction in this function) rather than statically.
static bool Init(malloc_table_t* aMallocTable) {
  // DMD is controlled by the |DMD| environment variable.
  const char* e = getenv("DMD");

  if (!e) {
    return false;
  }
  // Initialize the function table first, because StatusMsg uses
  // InfallibleAllocPolicy::malloc_, which uses it.
  gMallocTable = *aMallocTable;

  StatusMsg("$DMD = '%s'\n", e);

  gDMDBridge = InfallibleAllocPolicy::new_<DMDBridge>();

#ifndef XP_WIN
  // Avoid deadlocks when forking by acquiring our state lock prior to forking
  // and releasing it after forking. See |LogAlloc|'s |replace_init| for
  // in-depth details.
  //
  // Note: This must run after attempting an allocation so as to give the
  // system malloc a chance to insert its own atfork handler.
  pthread_atfork(prefork, postfork, postfork);
#endif
  // Parse $DMD env var.
  gOptions = InfallibleAllocPolicy::new_<Options>(e);

  gStateLock = InfallibleAllocPolicy::new_<Mutex>();

  gBernoulli = (FastBernoulliTrial*)InfallibleAllocPolicy::malloc_(
      sizeof(FastBernoulliTrial));
  ResetBernoulli();

  Thread::Init();

  {
    AutoLockState lock;

    gStackTraceTable = InfallibleAllocPolicy::new_<StackTraceTable>(8192);
    gLiveBlockTable = InfallibleAllocPolicy::new_<LiveBlockTable>(8192);

    // Create this even if the mode isn't Cumulative (albeit with a small
    // size), in case the mode is changed later on (as is done by SmokeDMD.cpp,
    // for example).
    size_t tableSize = gOptions->IsCumulativeMode() ? 8192 : 4;
    gDeadBlockTable = InfallibleAllocPolicy::new_<DeadBlockTable>(tableSize);
  }

  return true;
}

//---------------------------------------------------------------------------
// Block reporting and unreporting
//---------------------------------------------------------------------------

static void ReportHelper(const void* aPtr, bool aReportedOnAlloc) {
  if (!gOptions->IsDarkMatterMode() || !aPtr) {
    return;
  }

  Thread* t = Thread::Fetch();

  AutoBlockIntercepts block(t);
  AutoLockState lock;

  if (LiveBlockTable::Ptr p = gLiveBlockTable->lookup(aPtr)) {
    p->Report(t, aReportedOnAlloc);
  } else {
    // We have no record of the block. It must be a bogus pointer. This should
    // be extremely rare because Report() is almost always called in
    // conjunction with a malloc_size_of-style function. Print a message so
    // that we get some feedback.
    StatusMsg("Unknown pointer %p\n", aPtr);
  }
}

void DMDFuncs::Report(const void* aPtr) {
  ReportHelper(aPtr, /* onAlloc */ false);
}

void DMDFuncs::ReportOnAlloc(const void* aPtr) {
  ReportHelper(aPtr, /* onAlloc */ true);
}

//---------------------------------------------------------------------------
// DMD output
//---------------------------------------------------------------------------

// The version number of the output format. Increment this if you make
// backwards-incompatible changes to the format. See DMD.h for the version
// history.
static const int kOutputVersionNumber = 5;

// Note that, unlike most SizeOf* functions, this function does not take a
// |mozilla::MallocSizeOf| argument.  That's because those arguments are
// primarily to aid DMD track heap blocks... but DMD deliberately doesn't track
// heap blocks it allocated for itself!
//
// SizeOfInternal should be called while you're holding the state lock and
// while intercepts are blocked; SizeOf acquires the lock and blocks
// intercepts.

static void SizeOfInternal(Sizes* aSizes) {
  MOZ_ASSERT(gStateLock->IsLocked());
  MOZ_ASSERT(Thread::Fetch()->InterceptsAreBlocked());

  aSizes->Clear();

  StackTraceSet usedStackTraces;
  GatherUsedStackTraces(usedStackTraces);

  for (auto iter = gStackTraceTable->iter(); !iter.done(); iter.next()) {
    StackTrace* const& st = iter.get();

    if (usedStackTraces.has(st)) {
      aSizes->mStackTracesUsed += MallocSizeOf(st);
    } else {
      aSizes->mStackTracesUnused += MallocSizeOf(st);
    }
  }

  aSizes->mStackTraceTable =
      gStackTraceTable->shallowSizeOfIncludingThis(MallocSizeOf);

  aSizes->mLiveBlockTable =
      gLiveBlockTable->shallowSizeOfIncludingThis(MallocSizeOf);

  aSizes->mDeadBlockTable =
      gDeadBlockTable->shallowSizeOfIncludingThis(MallocSizeOf);
}

void DMDFuncs::SizeOf(Sizes* aSizes) {
  aSizes->Clear();

  AutoBlockIntercepts block(Thread::Fetch());
  AutoLockState lock;
  SizeOfInternal(aSizes);
}

void DMDFuncs::ClearReports() {
  if (!gOptions->IsDarkMatterMode()) {
    return;
  }

  AutoLockState lock;

  // Unreport all blocks that were marked reported by a memory reporter.  This
  // excludes those that were reported on allocation, because they need to keep
  // their reported marking.
  for (auto iter = gLiveBlockTable->iter(); !iter.done(); iter.next()) {
    iter.get().UnreportIfNotReportedOnAlloc();
  }
}

class ToIdStringConverter final {
 public:
  ToIdStringConverter() : mIdMap(512), mNextId(0) {}

  // Converts a pointer to a unique ID. Reuses the existing ID for the pointer
  // if it's been seen before.
  const char* ToIdString(const void* aPtr) {
    uint32_t id;
    PointerIdMap::AddPtr p = mIdMap.lookupForAdd(aPtr);
    if (!p) {
      id = mNextId++;
      MOZ_ALWAYS_TRUE(mIdMap.add(p, aPtr, id));
    } else {
      id = p->value();
    }
    return Base32(id);
  }

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return mIdMap.shallowSizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  // This function converts an integer to base-32. We use base-32 values for
  // indexing into the traceTable and the frameTable, for the following reasons.
  //
  // - Base-32 gives more compact indices than base-16.
  //
  // - 32 is a power-of-two, which makes the necessary div/mod calculations
  //   fast.
  //
  // - We can (and do) choose non-numeric digits for base-32. When
  //   inspecting/debugging the JSON output, non-numeric indices are easier to
  //   search for than numeric indices.
  //
  char* Base32(uint32_t aN) {
    static const char digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef";

    char* b = mIdBuf + kIdBufLen - 1;
    *b = '\0';
    do {
      b--;
      if (b == mIdBuf) {
        MOZ_CRASH("Base32 buffer too small");
      }
      *b = digits[aN % 32];
      aN /= 32;
    } while (aN);

    return b;
  }

  PointerIdMap mIdMap;
  uint32_t mNextId;

  // |mIdBuf| must have space for at least eight chars, which is the space
  // needed to hold 'Dffffff' (including the terminating null char), which is
  // the base-32 representation of 0xffffffff.
  static const size_t kIdBufLen = 16;
  char mIdBuf[kIdBufLen];
};

// Helper class for converting a pointer value to a string.
class ToStringConverter {
 public:
  const char* ToPtrString(const void* aPtr) {
    snprintf(kPtrBuf, sizeof(kPtrBuf) - 1, "%" PRIxPTR, (uintptr_t)aPtr);
    return kPtrBuf;
  }

 private:
  char kPtrBuf[32];
};

static void WriteBlockContents(JSONWriter& aWriter, const LiveBlock& aBlock) {
  size_t numWords = aBlock.ReqSize() / sizeof(uintptr_t*);
  if (numWords == 0) {
    return;
  }

  aWriter.StartArrayProperty("contents", aWriter.SingleLineStyle);
  {
    const uintptr_t** block = (const uintptr_t**)aBlock.Address();
    ToStringConverter sc;
    for (size_t i = 0; i < numWords; ++i) {
      aWriter.StringElement(MakeStringSpan(sc.ToPtrString(block[i])));
    }
  }
  aWriter.EndArray();
}

static void AnalyzeImpl(UniquePtr<JSONWriteFunc> aWriter) {
  // Some blocks may have been allocated while creating |aWriter|. Those blocks
  // will be freed at the end of this function when |write| is destroyed. The
  // allocations will have occurred while intercepts were not blocked, so the
  // frees better be as well, otherwise we'll get assertion failures.
  // Therefore, this declaration must precede the AutoBlockIntercepts
  // declaration, to ensure that |write| is destroyed *after* intercepts are
  // unblocked.
  JSONWriter writer(std::move(aWriter));

  AutoBlockIntercepts block(Thread::Fetch());
  AutoLockState lock;

  // Allocate this on the heap instead of the stack because it's fairly large.
  auto locService = InfallibleAllocPolicy::new_<CodeAddressService>();

  StackTraceSet usedStackTraces(512);
  PointerSet usedPcs(512);

  size_t iscSize;

  static int analysisCount = 1;
  StatusMsg("Dump %d {\n", analysisCount++);

  writer.Start();
  {
    writer.IntProperty("version", kOutputVersionNumber);

    writer.StartObjectProperty("invocation");
    {
      const char* var = gOptions->DMDEnvVar();
      if (var) {
        writer.StringProperty("dmdEnvVar", MakeStringSpan(var));
      } else {
        writer.NullProperty("dmdEnvVar");
      }

      writer.StringProperty("mode", MakeStringSpan(gOptions->ModeString()));
    }
    writer.EndObject();

    StatusMsg("  Constructing the heap block list...\n");

    ToIdStringConverter isc;
    ToStringConverter sc;

    writer.StartArrayProperty("blockList");
    {
      // Lambda that writes out a live block.
      auto writeLiveBlock = [&](const LiveBlock& aB, size_t aNum) {
        aB.AddStackTracesToTable(usedStackTraces);

        MOZ_ASSERT_IF(gOptions->IsScanMode(), aNum == 1);

        writer.StartObjectElement(writer.SingleLineStyle);
        {
          if (gOptions->IsScanMode()) {
            writer.StringProperty("addr",
                                  MakeStringSpan(sc.ToPtrString(aB.Address())));
            WriteBlockContents(writer, aB);
          }
          writer.IntProperty("req", aB.ReqSize());
          if (aB.SlopSize() > 0) {
            writer.IntProperty("slop", aB.SlopSize());
          }

          if (aB.AllocStackTrace()) {
            writer.StringProperty(
                "alloc", MakeStringSpan(isc.ToIdString(aB.AllocStackTrace())));
          }

          if (gOptions->IsDarkMatterMode() && aB.NumReports() > 0) {
            writer.StartArrayProperty("reps");
            {
              if (aB.ReportStackTrace1()) {
                writer.StringElement(
                    MakeStringSpan(isc.ToIdString(aB.ReportStackTrace1())));
              }
              if (aB.ReportStackTrace2()) {
                writer.StringElement(
                    MakeStringSpan(isc.ToIdString(aB.ReportStackTrace2())));
              }
            }
            writer.EndArray();
          }

          if (aNum > 1) {
            writer.IntProperty("num", aNum);
          }
        }
        writer.EndObject();
      };

      // Live blocks.
      if (!gOptions->IsScanMode()) {
        // At this point we typically have many LiveBlocks that differ only in
        // their address. Aggregate them to reduce the size of the output file.
        AggregatedLiveBlockTable agg(8192);
        for (auto iter = gLiveBlockTable->iter(); !iter.done(); iter.next()) {
          const LiveBlock& b = iter.get();
          b.AddStackTracesToTable(usedStackTraces);

          if (AggregatedLiveBlockTable::AddPtr p = agg.lookupForAdd(&b)) {
            p->value() += 1;
          } else {
            MOZ_ALWAYS_TRUE(agg.add(p, &b, 1));
          }
        }

        // Now iterate over the aggregated table.
        for (auto iter = agg.iter(); !iter.done(); iter.next()) {
          const LiveBlock& b = *iter.get().key();
          size_t num = iter.get().value();
          writeLiveBlock(b, num);
        }

      } else {
        // In scan mode we cannot aggregate because we print each live block's
        // address and contents.
        for (auto iter = gLiveBlockTable->iter(); !iter.done(); iter.next()) {
          const LiveBlock& b = iter.get();
          b.AddStackTracesToTable(usedStackTraces);

          writeLiveBlock(b, 1);
        }
      }

      // Dead blocks.
      for (auto iter = gDeadBlockTable->iter(); !iter.done(); iter.next()) {
        const DeadBlock& b = iter.get().key();
        b.AddStackTracesToTable(usedStackTraces);

        size_t num = iter.get().value();
        MOZ_ASSERT(num > 0);

        writer.StartObjectElement(writer.SingleLineStyle);
        {
          writer.IntProperty("req", b.ReqSize());
          if (b.SlopSize() > 0) {
            writer.IntProperty("slop", b.SlopSize());
          }
          if (b.AllocStackTrace()) {
            writer.StringProperty(
                "alloc", MakeStringSpan(isc.ToIdString(b.AllocStackTrace())));
          }

          if (num > 1) {
            writer.IntProperty("num", num);
          }
        }
        writer.EndObject();
      }
    }
    writer.EndArray();

    StatusMsg("  Constructing the stack trace table...\n");

    writer.StartObjectProperty("traceTable");
    {
      for (auto iter = usedStackTraces.iter(); !iter.done(); iter.next()) {
        const StackTrace* const st = iter.get();
        writer.StartArrayProperty(MakeStringSpan(isc.ToIdString(st)),
                                  writer.SingleLineStyle);
        {
          for (uint32_t i = 0; i < st->Length(); i++) {
            const void* pc = st->Pc(i);
            writer.StringElement(MakeStringSpan(isc.ToIdString(pc)));
            MOZ_ALWAYS_TRUE(usedPcs.put(pc));
          }
        }
        writer.EndArray();
      }
    }
    writer.EndObject();

    StatusMsg("  Constructing the stack frame table...\n");

    writer.StartObjectProperty("frameTable");
    {
      static const size_t locBufLen = 1024;
      char locBuf[locBufLen];

      for (auto iter = usedPcs.iter(); !iter.done(); iter.next()) {
        const void* const pc = iter.get();

        // Use 0 for the frame number. See the JSON format description comment
        // in DMD.h to understand why.
        locService->GetLocation(0, pc, locBuf, locBufLen);
        writer.StringProperty(MakeStringSpan(isc.ToIdString(pc)),
                              MakeStringSpan(locBuf));
      }
    }
    writer.EndObject();

    iscSize = isc.sizeOfExcludingThis(MallocSizeOf);
  }
  writer.End();

  if (gOptions->ShowDumpStats()) {
    Sizes sizes;
    SizeOfInternal(&sizes);

    static const size_t kBufLen = 64;
    char buf1[kBufLen];
    char buf2[kBufLen];
    char buf3[kBufLen];

    StatusMsg("  Execution measurements {\n");

    StatusMsg("    Data structures that persist after Dump() ends {\n");

    StatusMsg("      Used stack traces:    %10s bytes\n",
              Show(sizes.mStackTracesUsed, buf1, kBufLen));

    StatusMsg("      Unused stack traces:  %10s bytes\n",
              Show(sizes.mStackTracesUnused, buf1, kBufLen));

    StatusMsg("      Stack trace table:    %10s bytes (%s entries, %s used)\n",
              Show(sizes.mStackTraceTable, buf1, kBufLen),
              Show(gStackTraceTable->capacity(), buf2, kBufLen),
              Show(gStackTraceTable->count(), buf3, kBufLen));

    StatusMsg("      Live block table:     %10s bytes (%s entries, %s used)\n",
              Show(sizes.mLiveBlockTable, buf1, kBufLen),
              Show(gLiveBlockTable->capacity(), buf2, kBufLen),
              Show(gLiveBlockTable->count(), buf3, kBufLen));

    StatusMsg("      Dead block table:     %10s bytes (%s entries, %s used)\n",
              Show(sizes.mDeadBlockTable, buf1, kBufLen),
              Show(gDeadBlockTable->capacity(), buf2, kBufLen),
              Show(gDeadBlockTable->count(), buf3, kBufLen));

    StatusMsg("    }\n");
    StatusMsg("    Data structures that are destroyed after Dump() ends {\n");

    StatusMsg(
        "      Location service:      %10s bytes\n",
        Show(locService->SizeOfIncludingThis(MallocSizeOf), buf1, kBufLen));
    StatusMsg("      Used stack traces set: %10s bytes\n",
              Show(usedStackTraces.shallowSizeOfExcludingThis(MallocSizeOf),
                   buf1, kBufLen));
    StatusMsg(
        "      Used PCs set:          %10s bytes\n",
        Show(usedPcs.shallowSizeOfExcludingThis(MallocSizeOf), buf1, kBufLen));
    StatusMsg("      Pointer ID map:        %10s bytes\n",
              Show(iscSize, buf1, kBufLen));

    StatusMsg("    }\n");
    StatusMsg("    Counts {\n");

    size_t hits = locService->NumCacheHits();
    size_t misses = locService->NumCacheMisses();
    size_t requests = hits + misses;
    StatusMsg("      Location service:    %10s requests\n",
              Show(requests, buf1, kBufLen));

    size_t count = locService->CacheCount();
    size_t capacity = locService->CacheCapacity();
    StatusMsg(
        "      Location service cache:  "
        "%4.1f%% hit rate, %.1f%% occupancy at end\n",
        Percent(hits, requests), Percent(count, capacity));

    StatusMsg("    }\n");
    StatusMsg("  }\n");
  }

  InfallibleAllocPolicy::delete_(locService);

  StatusMsg("}\n");
}

void DMDFuncs::Analyze(UniquePtr<JSONWriteFunc> aWriter) {
  AnalyzeImpl(std::move(aWriter));
  ClearReports();
}

//---------------------------------------------------------------------------
// Testing
//---------------------------------------------------------------------------

void DMDFuncs::ResetEverything(const char* aOptions) {
  AutoLockState lock;

  // Reset options.
  InfallibleAllocPolicy::delete_(gOptions);
  gOptions = InfallibleAllocPolicy::new_<Options>(aOptions);

  // Clear all existing blocks.
  gLiveBlockTable->clear();
  gDeadBlockTable->clear();

  // Reset gBernoulli to a deterministic state. (Its current state depends on
  // all previous trials.)
  ResetBernoulli();
}

}  // namespace dmd
}  // namespace mozilla
