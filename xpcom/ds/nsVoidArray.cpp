/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsVoidArray.h"
#include "nsCRT.h"
#include "nsISizeOfHandler.h"
#include "nsString.h"

static const PRInt32 kGrowArrayBy = 4;
static const PRInt32 kLinearThreshold = 16;
static const PRInt32 kGrowthFactor = 1;

inline PRInt32
nsVoidArray::GetArraySize() const
{
  return mImpl ? PRInt32(mImpl->mBits & kArraySizeMask) : 0;
}

inline void
nsVoidArray::SetArraySize(PRInt32 aSize)
{
  NS_PRECONDITION(mImpl, "can't set size");
  mImpl->mBits &= ~kArraySizeMask;
  mImpl->mBits |= PRUint32(aSize & kArraySizeMask);
}

inline PRBool
nsVoidArray::IsArrayOwner() const
{
  return mImpl ? PRBool(mImpl->mBits & kArrayOwnerMask) : PR_FALSE;
}


inline void
nsVoidArray::SetArrayOwner(PRBool aOwner)
{
  NS_PRECONDITION(mImpl, "can't set owner");
  if (aOwner)
    mImpl->mBits |= kArrayOwnerMask;
  else
    mImpl->mBits &= ~kArrayOwnerMask;
}

nsVoidArray::nsVoidArray()
  : mImpl(nsnull)
{
  MOZ_COUNT_CTOR(nsVoidArray);
}

nsVoidArray::nsVoidArray(PRInt32 aCount)
  : mImpl(nsnull)
{
  MOZ_COUNT_CTOR(nsVoidArray);
  if (aCount) {
    char* bytes = new char[sizeof(Impl) + sizeof(void*) * (aCount - 1)];
    mImpl = NS_REINTERPRET_CAST(Impl*, bytes);
    if (mImpl) {
      mImpl->mBits = 0;
      SetArraySize(aCount);
      mImpl->mCount = aCount;
      nsCRT::memset(mImpl->mArray, 0, mImpl->mCount * sizeof(void*));
      SetArrayOwner(PR_TRUE);
    }
  }
}

nsVoidArray& nsVoidArray::operator=(const nsVoidArray& other)
{
  if (mImpl && IsArrayOwner())
    delete[] NS_REINTERPRET_CAST(char*, mImpl);

  PRInt32 otherCount = other.Count();
  if (otherCount) {
    char* bytes = new char[sizeof(Impl) + sizeof(void*) * (otherCount - 1)];
    mImpl = NS_REINTERPRET_CAST(Impl*, bytes);
    if (mImpl) {
      mImpl->mBits = 0;
      SetArraySize(otherCount);
      mImpl->mCount = otherCount;
      SetArrayOwner(PR_TRUE);

      nsCRT::memcpy(mImpl->mArray, other.mImpl->mArray, mImpl->mCount * sizeof(void*));
    }
  }
  else {
    mImpl = nsnull;
  }

  return *this;
}

nsVoidArray::~nsVoidArray()
{
  MOZ_COUNT_DTOR(nsVoidArray);
  if (mImpl && IsArrayOwner())
    delete[] NS_REINTERPRET_CAST(char*, mImpl);
}

void
nsVoidArray::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (aResult) {
    *aResult = sizeof(*this) + (mImpl ? sizeof(Impl) + (sizeof(void*) * (GetArraySize() - 1)) : 0);
  }
}

void* nsVoidArray::ElementAt(PRInt32 aIndex) const
{
  if (aIndex < 0 || aIndex >= Count()) {
    return nsnull;
  }
  return mImpl->mArray[aIndex];
}

