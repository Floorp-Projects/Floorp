/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsVoidArray.h"
#include "nsCRT.h"
#include "nsISizeOfHandler.h"
#include "nsString.h"

static PRInt32 kGrowArrayBy = 8;

nsVoidArray::nsVoidArray()
{
  mArray = nsnull;
  mArraySize = 0;
  mCount = 0;
}

nsVoidArray::nsVoidArray(PRInt32 aCount)
{
  NS_PRECONDITION(aCount > 0, "bad count");
  mCount = mArraySize = aCount;
  mArray = new void*[mCount];
  nsCRT::memset(mArray, 0, mCount * sizeof(void*));
}

nsVoidArray& nsVoidArray::operator=(const nsVoidArray& other)
{
  if (nsnull != mArray) {
    delete [] mArray;
  }
  PRInt32 otherCount = other.mCount;
  mArraySize = otherCount;
  mCount = otherCount;
  if (otherCount != 0) {
    mArray = new void*[otherCount];
    nsCRT::memcpy(mArray, other.mArray, otherCount * sizeof(void*));
  } else {
    mArray = nsnull;
  }
  return *this;
}

nsVoidArray::~nsVoidArray()
{
  if (nsnull != mArray) {
    delete [] mArray;
  }
}

void
nsVoidArray::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (aResult) {
    *aResult = sizeof(*this) + sizeof(void*) * mArraySize;
  }
}

void* nsVoidArray::ElementAt(PRInt32 aIndex) const
{
  if (PRUint32(aIndex) >= PRUint32(mCount)) {
    return nsnull;
  }
  return mArray[aIndex];
}

PRInt32 nsVoidArray::IndexOf(void* aPossibleElement) const
{
  void** ap = mArray;
  void** end = ap + mCount;
  while (ap < end) {
    if (*ap == aPossibleElement) {
      return ap - mArray;
    }
    ap++;
  }
  return -1;
}

PRBool nsVoidArray::InsertElementAt(void* aElement, PRInt32 aIndex)
{
  PRInt32 oldCount = mCount;
  if (PRUint32(aIndex) > PRUint32(oldCount)) {
    // An invalid index causes the insertion to fail
    return PR_FALSE;
  }

  if (oldCount + 1 > mArraySize) {
    // We have to grow the array
    PRInt32 newCount = oldCount + kGrowArrayBy;
    void** newArray = new void*[newCount];
    if (mArray != nsnull && aIndex != 0)
      nsCRT::memcpy(newArray, mArray, aIndex * sizeof(void*));
    PRInt32 slide = oldCount - aIndex;
    if (0 != slide) {
      // Slide data over to make room for the insertion
      nsCRT::memcpy(newArray + aIndex + 1, mArray + aIndex,
                    slide * sizeof(void*));
    }
    if (mArray != nsnull)
      delete [] mArray;
    mArray = newArray;
    mArraySize = newCount;
  } else {
    // The array is already large enough
    PRInt32 slide = oldCount - aIndex;
    if (0 != slide) {
      // Slide data over to make room for the insertion
      nsCRT::memmove(mArray + aIndex + 1, mArray + aIndex,
                     slide * sizeof(void*));
    }
  }
  mArray[aIndex] = aElement;
  mCount++;

  return PR_TRUE;
}

PRBool nsVoidArray::ReplaceElementAt(void* aElement, PRInt32 aIndex)
{
  if (PRUint32(aIndex) >= PRUint32(mArraySize)) {

    PRInt32 requestedCount = aIndex + 1;
    PRInt32 growDelta = requestedCount - mCount;
    PRInt32 newCount = mCount + (growDelta > kGrowArrayBy ? growDelta : kGrowArrayBy);
    void** newArray = new void*[newCount];
    nsCRT::memset(newArray, 0, newCount * sizeof(void*));
    if (newArray==nsnull)
       return PR_FALSE;
    if (mArray != nsnull && aIndex != 0) {
      nsCRT::memcpy(newArray, mArray, mCount * sizeof(void*));
      if (mArray != nsnull)
         delete [] mArray;
    }
    mArray = newArray;
    mArraySize = newCount;
  }
  mArray[aIndex] = aElement;
  if (aIndex >= mCount)
     mCount = aIndex+1;
  return PR_TRUE;
}

PRBool nsVoidArray::RemoveElementAt(PRInt32 aIndex)
{
  PRInt32 oldCount = mCount;
  if (PRUint32(aIndex) >= PRUint32(oldCount)) {
    // An invalid index causes the replace to fail
    return PR_FALSE;
  }

  // We don't need to move any elements if we're removing the
  // last element in the array
  if (aIndex < (oldCount - 1)) {
    nsCRT::memmove(mArray + aIndex, mArray + aIndex + 1,
                   (oldCount - 1 - aIndex) * sizeof(void*));
  }

  mCount--;
  return PR_TRUE;
}

