/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; c-file-offsets: ((substatement-open . 0)) -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "nsVoidArray.h"
#include "nsQuickSort.h"
#include "prmem.h"
#include "nsCRT.h"
#include "nsString.h"
#include "prbit.h"

/**
 * Grow the array by at least this many elements at a time.
 */
static const PRInt32 kMinGrowArrayBy = 8;
static const PRInt32 kMaxGrowArrayBy = 1024;

/**
 * This is the threshold (in bytes) of the mImpl struct, past which
 * we'll force the array to grow geometrically
 */
static const PRInt32 kLinearThreshold = 24 * sizeof(void *);

/**
 * Compute the number of bytes requires for the mImpl struct that will
 * hold |n| elements.
 */
#define SIZEOF_IMPL(n_) (sizeof(Impl) + sizeof(void *) * ((n_) - 1))


/**
 * Compute the number of elements that an mImpl struct of |n| bytes
 * will hold.
 */
#define CAPACITYOF_IMPL(n_) ((((n_) - sizeof(Impl)) / sizeof(void *)) + 1)

#if DEBUG_VOIDARRAY
#define MAXVOID 10

class VoidStats {
public:
  VoidStats();
  ~VoidStats();

};

static int sizesUsed; // number of the elements of the arrays used
static int sizesAlloced[MAXVOID]; // sizes of the allocations.  sorted
static int NumberOfSize[MAXVOID]; // number of this allocation size (1 per array)
static int AllocedOfSize[MAXVOID]; // number of this allocation size (each size for array used)
static int MaxAuto[MAXVOID];      // AutoArrays that maxed out at this size
static int GrowInPlace[MAXVOID];  // arrays this size that grew in-place via realloc

// these are per-allocation  
static int MaxElements[2000];     // # of arrays that maxed out at each size.

// statistics macros
#define ADD_TO_STATS(x,size) do {int i; for (i = 0; i < sizesUsed; i++) \
                                  { \
                                    if (sizesAlloced[i] == (int)(size)) \
                                    { ((x)[i])++; break; } \
                                  } \
                                  if (i >= sizesUsed && sizesUsed < MAXVOID) \
                                  { sizesAlloced[sizesUsed] = (size); \
                                    ((x)[sizesUsed++])++; break; \
                                  } \
                                } while (0)

#define SUB_FROM_STATS(x,size) do {int i; for (i = 0; i < sizesUsed; i++) \
                                    { \
                                      if (sizesAlloced[i] == (int)(size)) \
                                      { ((x)[i])--; break; } \
                                    } \
                                  } while (0)


VoidStats::VoidStats()
{
  sizesUsed = 1;
  sizesAlloced[0] = 0;
}

VoidStats::~VoidStats()
{
  int i;
  for (i = 0; i < sizesUsed; i++)
  {
    printf("Size %d:\n",sizesAlloced[i]);
    printf("\tNumber of VoidArrays this size (max):     %d\n",NumberOfSize[i]-MaxAuto[i]);
    printf("\tNumber of AutoVoidArrays this size (max): %d\n",MaxAuto[i]);
    printf("\tNumber of allocations this size (total):  %d\n",AllocedOfSize[i]);
    printf("\tNumber of GrowsInPlace this size (total): %d\n",GrowInPlace[i]);
  }
  printf("Max Size of VoidArray:\n");
  for (i = 0; i < (int)(sizeof(MaxElements)/sizeof(MaxElements[0])); i++)
  {
    if (MaxElements[i])
      printf("\t%d: %d\n",i,MaxElements[i]);
  }
}

// Just so constructor/destructor's get called
VoidStats gVoidStats;
#endif

inline void
nsVoidArray::SetArray(Impl *newImpl, PRInt32 aSize, PRInt32 aCount, PRBool owner)
{
  // old mImpl has been realloced and so we don't free/delete it
  NS_PRECONDITION(newImpl, "can't set size");
  mImpl = newImpl;
  mImpl->mCount = aCount;
  mImpl->mBits = PRUint32(aSize & kArraySizeMask) |
                 (owner ? kArrayOwnerMask : 0);
}

