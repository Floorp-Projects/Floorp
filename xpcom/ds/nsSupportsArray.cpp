/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Scott Collins <scc@mozilla.org>: |do_QueryElementAt|
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include <string.h>
#include "prbit.h"
#include "nsSupportsArray.h"
#include "nsSupportsArrayEnumerator.h"
#include "nsAString.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"

#if DEBUG_SUPPORTSARRAY
#define MAXSUPPORTS 20

class SupportsStats {
public:
  SupportsStats();
  ~SupportsStats();

};

static int sizesUsed; // number of the elements of the arrays used
static int sizesAlloced[MAXSUPPORTS]; // sizes of the allocations.  sorted
static int NumberOfSize[MAXSUPPORTS]; // number of this allocation size (1 per array)
static int AllocedOfSize[MAXSUPPORTS]; // number of this allocation size (each size for array used)
static int GrowInPlace[MAXSUPPORTS];

// these are per-allocation
static int MaxElements[3000];

// very evil
#define ADD_TO_STATS(x,size) do {int i; for (i = 0; i < sizesUsed; i++) \
                                  { \
                                    if (sizesAlloced[i] == (int)(size)) \
                                    { ((x)[i])++; break; } \
                                  } \
                                  if (i >= sizesUsed && sizesUsed < MAXSUPPORTS) \
                                  { sizesAlloced[sizesUsed] = (size); \
                                    ((x)[sizesUsed++])++; break; \
                                  } \
                                } while (0);

#define SUB_FROM_STATS(x,size) do {int i; for (i = 0; i < sizesUsed; i++) \
                                    { \
                                      if (sizesAlloced[i] == (int)(size)) \
                                      { ((x)[i])--; break; } \
                                    } \
                                  } while (0);


SupportsStats::SupportsStats()
{
  sizesUsed = 1;
  sizesAlloced[0] = 0;
}

SupportsStats::~SupportsStats()
{
  int i;
  for (i = 0; i < sizesUsed; i++)
  {
    printf("Size %d:\n",sizesAlloced[i]);
    printf("\tNumber of SupportsArrays this size (max):     %d\n",NumberOfSize[i]);
    printf("\tNumber of allocations this size (total):  %d\n",AllocedOfSize[i]);
    printf("\tNumber of GrowsInPlace this size (total): %d\n",GrowInPlace[i]);
  }
  printf("Max Size of SupportsArray:\n");
  for (i = 0; i < (int)(sizeof(MaxElements)/sizeof(MaxElements[0])); i++)
  {
    if (MaxElements[i])
      printf("\t%d: %d\n",i,MaxElements[i]);
  }
}

// Just so constructor/destructor get called
SupportsStats gSupportsStats;
#endif

nsresult
nsQueryElementAt::operator()( const nsIID& aIID, void** aResult ) const
  {
    nsresult status = mCollection
                        ? mCollection->QueryElementAt(mIndex, aIID, aResult)
                        : NS_ERROR_NULL_POINTER;

    if ( mErrorPtr )
      *mErrorPtr = status;

    return status;
  }

static const PRInt32 kGrowArrayBy = 8;
static const PRInt32 kLinearThreshold = 16 * sizeof(nsISupports *);

nsSupportsArray::nsSupportsArray()
{
  mArray = mAutoArray;
  mArraySize = kAutoArraySize;
  mCount = 0;
#if DEBUG_SUPPORTSARRAY
  mMaxCount = 0;
  mMaxSize = 0;
  ADD_TO_STATS(NumberOfSize,kAutoArraySize*sizeof(mArray[0]));
  MaxElements[0]++;
#endif
}

nsSupportsArray::~nsSupportsArray()
{
  DeleteArray();
}

