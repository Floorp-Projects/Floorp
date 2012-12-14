/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DMD.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef XP_WIN
#error "Windows not supported yet, sorry."
// XXX: This will be needed when Windows is supported (bug 819839).
//#include <process.h>
//#define getpid _getpid
#else
#include <unistd.h>
#endif

#ifdef ANDROID
#include <android/log.h>
#endif

#include "nscore.h"
#include "nsStackWalk.h"

#include "js/HashTable.h"
#include "js/Vector.h"

#include "mozilla/Assertions.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/Likely.h"

// MOZ_REPLACE_ONLY_MEMALIGN saves us from having to define
// replace_{posix_memalign,aligned_alloc,valloc}.  It requires defining
// PAGE_SIZE.  Nb: sysconf() is expensive, but it's only used for (the obsolete
// and rarely used) valloc.
#define MOZ_REPLACE_ONLY_MEMALIGN 1
#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#include "replace_malloc.h"
#undef MOZ_REPLACE_ONLY_MEMALIGN
#undef PAGE_SIZE

namespace mozilla {
namespace dmd {

//---------------------------------------------------------------------------
// Utilities
//---------------------------------------------------------------------------

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(T) \
  T(const T&);                      \
  void operator=(const T&)
#endif

static const malloc_table_t* gMallocTable = nullptr;

// This enables/disables DMD.
static bool gIsDMDRunning = false;

enum Mode {
  Normal,   // run normally
  Test,     // do some basic correctness tests
  Stress    // do some performance stress tests
};
static Mode gMode = Normal;

// This provides infallible allocations (they abort on OOM).  We use it for all
// of DMD's own allocations, which fall into the following three cases.
// - Direct allocations (the easy case).
// - Indirect allocations in js::{Vector,HashSet,HashMap} -- this class serves
//   as their AllocPolicy.
// - Other indirect allocations (e.g. NS_StackWalk) -- see the comments on
//   Thread::mBlockIntercepts and in replace_malloc for how these work.
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

  static void* calloc_(size_t aSize)
  {
    void* p = gMallocTable->calloc(1, aSize);
    ExitOnFailure(p);
    return p;
  }

  // This realloc_ is the one we use for direct reallocs within DMD.
  static void* realloc_(void* aPtr, size_t aNewSize)
  {
    void* p = gMallocTable->realloc(aPtr, aNewSize);
    ExitOnFailure(p);
    return p;
  }

  // This realloc_ is required for this to be a JS container AllocPolicy.
  static void* realloc_(void* aPtr, size_t aOldSize, size_t aNewSize)
  {
    return InfallibleAllocPolicy::realloc_(aPtr, aNewSize);
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
    char* s = (char*) gMallocTable->malloc(strlen(aStr) + 1);
    ExitOnFailure(s);
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

  static void reportAllocOverflow() { ExitOnFailure(nullptr); }
};

// This is only needed because of the |const void*| vs |void*| arg mismatch.
static size_t
MallocSizeOf(const void* aPtr)
{
  return gMallocTable->malloc_usable_size(const_cast<void*>(aPtr));
}

static void
StatusMsg(const char* aFmt, ...)
{
  va_list ap;
  va_start(ap, aFmt);
#ifdef ANDROID
  __android_log_vprint(ANDROID_LOG_INFO, "DMD", aFmt, ap);
#else
  // The +64 is easily enough for the "DMD[<pid>] " prefix and the NUL.
  char* fmt = (char*) InfallibleAllocPolicy::malloc_(strlen(aFmt) + 64);
  sprintf(fmt, "DMD[%d] %s", getpid(), aFmt);
  vfprintf(stderr, fmt, ap);
  InfallibleAllocPolicy::free_(fmt);
#endif
  va_end(ap);
}

/* static */ void
InfallibleAllocPolicy::ExitOnFailure(const void* aP)
{
  if (!aP) {
    StatusMsg("out of memory;  aborting\n");
    MOZ_CRASH();
  }
}

void
Writer::Write(const char* aFmt, ...) const
{
  va_list ap;
  va_start(ap, aFmt);
  mWriterFun(mWriteState, aFmt, ap);
  va_end(ap);
}

#define W(...) aWriter.Write(__VA_ARGS__);

#define WriteTitle(...)                                                       \
  W("------------------------------------------------------------------\n");  \
  W(__VA_ARGS__);                                                             \
  W("------------------------------------------------------------------\n\n");

MOZ_EXPORT void
FpWrite(void* aWriteState, const char* aFmt, va_list aAp)
{
  FILE* fp = static_cast<FILE*>(aWriteState);
  vfprintf(fp, aFmt, aAp);
}

static double
Percent(size_t part, size_t whole)
{
  return (whole == 0) ? 0 : 100 * (double)part / whole;
}

// Commifies the number and prepends a '~' if requested.  Best used with
// |kBufLen| and |gBuf[1234]|, because they should be big enough for any number
// we'll see.
static char*
Show(size_t n, char* buf, size_t buflen, bool addTilde = false)
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

  if (addTilde) {
    firstCharIndex--;
    buf[firstCharIndex] = '~';
  }

  MOZ_ASSERT(firstCharIndex >= 0);
  return &buf[firstCharIndex];
}

static const char*
Plural(size_t aN)
{
  return aN == 1 ? "" : "s";
}

// Used by calls to Show().
static const size_t kBufLen = 64;
static char gBuf1[kBufLen];
static char gBuf2[kBufLen];
static char gBuf3[kBufLen];
static char gBuf4[kBufLen];

static const size_t kNoSize = size_t(-1);

//---------------------------------------------------------------------------
// The global lock
//---------------------------------------------------------------------------

// MutexBase implements the platform-specific parts of a mutex.
#ifdef XP_WIN

#include <windows.h>

class MutexBase
{
  CRITICAL_SECTION mCS;

  DISALLOW_COPY_AND_ASSIGN(MutexBase);

public:
  MutexBase()
  {
    InitializeCriticalSection(&mCS);
  }

  ~MutexBase()
  {
    DeleteCriticalSection(&mCS);
  }

  void Lock()
  {
    EnterCriticalSection(&mCS);
  }

  void Unlock()
  {
    LeaveCriticalSection(&mCS);
  }
};

#else

#include <pthread.h>
#include <sys/types.h>

class MutexBase
{
  pthread_mutex_t mMutex;

  DISALLOW_COPY_AND_ASSIGN(MutexBase);

public:
  MutexBase()
    : mMutex(PTHREAD_MUTEX_INITIALIZER)
  {}

  void Lock()
  {
    pthread_mutex_lock(&mMutex);
  }

  void Unlock()
  {
    pthread_mutex_unlock(&mMutex);
  }
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

  bool IsLocked()
  {
    return mIsLocked;
  }
};

// This lock must be held while manipulating global state, such as
// gStackTraceTable, gLiveBlockTable, etc.
static Mutex* gStateLock = nullptr;

class AutoLockState
{
  DISALLOW_COPY_AND_ASSIGN(AutoLockState);

public:
  AutoLockState()
  {
    gStateLock->Lock();
  }
  ~AutoLockState()
  {
    gStateLock->Unlock();
  }
};

class AutoUnlockState
{
  DISALLOW_COPY_AND_ASSIGN(AutoUnlockState);

public:
  AutoUnlockState()
  {
    gStateLock->Unlock();
  }
  ~AutoUnlockState()
  {
    gStateLock->Lock();
  }
};

//---------------------------------------------------------------------------
// Thread-local storage and blocking of intercepts
//---------------------------------------------------------------------------

#ifdef XP_WIN

#define DMD_TLS_INDEX_TYPE              DWORD
#define DMD_CREATE_TLS_INDEX(i_)        PR_BEGIN_MACRO                        \
                                          (i_) = TlsAlloc();                  \
                                        PR_END_MACRO
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
  // indirectly call vanilla malloc via functions like NS_StackWalk.)
  bool mBlockIntercepts;

  Thread()
    : mBlockIntercepts(false)
  {}

  DISALLOW_COPY_AND_ASSIGN(Thread);

public:
  static Thread* Fetch();