// This does all allocation/reallocation of the array.
// It also will compact down to N - good for things that might grow a lot
// at times,  but usually are smaller, like JS deferred GC releases.
PRBool nsVoidArray::SizeTo(PRInt32 aSize)
{
  PRUint32 oldsize = GetArraySize();

  if (aSize == (PRInt32) oldsize)
    return PR_TRUE; // no change

  if (aSize <= 0)
  {
    // free the array if allocated
    if (mImpl)
    {
      if (IsArrayOwner())
      {
        PR_Free(NS_REINTERPRET_CAST(char *, mImpl));
        mImpl = nsnull;
      }
      else
      {
        mImpl->mCount = 0; // nsAutoVoidArray
      }
    }
    return PR_TRUE;
  }

  if (mImpl && IsArrayOwner())
  {
    // We currently own an array impl. Resize it appropriately.
    if (aSize < mImpl->mCount)
    {
      // XXX Note: we could also just resize to mCount
      return PR_TRUE;  // can't make it that small, ignore request
    }

    char* bytes = (char *) PR_Realloc(mImpl,SIZEOF_IMPL(aSize));
    Impl* newImpl = NS_REINTERPRET_CAST(Impl*, bytes);
    if (!newImpl)
      return PR_FALSE;

#if DEBUG_VOIDARRAY
    if (mImpl == newImpl)
      ADD_TO_STATS(GrowInPlace,oldsize);
    ADD_TO_STATS(AllocedOfSize,SIZEOF_IMPL(aSize));
    if (aSize > mMaxSize)
    {
      ADD_TO_STATS(NumberOfSize,SIZEOF_IMPL(aSize));
      if (oldsize)
        SUB_FROM_STATS(NumberOfSize,oldsize);
      mMaxSize = aSize;
      if (mIsAuto)
      {
        ADD_TO_STATS(MaxAuto,SIZEOF_IMPL(aSize));
        SUB_FROM_STATS(MaxAuto,oldsize);
      }
    }
#endif
    SetArray(newImpl,aSize,newImpl->mCount,PR_TRUE);
    return PR_TRUE;
  }

  // just allocate an array
  // allocate the exact size requested
  char* bytes = (char *) PR_Malloc(SIZEOF_IMPL(aSize));
  Impl* newImpl = NS_REINTERPRET_CAST(Impl*, bytes);
  if (!newImpl)
    return PR_FALSE;

#if DEBUG_VOIDARRAY
  ADD_TO_STATS(AllocedOfSize,SIZEOF_IMPL(aSize));
  if (aSize > mMaxSize)
  {
    ADD_TO_STATS(NumberOfSize,SIZEOF_IMPL(aSize));
    if (oldsize && !mImpl)
      SUB_FROM_STATS(NumberOfSize,oldsize);
    mMaxSize = aSize;
  }
#endif
  if (mImpl)
  {
#if DEBUG_VOIDARRAY
    ADD_TO_STATS(MaxAuto,SIZEOF_IMPL(aSize));
    SUB_FROM_STATS(MaxAuto,0);
    SUB_FROM_STATS(NumberOfSize,0);
    mIsAuto = PR_TRUE;
#endif
    // We must be growing an nsAutoVoidArray - copy since we didn't
    // realloc.
    memcpy(newImpl->mArray, mImpl->mArray,
                  mImpl->mCount * sizeof(mImpl->mArray[0]));
  }
    
  SetArray(newImpl,aSize,mImpl ? mImpl->mCount : 0,PR_TRUE);
  // no memset; handled later in ReplaceElementAt if needed
  return PR_TRUE;
}

PRBool nsVoidArray::GrowArrayBy(PRInt32 aGrowBy)
{
  // We have to grow the array. Grow by kMinGrowArrayBy slots if we're
  // smaller than kLinearThreshold bytes, or a power of two if we're
  // larger.  This is much more efficient with most memory allocators,
  // especially if it's very large, or of the allocator is binned.
  if (aGrowBy < kMinGrowArrayBy)
    aGrowBy = kMinGrowArrayBy;

  PRUint32 newCapacity = GetArraySize() + aGrowBy;  // Minimum increase
  PRUint32 newSize = SIZEOF_IMPL(newCapacity);

  if (newSize >= (PRUint32) kLinearThreshold)
  {
    // newCount includes enough space for at least kMinGrowArrayBy new
    // slots. Select the next power-of-two size in bytes above or
    // equal to that.
    // Also, limit the increase in size to about a VM page or two.
    if (GetArraySize() >= kMaxGrowArrayBy)
    {
      newCapacity = GetArraySize() + PR_MAX(kMaxGrowArrayBy,aGrowBy);
      newSize = SIZEOF_IMPL(newCapacity);
    }
    else
    {
      newSize = PR_BIT(PR_CeilingLog2(newSize));
      newCapacity = CAPACITYOF_IMPL(newSize);
    }
  }
  // frees old mImpl IF this succeeds
  if (!SizeTo(newCapacity))
    return PR_FALSE;

  return PR_TRUE;
}

nsVoidArray::nsVoidArray()
  : mImpl(nsnull)
{
  MOZ_COUNT_CTOR(nsVoidArray);
#if DEBUG_VOIDARRAY
  mMaxCount = 0;
  mMaxSize = 0;
  mIsAuto = PR_FALSE;
  ADD_TO_STATS(NumberOfSize,0);
  MaxElements[0]++;
#endif
}

nsVoidArray::nsVoidArray(PRInt32 aCount)
  : mImpl(nsnull)
{
  MOZ_COUNT_CTOR(nsVoidArray);
#if DEBUG_VOIDARRAY
  mMaxCount = 0;
  mMaxSize = 0;
  mIsAuto = PR_FALSE;
  MaxElements[0]++;
#endif
  SizeTo(aCount);
}

nsVoidArray& nsVoidArray::operator=(const nsVoidArray& other)
{
  PRInt32 otherCount = other.Count();
  PRInt32 maxCount = GetArraySize();
  if (otherCount)
  {
    if (otherCount > maxCount)
    {
      // frees old mImpl IF this succeeds
      if (!GrowArrayBy(otherCount-maxCount))
        return *this;      // XXX The allocation failed - don't do anything

      memcpy(mImpl->mArray, other.mImpl->mArray, otherCount * sizeof(mImpl->mArray[0]));
      mImpl->mCount = otherCount;
    }
    else
    {
      // the old array can hold the new array
      memcpy(mImpl->mArray, other.mImpl->mArray, otherCount * sizeof(mImpl->mArray[0]));
      mImpl->mCount = otherCount;
      // if it shrank a lot, compact it anyways
      if ((otherCount*2) < maxCount && maxCount > 100)
      {
        Compact();  // shrank by at least 50 entries
      }
    }
#if DEBUG_VOIDARRAY
     if (mImpl->mCount > mMaxCount &&
         mImpl->mCount < (PRInt32)(sizeof(MaxElements)/sizeof(MaxElements[0])))
     {
       MaxElements[mImpl->mCount]++;
       MaxElements[mMaxCount]--;
       mMaxCount = mImpl->mCount;
     }
#endif
  }
  else
  {
    if (mImpl && IsArrayOwner())
      PR_Free(NS_REINTERPRET_CAST(char*, mImpl));

    mImpl = nsnull;
  }

  return *this;
}

