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
#include <windows.h>
#include <process.h>
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
#ifdef XP_WIN
#define PAGE_SIZE GetPageSize()
static long GetPageSize()
{
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwPageSize;
}
static void* valloc(size_t size)
{
  return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}
#else
#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#endif
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
  {
    pthread_mutex_init(&mMutex, NULL);
  }

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
// gStackTraceTable, gBlockTable, etc.
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
  // indirectly call vanilla malloc via functions like NS_StackWalk.)
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

  bool InterceptsAreBlocked() const
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

// This class is used to print details about code locations.
class LocationService
{
  // WriteLocation() is the key function in this class.  It's basically a
  // wrapper around NS_DescribeCodeAddress.
  //
  // However, NS_DescribeCodeAddress is very slow on some platforms, and we
  // have lots of repeated (i.e. same PC) calls to it.  So we do some caching
  // of results.  Each cached result includes two strings (|mFunction| and
  // |mLibrary|), so we also optimize them for space in the following ways.
  //
  // - The number of distinct library names is small, e.g. a few dozen.  There
  //   is lots of repetition, especially of libxul.  So we intern them in their
  //   own table, which saves space over duplicating them for each cache entry.
  //
  // - The number of distinct function names is much higher, so we duplicate
  //   them in each cache entry.  That's more space-efficient than interning
  //   because entries containing single-occurrence function names are quickly
  //   overwritten, and their copies released.  In addition, empty function
  //   names are common, so we use nullptr to represent them compactly.

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

  typedef js::HashSet<const char*, StringHasher, InfallibleAllocPolicy>
          StringTable;

  StringTable mLibraryStrings;

  struct Entry
  {
    const void* mPc;        // the entry is unused if this is null
    char*       mFunction;  // owned by the Entry;  may be null
    const char* mLibrary;   // owned by mLibraryStrings;  never null
                            //   in a non-empty entry is in use
    ptrdiff_t   mLOffset;

    Entry()
      : mPc(nullptr), mFunction(nullptr), mLibrary(nullptr), mLOffset(0)
    {}

    ~Entry()
    {
      // We don't free mLibrary because it's externally owned.
      InfallibleAllocPolicy::free_(mFunction);
    }

    void Replace(const void* aPc, const char* aFunction, const char* aLibrary,
                 ptrdiff_t aLOffset)
    {
      mPc = aPc;

      // Convert "" to nullptr.  Otherwise, make a copy of the name.
      InfallibleAllocPolicy::free_(mFunction);
      mFunction =
        !aFunction[0] ? nullptr : InfallibleAllocPolicy::strdup_(aFunction);

      mLibrary = aLibrary;
      mLOffset = aLOffset;
    }

    size_t SizeOfExcludingThis() {
      // Don't measure mLibrary because it's externally owned.
      return MallocSizeOf(mFunction);
    }
  };

  // A direct-mapped cache.  When doing a dump just after starting desktop
  // Firefox (which is similar to dumping after a longer-running session,
  // thanks to the limit on how many records we dump), a cache with 2^24
  // entries (which approximates an infinite-entry cache) has a ~91% hit rate.
  // A cache with 2^12 entries has a ~83% hit rate, and takes up ~85 KiB (on
  // 32-bit platforms) or ~150 KiB (on 64-bit platforms).
  static const size_t kNumEntries = 1 << 12;
  static const size_t kMask = kNumEntries - 1;
  Entry mEntries[kNumEntries];

  size_t mNumCacheHits;
  size_t mNumCacheMisses;

public:
  LocationService()
    : mEntries(), mNumCacheHits(0), mNumCacheMisses(0)
  {
    (void)mLibraryStrings.init(64);
  }

  void WriteLocation(const Writer& aWriter, const void* aPc)
  {
    MOZ_ASSERT(gStateLock->IsLocked());

    uint32_t index = HashGeneric(aPc) & kMask;
    MOZ_ASSERT(index < kNumEntries);
    Entry& entry = mEntries[index];

    MOZ_ASSERT(aPc);    // important, because null represents an empty entry
    if (entry.mPc != aPc) {
      mNumCacheMisses++;

      // NS_DescribeCodeAddress can (on Linux) acquire a lock inside
      // the shared library loader.  Another thread might call malloc
      // while holding that lock (when loading a shared library).  So
      // we have to exit gStateLock around this call.  For details, see
      // https://bugzilla.mozilla.org/show_bug.cgi?id=363334#c3
      nsCodeAddressDetails details;
      {
        AutoUnlockState unlock;
        (void)NS_DescribeCodeAddress(const_cast<void*>(aPc), &details);
      }

      // Intern the library name.
      const char* library = nullptr;
      StringTable::AddPtr p = mLibraryStrings.lookupForAdd(details.library);
      if (!p) {
        library = InfallibleAllocPolicy::strdup_(details.library);
        (void)mLibraryStrings.add(p, library);
      } else {
        library = *p;
      }

      entry.Replace(aPc, details.function, library, details.loffset);

    } else {
      mNumCacheHits++;
    }

    MOZ_ASSERT(entry.mPc == aPc);

    // Sometimes we get nothing useful.  Just print "???" for the entire entry
    // so that fix-linux-stack.pl doesn't complain about an empty filename.
    if (!entry.mFunction && !entry.mLibrary[0] && entry.mLOffset == 0) {
      W("   ??? %p\n", entry.mPc);
    } else {
      // Use "???" for unknown functions.
      W("   %s[%s +0x%X] %p\n", entry.mFunction ? entry.mFunction : "???",
        entry.mLibrary, entry.mLOffset, entry.mPc);
    }
  }

