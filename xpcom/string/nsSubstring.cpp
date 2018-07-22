/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
#define ENABLE_STRING_STATS
#endif

#include "mozilla/Atomics.h"
#include "mozilla/MemoryReporting.h"

#ifdef ENABLE_STRING_STATS
#include <stdio.h>
#endif

#include <stdlib.h>
#include "nsAString.h"
#include "nsString.h"
#include "nsStringBuffer.h"
#include "nsDependentString.h"
#include "nsPrintfCString.h"
#include "nsMemory.h"
#include "prprf.h"
#include "nsCOMPtr.h"

#include "mozilla/IntegerPrintfMacros.h"
#ifdef XP_WIN
#include <windows.h>
#include <process.h>
#define getpid() _getpid()
#define pthread_self() GetCurrentThreadId()
#else
#include <pthread.h>
#include <unistd.h>
#endif

#ifdef STRING_BUFFER_CANARY
#define CHECK_STRING_BUFFER_CANARY(c)                                     \
  do {                                                                    \
    if ((c) != CANARY_OK) {                                               \
      MOZ_CRASH_UNSAFE_PRINTF("Bad canary value 0x%x", c);                  \
    }                                                                     \
  } while(0)
#else
#define CHECK_STRING_BUFFER_CANARY(c)                                     \
  do {                                                                    \
  } while(0)
#endif

using mozilla::Atomic;

// ---------------------------------------------------------------------------

static const char16_t gNullChar = 0;

char* const nsCharTraits<char>::sEmptyBuffer =
  (char*)const_cast<char16_t*>(&gNullChar);
char16_t* const nsCharTraits<char16_t>::sEmptyBuffer =
  const_cast<char16_t*>(&gNullChar);

// ---------------------------------------------------------------------------

#ifdef ENABLE_STRING_STATS
class nsStringStats
{
public:
  nsStringStats()
    : mAllocCount(0)
    , mReallocCount(0)
    , mFreeCount(0)
    , mShareCount(0)
  {
  }

  ~nsStringStats()
  {
    // this is a hack to suppress duplicate string stats printing
    // in seamonkey as a result of the string code being linked
    // into seamonkey and libxpcom! :-(
    if (!mAllocCount && !mAdoptCount) {
      return;
    }

    printf("nsStringStats\n");
    printf(" => mAllocCount:     % 10d\n", int(mAllocCount));
    printf(" => mReallocCount:   % 10d\n", int(mReallocCount));
    printf(" => mFreeCount:      % 10d", int(mFreeCount));
    if (mAllocCount > mFreeCount) {
      printf("  --  LEAKED %d !!!\n", mAllocCount - mFreeCount);
    } else {
      printf("\n");
    }
    printf(" => mShareCount:     % 10d\n", int(mShareCount));
    printf(" => mAdoptCount:     % 10d\n", int(mAdoptCount));
    printf(" => mAdoptFreeCount: % 10d", int(mAdoptFreeCount));
    if (mAdoptCount > mAdoptFreeCount) {
      printf("  --  LEAKED %d !!!\n", mAdoptCount - mAdoptFreeCount);
    } else {
      printf("\n");
    }
    printf(" => Process ID: %" PRIuPTR ", Thread ID: %" PRIuPTR "\n",
           uintptr_t(getpid()), uintptr_t(pthread_self()));
  }

  typedef Atomic<int32_t,
                 mozilla::SequentiallyConsistent,
                 mozilla::recordreplay::Behavior::DontPreserve> AtomicInt;

  AtomicInt mAllocCount;
  AtomicInt mReallocCount;
  AtomicInt mFreeCount;
  AtomicInt mShareCount;
  AtomicInt mAdoptCount;
  AtomicInt mAdoptFreeCount;
};
static nsStringStats gStringStats;
#define STRING_STAT_INCREMENT(_s) (gStringStats.m ## _s ## Count)++
#else
#define STRING_STAT_INCREMENT(_s)
#endif

// ---------------------------------------------------------------------------