PRBool nsVoidArray::RemoveElement(void* aElement)
{
  void** ep = mArray;
  void** end = ep + mCount;
  while (ep < end) {
    void* e = *ep++;
    if (e == aElement) {
      ep--;
      return RemoveElementAt(PRInt32(ep - mArray));
    }
  }
  return PR_FALSE;
}

void nsVoidArray::Clear()
{
  mCount = 0;
}

void nsVoidArray::Compact()
{
  PRInt32 count = mCount;
  if (mArraySize != count) {
    void** newArray = new void*[count];
    if (nsnull != newArray) {
      nsCRT::memcpy(newArray, mArray, count * sizeof(void*));
      delete [] mArray;
      mArray = newArray;
      mArraySize = count;
    }
  }
}

PRBool nsVoidArray::EnumerateForwards(nsVoidArrayEnumFunc aFunc, void* aData)
{
  PRInt32 index = -1;
  PRBool  running = PR_TRUE;

  while (running && (++index < mCount)) {
    running = (*aFunc)(mArray[index], aData);
  }
  return running;
}

PRBool nsVoidArray::EnumerateBackwards(nsVoidArrayEnumFunc aFunc, void* aData)
{
  PRInt32 index = mCount;
  PRBool  running = PR_TRUE;

  while (running && (0 <= --index)) {
    running = (*aFunc)(mArray[index], aData);
  }
  return running;
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
  if (nsnull != mArray) {
    delete mArray;
  }
  PRInt32 otherCount = other.mCount;
  mArraySize = otherCount;
  mCount = otherCount;
  if (0 < otherCount) {
    mArray = new void*[otherCount];
    while (0 <= --otherCount) {
      nsString* otherString = (nsString*)(other.mArray[otherCount]);
      mArray[otherCount] = new nsString(*otherString);
    }
  } else {
    mArray = nsnull;
  }
  return *this;
}

void  
nsStringArray::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  PRUint32 sum = 0;
  nsVoidArray::SizeOf(aHandler, &sum);
  PRInt32 index = mCount;
  while (0 <= --index) {
    nsString* string = (nsString*)mArray[index];
    PRUint32 size;
    string->SizeOf(aHandler, &size);
    sum += size;
  }
}

void 
nsStringArray::StringAt(PRInt32 aIndex, nsString& aString) const
{
  nsString* string = (nsString*)nsVoidArray::ElementAt(aIndex);
  if (nsnull != string) {
    aString = *string;
  }
  else {
    aString.Truncate();
  }
}

nsString*
nsStringArray::StringAt(PRInt32 aIndex) const
{
  return (nsString*)nsVoidArray::ElementAt(aIndex);
}

PRInt32 
nsStringArray::IndexOf(const nsString& aPossibleString) const
{
  void** ap = mArray;
  void** end = ap + mCount;
  while (ap < end) {
    nsString* string = (nsString*)*ap;
    if (string->Equals(aPossibleString)) {
      return ap - mArray;
    }
    ap++;
  }
  return -1;
}

PRInt32 
nsStringArray::IndexOfIgnoreCase(const nsString& aPossibleString) const
{
  void** ap = mArray;
  void** end = ap + mCount;
  while (ap < end) {
    nsString* string = (nsString*)*ap;
    if (string->EqualsIgnoreCase(aPossibleString)) {
      return ap - mArray;
    }
    ap++;
  }
  return -1;
}

PRBool 
nsStringArray::InsertStringAt(const nsString& aString, PRInt32 aIndex)
{
  nsString* string = new nsString(aString);
  if (nsVoidArray::InsertElementAt(string, aIndex)) {
    return PR_TRUE;
  }
  delete string;
  return PR_FALSE;
}

PRBool
nsStringArray::ReplaceStringAt(const nsString& aString, PRInt32 aIndex)
{
  nsString* string = (nsString*)nsVoidArray::ElementAt(aIndex);
  if (nsnull != string) {
    *string = aString;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool 
nsStringArray::RemoveString(const nsString& aString)
{
  PRInt32 index = IndexOf(aString);
  if (-1 < index) {
    return RemoveStringAt(index);
  }
  return PR_FALSE;
}

PRBool 
nsStringArray::RemoveStringIgnoreCase(const nsString& aString)
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
  PRInt32 index = mCount;
  while (0 <= --index) {
    nsString* string = (nsString*)mArray[index];
    delete string;
  }
  nsVoidArray::Clear();
}



PRBool 
nsStringArray::EnumerateForwards(nsStringArrayEnumFunc aFunc, void* aData)
{
  PRInt32 index = -1;
  PRBool  running = PR_TRUE;

  while (running && (++index < mCount)) {
    running = (*aFunc)(*((nsString*)mArray[index]), aData);
  }
  return running;
}

PRBool 
nsStringArray::EnumerateBackwards(nsStringArrayEnumFunc aFunc, void* aData)
{
  PRInt32 index = mCount;
  PRBool  running = PR_TRUE;

  while (running && (0 <= --index)) {
    running = (*aFunc)(*((nsString*)mArray[index]), aData);
  }
  return running;
}

