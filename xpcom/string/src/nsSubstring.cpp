/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
#define ENABLE_STRING_STATS
#endif

#ifdef ENABLE_STRING_STATS
#include <stdio.h>
#endif

#include <stdlib.h>
#include "nsSubstring.h"
#include "nsString.h"
#include "nsStringBuffer.h"
#include "nsDependentString.h"
#include "nsMemory.h"
#include "pratom.h"
#include "prprf.h"
#include "nsStaticAtom.h"

// ---------------------------------------------------------------------------

static PRUnichar gNullChar = 0;

char*      nsCharTraits<char>     ::sEmptyBuffer = (char*) &gNullChar;
PRUnichar* nsCharTraits<PRUnichar>::sEmptyBuffer =         &gNullChar;

// ---------------------------------------------------------------------------

#ifdef ENABLE_STRING_STATS
class nsStringStats
  {
    public:
      nsStringStats()
        : mAllocCount(0), mReallocCount(0), mFreeCount(0), mShareCount(0) {}

      ~nsStringStats()
        {
          // this is a hack to suppress duplicate string stats printing
          // in seamonkey as a result of the string code being linked
          // into seamonkey and libxpcom! :-(
          if (!mAllocCount && !mAdoptCount)
            return;

          printf("nsStringStats\n");
          printf(" => mAllocCount:     % 10d\n", mAllocCount);
          printf(" => mReallocCount:   % 10d\n", mReallocCount);
          printf(" => mFreeCount:      % 10d", mFreeCount);
          if (mAllocCount > mFreeCount)
            printf("  --  LEAKED %d !!!\n", mAllocCount - mFreeCount);
          else
            printf("\n");
          printf(" => mShareCount:     % 10d\n", mShareCount);
          printf(" => mAdoptCount:     % 10d\n", mAdoptCount);
          printf(" => mAdoptFreeCount: % 10d", mAdoptFreeCount);
          if (mAdoptCount > mAdoptFreeCount)
            printf("  --  LEAKED %d !!!\n", mAdoptCount - mAdoptFreeCount);
          else
            printf("\n");
        }

      PRInt32 mAllocCount;
      PRInt32 mReallocCount;
      PRInt32 mFreeCount;
      PRInt32 mShareCount;
      PRInt32 mAdoptCount;
      PRInt32 mAdoptFreeCount;
  };
static nsStringStats gStringStats;
#define STRING_STAT_INCREMENT(_s) PR_ATOMIC_INCREMENT(&gStringStats.m ## _s ## Count)
#else
#define STRING_STAT_INCREMENT(_s)
#endif

// ---------------------------------------------------------------------------