void
ReleaseData(void* aData, nsAString::DataFlags aFlags)
{
  if (aFlags & nsAString::DataFlags::REFCOUNTED) {
    nsStringBuffer::FromData(aData)->Release();
  } else if (aFlags & nsAString::DataFlags::OWNED) {
    free(aData);
    STRING_STAT_INCREMENT(AdoptFree);
    // Treat this as destruction of a "StringAdopt" object for leak
    // tracking purposes.
    MOZ_LOG_DTOR(aData, "StringAdopt", 1);
  }
  // otherwise, nothing to do.
}

// ---------------------------------------------------------------------------

// XXX or we could make nsStringBuffer be a friend of nsTAString

class nsAStringAccessor : public nsAString
{
private:
  nsAStringAccessor(); // NOT IMPLEMENTED

public:
  char_type* data() const
  {
    return mData;
  }
  size_type length() const
  {
    return mLength;
  }
  DataFlags flags() const
  {
    return mDataFlags;
  }

  void set(char_type* aData, size_type aLen, DataFlags aDataFlags)
  {
    ReleaseData(mData, mDataFlags);
    SetData(aData, aLen, aDataFlags);
  }
};

class nsACStringAccessor : public nsACString
{
private:
  nsACStringAccessor(); // NOT IMPLEMENTED

public:
  char_type* data() const
  {
    return mData;
  }
  size_type length() const
  {
    return mLength;
  }
  DataFlags flags() const
  {
    return mDataFlags;
  }

  void set(char_type* aData, size_type aLen, DataFlags aDataFlags)
  {
    ReleaseData(mData, mDataFlags);
    SetData(aData, aLen, aDataFlags);
  }
};

// ---------------------------------------------------------------------------

void
nsStringBuffer::AddRef()
{
  // Memory synchronization is not required when incrementing a
  // reference count.  The first increment of a reference count on a
  // thread is not important, since the first use of the object on a
  // thread can happen before it.  What is important is the transfer
  // of the pointer to that thread, which may happen prior to the
  // first increment on that thread.  The necessary memory
  // synchronization is done by the mechanism that transfers the
  // pointer between threads.
#ifdef NS_BUILD_REFCNT_LOGGING
  uint32_t count =
#endif
    mRefCount.fetch_add(1, std::memory_order_relaxed)
#ifdef NS_BUILD_REFCNT_LOGGING
    + 1
#endif
    ;
  STRING_STAT_INCREMENT(Share);
  NS_LOG_ADDREF(this, count, "nsStringBuffer", sizeof(*this));
}

void
nsStringBuffer::Release()
{
  CHECK_STRING_BUFFER_CANARY(mCanary);

  // Since this may be the last release on this thread, we need
  // release semantics so that prior writes on this thread are visible
  // to the thread that destroys the object when it reads mValue with
  // acquire semantics.
  uint32_t count = mRefCount.fetch_sub(1, std::memory_order_release) - 1;
  NS_LOG_RELEASE(this, count, "nsStringBuffer");
  if (count == 0) {
    // We're going to destroy the object on this thread, so we need
    // acquire semantics to synchronize with the memory released by
    // the last release on other threads, that is, to ensure that
    // writes prior to that release are now visible on this thread.
    count = mRefCount.load(std::memory_order_acquire);
#ifdef STRING_BUFFER_CANARY
    mCanary = CANARY_POISON;
#endif

    STRING_STAT_INCREMENT(Free);
    free(this); // we were allocated with |malloc|
  }
}

/**
 * Alloc returns a pointer to a new string header with set capacity.
 */
