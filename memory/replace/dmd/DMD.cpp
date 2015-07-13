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

#ifdef XP_WIN
#if defined(MOZ_OPTIMIZE) && !defined(MOZ_PROFILING)
#error "Optimized, DMD-enabled builds on Windows must be built with --enable-profiling"
#endif
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#ifdef ANDROID
#include <android/log.h>
#endif

#include "nscore.h"
#include "mozilla/StackWalk.h"

#include "js/HashTable.h"
#include "js/Vector.h"

#include "mozilla/Assertions.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/JSONWriter.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"

// CodeAddressService is defined entirely in the header, so this does not make
// DMD depend on XPCOM's object file.
#include "CodeAddressService.h"

// replace_malloc.h needs to be included before replace_malloc_bridge.h,
// which DMD.h includes, so DMD.h needs to be included after replace_malloc.h.
// MOZ_REPLACE_ONLY_MEMALIGN saves us from having to define
// replace_{posix_memalign,aligned_alloc,valloc}.  It requires defining
// PAGE_SIZE.  Nb: sysconf() is expensive, but it's only used for (the obsolete
// and rarely used) valloc.
#define MOZ_REPLACE_ONLY_MEMALIGN 1

#ifndef PAGE_SIZE
#define DMD_DEFINED_PAGE_SIZE
#ifdef XP_WIN
#define PAGE_SIZE GetPageSize()
static long GetPageSize()
{
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwPageSize;
}
#else // XP_WIN
#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#endif // XP_WIN
#endif // PAGE_SIZE
#include "replace_malloc.h"
#undef MOZ_REPLACE_ONLY_MEMALIGN
#ifdef DMD_DEFINED_PAGE_SIZE
#undef DMD_DEFINED_PAGE_SIZE
#undef PAGE_SIZE
#endif // DMD_DEFINED_PAGE_SIZE

#include "DMD.h"

namespace mozilla {
namespace dmd {

class DMDBridge : public ReplaceMallocBridge
{
  virtual DMDFuncs* GetDMDFuncs() override;
};

static DMDBridge* gDMDBridge;
static DMDFuncs gDMDFuncs;

DMDFuncs*
DMDBridge::GetDMDFuncs()
{
  return &gDMDFuncs;
}

inline void
StatusMsg(const char* aFmt, ...)
{
  va_list ap;
  va_start(ap, aFmt);
  gDMDFuncs.StatusMsg(aFmt, ap);
  va_end(ap);
}

//---------------------------------------------------------------------------
// Utilities
//---------------------------------------------------------------------------

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(T) \
  T(const T&);                      \
  void operator=(const T&)
#endif

static const malloc_table_t* gMallocTable = nullptr;

// Whether DMD finished initializing.
static bool gIsDMDInitialized = false;

// This provides infallible allocations (they abort on OOM).  We use it for all
// of DMD's own allocations, which fall into the following three cases.
//
// - Direct allocations (the easy case).
//
// - Indirect allocations in js::{Vector,HashSet,HashMap} -- this class serves
//   as their AllocPolicy.
//
// - Other indirect allocations (e.g. MozStackWalk) -- see the comments on
//   Thread::mBlockIntercepts and in replace_malloc for how these work.
//
// It would be nice if we could use the InfallibleAllocPolicy from mozalloc,
// but DMD cannot use mozalloc.
//
class InfallibleAllocPolicy
{
  static void ExitOnFailure(const void* aP);

public:
  static void* malloc_(size_t aSize)
  {
    void* p = gMallocTable->malloc(aSize);
    ExitOnFailure(p);
    return p;
  }

  template <typename T>
  static T* pod_malloc(size_t aNumElems)
  {
    if (aNumElems & mozilla::tl::MulOverflowMask<sizeof(T)>::value)
      return nullptr;
    void* p = gMallocTable->malloc(aNumElems * sizeof(T));
    ExitOnFailure(p);
    return (T*)p;
  }

  static void* calloc_(size_t aSize)
  {
    void* p = gMallocTable->calloc(1, aSize);
    ExitOnFailure(p);
    return p;
  }

  template <typename T>
  static T* pod_calloc(size_t aNumElems)
  {
    void* p = gMallocTable->calloc(aNumElems, sizeof(T));
    ExitOnFailure(p);
    return (T*)p;
  }

  // This realloc_ is the one we use for direct reallocs within DMD.
  static void* realloc_(void* aPtr, size_t aNewSize)
  {
    void* p = gMallocTable->realloc(aPtr, aNewSize);
    ExitOnFailure(p);
    return p;
  }

  // This realloc_ is required for this to be a JS container AllocPolicy.
  template <typename T>
  static T* pod_realloc(T* aPtr, size_t aOldSize, size_t aNewSize)
  {
    if (aNewSize & mozilla::tl::MulOverflowMask<sizeof(T)>::value)
      return nullptr;
    return (T*)InfallibleAllocPolicy::realloc_((void *)aPtr, aNewSize * sizeof(T));
  }

  static void* memalign_(size_t aAlignment, size_t aSize)
  {
    void* p = gMallocTable->memalign(aAlignment, aSize);
    ExitOnFailure(p);
    return p;
  }

  static void free_(void* aPtr) { gMallocTable->free(aPtr); }

  static char* strdup_(const char* aStr)
  {
    char* s = (char*) InfallibleAllocPolicy::malloc_(strlen(aStr) + 1);
    strcpy(s, aStr);
    return s;
  }

  template <class T>
  static T* new_()
  {
    void* mem = malloc_(sizeof(T));
    ExitOnFailure(mem);
    return new (mem) T;
  }

  template <class T, typename P1>
  static T* new_(P1 p1)
  {
    void* mem = malloc_(sizeof(T));
    ExitOnFailure(mem);
    return new (mem) T(p1);
  }

  template <class T>
  static void delete_(T *p)
  {
    if (p) {
      p->~T();
      InfallibleAllocPolicy::free_(p);
    }
  }