nsVoidArray::~nsVoidArray()
{
  MOZ_COUNT_DTOR(nsVoidArray);
  if (mImpl && IsArrayOwner())
    PR_Free(NS_REINTERPRET_CAST(char*, mImpl));
}

PRInt32 nsVoidArray::IndexOf(void* aPossibleElement) const
{
  if (mImpl)
  {
    void** ap = mImpl->mArray;
    void** end = ap + mImpl->mCount;
    while (ap < end)
    {
      if (*ap == aPossibleElement)
      {
        return ap - mImpl->mArray;
      }
      ap++;
    }
  }
  return -1;
}

PRBool nsVoidArray::InsertElementAt(void* aElement, PRInt32 aIndex)
{
  PRInt32 oldCount = Count();
  NS_ASSERTION(aIndex >= 0,"InsertElementAt(negative index)");
  if (PRUint32(aIndex) > PRUint32(oldCount))
  {
    // An invalid index causes the insertion to fail
    // Invalid indexes are ones that add more than one entry to the
    // array (i.e., they can append).
    return PR_FALSE;
  }

  if (oldCount >= GetArraySize())
  {
    if (!GrowArrayBy(1))
      return PR_FALSE;
  }
  // else the array is already large enough

  PRInt32 slide = oldCount - aIndex;
  if (0 != slide)
  {
    // Slide data over to make room for the insertion
    memmove(mImpl->mArray + aIndex + 1, mImpl->mArray + aIndex,
            slide * sizeof(mImpl->mArray[0]));
  }

  mImpl->mArray[aIndex] = aElement;
  mImpl->mCount++;

#if DEBUG_VOIDARRAY
  if (mImpl->mCount > mMaxCount &&
      mImpl->mCount < (PRInt32)(sizeof(MaxElements)/sizeof(MaxElements[0])))
  {
    MaxElements[mImpl->mCount]++;
    MaxElements[mMaxCount]--;
    mMaxCount = mImpl->mCount;
  }
#endif

  return PR_TRUE;
}

PRBool nsVoidArray::InsertElementsAt(const nsVoidArray& other, PRInt32 aIndex)
{
  PRInt32 oldCount = Count();
  PRInt32 otherCount = other.Count();

  NS_ASSERTION(aIndex >= 0,"InsertElementsAt(negative index)");
  if (PRUint32(aIndex) > PRUint32(oldCount))
  {
    // An invalid index causes the insertion to fail
    // Invalid indexes are ones that are more than one entry past the end of
    // the array (i.e., they can append).
    return PR_FALSE;
  }

  if (oldCount + otherCount > GetArraySize())
  {
    if (!GrowArrayBy(otherCount))
      return PR_FALSE;;
  }
  // else the array is already large enough

  PRInt32 slide = oldCount - aIndex;
  if (0 != slide)
  {
    // Slide data over to make room for the insertion
    memmove(mImpl->mArray + aIndex + otherCount, mImpl->mArray + aIndex,
            slide * sizeof(mImpl->mArray[0]));
  }

  for (PRInt32 i = 0; i < otherCount; i++)
  {
    // copy all the elements (destroys aIndex)
    mImpl->mArray[aIndex++] = other.mImpl->mArray[i];
    mImpl->mCount++;
  }

#if DEBUG_VOIDARRAY
  if (mImpl->mCount > mMaxCount &&
      mImpl->mCount < (PRInt32)(sizeof(MaxElements)/sizeof(MaxElements[0])))
  {
    MaxElements[mImpl->mCount]++;
    MaxElements[mMaxCount]--;
    mMaxCount = mImpl->mCount;
  }
#endif

  return PR_TRUE;
}

PRBool nsVoidArray::ReplaceElementAt(void* aElement, PRInt32 aIndex)
{
  NS_ASSERTION(aIndex >= 0,"ReplaceElementAt(negative index)");
  if (aIndex < 0)
    return PR_FALSE;

  // Unlike InsertElementAt, ReplaceElementAt can implicitly add more
  // than just the one element to the array.
  if (PRUint32(aIndex) >= PRUint32(GetArraySize()))
  {
    PRInt32 oldCount = Count();
    PRInt32 requestedCount = aIndex + 1;
    PRInt32 growDelta = requestedCount - oldCount;

    // frees old mImpl IF this succeeds
    if (!GrowArrayBy(growDelta))
      return PR_FALSE;
  }

  mImpl->mArray[aIndex] = aElement;
  if (aIndex >= mImpl->mCount)
  {
    // Make sure that any entries implicitly added to the array by this
    // ReplaceElementAt are cleared to 0.  Some users of this assume that.
    // This code means we don't have to memset when we allocate an array.
    if (aIndex > mImpl->mCount) // note: not >=
    {
      // For example, if mCount is 2, and we do a ReplaceElementAt for
      // element[5], then we need to set three entries ([2], [3], and [4])
      // to 0.
      memset(&mImpl->mArray[mImpl->mCount], 0,
             (aIndex - mImpl->mCount) * sizeof(mImpl->mArray[0]));
    }
    
     mImpl->mCount = aIndex + 1;

#if DEBUG_VOIDARRAY
     if (mImpl->mCount > mMaxCount &&
         mImpl->mCount < (PRInt32)(sizeof(MaxElements)/sizeof(MaxElements[0])))
     {
       MaxElements[mImpl->mCount]++;
       MaxElements[mMaxCount]--;
       mMaxCount = mImpl->mCount;
     }
#endif
  }

  return PR_TRUE;
}

