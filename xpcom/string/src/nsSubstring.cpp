/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef DEBUG
#define ENABLE_STRING_STATS
#endif

#ifdef ENABLE_STRING_STATS
#include <stdio.h>
#endif

#include <stdlib.h>
#include "nsSubstring.h"
#include "nsString.h"
#include "nsDependentString.h"
#include "nsMemory.h"
#include "pratom.h"

// ---------------------------------------------------------------------------

static const PRUnichar gNullChar = 0;

const char*      nsCharTraits<char>     ::sEmptyBuffer = (const char*) &gNullChar;
const PRUnichar* nsCharTraits<PRUnichar>::sEmptyBuffer =               &gNullChar;

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
#define STRING_STAT_INCREMENT(_s) PR_AtomicIncrement(&gStringStats.m ## _s ## Count)
#else
#define STRING_STAT_INCREMENT(_s)
#endif

// ---------------------------------------------------------------------------

  /**
   * This structure precedes the string buffers "we" allocate.  It may be the
   * case that nsTSubstring::mData does not point to one of these special
   * buffers.  The mFlags member variable distinguishes the buffer type.
   *
   * When this header is in use, it enables reference counting, and capacity
   * tracking.  NOTE: A string buffer can be modified only if its reference
   * count is 1.
   */
class nsStringHeader
  {
    private:

      PRInt32  mRefCount;
      PRUint32 mStorageSize;

    public:

      void AddRef()
        {
          PR_AtomicIncrement(&mRefCount);
          STRING_STAT_INCREMENT(Share);
        }

      void Release()
        {
          if (PR_AtomicDecrement(&mRefCount) == 0)
            {
              STRING_STAT_INCREMENT(Free);
              free(this); // we were allocated with |malloc|
            }
        }

        /**
         * Alloc returns a pointer to a new string header with set capacity.
         */
      static nsStringHeader* Alloc(size_t size)
        {
          STRING_STAT_INCREMENT(Alloc);
 
          NS_ASSERTION(size != 0, "zero capacity allocation not allowed");

          nsStringHeader *hdr =
              (nsStringHeader *) malloc(sizeof(nsStringHeader) + size);
          if (hdr)
            {
              hdr->mRefCount = 1;
              hdr->mStorageSize = size;
            }
          return hdr;
        }

      static nsStringHeader* Realloc(nsStringHeader* hdr, size_t size)
        {
          STRING_STAT_INCREMENT(Realloc);

          NS_ASSERTION(size != 0, "zero capacity allocation not allowed");

          // no point in trying to save ourselves if we hit this assertion
          NS_ASSERTION(!hdr->IsReadonly(), "|Realloc| attempted on readonly string");

          hdr = (nsStringHeader*) realloc(hdr, sizeof(nsStringHeader) + size);
          if (hdr)
            hdr->mStorageSize = size;

          return hdr;
        }

      static nsStringHeader* FromData(void* data)
        {
          return (nsStringHeader*) ( ((char*) data) - sizeof(nsStringHeader) );
        }

      void* Data() const
        {
          return (void*) ( ((char*) this) + sizeof(nsStringHeader) );
        }

      PRUint32 StorageSize() const
        {
          return mStorageSize;
        }

        /**
         * Because nsTSubstring allows only single threaded access, if this
         * method returns FALSE, then the caller can be sure that it has
         * exclusive access to the nsStringHeader and associated data.
         * However, if this function returns TRUE, then other strings may
         * rely on the data in this buffer being constant and other threads
         * may access this buffer simultaneously.
         */
      PRBool IsReadonly() const
        {
          return mRefCount > 1;
        }
  };

// ---------------------------------------------------------------------------

inline void
ReleaseData( void* data, PRUint32 flags )
  {
    if (flags & nsSubstring::F_SHARED)
      {
        nsStringHeader::FromData(data)->Release();
      }
    else if (flags & nsSubstring::F_OWNED)
      {
        nsMemory::Free(data);
        STRING_STAT_INCREMENT(AdoptFree);
      }
    // otherwise, nothing to do.
  }


  // define nsSubstring
#include "string-template-def-unichar.h"
#include "nsTSubstring.cpp"
#include "string-template-undef.h"

  // define nsCSubstring
#include "string-template-def-char.h"
#include "nsTSubstring.cpp"
#include "string-template-undef.h"