  static void reportAllocOverflow() { ExitOnFailure(nullptr); }
};

// This is only needed because of the |const void*| vs |void*| arg mismatch.
static size_t
MallocSizeOf(const void* aPtr)
{
  return gMallocTable->malloc_usable_size(const_cast<void*>(aPtr));
}

void
DMDFuncs::StatusMsg(const char* aFmt, va_list aAp)
{
#ifdef ANDROID
#ifdef MOZ_B2G_LOADER
  // Don't call __android_log_vprint() during initialization, or the magic file
  // descriptors will be occupied by android logcat.
  if (gIsDMDInitialized)
#endif
    __android_log_vprint(ANDROID_LOG_INFO, "DMD", aFmt, aAp);
#else
  // The +64 is easily enough for the "DMD[<pid>] " prefix and the NUL.
  char* fmt = (char*) InfallibleAllocPolicy::malloc_(strlen(aFmt) + 64);
  sprintf(fmt, "DMD[%d] %s", getpid(), aFmt);
  vfprintf(stderr, fmt, aAp);
  InfallibleAllocPolicy::free_(fmt);
#endif
}

/* static */ void
InfallibleAllocPolicy::ExitOnFailure(const void* aP)
{
  if (!aP) {
    MOZ_CRASH("DMD out of memory; aborting");
  }
}

static double
Percent(size_t part, size_t whole)
{
  return (whole == 0) ? 0 : 100 * (double)part / whole;
}

// Commifies the number.
static char*
Show(size_t n, char* buf, size_t buflen)
{
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

class Options
{
  template <typename T>
  struct NumOption
  {
    const T mDefault;
    const T mMax;
    T       mActual;
    NumOption(T aDefault, T aMax)
      : mDefault(aDefault), mMax(aMax), mActual(aDefault)
    {}
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
  enum Mode
  {
    // For each live block, this mode outputs: size (usable and slop),
    // allocation stack, and whether it's sampled. This mode is good for live
    // heap profiling.
    Live,

    // Like "Live", but for each live block it also outputs: zero or more
    // report stacks. This mode is good for identifying where memory reporters
    // should be added. This is the default mode.
    DarkMatter,

    // Like "Live", but also outputs the same data for dead blocks. This mode
    // does cumulative heap profiling, which is good for identifying where large
    // amounts of short-lived allocations occur.
    Cumulative,

    // Like "Live", but this mode also outputs for each live block the address
    // of the block and the values contained in the blocks. This mode is useful
    // for investigating leaks, by helping to figure out which blocks refer to
    // other blocks. This mode disables sampling.
    Scan
  };

  char* mDMDEnvVar;   // a saved copy, for later printing

  Mode mMode;
  NumOption<size_t> mSampleBelowSize;
  NumOption<uint32_t> mMaxFrames;
  bool mShowDumpStats;

  void BadArg(const char* aArg);
  static const char* ValueIfMatch(const char* aArg, const char* aOptionName);
  static bool GetLong(const char* aArg, const char* aOptionName,
                      long aMin, long aMax, long* aValue);
  static bool GetBool(const char* aArg, const char* aOptionName, bool* aValue);

public:
  explicit Options(const char* aDMDEnvVar);

  bool IsLiveMode()       const { return mMode == Live; }
  bool IsDarkMatterMode() const { return mMode == DarkMatter; }
  bool IsCumulativeMode() const { return mMode == Cumulative; }
  bool IsScanMode()       const { return mMode == Scan; }

  const char* ModeString() const;

  const char* DMDEnvVar() const { return mDMDEnvVar; }

  size_t SampleBelowSize() const { return mSampleBelowSize.mActual; }
  size_t MaxFrames()       const { return mMaxFrames.mActual; }
  size_t ShowDumpStats()   const { return mShowDumpStats; }
};

static Options *gOptions;

//---------------------------------------------------------------------------
// The global lock
//---------------------------------------------------------------------------

// MutexBase implements the platform-specific parts of a mutex.

#ifdef XP_WIN

class MutexBase
{
  CRITICAL_SECTION mCS;

  DISALLOW_COPY_AND_ASSIGN(MutexBase);

public:
  MutexBase()  { InitializeCriticalSection(&mCS); }
  ~MutexBase() { DeleteCriticalSection(&mCS); }

  void Lock()   { EnterCriticalSection(&mCS); }
  void Unlock() { LeaveCriticalSection(&mCS); }
};

#else

#include <pthread.h>
#include <sys/types.h>

class MutexBase
{
  pthread_mutex_t mMutex;

  DISALLOW_COPY_AND_ASSIGN(MutexBase);

public:
  MutexBase() { pthread_mutex_init(&mMutex, nullptr); }

  void Lock()   { pthread_mutex_lock(&mMutex); }
  void Unlock() { pthread_mutex_unlock(&mMutex); }
};

#endif

class Mutex : private MutexBase
{
  bool mIsLocked;

  DISALLOW_COPY_AND_ASSIGN(Mutex);

public:
  Mutex()
    : mIsLocked(false)
  {}

  void Lock()
  {
    MutexBase::Lock();
    MOZ_ASSERT(!mIsLocked);
    mIsLocked = true;
  }

  void Unlock()
  {
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

class AutoLockState
{
  DISALLOW_COPY_AND_ASSIGN(AutoLockState);

public:
  AutoLockState()  { gStateLock->Lock(); }
  ~AutoLockState() { gStateLock->Unlock(); }
};

class AutoUnlockState
{
  DISALLOW_COPY_AND_ASSIGN(AutoUnlockState);

public:
  AutoUnlockState()  { gStateLock->Unlock(); }
  ~AutoUnlockState() { gStateLock->Lock(); }
};

//---------------------------------------------------------------------------
// Thread-local storage and blocking of intercepts
//---------------------------------------------------------------------------

#ifdef XP_WIN

#define DMD_TLS_INDEX_TYPE              DWORD
#define DMD_CREATE_TLS_INDEX(i_)        do {                                  \
                                          (i_) = TlsAlloc();                  \
                                        } while (0)
#define DMD_DESTROY_TLS_INDEX(i_)       TlsFree((i_))
#define DMD_GET_TLS_DATA(i_)            TlsGetValue((i_))
#define DMD_SET_TLS_DATA(i_, v_)        TlsSetValue((i_), (v_))

#else

#include <pthread.h>

#define DMD_TLS_INDEX_TYPE               pthread_key_t
#define DMD_CREATE_TLS_INDEX(i_)         pthread_key_create(&(i_), nullptr)
#define DMD_DESTROY_TLS_INDEX(i_)        pthread_key_delete((i_))
#define DMD_GET_TLS_DATA(i_)             pthread_getspecific((i_))
#define DMD_SET_TLS_DATA(i_, v_)         pthread_setspecific((i_), (v_))

#endif

static DMD_TLS_INDEX_TYPE gTlsIndex;

class Thread
{
  // Required for allocation via InfallibleAllocPolicy::new_.
  friend class InfallibleAllocPolicy;

  // When true, this blocks intercepts, which allows malloc interception
  // functions to themselves call malloc.  (Nb: for direct calls to malloc we
  // can just use InfallibleAllocPolicy::{malloc_,new_}, but we sometimes
  // indirectly call vanilla malloc via functions like MozStackWalk.)
  bool mBlockIntercepts;

  Thread()
    : mBlockIntercepts(false)
  {}

  DISALLOW_COPY_AND_ASSIGN(Thread);

public:
  static Thread* Fetch();

  bool BlockIntercepts()
  {
    MOZ_ASSERT(!mBlockIntercepts);
    return mBlockIntercepts = true;
  }

  bool UnblockIntercepts()
  {
    MOZ_ASSERT(mBlockIntercepts);
    return mBlockIntercepts = false;
  }

  bool InterceptsAreBlocked() const { return mBlockIntercepts; }
};

/* static */ Thread*
Thread::Fetch()
{
  Thread* t = static_cast<Thread*>(DMD_GET_TLS_DATA(gTlsIndex));

  if (MOZ_UNLIKELY(!t)) {
    // This memory is never freed, even if the thread dies.  It's a leak, but
    // only a tiny one.
    t = InfallibleAllocPolicy::new_<Thread>();
    DMD_SET_TLS_DATA(gTlsIndex, t);
  }

  return t;
}

// An object of this class must be created (on the stack) before running any
// code that might allocate.
class AutoBlockIntercepts
{
  Thread* const mT;

  DISALLOW_COPY_AND_ASSIGN(AutoBlockIntercepts);

public:
  explicit AutoBlockIntercepts(Thread* aT)
    : mT(aT)
  {
    mT->BlockIntercepts();
  }
  ~AutoBlockIntercepts()
  {
    MOZ_ASSERT(mT->InterceptsAreBlocked());
    mT->UnblockIntercepts();
  }
};

//---------------------------------------------------------------------------
// Location service
//---------------------------------------------------------------------------

class StringTable
{
public:
  StringTable() { (void)mSet.init(64); }

  const char*
  Intern(const char* aString)
  {
    StringHashSet::AddPtr p = mSet.lookupForAdd(aString);
    if (p) {
      return *p;
    }

    const char* newString = InfallibleAllocPolicy::strdup_(aString);
    (void)mSet.add(p, newString);
    return newString;
  }

  size_t
  SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    size_t n = 0;
    n += mSet.sizeOfExcludingThis(aMallocSizeOf);
    for (auto r = mSet.all(); !r.empty(); r.popFront()) {
      n += aMallocSizeOf(r.front());
    }
    return n;
  }

private:
  struct StringHasher
  {
      typedef const char* Lookup;

      static uint32_t hash(const char* const& aS)
      {
          return HashString(aS);
      }

      static bool match(const char* const& aA, const char* const& aB)
      {
          return strcmp(aA, aB) == 0;
      }
  };

  typedef js::HashSet<const char*, StringHasher, InfallibleAllocPolicy> StringHashSet;

  StringHashSet mSet;
};

class StringAlloc
{
public:
  static char* copy(const char* aString)
  {
    return InfallibleAllocPolicy::strdup_(aString);
  }
  static void free(char* aString)
  {
    InfallibleAllocPolicy::free_(aString);
  }
};

struct DescribeCodeAddressLock
{
  static void Unlock() { gStateLock->Unlock(); }
  static void Lock() { gStateLock->Lock(); }
  static bool IsLocked() { return gStateLock->IsLocked(); }
};

typedef CodeAddressService<StringTable, StringAlloc, DescribeCodeAddressLock>
  CodeAddressService;

//---------------------------------------------------------------------------
// Stack traces
//---------------------------------------------------------------------------

class StackTrace
{
public:
  static const uint32_t MaxFrames = 24;

private:
  uint32_t mLength;             // The number of PCs.
  const void* mPcs[MaxFrames];  // The PCs themselves.  If --max-frames is less
                                // than 24, this array is bigger than
                                // necessary, but that case is unusual.

public:
  StackTrace() : mLength(0) {}

  uint32_t Length() const { return mLength; }
  const void* Pc(uint32_t i) const
  {
    MOZ_ASSERT(i < mLength);
    return mPcs[i];
  }

  uint32_t Size() const { return mLength * sizeof(mPcs[0]); }

  // The stack trace returned by this function is interned in gStackTraceTable,
  // and so is immortal and unmovable.
  static const StackTrace* Get(Thread* aT);

  // Hash policy.

  typedef StackTrace* Lookup;

  static uint32_t hash(const StackTrace* const& aSt)
  {
    return mozilla::HashBytes(aSt->mPcs, aSt->Size());
  }

  static bool match(const StackTrace* const& aA,
                    const StackTrace* const& aB)
  {
    return aA->mLength == aB->mLength &&
           memcmp(aA->mPcs, aB->mPcs, aA->Size()) == 0;
  }

private:
  static void StackWalkCallback(uint32_t aFrameNumber, void* aPc, void* aSp,
                                void* aClosure)
  {
    StackTrace* st = (StackTrace*) aClosure;
    MOZ_ASSERT(st->mLength < MaxFrames);
    st->mPcs[st->mLength] = aPc;
    st->mLength++;
    MOZ_ASSERT(st->mLength == aFrameNumber);
  }
};

typedef js::HashSet<StackTrace*, StackTrace, InfallibleAllocPolicy>
        StackTraceTable;
static StackTraceTable* gStackTraceTable = nullptr;

typedef js::HashSet<const StackTrace*, js::DefaultHasher<const StackTrace*>,
                    InfallibleAllocPolicy>
        StackTraceSet;

typedef js::HashSet<const void*, js::DefaultHasher<const void*>,
                    InfallibleAllocPolicy>
        PointerSet;
typedef js::HashMap<const void*, uint32_t, js::DefaultHasher<const void*>,
                    InfallibleAllocPolicy>
        PointerIdMap;

// We won't GC the stack trace table until it this many elements.
static uint32_t gGCStackTraceTableWhenSizeExceeds = 4 * 1024;

/* static */ const StackTrace*
StackTrace::Get(Thread* aT)
{
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
    uint32_t skipFrames = 2;
    if (MozStackWalk(StackWalkCallback, skipFrames,
                      gOptions->MaxFrames(), &tmp, 0, nullptr)) {
      // Handle the common case first.  All is ok.  Nothing to do.
    } else {
      tmp.mLength = 0;
    }
  }

  StackTraceTable::AddPtr p = gStackTraceTable->lookupForAdd(&tmp);
  if (!p) {
    StackTrace* stnew = InfallibleAllocPolicy::new_<StackTrace>(tmp);
    (void)gStackTraceTable->add(p, stnew);
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
class TaggedPtr
{
  union
  {
    T         mPtr;
    uintptr_t mUint;
  };

  static const uintptr_t kTagMask = uintptr_t(0x1);
  static const uintptr_t kPtrMask = ~kTagMask;

  static bool IsTwoByteAligned(T aPtr)
  {
    return (uintptr_t(aPtr) & kTagMask) == 0;
  }

public:
  TaggedPtr()
    : mPtr(nullptr)
  {}

  TaggedPtr(T aPtr, bool aBool)
    : mPtr(aPtr)
  {
    MOZ_ASSERT(IsTwoByteAligned(aPtr));
    uintptr_t tag = uintptr_t(aBool);
    MOZ_ASSERT(tag <= kTagMask);
    mUint |= (tag & kTagMask);
  }

  void Set(T aPtr, bool aBool)
  {
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
class LiveBlock
{
  const void*  mPtr;
  const size_t mReqSize;    // size requested

  // Ptr: |mAllocStackTrace| - stack trace where this block was allocated.
  // Tag bit 0: |mIsSampled| - was this block sampled? (if so, slop == 0).
  //
  // Only used in DarkMatter mode.
  TaggedPtr<const StackTrace* const>
    mAllocStackTrace_mIsSampled;

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
            const StackTrace* aAllocStackTrace, bool aIsSampled)
    : mPtr(aPtr)
    , mReqSize(aReqSize)
    , mAllocStackTrace_mIsSampled(aAllocStackTrace, aIsSampled)
    , mReportStackTrace_mReportedOnAlloc()     // all fields get zeroed
  {
    MOZ_ASSERT(aAllocStackTrace);
  }

  const void* Address() const { return mPtr; }

  size_t ReqSize() const { return mReqSize; }

  // Sampled blocks always have zero slop.
  size_t SlopSize() const
  {
    return IsSampled() ? 0 : MallocSizeOf(mPtr) - mReqSize;
  }

  size_t UsableSize() const
  {
    return IsSampled() ? mReqSize : MallocSizeOf(mPtr);
  }

  bool IsSampled() const
  {
    return mAllocStackTrace_mIsSampled.Tag();
  }

  const StackTrace* AllocStackTrace() const
  {
    return mAllocStackTrace_mIsSampled.Ptr();
  }

  const StackTrace* ReportStackTrace1() const
  {
    MOZ_ASSERT(gOptions->IsDarkMatterMode());
    return mReportStackTrace_mReportedOnAlloc[0].Ptr();
  }

  const StackTrace* ReportStackTrace2() const
  {
    MOZ_ASSERT(gOptions->IsDarkMatterMode());
    return mReportStackTrace_mReportedOnAlloc[1].Ptr();
  }

  bool ReportedOnAlloc1() const
  {
    MOZ_ASSERT(gOptions->IsDarkMatterMode());
    return mReportStackTrace_mReportedOnAlloc[0].Tag();
  }

  bool ReportedOnAlloc2() const
  {
    MOZ_ASSERT(gOptions->IsDarkMatterMode());
    return mReportStackTrace_mReportedOnAlloc[1].Tag();
  }

  void AddStackTracesToTable(StackTraceSet& aStackTraces) const
  {
    aStackTraces.put(AllocStackTrace());  // never null
    if (gOptions->IsDarkMatterMode()) {
      const StackTrace* st;
      if ((st = ReportStackTrace1())) {     // may be null
        aStackTraces.put(st);
      }
      if ((st = ReportStackTrace2())) {     // may be null
        aStackTraces.put(st);
      }
    }
  }

  uint32_t NumReports() const
  {
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
  void Report(Thread* aT, bool aReportedOnAlloc) const
  {
    MOZ_ASSERT(gOptions->IsDarkMatterMode());
    // We don't bother recording reports after the 2nd one.
    uint32_t numReports = NumReports();
    if (numReports < 2) {
      mReportStackTrace_mReportedOnAlloc[numReports].Set(StackTrace::Get(aT),
                                                         aReportedOnAlloc);
    }
  }

  void UnreportIfNotReportedOnAlloc() const
  {
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

  static uint32_t hash(const void* const& aPtr)
  {
    return mozilla::HashGeneric(aPtr);
  }

  static bool match(const LiveBlock& aB, const void* const& aPtr)
  {
    return aB.mPtr == aPtr;
  }
};

typedef js::HashSet<LiveBlock, LiveBlock, InfallibleAllocPolicy> LiveBlockTable;
static LiveBlockTable* gLiveBlockTable = nullptr;

// A freed heap block.
class DeadBlock
{
  const size_t mReqSize;    // size requested
  const size_t mSlopSize;   // slop above size requested

  // Ptr: |mAllocStackTrace| - stack trace where this block was allocated.
  // Tag bit 0: |mIsSampled| - was this block sampled? (if so, slop == 0).
  TaggedPtr<const StackTrace* const>
    mAllocStackTrace_mIsSampled;

public:
  DeadBlock()
    : mReqSize(0)
    , mSlopSize(0)
    , mAllocStackTrace_mIsSampled(nullptr, 0)
  {}

  explicit DeadBlock(const LiveBlock& aLb)
    : mReqSize(aLb.ReqSize())
    , mSlopSize(aLb.SlopSize())
    , mAllocStackTrace_mIsSampled(aLb.AllocStackTrace(), aLb.IsSampled())
  {
    MOZ_ASSERT(AllocStackTrace());
    MOZ_ASSERT_IF(IsSampled(), SlopSize() == 0);
  }

  ~DeadBlock() {}

  size_t ReqSize()    const { return mReqSize; }
  size_t SlopSize()   const { return mSlopSize; }
  size_t UsableSize() const { return mReqSize + mSlopSize; }

  bool IsSampled() const
  {
    return mAllocStackTrace_mIsSampled.Tag();
  }

  const StackTrace* AllocStackTrace() const
  {
    return mAllocStackTrace_mIsSampled.Ptr();
  }

  void AddStackTracesToTable(StackTraceSet& aStackTraces) const
  {
    aStackTraces.put(AllocStackTrace());  // never null
  }

  // Hash policy.

  typedef DeadBlock Lookup;

  static uint32_t hash(const DeadBlock& aB)
  {
    return mozilla::HashGeneric(aB.ReqSize(),
                                aB.SlopSize(),
                                aB.IsSampled(),
                                aB.AllocStackTrace());
  }

  static bool match(const DeadBlock& aA, const DeadBlock& aB)
  {
    return aA.ReqSize() == aB.ReqSize() &&
           aA.SlopSize() == aB.SlopSize() &&
           aA.IsSampled() == aB.IsSampled() &&
           aA.AllocStackTrace() == aB.AllocStackTrace();
  }
};

// For each unique DeadBlock value we store a count of how many actual dead
// blocks have that value.
typedef js::HashMap<DeadBlock, size_t, DeadBlock, InfallibleAllocPolicy>
  DeadBlockTable;
static DeadBlockTable* gDeadBlockTable = nullptr;

// Add the dead block to the dead block table, if that's appropriate.
void MaybeAddToDeadBlockTable(const DeadBlock& aDb)
{
  if (gOptions->IsCumulativeMode() && aDb.AllocStackTrace()) {
    AutoLockState lock;
    if (DeadBlockTable::AddPtr p = gDeadBlockTable->lookupForAdd(aDb)) {
      p->value() += 1;
    } else {
      gDeadBlockTable->add(p, aDb, 1);
    }
  }
}

// Add a pointer to each live stack trace into the given StackTraceSet.  (A
// stack trace is live if it's used by one of the live blocks.)
static void
GatherUsedStackTraces(StackTraceSet& aStackTraces)
{
  MOZ_ASSERT(gStateLock->IsLocked());
  MOZ_ASSERT(Thread::Fetch()->InterceptsAreBlocked());

  aStackTraces.finish();
  aStackTraces.init(512);

  for (auto r = gLiveBlockTable->all(); !r.empty(); r.popFront()) {
    r.front().AddStackTracesToTable(aStackTraces);
  }

  for (auto r = gDeadBlockTable->all(); !r.empty(); r.popFront()) {
    r.front().key().AddStackTracesToTable(aStackTraces);
  }
}

// Delete stack traces that we aren't using, and compact our hashtable.
static void
GCStackTraces()
{
  MOZ_ASSERT(gStateLock->IsLocked());
  MOZ_ASSERT(Thread::Fetch()->InterceptsAreBlocked());

  StackTraceSet usedStackTraces;
  GatherUsedStackTraces(usedStackTraces);

  // Delete all unused stack traces from gStackTraceTable.  The Enum destructor
  // will automatically rehash and compact the table.
  for (StackTraceTable::Enum e(*gStackTraceTable); !e.empty(); e.popFront()) {
    StackTrace* const& st = e.front();
    if (!usedStackTraces.has(st)) {
      e.removeFront();
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

static size_t gSmallBlockActualSizeCounter = 0;

static void
AllocCallback(void* aPtr, size_t aReqSize, Thread* aT)
{
  if (!aPtr) {
    return;
  }

  AutoLockState lock;
  AutoBlockIntercepts block(aT);

  size_t actualSize = gMallocTable->malloc_usable_size(aPtr);
  size_t sampleBelowSize = gOptions->SampleBelowSize();

  if (actualSize < sampleBelowSize) {
    // If this allocation is smaller than the sample-below size, increment the
    // cumulative counter.  Then, if that counter now exceeds the sample size,
    // blame this allocation for |sampleBelowSize| bytes.  This precludes the
    // measurement of slop.
    gSmallBlockActualSizeCounter += actualSize;
    if (gSmallBlockActualSizeCounter >= sampleBelowSize) {
      gSmallBlockActualSizeCounter -= sampleBelowSize;

      LiveBlock b(aPtr, sampleBelowSize, StackTrace::Get(aT),
                  /* isSampled */ true);
      (void)gLiveBlockTable->putNew(aPtr, b);
    }
  } else {
    // If this block size is larger than the sample size, record it exactly.
    LiveBlock b(aPtr, aReqSize, StackTrace::Get(aT), /* isSampled */ false);
    (void)gLiveBlockTable->putNew(aPtr, b);
  }
}

static void
FreeCallback(void* aPtr, Thread* aT, DeadBlock* aDeadBlock)
{
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
    // We have no record of the block. Do nothing. Either:
    // - We're sampling and we skipped this block. This is likely.
    // - It's a bogus pointer.
  }

  if (gStackTraceTable->count() > gGCStackTraceTableWhenSizeExceeds) {
    GCStackTraces();
  }
}

//---------------------------------------------------------------------------
// malloc/free interception
//---------------------------------------------------------------------------

static void Init(const malloc_table_t* aMallocTable);

} // namespace dmd
} // namespace mozilla

void
replace_init(const malloc_table_t* aMallocTable)
{
  mozilla::dmd::Init(aMallocTable);
}

ReplaceMallocBridge*
replace_get_bridge()
{
  return mozilla::dmd::gDMDBridge;
}

void*
replace_malloc(size_t aSize)
{
  using namespace mozilla::dmd;

  if (!gIsDMDInitialized) {
    // DMD hasn't started up, either because it wasn't enabled by the user, or
    // we're still in Init() and something has indirectly called malloc.  Do a
    // vanilla malloc.  (In the latter case, if it fails we'll crash.  But
    // OOM is highly unlikely so early on.)
    return gMallocTable->malloc(aSize);
  }

  Thread* t = Thread::Fetch();
  if (t->InterceptsAreBlocked()) {
    // Intercepts are blocked, which means this must be a call to malloc
    // triggered indirectly by DMD (e.g. via MozStackWalk).  Be infallible.
    return InfallibleAllocPolicy::malloc_(aSize);
  }

  // This must be a call to malloc from outside DMD.  Intercept it.
  void* ptr = gMallocTable->malloc(aSize);
  AllocCallback(ptr, aSize, t);
  return ptr;
}

void*
replace_calloc(size_t aCount, size_t aSize)
{
  using namespace mozilla::dmd;

  if (!gIsDMDInitialized) {
    return gMallocTable->calloc(aCount, aSize);
  }

  Thread* t = Thread::Fetch();
  if (t->InterceptsAreBlocked()) {
    return InfallibleAllocPolicy::calloc_(aCount * aSize);
  }

  void* ptr = gMallocTable->calloc(aCount, aSize);
  AllocCallback(ptr, aCount * aSize, t);
  return ptr;
}

void*
replace_realloc(void* aOldPtr, size_t aSize)
{
  using namespace mozilla::dmd;

  if (!gIsDMDInitialized) {
    return gMallocTable->realloc(aOldPtr, aSize);
  }

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
  void* ptr = gMallocTable->realloc(aOldPtr, aSize);
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
    AllocCallback(aOldPtr, gMallocTable->malloc_usable_size(aOldPtr), t);
  }
  return ptr;
}

void*
replace_memalign(size_t aAlignment, size_t aSize)
{
  using namespace mozilla::dmd;

  if (!gIsDMDInitialized) {
    return gMallocTable->memalign(aAlignment, aSize);
  }

  Thread* t = Thread::Fetch();
  if (t->InterceptsAreBlocked()) {
    return InfallibleAllocPolicy::memalign_(aAlignment, aSize);
  }

  void* ptr = gMallocTable->memalign(aAlignment, aSize);
  AllocCallback(ptr, aSize, t);
  return ptr;
}

void
replace_free(void* aPtr)
{
  using namespace mozilla::dmd;

  if (!gIsDMDInitialized) {
    gMallocTable->free(aPtr);
    return;
  }

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
  gMallocTable->free(aPtr);
}

namespace mozilla {
namespace dmd {

//---------------------------------------------------------------------------
// Options (Part 2)
//---------------------------------------------------------------------------

// Given an |aOptionName| like "foo", succeed if |aArg| has the form "foo=blah"
// (where "blah" is non-empty) and return the pointer to "blah".  |aArg| can
// have leading space chars (but not other whitespace).
const char*
Options::ValueIfMatch(const char* aArg, const char* aOptionName)
{
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
bool
Options::GetLong(const char* aArg, const char* aOptionName,
                 long aMin, long aMax, long* aValue)
{
  if (const char* optionValue = ValueIfMatch(aArg, aOptionName)) {
    char* endPtr;
    *aValue = strtol(optionValue, &endPtr, /* base */ 10);
    if (!*endPtr && aMin <= *aValue && *aValue <= aMax &&
        *aValue != LONG_MIN && *aValue != LONG_MAX) {
      return true;
    }
  }
  return false;
}

// Extracts a |bool| value for an option -- encoded as "yes" or "no" -- from an
// argument.
bool
Options::GetBool(const char* aArg, const char* aOptionName, bool* aValue)
{
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

// The sample-below default is a prime number close to 4096.
// - Why that size?  Because it's *much* faster but only moderately less precise
//   than a size of 1.
// - Why prime?  Because it makes our sampling more random.  If we used a size
//   of 4096, for example, then our alloc counter would only take on even
//   values, because jemalloc always rounds up requests sizes.  In contrast, a
//   prime size will explore all possible values of the alloc counter.
//
Options::Options(const char* aDMDEnvVar)
  : mDMDEnvVar(aDMDEnvVar ? InfallibleAllocPolicy::strdup_(aDMDEnvVar)
                          : nullptr)
  , mMode(DarkMatter)
  , mSampleBelowSize(4093, 100 * 100 * 1000)
  , mMaxFrames(StackTrace::MaxFrames, StackTrace::MaxFrames)
  , mShowDumpStats(false)
{
  // It's no longer necessary to set the DMD env var to "1" if you want default
  // options (you can leave it undefined) but we still accept "1" for
  // backwards compatibility.
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
      long myLong;
      bool myBool;
      if (strcmp(arg, "--mode=live") == 0) {
        mMode = Options::Live;
      } else if (strcmp(arg, "--mode=dark-matter") == 0) {
        mMode = Options::DarkMatter;
      } else if (strcmp(arg, "--mode=cumulative") == 0) {
        mMode = Options::Cumulative;
      } else if (strcmp(arg, "--mode=scan") == 0) {
        mMode = Options::Scan;

      } else if (GetLong(arg, "--sample-below", 1, mSampleBelowSize.mMax,
                 &myLong)) {
        mSampleBelowSize.mActual = myLong;

      } else if (GetLong(arg, "--max-frames", 1, mMaxFrames.mMax, &myLong)) {
        mMaxFrames.mActual = myLong;

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

  if (mMode == Options::Scan) {
    mSampleBelowSize.mActual = 1;
  }
}

void
Options::BadArg(const char* aArg)
{
  StatusMsg("\n");
  StatusMsg("Bad entry in the $DMD environment variable: '%s'.\n", aArg);
  StatusMsg("\n");
  StatusMsg("$DMD must be a whitespace-separated list of |--option=val|\n");
  StatusMsg("entries.\n");
  StatusMsg("\n");
  StatusMsg("The following options are allowed;  defaults are shown in [].\n");
  StatusMsg("  --mode=<mode>                Profiling mode [dark-matter]\n");
  StatusMsg("      where <mode> is one of: live, dark-matter, cumulative\n");
  StatusMsg("  --sample-below=<1..%d> Sample blocks smaller than this [%d]\n",
            int(mSampleBelowSize.mMax),
            int(mSampleBelowSize.mDefault));
  StatusMsg("                               (prime numbers are recommended)\n");
  StatusMsg("  --max-frames=<1..%d>         Max. depth of stack traces [%d]\n",
            int(mMaxFrames.mMax),
            int(mMaxFrames.mDefault));
  StatusMsg("  --show-dump-stats=<yes|no>   Show stats about dumps? [no]\n");
  StatusMsg("\n");
  exit(1);
}

const char*
Options::ModeString() const
{
  switch (mMode) {
  case Live:
    return "live";
  case DarkMatter:
    return "dark-matter";
  case Cumulative:
    return "cumulative";
  case Scan:
    return "scan";
  default:
    MOZ_ASSERT(false);
    return "(unknown DMD mode)";
  }
}

//---------------------------------------------------------------------------
// DMD start-up
//---------------------------------------------------------------------------

#ifdef XP_MACOSX
static void
NopStackWalkCallback(uint32_t aFrameNumber, void* aPc, void* aSp,
                     void* aClosure)
{
}
#endif

// WARNING: this function runs *very* early -- before all static initializers
// have run.  For this reason, non-scalar globals such as gStateLock and
// gStackTraceTable are allocated dynamically (so we can guarantee their
// construction in this function) rather than statically.
static void
Init(const malloc_table_t* aMallocTable)
{
  gMallocTable = aMallocTable;
  gDMDBridge = InfallibleAllocPolicy::new_<DMDBridge>();

  // DMD is controlled by the |DMD| environment variable.
  const char* e = getenv("DMD");

  if (e) {
    StatusMsg("$DMD = '%s'\n", e);
  } else {
    StatusMsg("$DMD is undefined\n", e);
  }

  // Parse $DMD env var.
  gOptions = InfallibleAllocPolicy::new_<Options>(e);

#ifdef XP_MACOSX
  // On Mac OS X we need to call StackWalkInitCriticalAddress() very early
  // (prior to the creation of any mutexes, apparently) otherwise we can get
  // hangs when getting stack traces (bug 821577).  But
  // StackWalkInitCriticalAddress() isn't exported from xpcom/, so instead we
  // just call MozStackWalk, because that calls StackWalkInitCriticalAddress().
  // See the comment above StackWalkInitCriticalAddress() for more details.
  (void)MozStackWalk(NopStackWalkCallback, /* skipFrames */ 0,
                     /* maxFrames */ 1, nullptr, 0, nullptr);
#endif

  gStateLock = InfallibleAllocPolicy::new_<Mutex>();

  gSmallBlockActualSizeCounter = 0;

  DMD_CREATE_TLS_INDEX(gTlsIndex);

  {
    AutoLockState lock;

    gStackTraceTable = InfallibleAllocPolicy::new_<StackTraceTable>();
    gStackTraceTable->init(8192);

    gLiveBlockTable = InfallibleAllocPolicy::new_<LiveBlockTable>();
    gLiveBlockTable->init(8192);

    // Create this even if the mode isn't Cumulative (albeit with a small
    // size), in case the mode is changed later on (as is done by SmokeDMD.cpp,
    // for example).
    gDeadBlockTable = InfallibleAllocPolicy::new_<DeadBlockTable>();
    gDeadBlockTable->init(gOptions->IsCumulativeMode() ? 8192 : 4);
  }

  gIsDMDInitialized = true;
}

//---------------------------------------------------------------------------
// Block reporting and unreporting
//---------------------------------------------------------------------------

static void
ReportHelper(const void* aPtr, bool aReportedOnAlloc)
{
  if (!gOptions->IsDarkMatterMode() || !aPtr) {
    return;
  }

  Thread* t = Thread::Fetch();

  AutoBlockIntercepts block(t);
  AutoLockState lock;

  if (LiveBlockTable::Ptr p = gLiveBlockTable->lookup(aPtr)) {
    p->Report(t, aReportedOnAlloc);
  } else {
    // We have no record of the block.  Do nothing.  Either:
    // - We're sampling and we skipped this block.  This is likely.
    // - It's a bogus pointer.  This is unlikely because Report() is almost
    //   always called in conjunction with a malloc_size_of-style function.
  }
}

void
DMDFuncs::Report(const void* aPtr)
{
  ReportHelper(aPtr, /* onAlloc */ false);
}

void
DMDFuncs::ReportOnAlloc(const void* aPtr)
{
  ReportHelper(aPtr, /* onAlloc */ true);
}

//---------------------------------------------------------------------------
// DMD output
//---------------------------------------------------------------------------

// The version number of the output format. Increment this if you make
// backwards-incompatible changes to the format. See DMD.h for the version
// history.
static const int kOutputVersionNumber = 4;

// Note that, unlike most SizeOf* functions, this function does not take a
// |mozilla::MallocSizeOf| argument.  That's because those arguments are
// primarily to aid DMD track heap blocks... but DMD deliberately doesn't track
// heap blocks it allocated for itself!
//
// SizeOfInternal should be called while you're holding the state lock and
// while intercepts are blocked; SizeOf acquires the lock and blocks
// intercepts.

static void
SizeOfInternal(Sizes* aSizes)
{
  MOZ_ASSERT(gStateLock->IsLocked());
  MOZ_ASSERT(Thread::Fetch()->InterceptsAreBlocked());

  aSizes->Clear();

  StackTraceSet usedStackTraces;
  GatherUsedStackTraces(usedStackTraces);

  for (auto r = gStackTraceTable->all(); !r.empty(); r.popFront()) {
    StackTrace* const& st = r.front();

    if (usedStackTraces.has(st)) {
      aSizes->mStackTracesUsed += MallocSizeOf(st);
    } else {
      aSizes->mStackTracesUnused += MallocSizeOf(st);
    }
  }

  aSizes->mStackTraceTable =
    gStackTraceTable->sizeOfIncludingThis(MallocSizeOf);

  aSizes->mLiveBlockTable = gLiveBlockTable->sizeOfIncludingThis(MallocSizeOf);

  aSizes->mDeadBlockTable = gDeadBlockTable->sizeOfIncludingThis(MallocSizeOf);
}

void
DMDFuncs::SizeOf(Sizes* aSizes)
{
  aSizes->Clear();

  AutoBlockIntercepts block(Thread::Fetch());
  AutoLockState lock;
  SizeOfInternal(aSizes);
}

void
DMDFuncs::ClearReports()
{
  if (!gOptions->IsDarkMatterMode()) {
    return;
  }

  AutoLockState lock;

  // Unreport all blocks that were marked reported by a memory reporter.  This
  // excludes those that were reported on allocation, because they need to keep
  // their reported marking.
  for (auto r = gLiveBlockTable->all(); !r.empty(); r.popFront()) {
    r.front().UnreportIfNotReportedOnAlloc();
  }
}

class ToIdStringConverter final
{
public:
  ToIdStringConverter() : mNextId(0) { mIdMap.init(512); }

  // Converts a pointer to a unique ID. Reuses the existing ID for the pointer
  // if it's been seen before.
  const char* ToIdString(const void* aPtr)
  {
    uint32_t id;
    PointerIdMap::AddPtr p = mIdMap.lookupForAdd(aPtr);
    if (!p) {
      id = mNextId++;
      (void)mIdMap.add(p, aPtr, id);
    } else {
      id = p->value();
    }
    return Base32(id);
  }

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return mIdMap.sizeOfExcludingThis(aMallocSizeOf);
  }

private:
  // This function converts an integer to base-32. |aBuf| must have space for at
  // least eight chars, which is the space needed to hold 'Dffffff' (including
  // the terminating null char), which is the base-32 representation of
  // 0xffffffff.
  //
  // We use base-32 values for indexing into the traceTable and the frameTable,
  // for the following reasons.
  //
  // - Base-32 gives more compact indices than base-16.
  //
  // - 32 is a power-of-two, which makes the necessary div/mod calculations fast.
  //
  // - We can (and do) choose non-numeric digits for base-32. When
  //   inspecting/debugging the JSON output, non-numeric indices are easier to
  //   search for than numeric indices.
  //
  char* Base32(uint32_t aN)
  {
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
  static const size_t kIdBufLen = 16;
  char mIdBuf[kIdBufLen];
};

// Helper class for converting a pointer value to a string.
class ToStringConverter
{
public:
  const char* ToPtrString(const void* aPtr)
  {
    snprintf(kPtrBuf, sizeof(kPtrBuf) - 1, "%" PRIxPTR, (uintptr_t)aPtr);
    return kPtrBuf;
  }

private:
  char kPtrBuf[32];
};

static void
WriteBlockContents(JSONWriter& aWriter, const LiveBlock& aBlock)
{
  MOZ_ASSERT(!aBlock.IsSampled(), "Sampled blocks do not have accurate sizes");

  size_t numWords = aBlock.ReqSize() / sizeof(uintptr_t*);
  if (numWords == 0) {
    return;
  }

  aWriter.StartArrayProperty("contents", aWriter.SingleLineStyle);
  {
    const uintptr_t** block = (const uintptr_t**)aBlock.Address();
    ToStringConverter sc;
    for (size_t i = 0; i < numWords; ++i) {
      aWriter.StringElement(sc.ToPtrString(block[i]));
    }
  }
  aWriter.EndArray();
}

static void
AnalyzeImpl(UniquePtr<JSONWriteFunc> aWriter)
{
  AutoBlockIntercepts block(Thread::Fetch());
  AutoLockState lock;

  // Allocate this on the heap instead of the stack because it's fairly large.
  auto locService = InfallibleAllocPolicy::new_<CodeAddressService>();

  StackTraceSet usedStackTraces;
  usedStackTraces.init(512);

  PointerSet usedPcs;
  usedPcs.init(512);

  size_t iscSize;

  static int analysisCount = 1;
  StatusMsg("Dump %d {\n", analysisCount++);

  JSONWriter writer(Move(aWriter));
  writer.Start();
  {
    writer.IntProperty("version", kOutputVersionNumber);

    writer.StartObjectProperty("invocation");
    {
      const char* var = gOptions->DMDEnvVar();
      if (var) {
        writer.StringProperty("dmdEnvVar", var);
      } else {
        writer.NullProperty("dmdEnvVar");
      }

      writer.StringProperty("mode", gOptions->ModeString());
      writer.IntProperty("sampleBelowSize", gOptions->SampleBelowSize());
    }
    writer.EndObject();

    StatusMsg("  Constructing the heap block list...\n");

    ToIdStringConverter isc;
    ToStringConverter sc;

    writer.StartArrayProperty("blockList");
    {
      // Live blocks.
      for (auto r = gLiveBlockTable->all(); !r.empty(); r.popFront()) {
        const LiveBlock& b = r.front();
        b.AddStackTracesToTable(usedStackTraces);

        writer.StartObjectElement(writer.SingleLineStyle);
        {
          if (!b.IsSampled()) {
            if (gOptions->IsScanMode()) {
              writer.StringProperty("addr", sc.ToPtrString(b.Address()));
              WriteBlockContents(writer, b);
            }
            writer.IntProperty("req", b.ReqSize());
            if (b.SlopSize() > 0) {
              writer.IntProperty("slop", b.SlopSize());
            }
          }
          writer.StringProperty("alloc", isc.ToIdString(b.AllocStackTrace()));
          if (gOptions->IsDarkMatterMode() && b.NumReports() > 0) {
            writer.StartArrayProperty("reps");
            {
              if (b.ReportStackTrace1()) {
                writer.StringElement(isc.ToIdString(b.ReportStackTrace1()));
              }
              if (b.ReportStackTrace2()) {
                writer.StringElement(isc.ToIdString(b.ReportStackTrace2()));
              }
            }
            writer.EndArray();
          }
        }
        writer.EndObject();
      }

      // Dead blocks.
      for (auto r = gDeadBlockTable->all(); !r.empty(); r.popFront()) {
        const DeadBlock& b = r.front().key();
        b.AddStackTracesToTable(usedStackTraces);

        size_t num = r.front().value();
        MOZ_ASSERT(num > 0);

        writer.StartObjectElement(writer.SingleLineStyle);
        {
          if (!b.IsSampled()) {
            writer.IntProperty("req", b.ReqSize());
            if (b.SlopSize() > 0) {
              writer.IntProperty("slop", b.SlopSize());
            }
          }
          writer.StringProperty("alloc", isc.ToIdString(b.AllocStackTrace()));

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
      for (auto r = usedStackTraces.all(); !r.empty(); r.popFront()) {
        const StackTrace* const st = r.front();
        writer.StartArrayProperty(isc.ToIdString(st), writer.SingleLineStyle);
        {
          for (uint32_t i = 0; i < st->Length(); i++) {
            const void* pc = st->Pc(i);
            writer.StringElement(isc.ToIdString(pc));
            usedPcs.put(pc);
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

      for (PointerSet::Enum e(usedPcs); !e.empty(); e.popFront()) {
        const void* const pc = e.front();

        // Use 0 for the frame number. See the JSON format description comment
        // in DMD.h to understand why.
        locService->GetLocation(0, pc, locBuf, locBufLen);
        writer.StringProperty(isc.ToIdString(pc), locBuf);
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
      Show(sizes.mStackTraceTable,       buf1, kBufLen),
      Show(gStackTraceTable->capacity(), buf2, kBufLen),
      Show(gStackTraceTable->count(),    buf3, kBufLen));

    StatusMsg("      Live block table:     %10s bytes (%s entries, %s used)\n",
      Show(sizes.mLiveBlockTable,       buf1, kBufLen),
      Show(gLiveBlockTable->capacity(), buf2, kBufLen),
      Show(gLiveBlockTable->count(),    buf3, kBufLen));

    StatusMsg("      Dead block table:     %10s bytes (%s entries, %s used)\n",
      Show(sizes.mDeadBlockTable,       buf1, kBufLen),
      Show(gDeadBlockTable->capacity(), buf2, kBufLen),
      Show(gDeadBlockTable->count(),    buf3, kBufLen));

    StatusMsg("    }\n");
    StatusMsg("    Data structures that are destroyed after Dump() ends {\n");

    StatusMsg("      Location service:      %10s bytes\n",
      Show(locService->SizeOfIncludingThis(MallocSizeOf), buf1, kBufLen));
    StatusMsg("      Used stack traces set: %10s bytes\n",
      Show(usedStackTraces.sizeOfExcludingThis(MallocSizeOf), buf1, kBufLen));
    StatusMsg("      Used PCs set:          %10s bytes\n",
      Show(usedPcs.sizeOfExcludingThis(MallocSizeOf), buf1, kBufLen));
    StatusMsg("      Pointer ID map:        %10s bytes\n",
      Show(iscSize, buf1, kBufLen));

    StatusMsg("    }\n");
    StatusMsg("    Counts {\n");

    size_t hits   = locService->NumCacheHits();
    size_t misses = locService->NumCacheMisses();
    size_t requests = hits + misses;
    StatusMsg("      Location service:    %10s requests\n",
      Show(requests, buf1, kBufLen));

    size_t count    = locService->CacheCount();
    size_t capacity = locService->CacheCapacity();
    StatusMsg("      Location service cache:  "
      "%4.1f%% hit rate, %.1f%% occupancy at end\n",
      Percent(hits, requests), Percent(count, capacity));

    StatusMsg("    }\n");
    StatusMsg("  }\n");
  }

  InfallibleAllocPolicy::delete_(locService);

  StatusMsg("}\n");
}

void
DMDFuncs::Analyze(UniquePtr<JSONWriteFunc> aWriter)
{
  AnalyzeImpl(Move(aWriter));
  ClearReports();
}

//---------------------------------------------------------------------------
// Testing
//---------------------------------------------------------------------------

void
DMDFuncs::ResetEverything(const char* aOptions)
{
  AutoLockState lock;

  // Reset options.
  InfallibleAllocPolicy::delete_(gOptions);
  gOptions = InfallibleAllocPolicy::new_<Options>(aOptions);

  // Clear all existing blocks.
  gLiveBlockTable->clear();
  gDeadBlockTable->clear();
  gSmallBlockActualSizeCounter = 0;
}

} // namespace dmd
} // namespace mozilla