// useful for doing LRU arrays
PRBool nsVoidArray::MoveElement(PRInt32 aFrom, PRInt32 aTo)
{
  void *tempElement;

  if (aTo == aFrom)
    return PR_TRUE;

  NS_ASSERTION(aTo >= 0 && aFrom >= 0,"MoveElement(negative index)");
  if (aTo >= Count() || aFrom >= Count())
  {
    // can't extend the array when moving an element.  Also catches mImpl = null
    return PR_FALSE;
  }
  tempElement = mImpl->mArray[aFrom];

  if (aTo < aFrom)
  {
    // Moving one element closer to the head; the elements inbetween move down
    memmove(mImpl->mArray + aTo + 1, mImpl->mArray + aTo,
            (aFrom-aTo) * sizeof(mImpl->mArray[0]));
    mImpl->mArray[aTo] = tempElement;
  }
  else // already handled aFrom == aTo
  {
    // Moving one element closer to the tail; the elements inbetween move up
    memmove(mImpl->mArray + aFrom, mImpl->mArray + aFrom + 1,
            (aTo-aFrom) * sizeof(mImpl->mArray[0]));
    mImpl->mArray[aTo] = tempElement;
  }

  return PR_TRUE;
}

PRBool nsVoidArray::RemoveElementsAt(PRInt32 aIndex, PRInt32 aCount)
{
  PRInt32 oldCount = Count();
  NS_ASSERTION(aIndex >= 0,"RemoveElementsAt(negative index)");
  if (PRUint32(aIndex) >= PRUint32(oldCount))
  {
    // An invalid index causes the replace to fail
    return PR_FALSE;
  }
  // Limit to available entries starting at aIndex
  if (aCount + aIndex > oldCount)
    aCount = oldCount - aIndex;

  // We don't need to move any elements if we're removing the
  // last element in the array
  if (aIndex < (oldCount - aCount))
  {
    memmove(mImpl->mArray + aIndex, mImpl->mArray + aIndex + aCount,
            (oldCount - (aIndex + aCount)) * sizeof(mImpl->mArray[0]));
  }

  mImpl->mCount -= aCount;
  return PR_TRUE;
}

PRBool nsVoidArray::RemoveElement(void* aElement)
{
  PRInt32 theIndex = IndexOf(aElement);
  if (theIndex != -1)
    return RemoveElementAt(theIndex);

  return PR_FALSE;
}

void nsVoidArray::Clear()
{
  if (mImpl)
  {
    mImpl->mCount = 0;
  }
}

void nsVoidArray::Compact()
{
  if (mImpl)
  {
    // XXX NOTE: this is quite inefficient in many cases if we're only
    // compacting by a little, but some callers care more about memory use.
    if (GetArraySize() > Count())
    {
      SizeTo(Count());
    }
  }
}

// Needed because we want to pass the pointer to the item in the array
// to the comparator function, not a pointer to the pointer in the array.
struct VoidArrayComparatorContext {
  nsVoidArrayComparatorFunc mComparatorFunc;
  void* mData;
};

PR_STATIC_CALLBACK(int)
VoidArrayComparator(const void* aElement1, const void* aElement2, void* aData)
{
  VoidArrayComparatorContext* ctx = NS_STATIC_CAST(VoidArrayComparatorContext*, aData);
  return (*ctx->mComparatorFunc)(*NS_STATIC_CAST(void* const*, aElement1),
                                 *NS_STATIC_CAST(void* const*, aElement2),
                                  ctx->mData);
}

void nsVoidArray::Sort(nsVoidArrayComparatorFunc aFunc, void* aData)
{
  if (mImpl && mImpl->mCount > 1)
  {
    VoidArrayComparatorContext ctx = {aFunc, aData};
    NS_QuickSort(mImpl->mArray, mImpl->mCount, sizeof(mImpl->mArray[0]),
                 VoidArrayComparator, &ctx);
  }
}

PRBool nsVoidArray::EnumerateForwards(nsVoidArrayEnumFunc aFunc, void* aData)
{
  PRInt32 index = -1;
  PRBool  running = PR_TRUE;

  if (mImpl)
  {
    while (running && (++index < mImpl->mCount))
    {
      running = (*aFunc)(mImpl->mArray[index], aData);
    }
  }
  return running;
}

PRBool nsVoidArray::EnumerateBackwards(nsVoidArrayEnumFunc aFunc, void* aData)
{
  PRBool  running = PR_TRUE;

  if (mImpl)
  {
    PRInt32 index = Count();
    while (running && (0 <= --index))
    {
      running = (*aFunc)(mImpl->mArray[index], aData);
    }
  }
  return running;
}

//----------------------------------------------------------------
// nsAutoVoidArray

nsAutoVoidArray::nsAutoVoidArray()
  : nsVoidArray()
{
  // Don't need to clear it.  Some users just call ReplaceElementAt(),
  // but we'll clear it at that time if needed to save CPU cycles.
#if DEBUG_VOIDARRAY
  mIsAuto = PR_TRUE;
  ADD_TO_STATS(MaxAuto,0);
#endif
  SetArray(NS_REINTERPRET_CAST(Impl*, mAutoBuf),kAutoBufSize,0,PR_FALSE);
}