PRBool nsSupportsArray::GrowArrayBy(PRInt32 aGrowBy)
{
  // We have to grow the array. Grow by kGrowArrayBy slots if we're smaller
  // than kLinearThreshold bytes, or a power of two if we're larger.
  // This is much more efficient with most memory allocators, especially
  // if it's very large, or of the allocator is binned.
  if (aGrowBy < kGrowArrayBy)
    aGrowBy = kGrowArrayBy;

  PRUint32 newCount = mArraySize + aGrowBy;  // Minimum increase
  PRUint32 newSize = sizeof(mArray[0]) * newCount;

  if (newSize >= (PRUint32) kLinearThreshold)
  {
    // newCount includes enough space for at least kGrowArrayBy new slots.
    // Select the next power-of-two size in bytes above that if newSize is
    // not a power of two.
    if (newSize & (newSize - 1))
      newSize = PR_BIT(PR_CeilingLog2(newSize));

    newCount = newSize / sizeof(mArray[0]);
  }
  // XXX This would be far more efficient in many allocators if we used
  // XXX PR_Realloc(), etc
  nsISupports** oldArray = mArray;

  mArray = new nsISupports*[newCount];
  if (!mArray) {                    // ran out of memory
    mArray = oldArray;
    return PR_FALSE;
  }
  mArraySize = newCount;

#if DEBUG_SUPPORTSARRAY
  if (oldArray == mArray) // can't happen without use of realloc
    ADD_TO_STATS(GrowInPlace,mCount);
  ADD_TO_STATS(AllocedOfSize,mArraySize*sizeof(mArray[0]));
  if (mArraySize > mMaxSize)
  {
    ADD_TO_STATS(NumberOfSize,mArraySize*sizeof(mArray[0]));
    if (oldArray != &(mAutoArray[0]))
      SUB_FROM_STATS(NumberOfSize,mCount*sizeof(mArray[0]));
    mMaxSize = mArraySize;
  }
#endif
  if (oldArray) {                   // need to move old data
    if (0 < mCount) {
      ::memcpy(mArray, oldArray, mCount * sizeof(nsISupports*));
    }
    if (oldArray != &(mAutoArray[0])) {
      delete[] oldArray;
    }
  }

  return PR_TRUE;
}

NS_METHOD
nsSupportsArray::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsCOMPtr<nsISupportsArray> it = new nsSupportsArray();
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  return it->QueryInterface(aIID, aResult);
}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsSupportsArray, nsISupportsArray, nsICollection, nsISerializable)