already_AddRefed<nsStringBuffer>
nsStringBuffer::Alloc(size_t aSize)
{
  NS_ASSERTION(aSize != 0, "zero capacity allocation not allowed");
  NS_ASSERTION(sizeof(nsStringBuffer) + aSize <= size_t(uint32_t(-1)) &&
               sizeof(nsStringBuffer) + aSize > aSize,
               "mStorageSize will truncate");

  nsStringBuffer* hdr =
    (nsStringBuffer*)malloc(sizeof(nsStringBuffer) + aSize);
  if (hdr) {
    STRING_STAT_INCREMENT(Alloc);

    hdr->mRefCount = 1;
    hdr->mStorageSize = aSize;
#ifdef STRING_BUFFER_CANARY
    hdr->mCanary = CANARY_OK;
#endif
    NS_LOG_ADDREF(hdr, 1, "nsStringBuffer", sizeof(*hdr));
  }
  return dont_AddRef(hdr);
}

nsStringBuffer*
nsStringBuffer::Realloc(nsStringBuffer* aHdr, size_t aSize)
{
  STRING_STAT_INCREMENT(Realloc);

  CHECK_STRING_BUFFER_CANARY(aHdr->mCanary);
  NS_ASSERTION(aSize != 0, "zero capacity allocation not allowed");
  NS_ASSERTION(sizeof(nsStringBuffer) + aSize <= size_t(uint32_t(-1)) &&
               sizeof(nsStringBuffer) + aSize > aSize,
               "mStorageSize will truncate");

  // no point in trying to save ourselves if we hit this assertion
  NS_ASSERTION(!aHdr->IsReadonly(), "|Realloc| attempted on readonly string");

  // Treat this as a release and addref for refcounting purposes, since we
  // just asserted that the refcount is 1.  If we don't do that, refcount
  // logging will claim we've leaked all sorts of stuff.
  NS_LOG_RELEASE(aHdr, 0, "nsStringBuffer");

  aHdr = (nsStringBuffer*)realloc(aHdr, sizeof(nsStringBuffer) + aSize);
  if (aHdr) {
    NS_LOG_ADDREF(aHdr, 1, "nsStringBuffer", sizeof(*aHdr));
    aHdr->mStorageSize = aSize;
  }

  return aHdr;
}

nsStringBuffer*
nsStringBuffer::FromString(const nsAString& aStr)
{
  const nsAStringAccessor* accessor =
    static_cast<const nsAStringAccessor*>(&aStr);

  if (!(accessor->flags() & nsAString::DataFlags::REFCOUNTED)) {
    return nullptr;
  }

  return FromData(accessor->data());
}

nsStringBuffer*
nsStringBuffer::FromString(const nsACString& aStr)
{
  const nsACStringAccessor* accessor =
    static_cast<const nsACStringAccessor*>(&aStr);

  if (!(accessor->flags() & nsACString::DataFlags::REFCOUNTED)) {
    return nullptr;
  }

  return FromData(accessor->data());
}

void
nsStringBuffer::ToString(uint32_t aLen, nsAString& aStr,
                         bool aMoveOwnership)
{
  char16_t* data = static_cast<char16_t*>(Data());

  nsAStringAccessor* accessor = static_cast<nsAStringAccessor*>(&aStr);
  MOZ_DIAGNOSTIC_ASSERT(data[aLen] == char16_t(0),
                        "data should be null terminated");

  nsAString::DataFlags flags =
    nsAString::DataFlags::REFCOUNTED | nsAString::DataFlags::TERMINATED;

  if (!aMoveOwnership) {
    AddRef();
  }
  accessor->set(data, aLen, flags);
}

void
nsStringBuffer::ToString(uint32_t aLen, nsACString& aStr,
                         bool aMoveOwnership)
{
  char* data = static_cast<char*>(Data());

  nsACStringAccessor* accessor = static_cast<nsACStringAccessor*>(&aStr);
  MOZ_DIAGNOSTIC_ASSERT(data[aLen] == char(0),
                        "data should be null terminated");

  nsACString::DataFlags flags =
    nsACString::DataFlags::REFCOUNTED | nsACString::DataFlags::TERMINATED;

  if (!aMoveOwnership) {
    AddRef();
  }
  accessor->set(data, aLen, flags);
}

size_t
nsStringBuffer::SizeOfIncludingThisIfUnshared(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return IsReadonly() ? 0 : aMallocSizeOf(this);
}