  bool blockIntercepts()
  {
    MOZ_ASSERT(!mBlockIntercepts);
    return mBlockIntercepts = true;
  }

  bool unblockIntercepts()
  {
    MOZ_ASSERT(mBlockIntercepts);
    return mBlockIntercepts = false;
  }

  bool interceptsAreBlocked() const
  {
    return mBlockIntercepts;
  }
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
  AutoBlockIntercepts(Thread* aT)
    : mT(aT)
  {
    mT->blockIntercepts();
  }
  ~AutoBlockIntercepts()
  {
    MOZ_ASSERT(mT->interceptsAreBlocked());
    mT->unblockIntercepts();
  }
};

//---------------------------------------------------------------------------
// Stack traces
//---------------------------------------------------------------------------

static void
PcInfo(const void* aPc, nsCodeAddressDetails* aDetails)
{
  // NS_DescribeCodeAddress can (on Linux) acquire a lock inside
  // the shared library loader.  Another thread might call malloc
  // while holding that lock (when loading a shared library).  So
  // we have to exit gStateLock around this call.  For details, see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=363334#c3
  {
    AutoUnlockState unlock;
    (void)NS_DescribeCodeAddress(const_cast<void*>(aPc), aDetails);
  }
  if (!aDetails->function[0]) {
    strcpy(aDetails->function, "???");
  }
}

class StackTrace
{
  static const uint32_t MaxDepth = 24;

  uint32_t mLength;             // The number of PCs.
  void* mPcs[MaxDepth];         // The PCs themselves.

public:
  StackTrace() : mLength(0) {}

  uint32_t Length() const { return mLength; }
  void* Pc(uint32_t i) const { MOZ_ASSERT(i < mLength); return mPcs[i]; }

  uint32_t Size() const { return mLength * sizeof(mPcs[0]); }

  // The stack trace returned by this function is interned in gStackTraceTable,
  // and so is immortal and unmovable.
  static const StackTrace* Get(Thread* aT);

  void Sort()
  {
    qsort(mPcs, mLength, sizeof(mPcs[0]), StackTrace::QsortCmp);
  }

  void Print(const Writer& aWriter) const;

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
  static void StackWalkCallback(void* aPc, void* aSp, void* aClosure)
  {
    StackTrace* st = (StackTrace*) aClosure;

    // Only fill to MaxDepth.
    // XXX: bug 818793 will allow early bailouts.
    if (st->mLength < MaxDepth) {
      st->mPcs[st->mLength] = aPc;
      st->mLength++;
    }
  }

  static int QsortCmp(const void* aA, const void* aB)
  {
    const void* const a = *static_cast<const void* const*>(aA);
    const void* const b = *static_cast<const void* const*>(aB);
    if (a < b) return -1;
    if (a > b) return  1;
    return 0;
  }
};

typedef js::HashSet<StackTrace*, StackTrace, InfallibleAllocPolicy>
        StackTraceTable;
static StackTraceTable* gStackTraceTable = nullptr;

void
StackTrace::Print(const Writer& aWriter) const
{
  if (mLength == 0) {
    W("   (empty)\n");
    return;
  }

  if (gMode == Test) {
    // Don't print anything because there's too much variation.
    W("   (stack omitted due to test mode)\n");
    return;
  }

  for (uint32_t i = 0; i < mLength; i++) {
    nsCodeAddressDetails details;
    void* pc = mPcs[i];
    PcInfo(pc, &details);
    if (details.function[0]) {
      W("   %s[%s +0x%X] %p\n", details.function, details.library,
        details.loffset, pc);
    }
  }
}