PRInt32 nsVoidArray::IndexOf(void* aPossibleElement) const
{
  if (mImpl) {
    void** ap = mImpl->mArray;
    void** end = ap + mImpl->mCount;
    while (ap < end) {
      if (*ap == aPossibleElement) {
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
  if (PRUint32(aIndex) > PRUint32(oldCount)) {
    // An invalid index causes the insertion to fail
    return PR_FALSE;
  }

  if (oldCount >= GetArraySize()) {
    // We have to grow the array. Grow by 4 slots if we're smaller
    // than 16 elements, or a power of two if we're larger.
    PRInt32 newCount = oldCount + ((oldCount >= kLinearThreshold) ? (oldCount / kGrowthFactor) : kGrowArrayBy);

    char* bytes = new char[sizeof(Impl) + sizeof(void*) * (newCount - 1)];
    Impl* newImpl = NS_REINTERPRET_CAST(Impl*, bytes);

    if (newImpl) {
      if (aIndex != 0)
        nsCRT::memcpy(newImpl->mArray, mImpl->mArray, aIndex * sizeof(void*));

      PRInt32 slide = oldCount - aIndex;
      if (0 != slide) {
        // Slide data over to make room for the insertion
        nsCRT::memcpy(newImpl->mArray + aIndex + 1, mImpl->mArray + aIndex,
                      slide * sizeof(void*));
      }

      if (IsArrayOwner())
        delete[] NS_REINTERPRET_CAST(char*, mImpl);

      mImpl = newImpl;
      mImpl->mBits = 0;
      SetArraySize(newCount);
      mImpl->mCount = oldCount;
      SetArrayOwner(PR_TRUE);
    }
  }
  else {
    // The array is already large enough
    PRInt32 slide = oldCount - aIndex;
    if (0 != slide) {
      // Slide data over to make room for the insertion
      nsCRT::memmove(mImpl->mArray + aIndex + 1, mImpl->mArray + aIndex,
                     slide * sizeof(void*));
    }
  }

  mImpl->mArray[aIndex] = aElement;
  mImpl->mCount++;

  return PR_TRUE;
}

PRBool nsVoidArray::ReplaceElementAt(void* aElement, PRInt32 aIndex)
{
  if (PRUint32(aIndex) >= PRUint32(GetArraySize())) {
    PRInt32 oldCount = Count();
    PRInt32 requestedCount = aIndex + 1;
    PRInt32 growDelta = requestedCount - oldCount;
    PRInt32 newCount = oldCount + (growDelta > kGrowArrayBy ? growDelta : kGrowArrayBy);

    char* bytes = new char[sizeof(Impl) + sizeof(void*) * (newCount - 1)];
    Impl* newImpl = NS_REINTERPRET_CAST(Impl*, bytes);

    if (newImpl) {
      nsCRT::memset(newImpl->mArray, 0, newCount * sizeof(void*));

      if (mImpl != nsnull && aIndex != 0)
        nsCRT::memcpy(newImpl->mArray, mImpl->mArray, mImpl->mCount * sizeof(void*));

      if (IsArrayOwner())
         delete[] NS_REINTERPRET_CAST(char*, mImpl);
      
      mImpl = newImpl;
      mImpl->mBits = 0;
      SetArraySize(newCount);
      mImpl->mCount = oldCount;
      SetArrayOwner(PR_TRUE);
    }
  }

  mImpl->mArray[aIndex] = aElement;
  if (aIndex >= mImpl->mCount)
     mImpl->mCount = aIndex + 1;

  return PR_TRUE;
}

PRBool nsVoidArray::RemoveElementAt(PRInt32 aIndex)
{
  PRInt32 oldCount = Count();
  if (PRUint32(aIndex) >= PRUint32(oldCount)) {
    // An invalid index causes the replace to fail
    return PR_FALSE;
  }

  // We don't need to move any elements if we're removing the
  // last element in the array
  if (aIndex < (oldCount - 1)) {
    nsCRT::memmove(mImpl->mArray + aIndex, mImpl->mArray + aIndex + 1,
                   (oldCount - 1 - aIndex) * sizeof(void*));
  }

  mImpl->mCount--;
  return PR_TRUE;
}

PRBool nsVoidArray::RemoveElement(void* aElement)
{
  if (mImpl) {
    void** ep = mImpl->mArray;
    void** end = ep + mImpl->mCount;
    while (ep < end) {
      void* e = *ep++;
      if (e == aElement) {
        ep--;
        return RemoveElementAt(PRInt32(ep - mImpl->mArray));
      }
    }
  }
  return PR_FALSE;
}

void nsVoidArray::Clear()
{
  if (mImpl) {
    mImpl->mCount = 0;
  }
}

void nsVoidArray::Compact()
{
  if (mImpl) {
    PRInt32 count = mImpl->mCount;
    if (IsArrayOwner() && GetArraySize() > count) {
      Impl* doomedImpl = mImpl;

      if (count > 0) {
        char* bytes = new char[sizeof(Impl) + sizeof(void*) * (count - 1)];
        Impl* newImpl = NS_REINTERPRET_CAST(Impl*, bytes);
        if (newImpl)
          nsCRT::memcpy(newImpl->mArray, mImpl->mArray, count * sizeof(void*));

        mImpl = newImpl;
        mImpl->mBits = 0;
        SetArraySize(count);
        mImpl->mCount = count;
        SetArrayOwner(PR_TRUE);
      }
      else {
        mImpl = nsnull;
      }

      delete[] NS_REINTERPRET_CAST(char*, doomedImpl);
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
  if (mImpl && mImpl->mCount > 1) {
    VoidArrayComparatorContext ctx = {aFunc, aData};
    NS_QuickSort(mImpl->mArray, mImpl->mCount, sizeof(void*),
                 VoidArrayComparator, &ctx);
  }
}

PRBool nsVoidArray::EnumerateForwards(nsVoidArrayEnumFunc aFunc, void* aData)
{
  PRInt32 index = -1;
  PRBool  running = PR_TRUE;

  if (mImpl) {
    while (running && (++index < mImpl->mCount)) {
      running = (*aFunc)(mImpl->mArray[index], aData);
    }
  }
  return running;
}

PRBool nsVoidArray::EnumerateBackwards(nsVoidArrayEnumFunc aFunc, void* aData)
{
  PRBool  running = PR_TRUE;

  if (mImpl) {
    PRInt32 index = Count();
    while (running && (0 <= --index)) {
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
  mImpl = NS_REINTERPRET_CAST(Impl*, mAutoBuf);
  mImpl->mBits = 0;
  SetArraySize(kAutoBufSize);
  mImpl->mCount = 0;
  SetArrayOwner(PR_FALSE);
}

//----------------------------------------------------------------
// nsStringArray

nsStringArray::nsStringArray(void)
  : nsVoidArray()
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
  for (PRInt32 i = Count() - 1; i >= 0; --i) {
    nsString* oldString = NS_STATIC_CAST(nsString*, other.ElementAt(i));
    mImpl->mArray[i] = new nsString(*oldString);
  }

  return *this;
}

void  
nsStringArray::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  PRUint32 sum = 0;
  nsVoidArray::SizeOf(aHandler, &sum);
  PRInt32 index = Count();
  while (0 <= --index) {
    nsString* string = NS_STATIC_CAST(nsString*, ElementAt(index));
    PRUint32 size;
    string->SizeOf(aHandler, &size);
    sum += size;
  }
}

void 
nsStringArray::StringAt(PRInt32 aIndex, nsAWritableString& aString) const
{
  nsString* string = NS_STATIC_CAST(nsString*, nsVoidArray::ElementAt(aIndex));
  if (nsnull != string) {
    aString.Assign(*string);
  }
  else {
    aString.Truncate();
  }
}

nsString*
nsStringArray::StringAt(PRInt32 aIndex) const
{
  return NS_STATIC_CAST(nsString*, nsVoidArray::ElementAt(aIndex));
}

PRInt32 
nsStringArray::IndexOf(const nsAReadableString& aPossibleString) const
{
  if (mImpl) {
    void** ap = mImpl->mArray;
    void** end = ap + mImpl->mCount;
    while (ap < end) {
      nsString* string = NS_STATIC_CAST(nsString*, *ap);
      if (string->Equals(aPossibleString)) {
        return ap - mImpl->mArray;
      }
      ap++;
    }
  }
  return -1;
}

PRInt32 
nsStringArray::IndexOfIgnoreCase(const nsAReadableString& aPossibleString) const
{
  if (mImpl) {
    void** ap = mImpl->mArray;
    void** end = ap + mImpl->mCount;
    while (ap < end) {
      nsString* string = NS_STATIC_CAST(nsString*, *ap);
      if (!Compare(*string, aPossibleString, nsCaseInsensitiveStringComparator())) {
        return ap - mImpl->mArray;
      }
      ap++;
    }
  }
  return -1;
}

PRBool 
nsStringArray::InsertStringAt(const nsAReadableString& aString, PRInt32 aIndex)
{
  nsString* string = new nsString(aString);
  if (nsVoidArray::InsertElementAt(string, aIndex)) {
    return PR_TRUE;
  }
  delete string;
  return PR_FALSE;
}

PRBool
nsStringArray::ReplaceStringAt(const nsAReadableString& aString,
                               PRInt32 aIndex)
{
  nsString* string = NS_STATIC_CAST(nsString*, nsVoidArray::ElementAt(aIndex));
  if (nsnull != string) {
    *string = aString;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool 
nsStringArray::RemoveString(const nsAReadableString& aString)
{
  PRInt32 index = IndexOf(aString);
  if (-1 < index) {
    return RemoveStringAt(index);
  }
  return PR_FALSE;
}

PRBool 
nsStringArray::RemoveStringIgnoreCase(const nsAReadableString& aString)
{
  PRInt32 index = IndexOfIgnoreCase(aString);
  if (-1 < index) {
    return RemoveStringAt(index);
  }
  return PR_FALSE;
}

PRBool nsStringArray::RemoveStringAt(PRInt32 aIndex)
{
  nsString* string = StringAt(aIndex);
  if (nsnull != string) {
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
  while (0 <= --index) {
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

PR_STATIC_CALLBACK(int)
CompareStringIgnoreCase(const nsString* aString1, const nsString* aString2, void*)
{
  return Compare(*aString1, *aString2, nsCaseInsensitiveStringComparator());
}

void nsStringArray::Sort(void)
{
  Sort(CompareString, nsnull);
}

void nsStringArray::SortIgnoreCase(void)
{
  Sort(CompareStringIgnoreCase, nsnull);
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

  if (mImpl) {
    while (running && (++index < mImpl->mCount)) {
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

  if (mImpl) {
    while (running && (0 <= --index)) {
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
  for (PRInt32 i = Count() - 1; i >= 0; --i) {
    nsCString* oldString = NS_STATIC_CAST(nsCString*, other.ElementAt(i));
    mImpl->mArray[i] = new nsCString(*oldString);
  }

  return *this;
}

void  
nsCStringArray::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  PRUint32 sum = 0;
  nsVoidArray::SizeOf(aHandler, &sum);
  PRInt32 index = Count();
  while (0 <= --index) {
    nsCString* string = NS_STATIC_CAST(nsCString*, mImpl->mArray[index]);
    PRUint32 size;
    string->SizeOf(aHandler, &size);
    sum += size;
  }
}

void 
nsCStringArray::CStringAt(PRInt32 aIndex, nsCString& aCString) const
{
  nsCString* string = NS_STATIC_CAST(nsCString*, nsVoidArray::ElementAt(aIndex));
  if (nsnull != string) {
    aCString = *string;
  }
  else {
    aCString.Truncate();
  }
}

nsCString*
nsCStringArray::CStringAt(PRInt32 aIndex) const
{
  return NS_STATIC_CAST(nsCString*, nsVoidArray::ElementAt(aIndex));
}

PRInt32 
nsCStringArray::IndexOf(const nsCString& aPossibleString) const
{
  if (mImpl) {
    void** ap = mImpl->mArray;
    void** end = ap + mImpl->mCount;
    while (ap < end) {
      nsCString* string = NS_STATIC_CAST(nsCString*, *ap);
      if (string->Equals(aPossibleString)) {
        return ap - mImpl->mArray;
      }
      ap++;
    }
  }
  return -1;
}

PRInt32 
nsCStringArray::IndexOfIgnoreCase(const nsCString& aPossibleString) const
{
  if (mImpl) {
    void** ap = mImpl->mArray;
    void** end = ap + mImpl->mCount;
    while (ap < end) {
      nsCString* string = NS_STATIC_CAST(nsCString*, *ap);
      if (string->EqualsIgnoreCase(aPossibleString)) {
        return ap - mImpl->mArray;
      }
      ap++;
    }
  }
  return -1;
}

PRBool 
nsCStringArray::InsertCStringAt(const nsCString& aCString, PRInt32 aIndex)
{
  nsCString* string = new nsCString(aCString);
  if (nsVoidArray::InsertElementAt(string, aIndex)) {
    return PR_TRUE;
  }
  delete string;
  return PR_FALSE;
}

PRBool
nsCStringArray::ReplaceCStringAt(const nsCString& aCString, PRInt32 aIndex)
{
  nsCString* string = NS_STATIC_CAST(nsCString*, nsVoidArray::ElementAt(aIndex));
  if (nsnull != string) {
    *string = aCString;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool 
nsCStringArray::RemoveCString(const nsCString& aCString)
{
  PRInt32 index = IndexOf(aCString);
  if (-1 < index) {
    return RemoveCStringAt(index);
  }
  return PR_FALSE;
}

PRBool 
nsCStringArray::RemoveCStringIgnoreCase(const nsCString& aCString)
{
  PRInt32 index = IndexOfIgnoreCase(aCString);
  if (-1 < index) {
    return RemoveCStringAt(index);
  }
  return PR_FALSE;
}

PRBool nsCStringArray::RemoveCStringAt(PRInt32 aIndex)
{
  nsCString* string = CStringAt(aIndex);
  if (nsnull != string) {
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
  while (0 <= --index) {
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

  if (mImpl) {
    PRInt32 index = -1;
    while (running && (++index < mImpl->mCount)) {
      running = (*aFunc)(*NS_STATIC_CAST(nsCString*, mImpl->mArray[index]), aData);
    }
  }
  return running;
}

PRBool 
nsCStringArray::EnumerateBackwards(nsCStringArrayEnumFunc aFunc, void* aData)
{
  PRBool  running = PR_TRUE;

  if (mImpl) {
    PRInt32 index = Count();
    while (running && (0 <= --index)) {
      running = (*aFunc)(*NS_STATIC_CAST(nsCString*, mImpl->mArray[index]), aData);
    }
  }
  return running;
}
