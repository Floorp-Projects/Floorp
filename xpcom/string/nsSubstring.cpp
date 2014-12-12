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
#include "nsSubstring.h"
#include "nsString.h"
#include "nsStringBuffer.h"
#include "nsDependentString.h"
#include "nsMemory.h"
#include "prprf.h"
#include "nsStaticAtom.h"
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

  Atomic<int32_t> mAllocCount;
  Atomic<int32_t> mReallocCount;
  Atomic<int32_t> mFreeCount;
  Atomic<int32_t> mShareCount;
  Atomic<int32_t> mAdoptCount;
  Atomic<int32_t> mAdoptFreeCount;
};
static nsStringStats gStringStats;
#define STRING_STAT_INCREMENT(_s) (gStringStats.m ## _s ## Count)++
#else
#define STRING_STAT_INCREMENT(_s)
#endif

// ---------------------------------------------------------------------------

inline void
ReleaseData(void* aData, uint32_t aFlags)
{
  if (aFlags & nsSubstring::F_SHARED) {
    nsStringBuffer::FromData(aData)->Release();
  } else if (aFlags & nsSubstring::F_OWNED) {
    nsMemory::Free(aData);
    STRING_STAT_INCREMENT(AdoptFree);
#ifdef NS_BUILD_REFCNT_LOGGING
    // Treat this as destruction of a "StringAdopt" object for leak
    // tracking purposes.
    NS_LogDtor(aData, "StringAdopt", 1);
#endif // NS_BUILD_REFCNT_LOGGING
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
  uint32_t flags() const
  {
    return mFlags;
  }

  void set(char_type* aData, size_type aLen, uint32_t aFlags)
  {
    ReleaseData(mData, mFlags);
    mData = aData;
    mLength = aLen;
    mFlags = aFlags;
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
  uint32_t flags() const
  {
    return mFlags;
  }

  void set(char_type* aData, size_type aLen, uint32_t aFlags)
  {
    ReleaseData(mData, mFlags);
    mData = aData;
    mLength = aLen;
    mFlags = aFlags;
  }
};

// ---------------------------------------------------------------------------

void
nsStringBuffer::AddRef()
{
  ++mRefCount;
  STRING_STAT_INCREMENT(Share);
  NS_LOG_ADDREF(this, mRefCount, "nsStringBuffer", sizeof(*this));
}

void
nsStringBuffer::Release()
{
  int32_t count = --mRefCount;
  NS_LOG_RELEASE(this, count, "nsStringBuffer");
  if (count == 0) {
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
    NS_LOG_ADDREF(hdr, 1, "nsStringBuffer", sizeof(*hdr));
  }
  return dont_AddRef(hdr);
}

nsStringBuffer*
nsStringBuffer::Realloc(nsStringBuffer* aHdr, size_t aSize)
{
  STRING_STAT_INCREMENT(Realloc);

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

  if (!(accessor->flags() & nsSubstring::F_SHARED)) {
    return nullptr;
  }

  return FromData(accessor->data());
}

nsStringBuffer*
nsStringBuffer::FromString(const nsACString& aStr)
{
  const nsACStringAccessor* accessor =
    static_cast<const nsACStringAccessor*>(&aStr);

  if (!(accessor->flags() & nsCSubstring::F_SHARED)) {
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
  NS_ASSERTION(data[aLen] == char16_t(0), "data should be null terminated");

  // preserve class flags
  uint32_t flags = accessor->flags();
  flags = (flags & 0xFFFF0000) | nsSubstring::F_SHARED | nsSubstring::F_TERMINATED;

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
  NS_ASSERTION(data[aLen] == char(0), "data should be null terminated");

  // preserve class flags
  uint32_t flags = accessor->flags();
  flags = (flags & 0xFFFF0000) | nsCSubstring::F_SHARED | nsCSubstring::F_TERMINATED;

  if (!aMoveOwnership) {
    AddRef();
  }
  accessor->set(data, aLen, flags);
}

size_t
nsStringBuffer::SizeOfIncludingThisMustBeUnshared(mozilla::MallocSizeOf aMallocSizeOf) const
{
  NS_ASSERTION(!IsReadonly(),
               "shared StringBuffer in SizeOfIncludingThisMustBeUnshared");
  return aMallocSizeOf(this);
}

size_t
nsStringBuffer::SizeOfIncludingThisIfUnshared(mozilla::MallocSizeOf aMallocSizeOf) const
{
  if (!IsReadonly()) {
    return SizeOfIncludingThisMustBeUnshared(aMallocSizeOf);
  }
  return 0;
}

size_t
nsStringBuffer::SizeOfIncludingThisEvenIfShared(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);
}

// ---------------------------------------------------------------------------


// define nsSubstring
#include "string-template-def-unichar.h"
#include "nsTSubstring.cpp"
#include "string-template-undef.h"

// define nsCSubstring
#include "string-template-def-char.h"
#include "nsTSubstring.cpp"
#include "string-template-undef.h"

// Check that internal and external strings have the same size.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=430581

#include "prlog.h"
#include "nsXPCOMStrings.h"

static_assert(sizeof(nsStringContainer_base) == sizeof(nsSubstring),
              "internal and external strings must have the same size");
