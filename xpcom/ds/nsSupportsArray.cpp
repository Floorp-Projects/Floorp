/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#include "nsSupportsArray.h"
#include "nsSupportsArrayEnumerator.h"

static const PRUint32 kGrowArrayBy = 8;

nsSupportsArray::nsSupportsArray()
{
  NS_INIT_REFCNT();
  mArray = &(mAutoArray[0]);
  mArraySize = kAutoArraySize;
  mCount = 0;
}

nsSupportsArray::~nsSupportsArray()
{
  DeleteArray();
}

NS_IMPL_ISUPPORTS(nsSupportsArray, nsISupportsArray::GetIID());

void nsSupportsArray::DeleteArray(void)
{
  Clear();
  if (mArray != &(mAutoArray[0])) {
    delete[] mArray;
    mArray = &(mAutoArray[0]);
    mArraySize = kAutoArraySize;
  }
}

NS_IMETHODIMP_(nsISupportsArray&) 
nsISupportsArray::operator=(const nsISupportsArray& other)
{
  NS_ASSERTION(0, "should be an abstract method");
  return *this; // bogus
}

NS_IMETHODIMP_(nsISupportsArray&) 
nsSupportsArray::operator=(const nsISupportsArray& aOther)
{
  PRUint32 otherCount = aOther.Count();

  if (otherCount > mArraySize) {
    DeleteArray();
    mArraySize = otherCount;
    mArray = new nsISupports*[mArraySize];
  }
  else {
    Clear();
  }
  mCount = otherCount;
  while (0 < otherCount--) {
    mArray[otherCount] = aOther.ElementAt(otherCount);
  }
  return *this;
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::Equals(const nsISupportsArray* aOther) const
{
  if (0 != aOther) {
    const nsSupportsArray* other = (const nsSupportsArray*)aOther;
    if (mCount == other->mCount) {
      PRUint32 index = mCount;
      while (0 < index--) {
        if (mArray[index] != other->mArray[index]) {
          return PR_FALSE;
        }
      }
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

NS_IMETHODIMP_(nsISupports*)
nsSupportsArray::ElementAt(PRUint32 aIndex) const
{
  if ((0 <= aIndex) && (aIndex < mCount)) {
    nsISupports*  element = mArray[aIndex];
    NS_ADDREF(element);
    return element;
  }
  return 0;
}

NS_IMETHODIMP_(PRInt32)
nsSupportsArray::IndexOf(const nsISupports* aPossibleElement, PRUint32 aStartIndex) const
{
  if ((0 <= aStartIndex) && (aStartIndex < mCount)) {
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
nsSupportsArray::LastIndexOf(const nsISupports* aPossibleElement) const
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
  if ((0 <= aIndex) && (aIndex <= mCount)) {
    if (mArraySize < (mCount + 1)) {  // need to grow the array
      mArraySize += kGrowArrayBy;
      nsISupports** oldArray = mArray;
      mArray = new nsISupports*[mArraySize];
      if (0 == mArray) { // ran out of memory
        mArray = oldArray;
        mArraySize -= kGrowArrayBy;
        return PR_FALSE;
      }
      if (0 != oldArray) { // need to move old data
        if (0 < aIndex) {
          ::memcpy(mArray, oldArray, aIndex * sizeof(nsISupports*));
        }
        PRUint32 slide = (mCount - aIndex);
        if (0 < slide) {
          ::memcpy(mArray + aIndex + 1, oldArray + aIndex, slide * sizeof(nsISupports*));
        }
        if (oldArray != &(mAutoArray[0])) {
          delete[] oldArray;
        }
      }
    }
    else {
      PRUint32 slide = (mCount - aIndex);
      if (0 < slide) {
        ::memmove(mArray + aIndex + 1, mArray + aIndex, slide * sizeof(nsISupports*));
      }
    }

    mArray[aIndex] = aElement;
    NS_ADDREF(aElement);
    mCount++;
    return PR_TRUE;
  }
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::ReplaceElementAt(nsISupports* aElement, PRUint32 aIndex)
{
  if ((0 <= aIndex) && (aIndex < mCount)) {
    NS_ADDREF(aElement);  // addref first in case it's the same object!
    NS_RELEASE(mArray[aIndex]);
    mArray[aIndex] = aElement;
    return PR_TRUE;
  }
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::RemoveElementAt(PRUint32 aIndex)
{
  if ((0 <= aIndex) && (aIndex < mCount)) {
    NS_RELEASE(mArray[aIndex]);
    mCount--;
    PRInt32 slide = (mCount - aIndex);
    if (0 < slide) {
      ::memmove(mArray + aIndex, mArray + aIndex + 1,
                slide * sizeof(nsISupports*));
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::RemoveElement(const nsISupports* aElement, PRUint32 aStartIndex)
{
  if ((0 <= aStartIndex) && (aStartIndex < mCount)) {
    nsISupports** ep = mArray;
    nsISupports** end = ep + mCount;
    while (ep < end) {
      if (*ep == aElement) {
        return RemoveElementAt(PRUint32(ep - mArray));
      }
      ep++;
    }
  }
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::RemoveLastElement(const nsISupports* aElement)
{
  if (0 < mCount) {
    nsISupports** ep = (mArray + mCount);
    while (mArray <= --ep) {
      if (*ep == aElement) {
        return RemoveElementAt(PRUint32(ep - mArray));
      }
    }
  }
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::AppendElements(nsISupportsArray* aElements)
{
  nsSupportsArray*  elements = (nsSupportsArray*)aElements;

  if (elements && (0 < elements->mCount)) {
    if (mArraySize < (mCount + elements->mCount)) {  // need to grow the array
      PRUint32 count = mCount + elements->mCount;
      PRUint32 oldSize = mArraySize;
      while (mArraySize < count) {  // ick
        mArraySize += kGrowArrayBy;
      }
      nsISupports** oldArray = mArray;
      mArray = new nsISupports*[mArraySize];
      if (0 == mArray) { // ran out of memory
        mArray = oldArray;
        mArraySize = oldSize;
        return PR_FALSE;
      }
      if (0 != oldArray) { // need to move old data
        if (0 < mCount) {
          ::memcpy(mArray, oldArray, mCount);
        }
        if (oldArray != &(mAutoArray[0])) {
          delete[] oldArray;
        }
      }
    }

    PRUint32 index = 0;
    while (index < elements->mCount) {
      NS_ADDREF(elements->mArray[index]);
      mArray[mCount++] = elements->mArray[index++];
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}

NS_IMETHODIMP
nsSupportsArray::Clear(void)
{
  if (0 < mCount) {
    do {
      --mCount;
      NS_RELEASE(mArray[mCount]);
    } while (0 != mCount);
  }
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsSupportsArray::Compact(void)
{
  if ((mArraySize != mCount) && (kAutoArraySize < mArraySize)) {
    nsISupports** oldArray = mArray;
    PRUint32 oldArraySize = mArraySize;
    if (mCount <= kAutoArraySize) {
      mArray = &(mAutoArray[0]);
      mArraySize = kAutoArraySize;
    }
    else {
      mArray = new nsISupports*[mCount];
      mArraySize = mCount;
    }
    if (0 == mArray) {
      mArray = oldArray;
      mArraySize = oldArraySize;
      return;
    }
    ::memcpy(mArray, oldArray, mCount * sizeof(nsISupports*));
    delete[] oldArray;
  }
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::EnumerateForwards(nsISupportsArrayEnumFunc aFunc, void* aData) const
{
  PRInt32 index = -1;
  PRBool  running = PR_TRUE;

  while (running && (++index < (PRInt32)mCount)) {
    running = (*aFunc)(mArray[index], aData);
  }
  return running;
}

NS_IMETHODIMP_(PRBool)
nsSupportsArray::EnumerateBackwards(nsISupportsArrayEnumFunc aFunc, void* aData) const
{
  PRUint32 index = mCount;
  PRBool  running = PR_TRUE;

  while (running && (0 < index--)) {
    running = (*aFunc)(mArray[index], aData);
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

NS_COM nsresult
NS_NewISupportsArray(nsISupportsArray** aInstancePtrResult)
{
  if (aInstancePtrResult == 0)
    return NS_ERROR_NULL_POINTER;

  nsSupportsArray *it = new nsSupportsArray();
  if (0 == it)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(it);
  *aInstancePtrResult = it;
  return NS_OK;
}