size_t
nsStringBuffer::SizeOfIncludingThisEvenIfShared(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);
}

#ifdef STRING_BUFFER_CANARY
void
nsStringBuffer::FromDataCanaryCheckFailed() const
{
  MOZ_CRASH_UNSAFE_PRINTF("Bad canary value 0x%x in FromData", mCanary);
}
#endif

// ---------------------------------------------------------------------------

// define nsAString
#include "nsTSubstring.cpp"

// Provide rust bindings to the nsA[C]String types
extern "C" {

// This is a no-op on release, so we ifdef it out such that using it in release
// results in a linker error.
#ifdef DEBUG
void Gecko_IncrementStringAdoptCount(void* aData)
{
  MOZ_LOG_CTOR(aData, "StringAdopt", 1);
}
#elif defined(MOZ_DEBUG_RUST)
void Gecko_IncrementStringAdoptCount(void *aData)
{
}
#endif

void Gecko_FinalizeCString(nsACString* aThis)
{
  aThis->~nsACString();
}

void Gecko_AssignCString(nsACString* aThis, const nsACString* aOther)
{
  aThis->Assign(*aOther);
}

void Gecko_TakeFromCString(nsACString* aThis, nsACString* aOther)
{
  aThis->Assign(std::move(*aOther));
}

void Gecko_AppendCString(nsACString* aThis, const nsACString* aOther)
{
  aThis->Append(*aOther);
}

void Gecko_SetLengthCString(nsACString* aThis, uint32_t aLength)
{
  aThis->SetLength(aLength);
}

bool Gecko_FallibleAssignCString(nsACString* aThis, const nsACString* aOther)
{
  return aThis->Assign(*aOther, mozilla::fallible);
}

bool Gecko_FallibleTakeFromCString(nsACString* aThis, nsACString* aOther)
{
  return aThis->Assign(std::move(*aOther), mozilla::fallible);
}

bool Gecko_FallibleAppendCString(nsACString* aThis, const nsACString* aOther)
{
  return aThis->Append(*aOther, mozilla::fallible);
}

bool Gecko_FallibleSetLengthCString(nsACString* aThis, uint32_t aLength)
{
  return aThis->SetLength(aLength, mozilla::fallible);
}

char* Gecko_BeginWritingCString(nsACString* aThis)
{
  return aThis->BeginWriting();
}

char* Gecko_FallibleBeginWritingCString(nsACString* aThis)
{
  return aThis->BeginWriting(mozilla::fallible);
}

void Gecko_FinalizeString(nsAString* aThis)
{
  aThis->~nsAString();
}

void Gecko_AssignString(nsAString* aThis, const nsAString* aOther)
{
  aThis->Assign(*aOther);
}

void Gecko_TakeFromString(nsAString* aThis, nsAString* aOther)
{
  aThis->Assign(std::move(*aOther));
}

void Gecko_AppendString(nsAString* aThis, const nsAString* aOther)
{
  aThis->Append(*aOther);
}

void Gecko_SetLengthString(nsAString* aThis, uint32_t aLength)
{
  aThis->SetLength(aLength);
}

bool Gecko_FallibleAssignString(nsAString* aThis, const nsAString* aOther)
{
  return aThis->Assign(*aOther, mozilla::fallible);
}

bool Gecko_FallibleTakeFromString(nsAString* aThis, nsAString* aOther)
{
  return aThis->Assign(std::move(*aOther), mozilla::fallible);
}

bool Gecko_FallibleAppendString(nsAString* aThis, const nsAString* aOther)
{
  return aThis->Append(*aOther, mozilla::fallible);
}

bool Gecko_FallibleSetLengthString(nsAString* aThis, uint32_t aLength)
{
  return aThis->SetLength(aLength, mozilla::fallible);
}

char16_t* Gecko_BeginWritingString(nsAString* aThis)
{
  return aThis->BeginWriting();
}

char16_t* Gecko_FallibleBeginWritingString(nsAString* aThis)
{
  return aThis->BeginWriting(mozilla::fallible);
}

} // extern "C"