/* static */ const StackTrace*
StackTrace::Get(Thread* aT)
{
  MOZ_ASSERT(gStateLock->IsLocked());
  MOZ_ASSERT(aT->interceptsAreBlocked());

  // On Windows, NS_StackWalk can acquire a lock from the shared library
  // loader.  Another thread might call malloc while holding that lock (when
  // loading a shared library).  So we can't be in gStateLock during the call
  // to NS_StackWalk.  For details, see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=374829#c8
  StackTrace tmp;
  {
#ifdef XP_WIN
    AutoUnlockState unlock;
#endif
    // In normal operation, skip=3 gets us past various malloc wrappers into
    // more interesting stuff.  But in test mode we need to skip a bit less to
    // sufficiently differentiate some similar stacks.
    uint32_t skip = (gMode == Test) ? 2 : 3;
    nsresult rv = NS_StackWalk(StackWalkCallback, skip, &tmp, 0, nullptr);
    if (NS_FAILED(rv) || tmp.mLength == 0) {
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

static const char* gUnreportedName = "unreported";

// A live heap block.
class LiveBlock
{
  const void*  mPtr;

  // This assumes that we'll never request an allocation of 2 GiB or more on
  // 32-bit platforms.
  static const size_t kReqBits = sizeof(size_t) * 8 - 1;    // 31 or 63
  const size_t mReqSize:kReqBits; // size requested
  const size_t mSampled:1;        // was this block sampled? (if so, slop == 0)

public:
  const StackTrace* const mAllocStackTrace;     // never null

  // Live blocks can be reported in two ways.
  // - The most common is via a memory reporter traversal -- the block is
  //   reported when the reporter runs, causing DMD to mark it as reported,
  //   and DMD must clear the marking once it has finished its analysis.
  // - Less common are ones that are reported immediately on allocation.  DMD
  //   must *not* clear the markings of these blocks once it has finished its
  //   analysis.  The |mReportedOnAlloc| field is set for such blocks.
  //
  // |mPtr| is used as the key in LiveBlockTable, so it's ok for these fields
  // to be |mutable|.
private:
  mutable const StackTrace* mReportStackTrace;  // nullptr if unreported
  mutable const char*       mReporterName;      // gUnreportedName if unreported
  mutable bool              mReportedOnAlloc;   // true if block was reported
                                                //   immediately after alloc

public:
  LiveBlock(const void* aPtr, size_t aReqSize,
            const StackTrace* aAllocStackTrace, bool aSampled)
    : mPtr(aPtr),
      mReqSize(aReqSize),
      mSampled(aSampled),
      mAllocStackTrace(aAllocStackTrace),
      mReportStackTrace(nullptr),
      mReporterName(gUnreportedName),
      mReportedOnAlloc(false)
 {
    if (mReqSize != aReqSize)
    {
      MOZ_CRASH();              // overflowed mReqSize
    }
    MOZ_ASSERT(IsSane());
  }

  bool IsSane() const
  {
    bool hasReporterName = mReporterName != gUnreportedName;
    return mAllocStackTrace &&
           (( mReportStackTrace &&  hasReporterName) ||
            (!mReportStackTrace && !hasReporterName && !mReportedOnAlloc));
  }

  size_t ReqSize() const { return mReqSize; }

  // Sampled blocks always have zero slop.
  size_t SlopSize() const
  {
    return mSampled ? 0 : MallocSizeOf(mPtr) - mReqSize;
  }

  size_t UsableSize() const
  {
    return mSampled ? mReqSize : MallocSizeOf(mPtr);
  }

  bool IsSampled() const { return mSampled; }

  bool IsReported() const
  {
    MOZ_ASSERT(IsSane());
    bool isRep = mReporterName != gUnreportedName;
    return isRep;
  }

  const StackTrace* ReportStackTrace() const { return mReportStackTrace; }
  const char* ReporterName() const { return mReporterName; }

  // This is |const| thanks to the |mutable| fields above.
  void Report(Thread* aT, const char* aReporterName, bool aReportedOnAlloc)
       const;

  void UnreportIfNotReportedOnAlloc() const;

  // Hash policy.

  typedef const void* Lookup;

  static uint32_t hash(const void* const& aPc)
  {
    return mozilla::HashGeneric(aPc);
  }

  static bool match(const LiveBlock& aB, const void* const& aPtr)
  {
    return aB.mPtr == aPtr;
  }
};

// Nb: js::DefaultHasher<void*> is a high quality hasher.
typedef js::HashSet<LiveBlock, LiveBlock, InfallibleAllocPolicy> LiveBlockTable;
static LiveBlockTable* gLiveBlockTable = nullptr;

//---------------------------------------------------------------------------
// malloc/free callbacks
//---------------------------------------------------------------------------

static size_t gSampleBelowSize = 0;
static size_t gSmallBlockActualSizeCounter = 0;

static void
AllocCallback(void* aPtr, size_t aReqSize, Thread* aT)
{
  MOZ_ASSERT(gIsDMDRunning);

  if (!aPtr) {
    return;
  }

  AutoLockState lock;
  AutoBlockIntercepts block(aT);

  size_t actualSize = gMallocTable->malloc_usable_size(aPtr);

  if (actualSize < gSampleBelowSize) {
    // If this allocation is smaller than the sample-below size, increment the
    // cumulative counter.  Then, if that counter now exceeds the sample size,
    // blame this allocation for gSampleBelowSize bytes.  This precludes the
    // measurement of slop.
    gSmallBlockActualSizeCounter += actualSize;
    if (gSmallBlockActualSizeCounter >= gSampleBelowSize) {
      gSmallBlockActualSizeCounter -= gSampleBelowSize;

      LiveBlock b(aPtr, gSampleBelowSize, StackTrace::Get(aT),
                  /* sampled */ true);
      (void)gLiveBlockTable->putNew(aPtr, b);
    }
  } else {
    // If this block size is larger than the sample size, record it exactly.
    LiveBlock b(aPtr, aReqSize, StackTrace::Get(aT), /* sampled */ false);
    (void)gLiveBlockTable->putNew(aPtr, b);
  }
}

static void
FreeCallback(void* aPtr, Thread* aT)
{
  MOZ_ASSERT(gIsDMDRunning);

  if (!aPtr) {
    return;
  }

  AutoLockState lock;
  AutoBlockIntercepts block(aT);

  gLiveBlockTable->remove(aPtr);
}

//---------------------------------------------------------------------------
// malloc/free interception
//---------------------------------------------------------------------------

static void Init(const malloc_table_t* aMallocTable);

}   // namespace dmd
}   // namespace mozilla

void
replace_init(const malloc_table_t* aMallocTable)
{
  mozilla::dmd::Init(aMallocTable);
}

void*
replace_malloc(size_t aSize)
{
  using namespace mozilla::dmd;

  if (!gIsDMDRunning) {
    // DMD hasn't started up, either because it wasn't enabled by the user, or
    // we're still in Init() and something has indirectly called malloc.  Do a
    // vanilla malloc.  (In the latter case, if it fails we'll crash.  But
    // OOM is highly unlikely so early on.)
    return gMallocTable->malloc(aSize);
  }

  Thread* t = Thread::Fetch();
  if (t->interceptsAreBlocked()) {
    // Intercepts are blocked, which means this must be a call to malloc
    // triggered indirectly by DMD (e.g. via NS_StackWalk).  Be infallible.
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

  if (!gIsDMDRunning) {
    return gMallocTable->calloc(aCount, aSize);
  }

  Thread* t = Thread::Fetch();
  if (t->interceptsAreBlocked()) {
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

  if (!gIsDMDRunning) {
    return gMallocTable->realloc(aOldPtr, aSize);
  }

  Thread* t = Thread::Fetch();
  if (t->interceptsAreBlocked()) {
    return InfallibleAllocPolicy::realloc_(aOldPtr, aSize);
  }

  // If |aOldPtr| is NULL, the call is equivalent to |malloc(aSize)|.
  if (!aOldPtr) {
    return replace_malloc(aSize);
  }

  // Be very careful here!  Must remove the block from the table before doing
  // the realloc to avoid races, just like in replace_free().
  // Nb: This does an unnecessary hashtable remove+add if the block doesn't
  // move, but doing better isn't worth the effort.
  FreeCallback(aOldPtr, t);
  void* ptr = gMallocTable->realloc(aOldPtr, aSize);
  if (ptr) {
    AllocCallback(ptr, aSize, t);
  } else {
    // If realloc fails, we re-insert the old pointer.  It will look like it
    // was allocated for the first time here, which is untrue, and the slop
    // bytes will be zero, which may be untrue.  But this case is rare and
    // doing better isn't worth the effort.
    AllocCallback(aOldPtr, gMallocTable->malloc_usable_size(aOldPtr), t);
  }
  return ptr;
}

void*
replace_memalign(size_t aAlignment, size_t aSize)
{
  using namespace mozilla::dmd;

  if (!gIsDMDRunning) {
    return gMallocTable->memalign(aAlignment, aSize);
  }

  Thread* t = Thread::Fetch();
  if (t->interceptsAreBlocked()) {
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

  if (!gIsDMDRunning) {
    gMallocTable->free(aPtr);
    return;
  }

  Thread* t = Thread::Fetch();
  if (t->interceptsAreBlocked()) {
    return InfallibleAllocPolicy::free_(aPtr);
  }

  // Do the actual free after updating the table.  Otherwise, another thread
  // could call malloc and get the freed block and update the table, and then
  // our update here would remove the newly-malloc'd block.
  FreeCallback(aPtr, t);
  gMallocTable->free(aPtr);
}

namespace mozilla {
namespace dmd {

//---------------------------------------------------------------------------
// Live and double-report block groups
//---------------------------------------------------------------------------

class LiveBlockKey
{
public:
  const StackTrace* const mAllocStackTrace;   // never null
protected:
  const StackTrace* const mReportStackTrace;  // nullptr if unreported
  const char*       const mReporterName;      // gUnreportedName if unreported

public:
  LiveBlockKey(const LiveBlock& aB)
    : mAllocStackTrace(aB.mAllocStackTrace),
      mReportStackTrace(aB.ReportStackTrace()),
      mReporterName(aB.ReporterName())
  {
    MOZ_ASSERT(IsSane());
  }

  bool IsSane() const
  {
    bool hasReporterName = mReporterName != gUnreportedName;
    return mAllocStackTrace &&
           (( mReportStackTrace &&  hasReporterName) ||
            (!mReportStackTrace && !hasReporterName));
  }

  bool IsReported() const
  {
    MOZ_ASSERT(IsSane());
    bool isRep = mReporterName != gUnreportedName;
    return isRep;
  }

  // Hash policy.
  //
  // hash() and match() both assume that identical reporter names have
  // identical pointers.  In practice this always happens because they are
  // static strings (as specified in the NS_MEMORY_REPORTER_MALLOC_SIZEOF_FUN
  // macro).  This is true even for multi-reporters.  (If it ever became
  // untrue, the worst that would happen is that some blocks that should be in
  // the same block group would end up in separate block groups.)

  typedef LiveBlockKey Lookup;

  static uint32_t hash(const LiveBlockKey& aKey)
  {
    return mozilla::HashGeneric(aKey.mAllocStackTrace,
                                aKey.mReportStackTrace,
                                aKey.mReporterName);
  }

  static bool match(const LiveBlockKey& aA, const LiveBlockKey& aB)
  {
    return aA.mAllocStackTrace  == aB.mAllocStackTrace &&
           aA.mReportStackTrace == aB.mReportStackTrace &&
           aA.mReporterName     == aB.mReporterName;
  }
};

class DoubleReportBlockKey
{
public:
  const StackTrace* const mAllocStackTrace;     // never null

protected:
  // When double-reports occur we record (and later print) the stack trace
  // and reporter name of *both* the reporting locations.
  const StackTrace* const mReportStackTrace1;   // never null
  const StackTrace* const mReportStackTrace2;   // never null
  const char*       const mReporterName1;       // never gUnreportedName
  const char*       const mReporterName2;       // never gUnreportedName

public:
  DoubleReportBlockKey(const StackTrace* aAllocStackTrace,
                       const StackTrace* aReportStackTrace1,
                       const StackTrace* aReportStackTrace2,
                       const char* aReporterName1,
                       const char* aReporterName2)
    : mAllocStackTrace(aAllocStackTrace),
      mReportStackTrace1(aReportStackTrace1),
      mReportStackTrace2(aReportStackTrace2),
      mReporterName1(aReporterName1),
      mReporterName2(aReporterName2)
  {
    MOZ_ASSERT(IsSane());
  }

  bool IsSane() const
  {
    return mAllocStackTrace &&
           mReportStackTrace1 &&
           mReportStackTrace2 &&
           mReporterName1 != gUnreportedName &&
           mReporterName2 != gUnreportedName;
  }

  // Hash policy.
  //
  // hash() and match() both assume that identical reporter names have
  // identical pointers.  See LiveBlockKey for more.

  typedef DoubleReportBlockKey Lookup;

  static uint32_t hash(const DoubleReportBlockKey& aKey)
  {
    return mozilla::HashGeneric(aKey.mAllocStackTrace,
                                aKey.mReportStackTrace1,
                                aKey.mReportStackTrace2,
                                aKey.mReporterName1,
                                aKey.mReporterName2);
  }

  static bool match(const DoubleReportBlockKey& aA,
                    const DoubleReportBlockKey& aB)
  {
    return aA.mAllocStackTrace   == aB.mAllocStackTrace &&
           aA.mReportStackTrace1 == aB.mReportStackTrace1 &&
           aA.mReportStackTrace2 == aB.mReportStackTrace2 &&
           aA.mReporterName1     == aB.mReporterName1 &&
           aA.mReporterName2     == aB.mReporterName2;
  }
};

class GroupSize
{
  static const size_t kReqBits = sizeof(size_t) * 8 - 1;  // 31 or 63

  size_t mReq;              // size requested
  size_t mSlop:kReqBits;    // slop bytes
  size_t mSampled:1;        // were one or more blocks contributing to this
                            //   GroupSize sampled?
public:
  GroupSize()
    : mReq(0),
      mSlop(0),
      mSampled(false)
  {}

  size_t Req()    const { return mReq; }
  size_t Slop()   const { return mSlop; }
  size_t Usable() const { return mReq + mSlop; }

  bool IsSampled() const { return mSampled; }

  void Add(const LiveBlock& aB)
  {
    mReq  += aB.ReqSize();
    mSlop += aB.SlopSize();
    mSampled = mSampled || aB.IsSampled();
  }

  void Add(const GroupSize& aGroupSize)
  {
    mReq  += aGroupSize.Req();
    mSlop += aGroupSize.Slop();
    mSampled = mSampled || aGroupSize.IsSampled();
  }

  static int Cmp(const GroupSize& aA, const GroupSize& aB)
  {
    // Primary sort: put bigger usable sizes before smaller usable sizes.
    if (aA.Usable() > aB.Usable()) return -1;
    if (aA.Usable() < aB.Usable()) return  1;

    // Secondary sort: put non-sampled groups before sampled groups.
    if (!aA.mSampled &&  aB.mSampled) return -1;
    if ( aA.mSampled && !aB.mSampled) return  1;

    return 0;
  }
};

class BlockGroup
{
protected:
  // {Live,DoubleReport}BlockKey serve as the key in
  // {Live,DoubleReport}BlockGroupTable.  Thes two fields constitute the value,
  // so it's ok for them to be |mutable|.
  mutable uint32_t  mNumBlocks;     // number of blocks with this LiveBlockKey
  mutable GroupSize mGroupSize;     // combined size of those blocks

public:
  BlockGroup()
    : mNumBlocks(0),
      mGroupSize()
  {}

  const GroupSize& GroupSize() const { return mGroupSize; }

  // This is |const| thanks to the |mutable| fields above.
  void Add(const LiveBlock& aB) const
  {
    mNumBlocks++;
    mGroupSize.Add(aB);
  }

  static const char* const kName;   // for PrintSortedGroups
};

const char* const BlockGroup::kName = "block";

// A group of one or more live heap blocks with a common LiveBlockKey.
class LiveBlockGroup : public LiveBlockKey, public BlockGroup
{
  friend class FrameGroup;      // FrameGroups are created from LiveBlockGroups

public:
  explicit LiveBlockGroup(const LiveBlockKey& aKey)
    : LiveBlockKey(aKey),
      BlockGroup()
  {}

  void Print(const Writer& aWriter, uint32_t aM, uint32_t aN,
             const char* aStr, const char* astr,
             size_t aCategoryUsableSize, size_t aCumulativeUsableSize,
             size_t aTotalUsableSize) const;

  static int QsortCmp(const void* aA, const void* aB)
  {
    const LiveBlockGroup* const a =
      *static_cast<const LiveBlockGroup* const*>(aA);
    const LiveBlockGroup* const b =
      *static_cast<const LiveBlockGroup* const*>(aB);

    return GroupSize::Cmp(a->mGroupSize, b->mGroupSize);
  }
};

typedef js::HashSet<LiveBlockGroup, LiveBlockGroup, InfallibleAllocPolicy>
        LiveBlockGroupTable;

void
LiveBlockGroup::Print(const Writer& aWriter, uint32_t aM, uint32_t aN,
                      const char* aStr, const char* astr,
                      size_t aCategoryUsableSize, size_t aCumulativeUsableSize,
                      size_t aTotalUsableSize) const
{
  bool showTilde = mGroupSize.IsSampled();

  W("%s: %s block%s in block group %s of %s\n",
    aStr,
    Show(mNumBlocks, gBuf1, kBufLen, showTilde), Plural(mNumBlocks),
    Show(aM, gBuf2, kBufLen),
    Show(aN, gBuf3, kBufLen));

  W(" %s bytes (%s requested / %s slop)\n",
    Show(mGroupSize.Usable(), gBuf1, kBufLen, showTilde),
    Show(mGroupSize.Req(),    gBuf2, kBufLen, showTilde),
    Show(mGroupSize.Slop(),   gBuf3, kBufLen, showTilde));

  W(" %4.2f%% of the heap (%4.2f%% cumulative); "
    " %4.2f%% of %s (%4.2f%% cumulative)\n",
    Percent(mGroupSize.Usable(), aTotalUsableSize),
    Percent(aCumulativeUsableSize, aTotalUsableSize),
    Percent(mGroupSize.Usable(), aCategoryUsableSize),
    astr,
    Percent(aCumulativeUsableSize, aCategoryUsableSize));

  W(" Allocated at\n");
  mAllocStackTrace->Print(aWriter);

  if (IsReported()) {
    W("\n Reported by '%s' at\n", mReporterName);
    mReportStackTrace->Print(aWriter);
  }

  W("\n");
}

// A group of one or more double-reported heap blocks with a common
// DoubleReportBlockKey.
class DoubleReportBlockGroup : public DoubleReportBlockKey, public BlockGroup
{
public:
  explicit DoubleReportBlockGroup(const DoubleReportBlockKey& aKey)
    : DoubleReportBlockKey(aKey),
      BlockGroup()
  {}

  void Print(const Writer& aWriter, uint32_t aM, uint32_t aN,
             const char* aStr, const char* astr,
             size_t aCategoryUsableSize, size_t aCumulativeUsableSize,
             size_t aTotalUsableSize) const;

  static int QsortCmp(const void* aA, const void* aB)
  {
    const DoubleReportBlockGroup* const a =
      *static_cast<const DoubleReportBlockGroup* const*>(aA);
    const DoubleReportBlockGroup* const b =
      *static_cast<const DoubleReportBlockGroup* const*>(aB);

    return GroupSize::Cmp(a->mGroupSize, b->mGroupSize);
  }
};

typedef js::HashSet<DoubleReportBlockGroup, DoubleReportBlockGroup,
                    InfallibleAllocPolicy> DoubleReportBlockGroupTable;
DoubleReportBlockGroupTable* gDoubleReportBlockGroupTable = nullptr;

void
DoubleReportBlockGroup::Print(const Writer& aWriter, uint32_t aM, uint32_t aN,
                              const char* aStr, const char* astr,
                              size_t aCategoryUsableSize,
                              size_t aCumulativeUsableSize,
                              size_t aTotalUsableSize) const
{
  bool showTilde = mGroupSize.IsSampled();

  W("%s: %s block%s in block group %s of %s\n",
    aStr,
    Show(mNumBlocks, gBuf1, kBufLen, showTilde), Plural(mNumBlocks),
    Show(aM, gBuf2, kBufLen),
    Show(aN, gBuf3, kBufLen));

  W(" %s bytes (%s requested / %s slop)\n",
    Show(mGroupSize.Usable(), gBuf1, kBufLen, showTilde),
    Show(mGroupSize.Req(),    gBuf2, kBufLen, showTilde),
    Show(mGroupSize.Slop(),   gBuf3, kBufLen, showTilde));

  W(" Allocated at\n");
  mAllocStackTrace->Print(aWriter);

  W("\n Previously reported by '%s' at\n", mReporterName1);
  mReportStackTrace1->Print(aWriter);

  W("\n Now reported by '%s' at\n", mReporterName2);
  mReportStackTrace2->Print(aWriter);

  W("\n");
}

//---------------------------------------------------------------------------
// Stack frame groups
//---------------------------------------------------------------------------

// A group of one or more stack frames (from heap block allocation stack
// traces) with a common PC.
class FrameGroup
{
  // mPc is used as the key in FrameGroupTable, and the other members
  // constitute the value, so it's ok for them to be |mutable|.
  const void* const mPc;
  mutable size_t    mNumBlocks;
  mutable size_t    mNumBlockGroups;
  mutable GroupSize mGroupSize;

public:
  explicit FrameGroup(const void* aPc)
    : mPc(aPc),
      mNumBlocks(0),
      mNumBlockGroups(0),
      mGroupSize()
  {}

  const GroupSize& GroupSize() const { return mGroupSize; }

  // This is |const| thanks to the |mutable| fields above.
  void Add(const LiveBlockGroup& aBg) const
  {
    mNumBlocks += aBg.mNumBlocks;
    mNumBlockGroups++;
    mGroupSize.Add(aBg.mGroupSize);
  }

  void Print(const Writer& aWriter, uint32_t aM, uint32_t aN,
             const char* aStr, const char* astr,
             size_t aCategoryUsableSize, size_t aCumulativeUsableSize,
             size_t aTotalUsableSize) const;

  static int QsortCmp(const void* aA, const void* aB)
  {
    const FrameGroup* const a = *static_cast<const FrameGroup* const*>(aA);
    const FrameGroup* const b = *static_cast<const FrameGroup* const*>(aB);

    return GroupSize::Cmp(a->mGroupSize, b->mGroupSize);
  }

  static const char* const kName;   // for PrintSortedGroups

  // Hash policy.

  typedef const void* Lookup;

  static uint32_t hash(const void* const& aPc)
  {
    return mozilla::HashGeneric(aPc);
  }

  static bool match(const FrameGroup& aFg, const void* const& aPc)
  {
    return aFg.mPc == aPc;
  }
};

const char* const FrameGroup::kName = "frame";

typedef js::HashSet<FrameGroup, FrameGroup, InfallibleAllocPolicy>
        FrameGroupTable;

void
FrameGroup::Print(const Writer& aWriter, uint32_t aM, uint32_t aN,
                  const char* aStr, const char* astr,
                  size_t aCategoryUsableSize, size_t aCumulativeUsableSize,
                  size_t aTotalUsableSize) const
{
  (void)aCumulativeUsableSize;

  bool showTilde = mGroupSize.IsSampled();

  nsCodeAddressDetails details;
  PcInfo(mPc, &details);

  W("%s: %s block%s and %s block group%s in frame group %s of %s\n",
    aStr,
    Show(mNumBlocks, gBuf1, kBufLen, showTilde), Plural(mNumBlocks),
    Show(mNumBlockGroups, gBuf2, kBufLen, showTilde), Plural(mNumBlockGroups),
    Show(aM, gBuf3, kBufLen),
    Show(aN, gBuf4, kBufLen));

  W(" %s bytes (%s requested / %s slop)\n",
    Show(mGroupSize.Usable(), gBuf1, kBufLen, showTilde),
    Show(mGroupSize.Req(),    gBuf2, kBufLen, showTilde),
    Show(mGroupSize.Slop(),   gBuf3, kBufLen, showTilde));

  W(" %4.2f%% of the heap;  %4.2f%% of %s\n",
    Percent(mGroupSize.Usable(), aTotalUsableSize),
    Percent(mGroupSize.Usable(), aCategoryUsableSize),
    astr);

  W(" PC is\n");
  W("   %s[%s +0x%X] %p\n\n", details.function, details.library,
    details.loffset, mPc);
}

//---------------------------------------------------------------------------
// DMD start-up
//---------------------------------------------------------------------------

static void RunTestMode(FILE* fp);
static void RunStressMode(FILE* fp);

static const char* gDMDEnvVar = nullptr;

// Given an |aOptionName| like "foo", succeed if |aArg| has the form "foo=blah"
// (where "blah" is non-empty) and return the pointer to "blah".  |aArg| can
// have leading space chars (but not other whitespace).
static const char*
OptionValueIfMatch(const char* aArg, const char* aOptionName)
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
static bool
OptionLong(const char* aArg, const char* aOptionName, long aMin, long aMax,
           long* aN)
{
  if (const char* optionValue = OptionValueIfMatch(aArg, aOptionName)) {
    char* endPtr;
    *aN = strtol(optionValue, &endPtr, /* base */ 10);
    if (!*endPtr && aMin <= *aN && *aN <= aMax &&
        *aN != LONG_MIN && *aN != LONG_MAX) {
      return true;
    }
  }
  return false;
}

static const size_t gMaxSampleBelowSize = 100 * 1000 * 1000;    // bytes

// Default to sampling with a sample-below size that's a prime number close to
// 4096.
//
// Using a sample-below size ~= 4096 is much faster than using a sample-below
// size of 1, and it's not much less accurate in practice, so it's a reasonable
// default.
//
// Using a prime sample-below size makes our sampling more random.  If we used
// instead a sample-below size of 4096, for example, then if all our allocation
// sizes were even (which they likely are, due to how jemalloc rounds up), our
// alloc counter would take on only even values.
//
// In contrast, using a prime sample-below size lets us explore all possible
// values of the alloc counter.
static const size_t gDefaultSampleBelowSize = 4093;

static void
BadArg(const char* aArg)
{
  StatusMsg("\n");
  StatusMsg("Bad entry in the $DMD environment variable: '%s'.\n", aArg);
  StatusMsg("\n");
  StatusMsg("Valid values of $DMD are:\n");
  StatusMsg("- undefined or \"\" or \"0\", which disables DMD, or\n");
  StatusMsg("- \"1\", which enables it with the default options, or\n");
  StatusMsg("- a whitespace-separated list of |--option=val| entries, which\n");
  StatusMsg("  enables it with non-default options.\n");
  StatusMsg("\n");
  StatusMsg("The following options are allowed;  defaults are shown in [].\n");
  StatusMsg("  --sample-below=<1..%d> Sample blocks smaller than this [%d]\n"
            "                         (prime numbers recommended).\n",
            int(gMaxSampleBelowSize), int(gDefaultSampleBelowSize));
  StatusMsg("  --mode=<normal|test|stress>   Which mode to run in? [normal]\n");
  StatusMsg("\n");
  exit(1);
}

// Note that fopen() can allocate.
static FILE*
OpenTestOrStressFile(const char* aFilename)
{
  FILE* fp = fopen(aFilename, "w");
  if (!fp) {
    StatusMsg("can't create %s file: %s\n", aFilename, strerror(errno));
    exit(1);
  }
  return fp;
}

// WARNING: this function runs *very* early -- before all static initializers
// have run.  For this reason, non-scalar globals such as gStateLock and
// gStackTraceTable are allocated dynamically (so we can guarantee their
// construction in this function) rather than statically.
static void
Init(const malloc_table_t* aMallocTable)
{
  MOZ_ASSERT(!gIsDMDRunning);

  gMallocTable = aMallocTable;

  // Set defaults of things that can be affected by the $DMD env var.
  gMode = Normal;
  gSampleBelowSize = gDefaultSampleBelowSize;

  // DMD is controlled by the |DMD| environment variable.
  // - If it's unset or empty or "0", DMD doesn't run.
  // - Otherwise, the contents dictate DMD's behaviour.

  char* e = getenv("DMD");

  StatusMsg("$DMD = '%s'\n", e);

  if (!e || strcmp(e, "") == 0 || strcmp(e, "0") == 0) {
    StatusMsg("DMD is not enabled\n");
    return;
  }

  // Save it so we can print it in Dump().
  gDMDEnvVar = e = InfallibleAllocPolicy::strdup_(e);

  if (strcmp(e, "1") != 0) {
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
      if (OptionLong(arg, "--sample-below", 1, gMaxSampleBelowSize, &myLong)) {
        gSampleBelowSize = myLong;

      } else if (strcmp(arg, "--mode=normal") == 0) {
        gMode = Normal;
      } else if (strcmp(arg, "--mode=test")   == 0) {
        gMode = Test;
      } else if (strcmp(arg, "--mode=stress") == 0) {
        gMode = Stress;

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

  // Finished parsing $DMD.

  StatusMsg("DMD is enabled\n");

  gStateLock = InfallibleAllocPolicy::new_<Mutex>();

  gSmallBlockActualSizeCounter = 0;

  DMD_CREATE_TLS_INDEX(gTlsIndex);

  gStackTraceTable = InfallibleAllocPolicy::new_<StackTraceTable>();
  gStackTraceTable->init(8192);

  gLiveBlockTable = InfallibleAllocPolicy::new_<LiveBlockTable>();
  gLiveBlockTable->init(8192);

  gDoubleReportBlockGroupTable =
    InfallibleAllocPolicy::new_<DoubleReportBlockGroupTable>();
  gDoubleReportBlockGroupTable->init(0);

  if (gMode == Test) {
    // OpenTestOrStressFile() can allocate.  So do this before setting
    // gIsDMDRunning so those allocations don't show up in our results.  Once
    // gIsDMDRunning is set we are intercepting malloc et al. in earnest.
    FILE* fp = OpenTestOrStressFile("test.dmd");
    gIsDMDRunning = true;

    StatusMsg("running test mode...\n");
    RunTestMode(fp);
    StatusMsg("finished test mode\n");
    fclose(fp);
    exit(0);
  }

  if (gMode == Stress) {
    FILE* fp = OpenTestOrStressFile("stress.dmd");
    gIsDMDRunning = true;

    StatusMsg("running stress mode...\n");
    RunStressMode(fp);
    StatusMsg("finished stress mode\n");
    fclose(fp);
    exit(0);
  }

  gIsDMDRunning = true;
}

//---------------------------------------------------------------------------
// DMD reporting and unreporting
//---------------------------------------------------------------------------

void
LiveBlock::Report(Thread* aT, const char* aReporterName, bool aOnAlloc) const
{
  if (IsReported()) {
    DoubleReportBlockKey doubleReportKey(mAllocStackTrace,
                                         mReportStackTrace, StackTrace::Get(aT),
                                         mReporterName, aReporterName);
    DoubleReportBlockGroupTable::AddPtr p =
      gDoubleReportBlockGroupTable->lookupForAdd(doubleReportKey);
    if (!p) {
      DoubleReportBlockGroup bg(doubleReportKey);
      (void)gDoubleReportBlockGroupTable->add(p, bg);
    }
    p->Add(*this);

  } else {
    mReporterName     = aReporterName;
    mReportStackTrace = StackTrace::Get(aT);
    mReportedOnAlloc  = aOnAlloc;
  }
}

void
LiveBlock::UnreportIfNotReportedOnAlloc() const
{
  if (!mReportedOnAlloc) {
    mReporterName     = gUnreportedName;
    mReportStackTrace = nullptr;
  }
}

static void
ReportHelper(const void* aPtr, const char* aReporterName, bool aOnAlloc)
{
  if (!gIsDMDRunning || !aPtr) {
    return;
  }

  Thread* t = Thread::Fetch();

  AutoBlockIntercepts block(t);
  AutoLockState lock;

  if (LiveBlockTable::Ptr p = gLiveBlockTable->lookup(aPtr)) {
    p->Report(t, aReporterName, aOnAlloc);
  } else {
    // We have no record of the block.  Do nothing.  Either:
    // - We're sampling and we skipped this block.  This is likely.
    // - It's a bogus pointer.  This is unlikely because Report() is almost
    //   always called in conjunction with a malloc_size_of-style function.
  }
}

MOZ_EXPORT void
Report(const void* aPtr, const char* aReporterName)
{
  ReportHelper(aPtr, aReporterName, /* onAlloc */ false);
}

MOZ_EXPORT void
ReportOnAlloc(const void* aPtr, const char* aReporterName)
{
  ReportHelper(aPtr, aReporterName, /* onAlloc */ true);
}

//---------------------------------------------------------------------------
// DMD output
//---------------------------------------------------------------------------

// This works for LiveBlockGroups, DoubleReportBlockGroups and FrameGroups.
template <class TGroup>
static void
PrintSortedGroups(const Writer& aWriter, const char* aStr, const char* astr,
                  const js::HashSet<TGroup, TGroup, InfallibleAllocPolicy>& aTGroupTable,
                  size_t aCategoryUsableSize, size_t aTotalUsableSize)
{
  const char* name = TGroup::kName;
  StatusMsg("  creating and sorting %s %s group array...\n", astr, name);

  // Convert the table into a sorted array.
  js::Vector<const TGroup*, 0, InfallibleAllocPolicy> tgArray;
  tgArray.reserve(aTGroupTable.count());
  typedef js::HashSet<TGroup, TGroup, InfallibleAllocPolicy> TGroupTable;
  for (typename TGroupTable::Range r = aTGroupTable.all();
       !r.empty();
       r.popFront()) {
    tgArray.infallibleAppend(&r.front());
  }
  qsort(tgArray.begin(), tgArray.length(), sizeof(tgArray[0]),
        TGroup::QsortCmp);

  WriteTitle("%s %ss\n", aStr, name);

  if (tgArray.length() == 0) {
    W("(none)\n\n");
    return;
  }

  // Limit the number of block groups printed, because fix-linux-stack.pl is
  // too damn slow.  Note that we don't break out of this loop because we need
  // to keep adding to |cumulativeUsableSize|.
  static const uint32_t MaxTGroups = 1000;
  uint32_t numTGroups = tgArray.length();

  StatusMsg("  printing %s %s group array...\n", astr, name);
  size_t cumulativeUsableSize = 0;
  for (uint32_t i = 0; i < numTGroups; i++) {
    const TGroup* tg = tgArray[i];
    cumulativeUsableSize += tg->GroupSize().Usable();
    if (i < MaxTGroups) {
      tg->Print(aWriter, i+1, numTGroups, aStr, astr, aCategoryUsableSize,
                cumulativeUsableSize, aTotalUsableSize);
    } else if (i == MaxTGroups) {
      W("%s: stopping after %s %s groups\n\n", aStr,
        Show(MaxTGroups, gBuf1, kBufLen), name);
    }
  }

  MOZ_ASSERT(aCategoryUsableSize == kNoSize ||
             aCategoryUsableSize == cumulativeUsableSize);
}

static void
PrintSortedBlockAndFrameGroups(const Writer& aWriter,
                               const char* aStr, const char* astr,
                               const LiveBlockGroupTable& aLiveBlockGroupTable,
                               size_t aCategoryUsableSize,
                               size_t aTotalUsableSize)
{
  PrintSortedGroups(aWriter, aStr, astr, aLiveBlockGroupTable,
                    aCategoryUsableSize, aTotalUsableSize);

  // Frame groups are totally dependent on vagaries of stack traces, so we
  // can't show them in test mode.
  if (gMode == Test) {
    return;
  }

  FrameGroupTable frameGroupTable;
  frameGroupTable.init(2048);
  for (LiveBlockGroupTable::Range r = aLiveBlockGroupTable.all();
       !r.empty();
       r.popFront()) {
    const LiveBlockGroup& bg = r.front();
    const StackTrace* st = bg.mAllocStackTrace;

    // A single PC can appear multiple times in a stack trace.  We ignore
    // duplicates by first sorting and then ignoring adjacent duplicates.
    StackTrace sorted(*st);
    sorted.Sort();              // sorts the copy, not the original
    void* prevPc = (void*)intptr_t(-1);
    for (uint32_t i = 0; i < sorted.Length(); i++) {
      void* pc = sorted.Pc(i);
      if (pc == prevPc) {
        continue;               // ignore duplicate
      }
      prevPc = pc;

      FrameGroupTable::AddPtr p = frameGroupTable.lookupForAdd(pc);
      if (!p) {
        FrameGroup fg(pc);
        (void)frameGroupTable.add(p, fg);
      }
      p->Add(bg);
    }
  }
  PrintSortedGroups(aWriter, aStr, astr, frameGroupTable, kNoSize,
                    aTotalUsableSize);
}

// Note that, unlike most SizeOf* functions, this function does not take a
// |nsMallocSizeOfFun| argument.  That's because those arguments are primarily
// to aid DMD track heap blocks... but DMD deliberately doesn't track heap
// blocks it allocated for itself!
MOZ_EXPORT void
SizeOf(Sizes* aSizes)
{
  aSizes->mStackTraces = 0;
  for (StackTraceTable::Range r = gStackTraceTable->all();
       !r.empty();
       r.popFront()) {
    StackTrace* const& st = r.front();
    aSizes->mStackTraces += MallocSizeOf(st);
  }

  aSizes->mStackTraceTable =
    gStackTraceTable->sizeOfIncludingThis(MallocSizeOf);

  aSizes->mLiveBlockTable = gLiveBlockTable->sizeOfIncludingThis(MallocSizeOf);

  aSizes->mDoubleReportTable =
    gDoubleReportBlockGroupTable->sizeOfIncludingThis(MallocSizeOf);
}

static void
ClearState()
{
  // Unreport all blocks, except those that were reported on allocation,
  // because they need to keep their reported marking.
  for (LiveBlockTable::Range r = gLiveBlockTable->all();
       !r.empty();
       r.popFront()) {
    r.front().UnreportIfNotReportedOnAlloc();
  }

  // Clear errors.
  gDoubleReportBlockGroupTable->finish();
  gDoubleReportBlockGroupTable->init();
}

MOZ_EXPORT void
Dump(Writer aWriter)
{
  if (!gIsDMDRunning) {
    const char* msg = "cannot Dump();  DMD was not enabled at startup\n";
    StatusMsg("%s", msg);
    W("%s", msg);
    return;
  }

  AutoBlockIntercepts block(Thread::Fetch());
  AutoLockState lock;

  static int dumpCount = 1;
  StatusMsg("Dump %d {\n", dumpCount++);

  StatusMsg("  gathering live block groups...\n");

  LiveBlockGroupTable unreportedLiveBlockGroupTable;
  (void)unreportedLiveBlockGroupTable.init(1024);
  size_t unreportedUsableSize = 0;

  LiveBlockGroupTable reportedLiveBlockGroupTable;
  (void)reportedLiveBlockGroupTable.init(1024);
  size_t reportedUsableSize = 0;

  bool anyBlocksSampled = false;

  for (LiveBlockTable::Range r = gLiveBlockTable->all();
       !r.empty();
       r.popFront()) {
    const LiveBlock& b = r.front();

    size_t& size = !b.IsReported() ? unreportedUsableSize : reportedUsableSize;
    size += b.UsableSize();

    LiveBlockGroupTable& table = !b.IsReported()
                               ? unreportedLiveBlockGroupTable
                               : reportedLiveBlockGroupTable;
    LiveBlockKey liveKey(b);
    LiveBlockGroupTable::AddPtr p = table.lookupForAdd(liveKey);
    if (!p) {
      LiveBlockGroup bg(b);
      (void)table.add(p, bg);
    }
    p->Add(b);

    anyBlocksSampled = anyBlocksSampled || b.IsSampled();
  }
  size_t totalUsableSize = unreportedUsableSize + reportedUsableSize;

  WriteTitle("Invocation\n");
  W("$DMD = '%s'\n", gDMDEnvVar);
  W("Sample-below size = %lld\n\n", (long long)(gSampleBelowSize));

  PrintSortedGroups(aWriter, "Double-reported", "double-reported",
                    *gDoubleReportBlockGroupTable, kNoSize, kNoSize);

  PrintSortedBlockAndFrameGroups(aWriter, "Unreported", "unreported",
                                 unreportedLiveBlockGroupTable,
                                 unreportedUsableSize, totalUsableSize);

  PrintSortedBlockAndFrameGroups(aWriter, "Reported", "reported",
                                 reportedLiveBlockGroupTable,
                                 reportedUsableSize, totalUsableSize);

  bool showTilde = anyBlocksSampled;
  WriteTitle("Summary\n");
  W("Total:      %s bytes\n",
    Show(totalUsableSize, gBuf1, kBufLen, showTilde));
  W("Reported:   %s bytes (%5.2f%%)\n",
    Show(reportedUsableSize, gBuf1, kBufLen, showTilde),
    Percent(reportedUsableSize, totalUsableSize));
  W("Unreported: %s bytes (%5.2f%%)\n",
    Show(unreportedUsableSize, gBuf1, kBufLen, showTilde),
    Percent(unreportedUsableSize, totalUsableSize));

  W("\n");

  // Stats are non-deterministic, so don't show them in test mode.
  if (gMode != Test) {
    Sizes sizes;
    SizeOf(&sizes);

    WriteTitle("Execution measurements\n");

    W("Data structures that persist after Dump() ends:\n");

    W("  Stack traces:        %10s bytes\n",
      Show(sizes.mStackTraces, gBuf1, kBufLen));

    W("  Stack trace table:   %10s bytes (%s entries, %s used)\n",
      Show(sizes.mStackTraceTable,       gBuf1, kBufLen),
      Show(gStackTraceTable->capacity(), gBuf2, kBufLen),
      Show(gStackTraceTable->count(),    gBuf3, kBufLen));

    W("  Live block table:    %10s bytes (%s entries, %s used)\n",
      Show(sizes.mLiveBlockTable,       gBuf1, kBufLen),
      Show(gLiveBlockTable->capacity(), gBuf2, kBufLen),
      Show(gLiveBlockTable->count(),    gBuf3, kBufLen));

    W("\nData structures that are cleared after Dump() ends:\n");

    W("  Double-report table: %10s bytes (%s entries, %s used)\n",
      Show(sizes.mDoubleReportTable,                 gBuf1, kBufLen),
      Show(gDoubleReportBlockGroupTable->capacity(), gBuf2, kBufLen),
      Show(gDoubleReportBlockGroupTable->count(),    gBuf3, kBufLen));

    size_t unreportedSize =
      unreportedLiveBlockGroupTable.sizeOfIncludingThis(MallocSizeOf);
    W("  Unreported table:    %10s bytes (%s entries, %s used)\n",
      Show(unreportedSize,                           gBuf1, kBufLen),
      Show(unreportedLiveBlockGroupTable.capacity(), gBuf2, kBufLen),
      Show(unreportedLiveBlockGroupTable.count(),    gBuf3, kBufLen));

    size_t reportedSize =
      reportedLiveBlockGroupTable.sizeOfIncludingThis(MallocSizeOf);
    W("  Reported table:      %10s bytes (%s entries, %s used)\n",
      Show(reportedSize,                           gBuf1, kBufLen),
      Show(reportedLiveBlockGroupTable.capacity(), gBuf2, kBufLen),
      Show(reportedLiveBlockGroupTable.count(),    gBuf3, kBufLen));

    W("\n");
  }

  ClearState();

  StatusMsg("}\n");
}

//---------------------------------------------------------------------------
// Testing
//---------------------------------------------------------------------------

// This function checks that heap blocks that have the same stack trace but
// different (or no) reporters get aggregated separately.
void foo()
{
   char* a[6];
   for (int i = 0; i < 6; i++) {
      a[i] = (char*) malloc(128 - 16*i);
   }

   for (int i = 0; i <= 1; i++)
      Report(a[i], "a01");              // reported
   Report(a[2], "a23");                 // reported
   Report(a[3], "a23");                 // reported
   // a[4], a[5] unreported
}

// This stops otherwise-unused variables from being optimized away.
static void
UseItOrLoseIt(void* a)
{
  if (a == 0) {
    fprintf(stderr, "UseItOrLoseIt: %p\n", a);
  }
}

// The output from this should be compared against test-expected.dmd.  It's
// been tested on Linux64, and probably will give different results on other
// platforms.
static void
RunTestMode(FILE* fp)
{
  Writer writer(FpWrite, fp);

  // The first part of this test requires sampling to be disabled.
  gSampleBelowSize = 1;

  // 0th Dump.  Zero for everything.
  Dump(writer);

  // 1st Dump: 1 freed, 9 out of 10 unreported.
  // 2nd Dump: still present and unreported.
  int i;
  char* a;
  for (i = 0; i < 10; i++) {
      a = (char*) malloc(100);
      UseItOrLoseIt(a);
  }
  free(a);

  // Min-sized block.
  // 1st Dump: reported.
  // 2nd Dump: re-reported, twice;  double-report warning.
  char* a2 = (char*) malloc(0);
  Report(a2, "a2");

  // Operator new[].
  // 1st Dump: reported.
  // 2nd Dump: reportedness carries over, due to ReportOnAlloc.
  char* b = new char[10];
  ReportOnAlloc(b, "b");

  // ReportOnAlloc, then freed.
  // 1st Dump: freed, irrelevant.
  // 2nd Dump: freed, irrelevant.
  char* b2 = new char;
  ReportOnAlloc(b2, "b2");
  free(b2);

  // 1st Dump: reported, plus 3 double-report warnings.
  // 2nd Dump: freed, irrelevant.
  char* c = (char*) calloc(10, 3);
  Report(c, "c");
  for (int i = 0; i < 3; i++) {
    Report(c, "c");
  }

  // 1st Dump: ignored.
  // 2nd Dump: irrelevant.
  Report((void*)(intptr_t)i, "d");

  // jemalloc rounds this up to 8192.
  // 1st Dump: reported.
  // 2nd Dump: freed.
  char* e = (char*) malloc(4096);
  e = (char*) realloc(e, 4097);
  Report(e, "e");

  // First realloc is like malloc;  second realloc is shrinking.
  // 1st Dump: reported.
  // 2nd Dump: re-reported.
  char* e2 = (char*) realloc(nullptr, 1024);
  e2 = (char*) realloc(e2, 512);
  Report(e2, "e2");

  // First realloc is like malloc;  second realloc creates a min-sized block.
  // 1st Dump: reported (re-use "a2" reporter name because the order of this
  //           report and the "a2" above is non-deterministic).
  // 2nd Dump: freed, irrelevant.
  char* e3 = (char*) realloc(nullptr, 1024);
  e3 = (char*) realloc(e3, 0);
  MOZ_ASSERT(e3);
  Report(e3, "a2");

  // 1st Dump: freed, irrelevant.
  // 2nd Dump: freed, irrelevant.
  char* f = (char*) malloc(64);
  free(f);

  // 1st Dump: ignored.
  // 2nd Dump: irrelevant.
  Report((void*)(intptr_t)0x0, "zero");

  // 1st Dump: mixture of reported and unreported.
  // 2nd Dump: all unreported.
  foo();
  foo();

  // All the odd-ball ones.
  // 1st Dump: all unreported.
  // 2nd Dump: all freed, irrelevant.
  // XXX: no memalign on Mac
//void* x = memalign(64, 65);           // rounds up to 128
//UseItOrLoseIt(x);
  // XXX: posix_memalign doesn't work on B2G, apparently
//void* y;
//posix_memalign(&y, 128, 129);         // rounds up to 256
//UseItOrLoseIt(y);
  void* z = valloc(1);                  // rounds up to 4096
  UseItOrLoseIt(z);
//aligned_alloc(64, 256);               // XXX: C11 only

  // 1st Dump.
  Dump(writer);

  //---------

  Report(a2, "a2b");
  Report(a2, "a2b");
  free(c);
  free(e);
  Report(e2, "e2b");
  free(e3);
//free(x);
//free(y);
  free(z);

  // 2nd Dump.
  Dump(writer);

  //---------

  // Clear all knowledge of existing blocks to give us a clean slate.
  gLiveBlockTable->clear();

  // Reset the counter just in case |sample-size| was specified in $DMD.
  // Otherwise the assertions fail.
  gSmallBlockActualSizeCounter = 0;
  gSampleBelowSize = 128;

  char* s;

  // This equals the sample size, and so is recorded exactly.  It should be
  // listed before groups of the same size that are sampled.
  s = (char*) malloc(128);
  UseItOrLoseIt(s);

  // This exceeds the sample size, and so is recorded exactly.
  s = (char*) malloc(144);
  UseItOrLoseIt(s);

  // These together constitute exactly one sample.
  for (int i = 0; i < 16; i++) {
    s = (char*) malloc(8);
    UseItOrLoseIt(s);
  }
  MOZ_ASSERT(gSmallBlockActualSizeCounter == 0);

  // These fall 8 bytes short of a full sample.
  for (int i = 0; i < 15; i++) {
    s = (char*) malloc(8);
    UseItOrLoseIt(s);
  }
  MOZ_ASSERT(gSmallBlockActualSizeCounter == 120);

  // This exceeds the sample size, and so is recorded exactly.
  s = (char*) malloc(256);
  UseItOrLoseIt(s);
  MOZ_ASSERT(gSmallBlockActualSizeCounter == 120);

  // This gets more than to a full sample from the |i < 15| loop above.
  s = (char*) malloc(96);
  UseItOrLoseIt(s);
  MOZ_ASSERT(gSmallBlockActualSizeCounter == 88);

  // This gets to another full sample.
  for (int i = 0; i < 5; i++) {
    s = (char*) malloc(8);
    UseItOrLoseIt(s);
  }
  MOZ_ASSERT(gSmallBlockActualSizeCounter == 0);

  // This allocates 16, 32, ..., 128 bytes, which results a block group that
  // contains a mix of sample and non-sampled blocks, and so should be printed
  // with '~' signs.
  for (int i = 1; i <= 8; i++) {
    s = (char*) malloc(i * 16);
    UseItOrLoseIt(s);
  }
  MOZ_ASSERT(gSmallBlockActualSizeCounter == 64);

  // At the end we're 64 bytes into the current sample so we report ~1,424
  // bytes of allocation overall, which is 64 less than the real value 1,488.

  Dump(writer);
}

//---------------------------------------------------------------------------
// Stress testing microbenchmark
//---------------------------------------------------------------------------

MOZ_NEVER_INLINE static void
stress5()
{
  for (int i = 0; i < 10; i++) {
    void* x = malloc(64);
    UseItOrLoseIt(x);
    if (i & 1) {
      free(x);
    }
  }
}

MOZ_NEVER_INLINE static void
stress4()
{
  stress5(); stress5(); stress5(); stress5(); stress5();
  stress5(); stress5(); stress5(); stress5(); stress5();
}

MOZ_NEVER_INLINE static void
stress3()
{
  for (int i = 0; i < 10; i++) {
    stress4();
  }
}

MOZ_NEVER_INLINE static void
stress2()
{
  stress3(); stress3(); stress3(); stress3(); stress3();
  stress3(); stress3(); stress3(); stress3(); stress3();
}

MOZ_NEVER_INLINE static void
stress1()
{
  for (int i = 0; i < 10; i++) {
    stress2();
  }
}

// This stress test does lots of allocations and frees, which is where most of
// DMD's overhead occurs.  It allocates 1,000,000 64-byte blocks, spread evenly
// across 1,000 distinct stack traces.  It frees every second one immediately
// after allocating it.
//
// It's highly artificial, but it's deterministic and easy to run.  It can be
// timed under different conditions to glean performance data.
static void
RunStressMode(FILE* fp)
{
  Writer writer(FpWrite, fp);

  // Disable sampling for maximum stress.
  gSampleBelowSize = 1;

  stress1(); stress1(); stress1(); stress1(); stress1();
  stress1(); stress1(); stress1(); stress1(); stress1();

  Dump(writer);
}

}   // namespace dmd
}   // namespace mozilla