void nsAutoVoidArray::Clear()
{
  // We don't have to free on Clear, but since we have a built-in buffer,
  // it's worth considering.
  nsVoidArray::Clear();
  if (IsArrayOwner() && GetArraySize() > 4*kAutoBufSize)
    SizeTo(0);     // we override CompactTo - delete and repoint at auto array
}

PRBool nsAutoVoidArray::SizeTo(PRInt32 aSize)
{
  if (!nsVoidArray::SizeTo(aSize))
    return PR_FALSE;

  if (!mImpl)
  {
    // reset the array to point to our autobuf
    SetArray(NS_REINTERPRET_CAST(Impl*, mAutoBuf),kAutoBufSize,0,PR_FALSE);
  }
  return PR_TRUE;
}

void nsAutoVoidArray::Compact()
{
  nsVoidArray::Compact();
  if (!mImpl)
  {
    // reset the array to point to our autobuf
    SetArray(NS_REINTERPRET_CAST(Impl*, mAutoBuf),kAutoBufSize,0,PR_FALSE);
  }
}

//----------------------------------------------------------------
// nsStringArray

nsStringArray::nsStringArray(void)
  : nsVoidArray()
{
}

nsStringArray::nsStringArray(PRInt32 aCount)
  : nsVoidArray(aCount)
{
}

nsStringArray::~nsStringArray(void)
{
  Clear();
}

nsStringArray& 
nsStringArray::operator=(const nsStringArray& other)
{
  // Copy the pointers
  nsVoidArray::operator=(other);

  // Now copy the strings
  for (PRInt32 i = Count() - 1; i >= 0; --i)
  {
    nsString* oldString = NS_STATIC_CAST(nsString*, other.ElementAt(i));
    mImpl->mArray[i] = new nsString(*oldString);
  }

  return *this;
}

void 
nsStringArray::StringAt(PRInt32 aIndex, nsAString& aString) const
{
  nsString* string = NS_STATIC_CAST(nsString*, nsVoidArray::ElementAt(aIndex));
  if (nsnull != string)
  {
    aString.Assign(*string);
  }
  else
  {
    aString.Truncate();
  }
}

nsString*
nsStringArray::StringAt(PRInt32 aIndex) const
{
  return NS_STATIC_CAST(nsString*, nsVoidArray::ElementAt(aIndex));
}

PRInt32 
nsStringArray::IndexOf(const nsAString& aPossibleString) const
{
  if (mImpl)
  {
    void** ap = mImpl->mArray;
    void** end = ap + mImpl->mCount;
    while (ap < end)
    {
      nsString* string = NS_STATIC_CAST(nsString*, *ap);
      if (string->Equals(aPossibleString))
      {
        return ap - mImpl->mArray;
      }
      ap++;
    }
  }
  return -1;
}

PRBool 
nsStringArray::InsertStringAt(const nsAString& aString, PRInt32 aIndex)
{
  nsString* string = new nsString(aString);
  if (nsVoidArray::InsertElementAt(string, aIndex))
  {
    return PR_TRUE;
  }
  delete string;
  return PR_FALSE;
}