  size_t SizeOfIncludingThis()
  {
    size_t n = MallocSizeOf(this);
    for (uint32_t i = 0; i < kNumEntries; i++) {
      n += mEntries[i].SizeOfExcludingThis();
    }

    n += mLibraryStrings.sizeOfExcludingThis(MallocSizeOf);
    for (StringTable::Range r = mLibraryStrings.all();
         !r.empty();
         r.popFront()) {
      n += MallocSizeOf(r.front());
    }

    return n;
  }

  size_t CacheCapacity() const { return kNumEntries; }

  size_t CacheCount() const
  {
    size_t n = 0;
    for (size_t i = 0; i < kNumEntries; i++) {
      if (mEntries[i].mPc) {
        n++;
      }
    }
    return n;
  }

  size_t NumCacheHits()   const { return mNumCacheHits; }
  size_t NumCacheMisses() const { return mNumCacheMisses; }
};

//---------------------------------------------------------------------------
// Stack traces
//---------------------------------------------------------------------------

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

  void Print(const Writer& aWriter, LocationService* aLocService) const;

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
StackTrace::Print(const Writer& aWriter, LocationService* aLocService) const
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
    aLocService->WriteLocation(aWriter, Pc(i));
  }
}

/* static */ const StackTrace*
StackTrace::Get(Thread* aT)
{
  MOZ_ASSERT(gStateLock->IsLocked());
  MOZ_ASSERT(aT->InterceptsAreBlocked());

  // On Windows, NS_StackWalk can acquire a lock from the shared library
  // loader.  Another thread might call malloc while holding that lock (when
  // loading a shared library).  So we can't be in gStateLock during the call
  // to NS_StackWalk.  For details, see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=374829#c8
  // On Linux, something similar can happen;  see bug 824340.
  // So let's just release it on all platforms.
  StackTrace tmp;
  {
    AutoUnlockState unlock;
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

// A live heap block.
class Block
{
  const void*  mPtr;
  const size_t mReqSize;    // size requested

  // Ptr: |mAllocStackTrace| - stack trace where this block was allocated.
  // Tag bit 0: |mSampled| - was this block sampled? (if so, slop == 0).
  TaggedPtr<const StackTrace* const>
    mAllocStackTrace_mSampled;

  // This array has two elements because we record at most two reports of a
  // block.
  // - Ptr: |mReportStackTrace| - stack trace where this block was reported.
  //   nullptr if not reported.
  // - Tag bit 0: |mReportedOnAlloc| - was the block reported immediately on
  //   allocation?  If so, DMD must not clear the report at the end of Dump().
  //   Only relevant if |mReportStackTrace| is non-nullptr.
  //
  // |mPtr| is used as the key in BlockTable, so it's ok for this member
  // to be |mutable|.
  mutable TaggedPtr<const StackTrace*> mReportStackTrace_mReportedOnAlloc[2];

public:
  Block(const void* aPtr, size_t aReqSize, const StackTrace* aAllocStackTrace,
        bool aSampled)
    : mPtr(aPtr),
      mReqSize(aReqSize),
      mAllocStackTrace_mSampled(aAllocStackTrace, aSampled),
      mReportStackTrace_mReportedOnAlloc()     // all fields get zeroed
  {
    MOZ_ASSERT(aAllocStackTrace);
  }

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
    return mAllocStackTrace_mSampled.Tag();
  }

  const StackTrace* AllocStackTrace() const
  {
    return mAllocStackTrace_mSampled.Ptr();
  }

  const StackTrace* ReportStackTrace1() const {
    return mReportStackTrace_mReportedOnAlloc[0].Ptr();
  }

  const StackTrace* ReportStackTrace2() const {
    return mReportStackTrace_mReportedOnAlloc[1].Ptr();
  }

  bool ReportedOnAlloc1() const {
    return mReportStackTrace_mReportedOnAlloc[0].Tag();
  }

  bool ReportedOnAlloc2() const {
    return mReportStackTrace_mReportedOnAlloc[1].Tag();
  }

  uint32_t NumReports() const {
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
    // We don't bother recording reports after the 2nd one.
    uint32_t numReports = NumReports();
    if (numReports < 2) {
      mReportStackTrace_mReportedOnAlloc[numReports].Set(StackTrace::Get(aT),
                                                         aReportedOnAlloc);
    }
  }

  void UnreportIfNotReportedOnAlloc() const
  {
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

  static bool match(const Block& aB, const void* const& aPtr)
  {
    return aB.mPtr == aPtr;
  }
};

typedef js::HashSet<Block, Block, InfallibleAllocPolicy> BlockTable;
static BlockTable* gBlockTable = nullptr;

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

      Block b(aPtr, gSampleBelowSize, StackTrace::Get(aT), /* sampled */ true);
      (void)gBlockTable->putNew(aPtr, b);
    }
  } else {
    // If this block size is larger than the sample size, record it exactly.
    Block b(aPtr, aReqSize, StackTrace::Get(aT), /* sampled */ false);
    (void)gBlockTable->putNew(aPtr, b);
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

  gBlockTable->remove(aPtr);
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
  if (t->InterceptsAreBlocked()) {
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

  if (!gIsDMDRunning) {
    return gMallocTable->realloc(aOldPtr, aSize);
  }

  Thread* t = Thread::Fetch();
  if (t->InterceptsAreBlocked()) {
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

  if (!gIsDMDRunning) {
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
  FreeCallback(aPtr, t);
  gMallocTable->free(aPtr);
}

namespace mozilla {
namespace dmd {

//---------------------------------------------------------------------------
// Stack trace records
//---------------------------------------------------------------------------

class TraceRecordKey
{
public:
  const StackTrace* const mAllocStackTrace;   // never null
protected:
  const StackTrace* const mReportStackTrace1; // nullptr if unreported
  const StackTrace* const mReportStackTrace2; // nullptr if not 2x-reported

public:
  TraceRecordKey(const Block& aB)
    : mAllocStackTrace(aB.AllocStackTrace()),
      mReportStackTrace1(aB.ReportStackTrace1()),
      mReportStackTrace2(aB.ReportStackTrace2())
  {
    MOZ_ASSERT(mAllocStackTrace);
  }

  // Hash policy.

  typedef TraceRecordKey Lookup;

  static uint32_t hash(const TraceRecordKey& aKey)
  {
    return mozilla::HashGeneric(aKey.mAllocStackTrace,
                                aKey.mReportStackTrace1,
                                aKey.mReportStackTrace2);
  }

  static bool match(const TraceRecordKey& aA, const TraceRecordKey& aB)
  {
    return aA.mAllocStackTrace   == aB.mAllocStackTrace &&
           aA.mReportStackTrace1 == aB.mReportStackTrace1 &&
           aA.mReportStackTrace2 == aB.mReportStackTrace2;
  }
};

class RecordSize
{
  static const size_t kReqBits = sizeof(size_t) * 8 - 1;  // 31 or 63

  size_t mReq;              // size requested
  size_t mSlop:kReqBits;    // slop bytes
  size_t mSampled:1;        // were one or more blocks contributing to this
                            //   RecordSize sampled?
public:
  RecordSize()
    : mReq(0),
      mSlop(0),
      mSampled(false)
  {}

  size_t Req()    const { return mReq; }
  size_t Slop()   const { return mSlop; }
  size_t Usable() const { return mReq + mSlop; }

  bool IsSampled() const { return mSampled; }

  void Add(const Block& aB)
  {
    mReq  += aB.ReqSize();
    mSlop += aB.SlopSize();
    mSampled = mSampled || aB.IsSampled();
  }

  void Add(const RecordSize& aRecordSize)
  {
    mReq  += aRecordSize.Req();
    mSlop += aRecordSize.Slop();
    mSampled = mSampled || aRecordSize.IsSampled();
  }

  static int Cmp(const RecordSize& aA, const RecordSize& aB)
  {
    // Primary sort: put bigger usable sizes first.
    if (aA.Usable() > aB.Usable()) return -1;
    if (aA.Usable() < aB.Usable()) return  1;

    // Secondary sort: put bigger requested sizes first.
    if (aA.Req() > aB.Req()) return -1;
    if (aA.Req() < aB.Req()) return  1;

    // Tertiary sort: put non-sampled records before sampled records.
    if (!aA.mSampled &&  aB.mSampled) return -1;
    if ( aA.mSampled && !aB.mSampled) return  1;

    return 0;
  }
};

// A collection of one or more heap blocks with a common TraceRecordKey.
class TraceRecord : public TraceRecordKey
{
  // The TraceRecordKey base class serves as the key in TraceRecordTables.
  // These two fields constitute the value, so it's ok for them to be
  // |mutable|.
  mutable uint32_t    mNumBlocks; // number of blocks with this TraceRecordKey
  mutable RecordSize mRecordSize; // combined size of those blocks

public:
  explicit TraceRecord(const TraceRecordKey& aKey)
    : TraceRecordKey(aKey),
      mNumBlocks(0),
      mRecordSize()
  {}

  uint32_t NumBlocks() const { return mNumBlocks; }

  const RecordSize& GetRecordSize() const { return mRecordSize; }

  // This is |const| thanks to the |mutable| fields above.
  void Add(const Block& aB) const
  {
    mNumBlocks++;
    mRecordSize.Add(aB);
  }

  static const char* const kRecordKind;   // for PrintSortedRecords

  void Print(const Writer& aWriter, LocationService* aLocService,
             uint32_t aM, uint32_t aN, const char* aStr, const char* astr,
             size_t aCategoryUsableSize, size_t aCumulativeUsableSize,
             size_t aTotalUsableSize) const;

  static int QsortCmp(const void* aA, const void* aB)
  {
    const TraceRecord* const a = *static_cast<const TraceRecord* const*>(aA);
    const TraceRecord* const b = *static_cast<const TraceRecord* const*>(aB);

    return RecordSize::Cmp(a->mRecordSize, b->mRecordSize);
  }
};

const char* const TraceRecord::kRecordKind = "trace";

typedef js::HashSet<TraceRecord, TraceRecord, InfallibleAllocPolicy>
        TraceRecordTable;

void
TraceRecord::Print(const Writer& aWriter, LocationService* aLocService,
                   uint32_t aM, uint32_t aN, const char* aStr, const char* astr,
                   size_t aCategoryUsableSize, size_t aCumulativeUsableSize,
                   size_t aTotalUsableSize) const
{
  bool showTilde = mRecordSize.IsSampled();

  W("%s: %s block%s in stack trace record %s of %s\n",
    aStr,
    Show(mNumBlocks, gBuf1, kBufLen, showTilde), Plural(mNumBlocks),
    Show(aM, gBuf2, kBufLen),
    Show(aN, gBuf3, kBufLen));

  W(" %s bytes (%s requested / %s slop)\n",
    Show(mRecordSize.Usable(), gBuf1, kBufLen, showTilde),
    Show(mRecordSize.Req(),    gBuf2, kBufLen, showTilde),
    Show(mRecordSize.Slop(),   gBuf3, kBufLen, showTilde));

  W(" %4.2f%% of the heap (%4.2f%% cumulative); "
    " %4.2f%% of %s (%4.2f%% cumulative)\n",
    Percent(mRecordSize.Usable(), aTotalUsableSize),
    Percent(aCumulativeUsableSize, aTotalUsableSize),
    Percent(mRecordSize.Usable(), aCategoryUsableSize),
    astr,
    Percent(aCumulativeUsableSize, aCategoryUsableSize));

  W(" Allocated at\n");
  mAllocStackTrace->Print(aWriter, aLocService);

  if (mReportStackTrace1) {
    W("\n Reported at\n");
    mReportStackTrace1->Print(aWriter, aLocService);
  }
  if (mReportStackTrace2) {
    W("\n Reported again at\n");
    mReportStackTrace2->Print(aWriter, aLocService);
  }

  W("\n");
}

//---------------------------------------------------------------------------
// Stack frame records
//---------------------------------------------------------------------------

// A collection of one or more stack frames (from heap block allocation stack
// traces) with a common PC.
class FrameRecord
{
  // mPc is used as the key in FrameRecordTable, and the other members
  // constitute the value, so it's ok for them to be |mutable|.
  const void* const  mPc;
  mutable size_t     mNumBlocks;
  mutable size_t     mNumTraceRecords;
  mutable RecordSize mRecordSize;

public:
  explicit FrameRecord(const void* aPc)
    : mPc(aPc),
      mNumBlocks(0),
      mNumTraceRecords(0),
      mRecordSize()
  {}

  const RecordSize& GetRecordSize() const { return mRecordSize; }

  // This is |const| thanks to the |mutable| fields above.
  void Add(const TraceRecord& aTr) const
  {
    mNumBlocks += aTr.NumBlocks();
    mNumTraceRecords++;
    mRecordSize.Add(aTr.GetRecordSize());
  }

  void Print(const Writer& aWriter, LocationService* aLocService,
             uint32_t aM, uint32_t aN, const char* aStr, const char* astr,
             size_t aCategoryUsableSize, size_t aCumulativeUsableSize,
             size_t aTotalUsableSize) const;

  static int QsortCmp(const void* aA, const void* aB)
  {
    const FrameRecord* const a = *static_cast<const FrameRecord* const*>(aA);
    const FrameRecord* const b = *static_cast<const FrameRecord* const*>(aB);

    return RecordSize::Cmp(a->mRecordSize, b->mRecordSize);
  }

  static const char* const kRecordKind;   // for PrintSortedRecords

  // Hash policy.

  typedef const void* Lookup;

  static uint32_t hash(const void* const& aPc)
  {
    return mozilla::HashGeneric(aPc);
  }

  static bool match(const FrameRecord& aFr, const void* const& aPc)
  {
    return aFr.mPc == aPc;
  }
};

const char* const FrameRecord::kRecordKind = "frame";

typedef js::HashSet<FrameRecord, FrameRecord, InfallibleAllocPolicy>
        FrameRecordTable;

void
FrameRecord::Print(const Writer& aWriter, LocationService* aLocService,
                   uint32_t aM, uint32_t aN, const char* aStr, const char* astr,
                   size_t aCategoryUsableSize, size_t aCumulativeUsableSize,
                   size_t aTotalUsableSize) const
{
  (void)aCumulativeUsableSize;

  bool showTilde = mRecordSize.IsSampled();

  W("%s: %s block%s from %s stack trace record%s in stack frame record %s of %s\n",
    aStr,
    Show(mNumBlocks, gBuf1, kBufLen, showTilde), Plural(mNumBlocks),
    Show(mNumTraceRecords, gBuf2, kBufLen, showTilde), Plural(mNumTraceRecords),
    Show(aM, gBuf3, kBufLen),
    Show(aN, gBuf4, kBufLen));

  W(" %s bytes (%s requested / %s slop)\n",
    Show(mRecordSize.Usable(), gBuf1, kBufLen, showTilde),
    Show(mRecordSize.Req(),    gBuf2, kBufLen, showTilde),
    Show(mRecordSize.Slop(),   gBuf3, kBufLen, showTilde));

  W(" %4.2f%% of the heap;  %4.2f%% of %s\n",
    Percent(mRecordSize.Usable(), aTotalUsableSize),
    Percent(mRecordSize.Usable(), aCategoryUsableSize),
    astr);

  W(" PC is\n");
  aLocService->WriteLocation(aWriter, mPc);
  W("\n");
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

#ifdef XP_MACOSX
static void
NopStackWalkCallback(void* aPc, void* aSp, void* aClosure)
{
}
#endif

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

#ifdef XP_MACOSX
  // On Mac OS X we need to call StackWalkInitCriticalAddress() very early
  // (prior to the creation of any mutexes, apparently) otherwise we can get
  // hangs when getting stack traces (bug 821577).  But
  // StackWalkInitCriticalAddress() isn't exported from xpcom/, so instead we
  // just call NS_StackWalk, because that calls StackWalkInitCriticalAddress().
  // See the comment above StackWalkInitCriticalAddress() for more details.
  (void)NS_StackWalk(NopStackWalkCallback, 0, nullptr, 0, nullptr);
#endif

  gStateLock = InfallibleAllocPolicy::new_<Mutex>();

  gSmallBlockActualSizeCounter = 0;

  DMD_CREATE_TLS_INDEX(gTlsIndex);

  gStackTraceTable = InfallibleAllocPolicy::new_<StackTraceTable>();
  gStackTraceTable->init(8192);

  gBlockTable = InfallibleAllocPolicy::new_<BlockTable>();
  gBlockTable->init(8192);

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

static void
ReportHelper(const void* aPtr, bool aReportedOnAlloc)
{
  if (!gIsDMDRunning || !aPtr) {
    return;
  }

  Thread* t = Thread::Fetch();

  AutoBlockIntercepts block(t);
  AutoLockState lock;

  if (BlockTable::Ptr p = gBlockTable->lookup(aPtr)) {
    p->Report(t, aReportedOnAlloc);
  } else {
    // We have no record of the block.  Do nothing.  Either:
    // - We're sampling and we skipped this block.  This is likely.
    // - It's a bogus pointer.  This is unlikely because Report() is almost
    //   always called in conjunction with a malloc_size_of-style function.
  }
}

MOZ_EXPORT void
Report(const void* aPtr)
{
  ReportHelper(aPtr, /* onAlloc */ false);
}

MOZ_EXPORT void
ReportOnAlloc(const void* aPtr)
{
  ReportHelper(aPtr, /* onAlloc */ true);
}

//---------------------------------------------------------------------------
// DMD output
//---------------------------------------------------------------------------

// This works for both TraceRecords and StackFrameRecords.
template <class Record>
static void
PrintSortedRecords(const Writer& aWriter, LocationService* aLocService,
                   const char* aStr, const char* astr,
                   const js::HashSet<Record, Record, InfallibleAllocPolicy>&
                         aRecordTable,
                   size_t aCategoryUsableSize, size_t aTotalUsableSize)
{
  const char* kind = Record::kRecordKind;
  StatusMsg("  creating and sorting %s stack %s record array...\n", astr, kind);

  // Convert the table into a sorted array.
  js::Vector<const Record*, 0, InfallibleAllocPolicy> recordArray;
  recordArray.reserve(aRecordTable.count());
  typedef js::HashSet<Record, Record, InfallibleAllocPolicy> RecordTable;
  for (typename RecordTable::Range r = aRecordTable.all();
       !r.empty();
       r.popFront()) {
    recordArray.infallibleAppend(&r.front());
  }
  qsort(recordArray.begin(), recordArray.length(), sizeof(recordArray[0]),
        Record::QsortCmp);

  WriteTitle("%s stack %s records\n", aStr, kind);

  if (recordArray.length() == 0) {
    W("(none)\n\n");
    return;
  }

  // Limit the number of records printed, because fix-linux-stack.pl is too
  // damn slow.  Note that we don't break out of this loop because we need to
  // keep adding to |cumulativeUsableSize|.
  static const uint32_t MaxRecords = 1000;
  uint32_t numRecords = recordArray.length();

  StatusMsg("  printing %s stack %s record array...\n", astr, kind);
  size_t cumulativeUsableSize = 0;
  for (uint32_t i = 0; i < numRecords; i++) {
    const Record* r = recordArray[i];
    cumulativeUsableSize += r->GetRecordSize().Usable();
    if (i < MaxRecords) {
      r->Print(aWriter, aLocService, i+1, numRecords, aStr, astr,
               aCategoryUsableSize, cumulativeUsableSize, aTotalUsableSize);
    } else if (i == MaxRecords) {
      W("%s: stopping after %s stack %s records\n\n", aStr,
        Show(MaxRecords, gBuf1, kBufLen), kind);
    }
  }

  MOZ_ASSERT(aCategoryUsableSize == kNoSize ||
             aCategoryUsableSize == cumulativeUsableSize);
}

static void
PrintSortedTraceAndFrameRecords(const Writer& aWriter,
                                LocationService* aLocService,
                                const char* aStr, const char* astr,
                                const TraceRecordTable& aTraceRecordTable,
                                size_t aCategoryUsableSize,
                                size_t aTotalUsableSize)
{
  PrintSortedRecords(aWriter, aLocService, aStr, astr, aTraceRecordTable,
                     aCategoryUsableSize, aTotalUsableSize);

  // Frame records are totally dependent on vagaries of stack traces, so we
  // can't show them in test mode.
  if (gMode == Test) {
    return;
  }

  FrameRecordTable frameRecordTable;
  (void)frameRecordTable.init(2048);
  for (TraceRecordTable::Range r = aTraceRecordTable.all();
       !r.empty();
       r.popFront()) {
    const TraceRecord& tr = r.front();
    const StackTrace* st = tr.mAllocStackTrace;

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

      FrameRecordTable::AddPtr p = frameRecordTable.lookupForAdd(pc);
      if (!p) {
        FrameRecord fr(pc);
        (void)frameRecordTable.add(p, fr);
      }
      p->Add(tr);
    }
  }
  PrintSortedRecords(aWriter, aLocService, aStr, astr, frameRecordTable,
                     kNoSize, aTotalUsableSize);
}

// Note that, unlike most SizeOf* functions, this function does not take a
// |nsMallocSizeOfFun| argument.  That's because those arguments are primarily
// to aid DMD track heap blocks... but DMD deliberately doesn't track heap
// blocks it allocated for itself!
//
// SizeOfInternal should be called while you're holding the state lock and while
// intercepts are blocked; SizeOf acquires the lock and blocks intercepts.

static void
SizeOfInternal(Sizes* aSizes)
{
  MOZ_ASSERT(gStateLock->IsLocked());
  MOZ_ASSERT(Thread::Fetch()->InterceptsAreBlocked());

  aSizes->Clear();

  if (!gIsDMDRunning) {
    return;
  }

  js::HashSet<const StackTrace*, js::DefaultHasher<const StackTrace*>,
              InfallibleAllocPolicy> usedStackTraces;
  usedStackTraces.init(1024);

  for(BlockTable::Range r = gBlockTable->all(); !r.empty(); r.popFront()) {
    const Block& b = r.front();
    usedStackTraces.put(b.AllocStackTrace());
    usedStackTraces.put(b.ReportStackTrace1());
    usedStackTraces.put(b.ReportStackTrace2());
  }

  for (StackTraceTable::Range r = gStackTraceTable->all();
       !r.empty();
       r.popFront()) {
    StackTrace* const& st = r.front();

    if (usedStackTraces.has(st)) {
      aSizes->mStackTracesUsed += MallocSizeOf(st);
    } else {
      aSizes->mStackTracesUnused += MallocSizeOf(st);
    }
  }

  aSizes->mStackTraceTable =
    gStackTraceTable->sizeOfIncludingThis(MallocSizeOf);

  aSizes->mBlockTable = gBlockTable->sizeOfIncludingThis(MallocSizeOf);
}

MOZ_EXPORT void
SizeOf(Sizes* aSizes)
{
  aSizes->Clear();

  if (!gIsDMDRunning) {
    return;
  }

  AutoBlockIntercepts block(Thread::Fetch());
  AutoLockState lock;
  SizeOfInternal(aSizes);
}

static void
ClearGlobalState()
{
  // Unreport all blocks, except those that were reported on allocation,
  // because they need to keep their reported marking.
  for (BlockTable::Range r = gBlockTable->all(); !r.empty(); r.popFront()) {
    r.front().UnreportIfNotReportedOnAlloc();
  }
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

  StatusMsg("  gathering stack trace records...\n");

  TraceRecordTable unreportedTraceRecordTable;
  (void)unreportedTraceRecordTable.init(1024);
  size_t unreportedUsableSize = 0;
  size_t unreportedNumBlocks = 0;

  TraceRecordTable onceReportedTraceRecordTable;
  (void)onceReportedTraceRecordTable.init(1024);
  size_t onceReportedUsableSize = 0;
  size_t onceReportedNumBlocks = 0;

  TraceRecordTable twiceReportedTraceRecordTable;
  (void)twiceReportedTraceRecordTable.init(0);
  size_t twiceReportedUsableSize = 0;
  size_t twiceReportedNumBlocks = 0;

  bool anyBlocksSampled = false;

  for (BlockTable::Range r = gBlockTable->all(); !r.empty(); r.popFront()) {
    const Block& b = r.front();

    TraceRecordTable* table;
    uint32_t numReports = b.NumReports();
    if (numReports == 0) {
      unreportedUsableSize += b.UsableSize();
      unreportedNumBlocks++;
      table = &unreportedTraceRecordTable;
    } else if (numReports == 1) {
      onceReportedUsableSize += b.UsableSize();
      onceReportedNumBlocks++;
      table = &onceReportedTraceRecordTable;
    } else {
      MOZ_ASSERT(numReports == 2);
      twiceReportedUsableSize += b.UsableSize();
      twiceReportedNumBlocks++;
      table = &twiceReportedTraceRecordTable;
    }
    TraceRecordKey key(b);
    TraceRecordTable::AddPtr p = table->lookupForAdd(key);
    if (!p) {
      TraceRecord tr(b);
      (void)table->add(p, tr);
    }
    p->Add(b);

    anyBlocksSampled = anyBlocksSampled || b.IsSampled();
  }
  size_t totalUsableSize =
    unreportedUsableSize + onceReportedUsableSize + twiceReportedUsableSize;
  size_t totalNumBlocks =
    unreportedNumBlocks + onceReportedNumBlocks + twiceReportedNumBlocks;

  WriteTitle("Invocation\n");
  W("$DMD = '%s'\n", gDMDEnvVar);
  W("Sample-below size = %lld\n\n", (long long)(gSampleBelowSize));

  // Allocate this on the heap instead of the stack because it's fairly large.
  LocationService* locService = InfallibleAllocPolicy::new_<LocationService>();

  PrintSortedRecords(aWriter, locService, "Twice-reported", "twice-reported",
                     twiceReportedTraceRecordTable, twiceReportedUsableSize,
                     totalUsableSize);

  PrintSortedTraceAndFrameRecords(aWriter, locService,
                                  "Unreported", "unreported",
                                  unreportedTraceRecordTable,
                                  unreportedUsableSize, totalUsableSize);

  PrintSortedTraceAndFrameRecords(aWriter, locService,
                                 "Once-reported", "once-reported",
                                 onceReportedTraceRecordTable,
                                 onceReportedUsableSize, totalUsableSize);

  bool showTilde = anyBlocksSampled;
  WriteTitle("Summary\n");

  W("Total:          %12s bytes (%6.2f%%) in %7s blocks (%6.2f%%)\n",
    Show(totalUsableSize, gBuf1, kBufLen, showTilde),
    100.0,
    Show(totalNumBlocks,  gBuf2, kBufLen, showTilde),
    100.0);

  W("Unreported:     %12s bytes (%6.2f%%) in %7s blocks (%6.2f%%)\n",
    Show(unreportedUsableSize, gBuf1, kBufLen, showTilde),
    Percent(unreportedUsableSize, totalUsableSize),
    Show(unreportedNumBlocks, gBuf2, kBufLen, showTilde),
    Percent(unreportedNumBlocks, totalNumBlocks));

  W("Once-reported:  %12s bytes (%6.2f%%) in %7s blocks (%6.2f%%)\n",
    Show(onceReportedUsableSize, gBuf1, kBufLen, showTilde),
    Percent(onceReportedUsableSize, totalUsableSize),
    Show(onceReportedNumBlocks, gBuf2, kBufLen, showTilde),
    Percent(onceReportedNumBlocks, totalNumBlocks));

  W("Twice-reported: %12s bytes (%6.2f%%) in %7s blocks (%6.2f%%)\n",
    Show(twiceReportedUsableSize, gBuf1, kBufLen, showTilde),
    Percent(twiceReportedUsableSize, totalUsableSize),
    Show(twiceReportedNumBlocks, gBuf2, kBufLen, showTilde),
    Percent(twiceReportedNumBlocks, totalNumBlocks));

  W("\n");

  // Stats are non-deterministic, so don't show them in test mode.
  if (gMode != Test) {
    Sizes sizes;
    SizeOfInternal(&sizes);

    WriteTitle("Execution measurements\n");

    W("Data structures that persist after Dump() ends:\n");

    W("  Used stack traces:    %10s bytes\n",
      Show(sizes.mStackTracesUsed, gBuf1, kBufLen));

    W("  Unused stack traces:  %10s bytes\n",
      Show(sizes.mStackTracesUnused, gBuf1, kBufLen));

    W("  Stack trace table:    %10s bytes (%s entries, %s used)\n",
      Show(sizes.mStackTraceTable,       gBuf1, kBufLen),
      Show(gStackTraceTable->capacity(), gBuf2, kBufLen),
      Show(gStackTraceTable->count(),    gBuf3, kBufLen));

    W("  Block table:          %10s bytes (%s entries, %s used)\n",
      Show(sizes.mBlockTable,       gBuf1, kBufLen),
      Show(gBlockTable->capacity(), gBuf2, kBufLen),
      Show(gBlockTable->count(),    gBuf3, kBufLen));

    W("\nData structures that are destroyed after Dump() ends:\n");

    size_t unreportedSize =
      unreportedTraceRecordTable.sizeOfIncludingThis(MallocSizeOf);
    W("  Unreported table:     %10s bytes (%s entries, %s used)\n",
      Show(unreportedSize,                        gBuf1, kBufLen),
      Show(unreportedTraceRecordTable.capacity(), gBuf2, kBufLen),
      Show(unreportedTraceRecordTable.count(),    gBuf3, kBufLen));

    size_t onceReportedSize =
      onceReportedTraceRecordTable.sizeOfIncludingThis(MallocSizeOf);
    W("  Once-reported table:  %10s bytes (%s entries, %s used)\n",
      Show(onceReportedSize,                        gBuf1, kBufLen),
      Show(onceReportedTraceRecordTable.capacity(), gBuf2, kBufLen),
      Show(onceReportedTraceRecordTable.count(),    gBuf3, kBufLen));

    size_t twiceReportedSize =
      twiceReportedTraceRecordTable.sizeOfIncludingThis(MallocSizeOf);
    W("  Twice-reported table: %10s bytes (%s entries, %s used)\n",
      Show(twiceReportedSize,                        gBuf1, kBufLen),
      Show(twiceReportedTraceRecordTable.capacity(), gBuf2, kBufLen),
      Show(twiceReportedTraceRecordTable.count(),    gBuf3, kBufLen));

    W("  Location service:     %10s bytes\n",
      Show(locService->SizeOfIncludingThis(), gBuf1, kBufLen));

    W("\nCounts:\n");

    size_t hits   = locService->NumCacheHits();
    size_t misses = locService->NumCacheMisses();
    size_t requests = hits + misses;
    W("  Location service:    %10s requests\n",
      Show(requests, gBuf1, kBufLen));

    size_t count    = locService->CacheCount();
    size_t capacity = locService->CacheCapacity();
    W("  Location service cache:  %4.1f%% hit rate, %.1f%% occupancy at end\n",
      Percent(hits, requests), Percent(count, capacity));

    W("\n");
  }

  InfallibleAllocPolicy::delete_(locService);

  ClearGlobalState();

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
      Report(a[i]);                     // reported
   Report(a[2]);                        // reported
   Report(a[3]);                        // reported
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
  // 2nd Dump: thrice-reported.
  char* a2 = (char*) malloc(0);
  Report(a2);

  // Operator new[].
  // 1st Dump: reported.
  // 2nd Dump: reportedness carries over, due to ReportOnAlloc.
  char* b = new char[10];
  ReportOnAlloc(b);

  // ReportOnAlloc, then freed.
  // 1st Dump: freed, irrelevant.
  // 2nd Dump: freed, irrelevant.
  char* b2 = new char;
  ReportOnAlloc(b2);
  free(b2);

  // 1st Dump: reported 4 times.
  // 2nd Dump: freed, irrelevant.
  char* c = (char*) calloc(10, 3);
  Report(c);
  for (int i = 0; i < 3; i++) {
    Report(c);
  }

  // 1st Dump: ignored.
  // 2nd Dump: irrelevant.
  Report((void*)(intptr_t)i);

  // jemalloc rounds this up to 8192.
  // 1st Dump: reported.
  // 2nd Dump: freed.
  char* e = (char*) malloc(4096);
  e = (char*) realloc(e, 4097);
  Report(e);

  // First realloc is like malloc;  second realloc is shrinking.
  // 1st Dump: reported.
  // 2nd Dump: re-reported.
  char* e2 = (char*) realloc(nullptr, 1024);
  e2 = (char*) realloc(e2, 512);
  Report(e2);

  // First realloc is like malloc;  second realloc creates a min-sized block.
  // 1st Dump: reported.
  // 2nd Dump: freed, irrelevant.
  char* e3 = (char*) realloc(nullptr, 1024);
  e3 = (char*) realloc(e3, 0);
  MOZ_ASSERT(e3);
  Report(e3);

  // 1st Dump: freed, irrelevant.
  // 2nd Dump: freed, irrelevant.
  char* f = (char*) malloc(64);
  free(f);

  // 1st Dump: ignored.
  // 2nd Dump: irrelevant.
  Report((void*)(intptr_t)0x0);

  // 1st Dump: mixture of reported and unreported.
  // 2nd Dump: all unreported.
  foo();
  foo();

  // 1st Dump: twice-reported.
  // 2nd Dump: twice-reported.
  char* g1 = (char*) malloc(77);
  ReportOnAlloc(g1);
  ReportOnAlloc(g1);

  // 1st Dump: twice-reported.
  // 2nd Dump: once-reported.
  char* g2 = (char*) malloc(78);
  Report(g2);
  ReportOnAlloc(g2);

  // 1st Dump: twice-reported.
  // 2nd Dump: once-reported.
  char* g3 = (char*) malloc(79);
  ReportOnAlloc(g3);
  Report(g3);

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

  Report(a2);
  Report(a2);
  free(c);
  free(e);
  Report(e2);
  free(e3);
//free(x);
//free(y);
  free(z);

  // 2nd Dump.
  Dump(writer);

  //---------

  // Clear all knowledge of existing blocks to give us a clean slate.
  gBlockTable->clear();

  // Reset the counter just in case |sample-size| was specified in $DMD.
  // Otherwise the assertions fail.
  gSmallBlockActualSizeCounter = 0;
  gSampleBelowSize = 128;

  char* s;

  // This equals the sample size, and so is reported exactly.  It should be
  // listed before records of the same size that are sampled.
  s = (char*) malloc(128);
  UseItOrLoseIt(s);

  // This exceeds the sample size, and so is reported exactly.
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

  // This allocates 16, 32, ..., 128 bytes, which results in a stack trace
  // record that contains a mix of sample and non-sampled blocks, and so should
  // be printed with '~' signs.
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