NS_IMETHODIMP
nsSupportsArray::Read(nsIObjectInputStream *aStream)
{
  nsresult rv;

  PRUint32 newArraySize;
  rv = aStream->Read32(&newArraySize);

  if (newArraySize <= kAutoArraySize) {
    if (mArray != mAutoArray) {
      delete[] mArray;
      mArray = mAutoArray;
    }
    newArraySize = kAutoArraySize;
  }
  else {
    if (newArraySize <= mArraySize) {
      // Keep non-default-size mArray, it's more than big enough.
      newArraySize = mArraySize;
    }
    else {
      nsISupports** array = new nsISupports*[newArraySize];
      if (!array)
        return NS_ERROR_OUT_OF_MEMORY;
      if (mArray != mAutoArray)
        delete[] mArray;
      mArray = array;
    }
  }
  mArraySize = newArraySize;

  rv = aStream->Read32(&mCount);
  if (NS_FAILED(rv)) return rv;

  NS_ASSERTION(mCount <= mArraySize, "overlarge mCount!");
  if (mCount > mArraySize)
    mCount = mArraySize;

  for (PRUint32 i = 0; i < mCount; i++) {
    rv = aStream->ReadObject(PR_TRUE, &mArray[i]);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSupportsArray::Write(nsIObjectOutputStream *aStream)
{
  nsresult rv;

  rv = aStream->Write32(mArraySize);
  if (NS_FAILED(rv)) return rv;

  rv = aStream->Write32(mCount);
  if (NS_FAILED(rv)) return rv;

  for (PRUint32 i = 0; i < mCount; i++) {
    rv = aStream->WriteObject(mArray[i], PR_TRUE);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

void nsSupportsArray::DeleteArray(void)
{
  Clear();
  if (mArray != &(mAutoArray[0])) {
    delete[] mArray;
    mArray = mAutoArray;
    mArraySize = kAutoArraySize;
  }
}


NS_IMETHODIMP_(PRBool)
nsSupportsArray::Equals(const nsISupportsArray* aOther)
{
  if (aOther) {
    PRUint32 countOther;
    nsISupportsArray* other = NS_CONST_CAST(nsISupportsArray*, aOther);
    nsresult rv = other->Count(&countOther);
    if (NS_FAILED( rv ))
      return PR_FALSE;

    if (mCount == countOther) {
      PRUint32 index = mCount;
      nsCOMPtr<nsISupports> otherElem;
      while (index--) {
        if (NS_FAILED(other->GetElementAt(index, getter_AddRefs(otherElem))))
          return PR_FALSE;
        if (mArray[index] != otherElem)
          return PR_FALSE;
      }
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

NS_IMETHODIMP_(nsISupports*)
nsSupportsArray::ElementAt(PRUint32 aIndex)
{
  if (aIndex < mCount) {
    nsISupports*  element = mArray[aIndex];
    NS_IF_ADDREF(element);
    return element;
  }
  return 0;
}

NS_IMETHODIMP_(PRInt32)
nsSupportsArray::IndexOf(const nsISupports* aPossibleElement)
{
  return IndexOfStartingAt(aPossibleElement, 0);
}

NS_IMETHODIMP_(PRInt32)
nsSupportsArray::IndexOfStartingAt(const nsISupports* aPossibleElement,
                                   PRUint32 aStartIndex)
{
  if (aStartIndex < mCount) {
    const nsISupports** start = (const nsISupports**)mArray;  // work around goofy compiler behavior
    const nsISupports** ep = (start + aStartIndex);
    const nsISupports** end = (start + mCount);
    while (ep < end) {
      if (aPossibleElement == *ep) {
        return (ep - start);
      }
      ep++;
    }
  }
  return -1;
}

NS_IMETHODIMP_(PRInt32)
nsSupportsArray::LastIndexOf(const nsISupports* aPossibleElement)
{
  if (0 < mCount) {
    const nsISupports** start = (const nsISupports**)mArray;  // work around goofy compiler behavior
    const nsISupports** ep = (start + mCount);
    while (start <= --ep) {
      if (aPossibleElement == *ep) {
        return (ep - start);
      }
    }
  }
  return -1;
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::InsertElementAt(nsISupports* aElement, PRUint32 aIndex)
{
  if (aIndex <= mCount) {
    if (mArraySize < (mCount + 1)) {
      // need to grow the array
      if (!GrowArrayBy(1))
        return PR_FALSE;
    }

    // Could be slightly more efficient if GrowArrayBy knew about the
    // split, but the difference is trivial.
    PRUint32 slide = (mCount - aIndex);
    if (0 < slide) {
      ::memmove(mArray + aIndex + 1, mArray + aIndex, slide * sizeof(nsISupports*));
    }

    mArray[aIndex] = aElement;
    NS_IF_ADDREF(aElement);
    mCount++;

#if DEBUG_SUPPORTSARRAY
    if (mCount > mMaxCount &&
        mCount < (PRInt32)(sizeof(MaxElements)/sizeof(MaxElements[0])))
    {
      MaxElements[mCount]++;
      MaxElements[mMaxCount]--;
      mMaxCount = mCount;
    }
#endif
    return PR_TRUE;
  }
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::InsertElementsAt(nsISupportsArray* aElements, PRUint32 aIndex)
{
  if (!aElements) {
    return PR_FALSE;
  }
  PRUint32 countElements;
  if (NS_FAILED( aElements->Count( &countElements ) ))
    return PR_FALSE;

  if (aIndex <= mCount) {
    if (mArraySize < (mCount + countElements)) {
      // need to grow the array
      if (!GrowArrayBy(countElements))
        return PR_FALSE;
    }

    // Could be slightly more efficient if GrowArrayBy knew about the
    // split, but the difference is trivial.
    PRUint32 slide = (mCount - aIndex);
    if (0 < slide) {
      ::memmove(mArray + aIndex + countElements, mArray + aIndex,
                slide * sizeof(nsISupports*));
    }

    for (PRUint32 i = 0; i < countElements; ++i, ++mCount) {
      // use GetElementAt to copy and do AddRef for us
      if (NS_FAILED( aElements->GetElementAt( i, mArray + aIndex + i) ))
        return PR_FALSE;
    }

#if DEBUG_SUPPORTSARRAY
    if (mCount > mMaxCount &&
        mCount < (PRInt32)(sizeof(MaxElements)/sizeof(MaxElements[0])))
    {
      MaxElements[mCount]++;
      MaxElements[mMaxCount]--;
      mMaxCount = mCount;
    }
#endif
    return PR_TRUE;
  }
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::ReplaceElementAt(nsISupports* aElement, PRUint32 aIndex)
{
  if (aIndex < mCount) {
    NS_IF_ADDREF(aElement);  // addref first in case it's the same object!
    NS_IF_RELEASE(mArray[aIndex]);
    mArray[aIndex] = aElement;
    return PR_TRUE;
  }
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::RemoveElementsAt(PRUint32 aIndex, PRUint32 aCount)
{
  if (aIndex + aCount <= mCount) {
    for (PRUint32 i = 0; i < aCount; i++)
      NS_IF_RELEASE(mArray[aIndex+i]);
    mCount -= aCount;
    PRInt32 slide = (mCount - aIndex);
    if (0 < slide) {
      ::memmove(mArray + aIndex, mArray + aIndex + aCount,
                slide * sizeof(nsISupports*));
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::RemoveElement(const nsISupports* aElement, PRUint32 aStartIndex)
{
  PRInt32 theIndex = IndexOfStartingAt(aElement,aStartIndex);
  if (theIndex >= 0)
    return RemoveElementAt(theIndex);

  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::RemoveLastElement(const nsISupports* aElement)
{
  PRInt32 theIndex = LastIndexOf(aElement);
  if (theIndex >= 0)
    return RemoveElementAt(theIndex);

  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::MoveElement(PRInt32 aFrom, PRInt32 aTo)
{
  nsISupports *tempElement;

  if (aTo == aFrom)
    return PR_TRUE;

  if (aTo < 0 || aFrom < 0 ||
      (PRUint32) aTo >= mCount || (PRUint32) aFrom >= mCount)
  {
    // can't extend the array when moving an element.  Also catches mImpl = null
    return PR_FALSE;
  }
  tempElement = mArray[aFrom];

  if (aTo < aFrom)
  {
    // Moving one element closer to the head; the elements inbetween move down
    ::memmove(mArray + aTo + 1, mArray + aTo,
              (aFrom-aTo) * sizeof(mArray[0]));
    mArray[aTo] = tempElement;
  }
  else // already handled aFrom == aTo
  {
    // Moving one element closer to the tail; the elements inbetween move up
    ::memmove(mArray + aFrom, mArray + aFrom + 1,
              (aTo-aFrom) * sizeof(mArray[0]));
    mArray[aTo] = tempElement;
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsSupportsArray::Clear(void)
{
  if (0 < mCount) {
    do {
      --mCount;
      NS_IF_RELEASE(mArray[mCount]);
    } while (0 != mCount);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsArray::Compact(void)
{
#if DEBUG_SUPPORTSARRAY
  PRUint32 oldArraySize = mArraySize;
#endif
  if ((mArraySize != mCount) && (kAutoArraySize < mArraySize)) {
    nsISupports** oldArray = mArray;
    if (mCount <= kAutoArraySize) {
      mArray = mAutoArray;
      mArraySize = kAutoArraySize;
    }
    else {
      mArray = new nsISupports*[mCount];
      if (!mArray) {
        mArray = oldArray;
        return NS_OK;
      }
      mArraySize = mCount;
    }
#if DEBUG_SUPPORTSARRAY
    if (oldArray == mArray &&
        oldArray != &(mAutoArray[0])) // can't happen without use of realloc
      ADD_TO_STATS(GrowInPlace,oldArraySize);
    if (oldArray != &(mAutoArray[0]))
      ADD_TO_STATS(AllocedOfSize,mArraySize*sizeof(mArray[0]));
#endif
    ::memcpy(mArray, oldArray, mCount * sizeof(nsISupports*));
    delete[] oldArray;
  }
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::SizeTo(PRInt32 aSize)
{
#if DEBUG_SUPPORTSARRAY
  PRUint32 oldArraySize = mArraySize;
#endif
  NS_ASSERTION(aSize >= 0, "negative aSize!");

  // XXX for aSize < mCount we could resize to mCount
  if (mArraySize == (PRUint32) aSize || (PRUint32) aSize < mCount)
    return PR_TRUE;     // nothing to do

  // switch back to autoarray if possible
  nsISupports** oldArray = mArray;
  if ((PRUint32) aSize <= kAutoArraySize) {
    mArray = mAutoArray;
    mArraySize = kAutoArraySize;
  }
  else {
    mArray = new nsISupports*[aSize];
    if (!mArray) {
      mArray = oldArray;
      return PR_FALSE;
    }
    mArraySize = aSize;
  }
#if DEBUG_SUPPORTSARRAY
  if (oldArray == mArray &&
      oldArray != &(mAutoArray[0])) // can't happen without use of realloc
    ADD_TO_STATS(GrowInPlace,oldArraySize);
  if (oldArray != &(mAutoArray[0]))
    ADD_TO_STATS(AllocedOfSize,mArraySize*sizeof(mArray[0]));
#endif
  ::memcpy(mArray, oldArray, mCount * sizeof(nsISupports*));
  if (oldArray != mAutoArray)
    delete[] oldArray;

  return PR_TRUE;
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::EnumerateForwards(nsISupportsArrayEnumFunc aFunc, void* aData)
{
  PRInt32 aIndex = -1;
  PRBool  running = PR_TRUE;

  while (running && (++aIndex < (PRInt32)mCount)) {
    running = (*aFunc)(mArray[aIndex], aData);
  }
  return running;
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::EnumerateBackwards(nsISupportsArrayEnumFunc aFunc, void* aData)
{
  PRUint32 aIndex = mCount;
  PRBool  running = PR_TRUE;

  while (running && (0 < aIndex--)) {
    running = (*aFunc)(mArray[aIndex], aData);
  }
  return running;
}

NS_IMETHODIMP
nsSupportsArray::Enumerate(nsIEnumerator* *result)
{
  nsSupportsArrayEnumerator* e = new nsSupportsArrayEnumerator(this);
  if (!e)
    return NS_ERROR_OUT_OF_MEMORY;
  *result = e;
  NS_ADDREF(e);
  return NS_OK;
}

static PRBool
CopyElement(nsISupports* aElement, void *aData)
{
  nsresult rv;
  nsISupportsArray* newArray = (nsISupportsArray*)aData;
  rv = newArray->AppendElement(aElement);
  return NS_SUCCEEDED(rv);
}

NS_IMETHODIMP
nsSupportsArray::Clone(nsISupportsArray* *result)
{
  nsresult rv;
  nsISupportsArray* newArray;
  rv = NS_NewISupportsArray(&newArray);
  PRBool ok = EnumerateForwards(CopyElement, newArray);
  if (!ok) return NS_ERROR_OUT_OF_MEMORY;
  *result = newArray;
  return NS_OK;
}

NS_COM nsresult
NS_NewISupportsArray(nsISupportsArray** aInstancePtrResult)
{
  nsresult rv;
  rv = nsSupportsArray::Create(NULL, NS_GET_IID(nsISupportsArray),
                               (void**)aInstancePtrResult);
  return rv;
}