PRBool
nsStringArray::ReplaceStringAt(const nsAString& aString,
                               PRInt32 aIndex)
{
  nsString* string = NS_STATIC_CAST(nsString*, nsVoidArray::ElementAt(aIndex));
  if (nsnull != string)
  {
    *string = aString;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool 
nsStringArray::RemoveString(const nsAString& aString)
{
  PRInt32 index = IndexOf(aString);
  if (-1 < index)
  {
    return RemoveStringAt(index);
  }
  return PR_FALSE;
}

PRBool nsStringArray::RemoveStringAt(PRInt32 aIndex)
{
  nsString* string = StringAt(aIndex);
  if (nsnull != string)
  {
    nsVoidArray::RemoveElementAt(aIndex);
    delete string;
    return PR_TRUE;
  }
  return PR_FALSE;
}

void 
nsStringArray::Clear(void)
{
  PRInt32 index = Count();
  while (0 <= --index)
  {
    nsString* string = NS_STATIC_CAST(nsString*, mImpl->mArray[index]);
    delete string;
  }
  nsVoidArray::Clear();
}

PR_STATIC_CALLBACK(int)
CompareString(const nsString* aString1, const nsString* aString2, void*)
{
  return Compare(*aString1, *aString2);
}

void nsStringArray::Sort(void)
{
  Sort(CompareString, nsnull);
}

void nsStringArray::Sort(nsStringArrayComparatorFunc aFunc, void* aData)
{
  nsVoidArray::Sort(NS_REINTERPRET_CAST(nsVoidArrayComparatorFunc, aFunc), aData);
}

PRBool 
nsStringArray::EnumerateForwards(nsStringArrayEnumFunc aFunc, void* aData)
{
  PRInt32 index = -1;
  PRBool  running = PR_TRUE;

  if (mImpl)
  {
    while (running && (++index < mImpl->mCount))
    {
      running = (*aFunc)(*NS_STATIC_CAST(nsString*, mImpl->mArray[index]), aData);
    }
  }
  return running;
}

PRBool 
nsStringArray::EnumerateBackwards(nsStringArrayEnumFunc aFunc, void* aData)
{
  PRInt32 index = Count();
  PRBool  running = PR_TRUE;

  if (mImpl)
  {
    while (running && (0 <= --index))
    {
      running = (*aFunc)(*NS_STATIC_CAST(nsString*, mImpl->mArray[index]), aData);
    }
  }
  return running;
}



//----------------------------------------------------------------
// nsCStringArray

nsCStringArray::nsCStringArray(void)
  : nsVoidArray()
{
}

// Parses a given string using the delimiter passed in and appends items
// parsed to the array.
void
nsCStringArray::ParseString(const char* string, const char* delimiter)
{
  if (string && *string && delimiter && *delimiter) {
    char *newStr;
    char *rest = nsCRT::strdup(string);
    char *token = nsCRT::strtok(rest, delimiter, &newStr);

    while (token) {
      if (*token) {
        /* calling AppendElement(void*) to avoid extra nsCString copy */
        AppendElement(new nsCString(token));
      }
      token = nsCRT::strtok(newStr, delimiter, &newStr);
    }
    PR_FREEIF(rest);
  }
}

nsCStringArray::nsCStringArray(PRInt32 aCount)
  : nsVoidArray(aCount)
{
}

nsCStringArray::~nsCStringArray(void)
{
  Clear();
}

nsCStringArray& 
nsCStringArray::operator=(const nsCStringArray& other)
{
  // Copy the pointers
  nsVoidArray::operator=(other);

  // Now copy the strings
  for (PRInt32 i = Count() - 1; i >= 0; --i)
  {
    nsCString* oldString = NS_STATIC_CAST(nsCString*, other.ElementAt(i));
    mImpl->mArray[i] = new nsCString(*oldString);
  }

  return *this;
}

void 
nsCStringArray::CStringAt(PRInt32 aIndex, nsACString& aCString) const
{
  nsCString* string = NS_STATIC_CAST(nsCString*, nsVoidArray::ElementAt(aIndex));
  if (nsnull != string)
  {
    aCString = *string;
  }
  else
  {
    aCString.Truncate();
  }
}

nsCString*
nsCStringArray::CStringAt(PRInt32 aIndex) const
{
  return NS_STATIC_CAST(nsCString*, nsVoidArray::ElementAt(aIndex));
}

PRInt32 
nsCStringArray::IndexOf(const nsACString& aPossibleString) const
{
  if (mImpl)
  {
    void** ap = mImpl->mArray;
    void** end = ap + mImpl->mCount;
    while (ap < end)
    {
      nsCString* string = NS_STATIC_CAST(nsCString*, *ap);
      if (string->Equals(aPossibleString))
      {
        return ap - mImpl->mArray;
      }
      ap++;
    }
  }
  return -1;
}

PRInt32 
nsCStringArray::IndexOfIgnoreCase(const nsACString& aPossibleString) const
{
  if (mImpl)
  {
    void** ap = mImpl->mArray;
    void** end = ap + mImpl->mCount;
    while (ap < end)
    {
      nsCString* string = NS_STATIC_CAST(nsCString*, *ap);
      if (string->Equals(aPossibleString, nsCaseInsensitiveCStringComparator()))
      {
        return ap - mImpl->mArray;
      }
      ap++;
    }
  }
  return -1;
}

PRBool 
nsCStringArray::InsertCStringAt(const nsACString& aCString, PRInt32 aIndex)
{
  nsCString* string = new nsCString(aCString);
  if (nsVoidArray::InsertElementAt(string, aIndex))
  {
    return PR_TRUE;
  }
  delete string;
  return PR_FALSE;
}

PRBool
nsCStringArray::ReplaceCStringAt(const nsACString& aCString, PRInt32 aIndex)
{
  nsCString* string = NS_STATIC_CAST(nsCString*, nsVoidArray::ElementAt(aIndex));
  if (nsnull != string)
  {
    *string = aCString;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool 
nsCStringArray::RemoveCString(const nsACString& aCString)
{
  PRInt32 index = IndexOf(aCString);
  if (-1 < index)
  {
    return RemoveCStringAt(index);
  }
  return PR_FALSE;
}

PRBool 
nsCStringArray::RemoveCStringIgnoreCase(const nsACString& aCString)
{
  PRInt32 index = IndexOfIgnoreCase(aCString);
  if (-1 < index)
  {
    return RemoveCStringAt(index);
  }
  return PR_FALSE;
}

PRBool nsCStringArray::RemoveCStringAt(PRInt32 aIndex)
{
  nsCString* string = CStringAt(aIndex);
  if (nsnull != string)
  {
    nsVoidArray::RemoveElementAt(aIndex);
    delete string;
    return PR_TRUE;
  }
  return PR_FALSE;
}

void 
nsCStringArray::Clear(void)
{
  PRInt32 index = Count();
  while (0 <= --index)
  {
    nsCString* string = NS_STATIC_CAST(nsCString*, mImpl->mArray[index]);
    delete string;
  }
  nsVoidArray::Clear();
}

PR_STATIC_CALLBACK(int)
CompareCString(const nsCString* aCString1, const nsCString* aCString2, void*)
{
  return Compare(*aCString1, *aCString2);
}

PR_STATIC_CALLBACK(int)
CompareCStringIgnoreCase(const nsCString* aCString1, const nsCString* aCString2, void*)
{
  return Compare(*aCString1, *aCString2, nsCaseInsensitiveCStringComparator());
}

void nsCStringArray::Sort(void)
{
  Sort(CompareCString, nsnull);
}

void nsCStringArray::SortIgnoreCase(void)
{
  Sort(CompareCStringIgnoreCase, nsnull);
}

void nsCStringArray::Sort(nsCStringArrayComparatorFunc aFunc, void* aData)
{
  nsVoidArray::Sort(NS_REINTERPRET_CAST(nsVoidArrayComparatorFunc, aFunc), aData);
}

PRBool 
nsCStringArray::EnumerateForwards(nsCStringArrayEnumFunc aFunc, void* aData)
{
  PRBool  running = PR_TRUE;

  if (mImpl)
  {
    PRInt32 index = -1;
    while (running && (++index < mImpl->mCount))
    {
      running = (*aFunc)(*NS_STATIC_CAST(nsCString*, mImpl->mArray[index]), aData);
    }
  }
  return running;
}

PRBool 
nsCStringArray::EnumerateBackwards(nsCStringArrayEnumFunc aFunc, void* aData)
{
  PRBool  running = PR_TRUE;

  if (mImpl)
  {
    PRInt32 index = Count();
    while (running && (0 <= --index))
    {
      running = (*aFunc)(*NS_STATIC_CAST(nsCString*, mImpl->mArray[index]), aData);
    }
  }
  return running;
}


//----------------------------------------------------------------------
// NOTE: nsSmallVoidArray elements MUST all have the low bit as 0.
// This means that normally it's only used for pointers, and in particular
// structures or objects.
nsSmallVoidArray::nsSmallVoidArray()
{
  mChildren = nsnull;
}

nsSmallVoidArray::~nsSmallVoidArray()
{
  if (HasVector())
  {
    nsVoidArray* vector = GetChildVector();
    delete vector;
  }
}

nsSmallVoidArray& 
nsSmallVoidArray::operator=(nsSmallVoidArray& other)
{
  nsVoidArray* ourArray = GetChildVector();
  nsVoidArray* otherArray = other.GetChildVector();

  if (HasVector())
  {
    if (other.HasVector())
    {
      // if both are real arrays, just use array= */
      *ourArray = *otherArray;
    }
    else
    {
      // we have an array, but the other doesn't.
      otherArray = other.SwitchToVector();
      if (otherArray)
        *ourArray = *otherArray;
    }
  }
  else
  {
    if (other.HasVector())
    {
      // we have no array, but other does
      ourArray = SwitchToVector();
      if (ourArray)
        *ourArray = *otherArray;
    }
    else
    {
      // neither has an array (either may have 0 or 1 items)
      SetSingleChild(other.GetSingleChild());
    }
  }
  return *this;
}

PRInt32
nsSmallVoidArray::GetArraySize() const
{
  nsVoidArray* vector = GetChildVector();
  if (vector)
    return vector->GetArraySize();

  return 1;
}

PRInt32
nsSmallVoidArray::Count() const
{
  if (HasSingleChild())
  {
    return 1;
  }
  else {
    nsVoidArray* vector = GetChildVector();
    if (vector)
      return vector->Count();
  }

  return 0;
}

void*
nsSmallVoidArray::ElementAt(PRInt32 aIndex) const
{
  if (HasSingleChild())
  {
    if (0 == aIndex)
      return (void*)GetSingleChild();
  }
  else
  {
    nsVoidArray* vector = GetChildVector();
    if (vector)
      return vector->ElementAt(aIndex);
  }

  return nsnull;
}

PRInt32
nsSmallVoidArray::IndexOf(void* aPossibleElement) const
{
  if (HasSingleChild())
  {
    if (aPossibleElement == (void*)GetSingleChild())
      return 0;
  }
  else
  {
    nsVoidArray* vector = GetChildVector();
    if (vector)
      return vector->IndexOf(aPossibleElement);
  }

  return -1;
}

PRBool
nsSmallVoidArray::InsertElementAt(void* aElement, PRInt32 aIndex)
{
  nsVoidArray* vector;
  NS_ASSERTION(!(PtrBits(aElement) & 0x1),"Attempt to add element with 0x1 bit set to nsSmallVoidArray");
  NS_ASSERTION(aElement != nsnull,"Attempt to add a NULL element to an nsSmallVoidArray");

  if (HasSingleChild())
  {
    vector = SwitchToVector();
  }
  else
  {
    vector = GetChildVector();
    if (!vector)
    {
      if (0 == aIndex)
      {
        SetSingleChild(aElement);
        return PR_TRUE;
      }
      return PR_FALSE;
    }
  }

  return vector->InsertElementAt(aElement, aIndex);
}

PRBool nsSmallVoidArray::InsertElementsAt(const nsVoidArray &other, PRInt32 aIndex)
{
  nsVoidArray* vector;
  PRInt32 count = other.Count();
  if (count == 0)
    return PR_TRUE;

#ifdef DEBUG  
  for (int i = 0; i < count; i++)
  {
    NS_ASSERTION(!(PtrBits(other.ElementAt(i)) & 0x1),"Attempt to add element with 0x1 bit set to nsSmallVoidArray");
    NS_ASSERTION(other.ElementAt(i) != nsnull,"Attempt to add a NULL element to an nsSmallVoidArray");
  }
#endif

  if (!HasVector())
  {
    if (HasSingleChild() || count > 1 || aIndex > 0)
    {
      vector = SwitchToVector();
    }
    else
    {
      // count == 1, aIndex == 0, no elements already
      SetSingleChild(other[0]);
      return PR_TRUE;
    }
  }
  else
  {
    vector = GetChildVector();
  }

  if (vector)
  {
    return vector->InsertElementsAt(other,aIndex);
  }
  return PR_TRUE;
}

PRBool
nsSmallVoidArray::ReplaceElementAt(void* aElement, PRInt32 aIndex)
{
  NS_ASSERTION(!(PtrBits(aElement) & 0x1),"Attempt to add element with 0x1 bit set to nsSmallVoidArray");
  NS_ASSERTION(aElement != nsnull,"Attempt to add a NULL element to an nsSmallVoidArray");

  if (HasSingleChild())
  {
    if (aIndex == 0)
    {
      SetSingleChild(aElement);
      return PR_TRUE;
    }
    return PR_FALSE;
  }
  else
  {
    nsVoidArray* vector = GetChildVector();
    if (vector)
      return vector->ReplaceElementAt(aElement, aIndex);

    return PR_FALSE;
  }
}

PRBool
nsSmallVoidArray::AppendElement(void* aElement)
{
  NS_ASSERTION(!(PtrBits(aElement) & 0x1),"Attempt to add element with 0x1 bit set to nsSmallVoidArray");
  NS_ASSERTION(aElement != nsnull,"Attempt to add a NULL element to an nsSmallVoidArray");

  nsVoidArray* vector;
  if (HasSingleChild())
  {
    vector = SwitchToVector();
  }
  else
  {
    vector = GetChildVector();
    if (!vector)
    {
      SetSingleChild(aElement);
      return PR_TRUE;
    }
  }

  return vector->AppendElement(aElement);
}

PRBool
nsSmallVoidArray::RemoveElement(void* aElement)
{
  if (HasSingleChild())
  {
    if (aElement == GetSingleChild())
    {
      SetSingleChild(nsnull);
      return PR_TRUE;
    }
  }
  else
  {
    nsVoidArray* vector = GetChildVector();
    if (vector)
      return vector->RemoveElement(aElement);
  }

  return PR_FALSE;
}

PRBool
nsSmallVoidArray::RemoveElementAt(PRInt32 aIndex)
{
  if (HasSingleChild())
  {
    if (0 == aIndex)
    {
      SetSingleChild(nsnull);
      return PR_TRUE;
    }
  }
  else
  {
    nsVoidArray* vector = GetChildVector();
    if (vector)
    {
      return vector->RemoveElementAt(aIndex);
    }
  }

  return PR_FALSE;
}

PRBool
nsSmallVoidArray::RemoveElementsAt(PRInt32 aIndex, PRInt32 aCount)
{
  nsVoidArray* vector = GetChildVector();

  if (aCount == 0)
    return PR_TRUE;

  if (HasSingleChild())
  {
    if (aIndex == 0)
      SetSingleChild(nsnull);
    return PR_TRUE;
  }

  if (!vector)
    return PR_TRUE;

  // complex case; remove entries from an array
  return vector->RemoveElementsAt(aIndex,aCount);
}

void
nsSmallVoidArray::Clear()
{
  if (HasVector())
  {
    nsVoidArray* vector = GetChildVector();
    vector->Clear();
  }
  else
  {
    SetSingleChild(nsnull);
  }
}

PRBool
nsSmallVoidArray::SizeTo(PRInt32 aMin)
{
  nsVoidArray* vector;
  if (!HasVector())
  {
    if (aMin <= 1)
      return PR_TRUE;
    vector = SwitchToVector();
  }
  else
  {
    vector = GetChildVector();
    if (aMin <= 1)
    {
      void *prev = nsnull;
      if (vector->Count() == 1)
      {
        prev = vector->ElementAt(0);
      }
      delete vector;
      SetSingleChild(prev);
      return PR_TRUE;
    }
  }
  return vector->SizeTo(aMin);
}

void
nsSmallVoidArray::Compact()
{
  if (!HasSingleChild())
  {
    nsVoidArray* vector = GetChildVector();
    if (vector)
      vector->Compact();
  }
}

void
nsSmallVoidArray::Sort(nsVoidArrayComparatorFunc aFunc, void* aData)
{
  if (HasVector())
  {
    nsVoidArray* vector = GetChildVector();
    vector->Sort(aFunc,aData);
  }
}

PRBool
nsSmallVoidArray::EnumerateForwards(nsVoidArrayEnumFunc aFunc, void* aData)
{
  if (HasVector())
  {
    nsVoidArray* vector = GetChildVector();
    return vector->EnumerateForwards(aFunc,aData);
  }
  if (HasSingleChild())
  {
    return (*aFunc)(GetSingleChild(), aData);
  }
  return PR_TRUE;
}

PRBool
nsSmallVoidArray::EnumerateBackwards(nsVoidArrayEnumFunc aFunc, void* aData)
{
  if (HasVector())
  {
    nsVoidArray* vector = GetChildVector();
    return vector->EnumerateBackwards(aFunc,aData);
  }
  if (HasSingleChild())
  {
    return (*aFunc)(GetSingleChild(), aData);
  }
  return PR_TRUE;
}

void
nsSmallVoidArray::SetSingleChild(void* aChild)
{
  if (aChild)
    mChildren = (void*)(PtrBits(aChild) | 0x1);
  else
    mChildren = nsnull;
}

nsVoidArray*
nsSmallVoidArray::SwitchToVector()
{
  void* child = GetSingleChild();

  mChildren = (void*)new nsAutoVoidArray();
  nsVoidArray* vector = GetChildVector();
  if (vector && child)
    vector->AppendElement(child);

  return vector;
}