inline void
ReleaseData( void* data, PRUint32 flags )
  {
    if (flags & nsSubstring::F_SHARED)
      {
        nsStringBuffer::FromData(data)->Release();
      }
    else if (flags & nsSubstring::F_OWNED)
      {
        nsMemory::Free(data);
        STRING_STAT_INCREMENT(AdoptFree);
#ifdef NS_BUILD_REFCNT_LOGGING
        // Treat this as destruction of a "StringAdopt" object for leak
        // tracking purposes.
        NS_LogDtor(data, "StringAdopt", 1);
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
      char_type  *data() const   { return mData; }
      size_type   length() const { return mLength; }
      PRUint32    flags() const  { return mFlags; }

      void set(char_type *data, size_type len, PRUint32 flags)
        {
          ReleaseData(mData, mFlags);
          mData = data;
          mLength = len;
          mFlags = flags;
        }
  };

class nsACStringAccessor : public nsACString
  {
    private:
      nsACStringAccessor(); // NOT IMPLEMENTED

    public:
      char_type  *data() const   { return mData; }
      size_type   length() const { return mLength; }
      PRUint32    flags() const  { return mFlags; }

      void set(char_type *data, size_type len, PRUint32 flags)
        {
          ReleaseData(mData, mFlags);
          mData = data;
          mLength = len;
          mFlags = flags;
        }
  };

// ---------------------------------------------------------------------------

void
nsStringBuffer::AddRef()
  {
    PR_ATOMIC_INCREMENT(&mRefCount);
    STRING_STAT_INCREMENT(Share);
    NS_LOG_ADDREF(this, mRefCount, "nsStringBuffer", sizeof(*this));
  }

void
nsStringBuffer::Release()
  {
    PRInt32 count = PR_ATOMIC_DECREMENT(&mRefCount);
    NS_LOG_RELEASE(this, count, "nsStringBuffer");
    if (count == 0)
      {
        STRING_STAT_INCREMENT(Free);
        free(this); // we were allocated with |malloc|
      }
  }

  /**
   * Alloc returns a pointer to a new string header with set capacity.
   */
nsStringBuffer*
nsStringBuffer::Alloc(size_t size)
  {
    NS_ASSERTION(size != 0, "zero capacity allocation not allowed");
    NS_ASSERTION(sizeof(nsStringBuffer) + size <= size_t(PRUint32(-1)) &&
                 sizeof(nsStringBuffer) + size > size,
                 "mStorageSize will truncate");

    nsStringBuffer *hdr =
        (nsStringBuffer *) malloc(sizeof(nsStringBuffer) + size);
    if (hdr)
      {
        STRING_STAT_INCREMENT(Alloc);

        hdr->mRefCount = 1;
        hdr->mStorageSize = size;
        NS_LOG_ADDREF(hdr, 1, "nsStringBuffer", sizeof(*hdr));
      }
    return hdr;
  }

nsStringBuffer*
nsStringBuffer::Realloc(nsStringBuffer* hdr, size_t size)
  {
    STRING_STAT_INCREMENT(Realloc);

    NS_ASSERTION(size != 0, "zero capacity allocation not allowed");
    NS_ASSERTION(sizeof(nsStringBuffer) + size <= size_t(PRUint32(-1)) &&
                 sizeof(nsStringBuffer) + size > size,
                 "mStorageSize will truncate");

    // no point in trying to save ourselves if we hit this assertion
    NS_ASSERTION(!hdr->IsReadonly(), "|Realloc| attempted on readonly string");

    // Treat this as a release and addref for refcounting purposes, since we
    // just asserted that the refcount is 1.  If we don't do that, refcount
    // logging will claim we've leaked all sorts of stuff.
    NS_LOG_RELEASE(hdr, 0, "nsStringBuffer");
    
    hdr = (nsStringBuffer*) realloc(hdr, sizeof(nsStringBuffer) + size);
    if (hdr) {
      NS_LOG_ADDREF(hdr, 1, "nsStringBuffer", sizeof(*hdr));
      hdr->mStorageSize = size;
    }

    return hdr;
  }

nsStringBuffer*
nsStringBuffer::FromString(const nsAString& str)
  {
    const nsAStringAccessor* accessor =
        static_cast<const nsAStringAccessor*>(&str);

    if (!(accessor->flags() & nsSubstring::F_SHARED))
      return nsnull;

    return FromData(accessor->data());
  }

nsStringBuffer*
nsStringBuffer::FromString(const nsACString& str)
  {
    const nsACStringAccessor* accessor =
        static_cast<const nsACStringAccessor*>(&str);

    if (!(accessor->flags() & nsCSubstring::F_SHARED))
      return nsnull;

    return FromData(accessor->data());
  }

void
nsStringBuffer::ToString(PRUint32 len, nsAString &str,
                         bool aMoveOwnership)
  {
    PRUnichar* data = static_cast<PRUnichar*>(Data());

    nsAStringAccessor* accessor = static_cast<nsAStringAccessor*>(&str);
    NS_ASSERTION(data[len] == PRUnichar(0), "data should be null terminated");

    // preserve class flags
    PRUint32 flags = accessor->flags();
    flags = (flags & 0xFFFF0000) | nsSubstring::F_SHARED | nsSubstring::F_TERMINATED;

    if (!aMoveOwnership) {
      AddRef();
    }
    accessor->set(data, len, flags);
  }

void
nsStringBuffer::ToString(PRUint32 len, nsACString &str,
                         bool aMoveOwnership)
  {
    char* data = static_cast<char*>(Data());

    nsACStringAccessor* accessor = static_cast<nsACStringAccessor*>(&str);
    NS_ASSERTION(data[len] == char(0), "data should be null terminated");

    // preserve class flags
    PRUint32 flags = accessor->flags();
    flags = (flags & 0xFFFF0000) | nsCSubstring::F_SHARED | nsCSubstring::F_TERMINATED;

    if (!aMoveOwnership) {
      AddRef();
    }
    accessor->set(data, len, flags);
  }

size_t
nsStringBuffer::SizeOfIncludingThisMustBeUnshared(nsMallocSizeOfFun aMallocSizeOf) const
  {
    NS_ASSERTION(!IsReadonly(),
                 "shared StringBuffer in SizeOfIncludingThisMustBeUnshared");
    return aMallocSizeOf(this);
  }

size_t
nsStringBuffer::SizeOfIncludingThisIfUnshared(nsMallocSizeOfFun aMallocSizeOf) const
  {
    if (!IsReadonly())
      {
        return SizeOfIncludingThisMustBeUnshared(aMallocSizeOf);
      }
    return 0;
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

MOZ_STATIC_ASSERT(sizeof(nsStringContainer_base) == sizeof(nsSubstring),
                  "internal and external strings must have the same size");
