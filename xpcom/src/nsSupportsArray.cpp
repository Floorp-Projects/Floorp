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

#include "nsISupportsArray.h"

static NS_DEFINE_IID(kISupportsArrayIID, NS_ISUPPORTSARRAY_IID);

static const PRInt32 kGrowArrayBy = 8;
static const PRInt32 kAutoArraySize = 4;

class SupportsArrayImpl : public nsISupportsArray {
public:
  SupportsArrayImpl(void);
  ~SupportsArrayImpl(void);

  NS_DECL_ISUPPORTS

  NS_IMETHOD_(nsISupportsArray&) operator=(const nsISupportsArray& aOther);
  NS_IMETHOD_(PRBool) operator==(const nsISupportsArray& aOther) const { return Equals(&aOther); }
  NS_IMETHOD_(PRBool) Equals(const nsISupportsArray* aOther) const;

  NS_IMETHOD_(PRInt32) Count(void) const {  return mCount;  }

  NS_IMETHOD_(nsISupports*) ElementAt(PRInt32 aIndex) const;
  NS_IMETHOD_(nsISupports*) operator[](PRInt32 aIndex) const { return ElementAt(aIndex); }

  NS_IMETHOD_(PRInt32) IndexOf(const nsISupports* aPossibleElement, PRInt32 aStartIndex = 0) const;
  NS_IMETHOD_(PRInt32) LastIndexOf(const nsISupports* aPossibleElement) const;

  NS_IMETHOD_(PRBool) InsertElementAt(nsISupports* aElement, PRInt32 aIndex);

  NS_IMETHOD_(PRBool) ReplaceElementAt(nsISupports* aElement, PRInt32 aIndex);

  NS_IMETHOD_(PRBool) AppendElement(nsISupports* aElement) {
    return InsertElementAt(aElement, mCount);
  }

  NS_IMETHOD_(PRBool) RemoveElementAt(PRInt32 aIndex);
  NS_IMETHOD_(PRBool) RemoveElement(const nsISupports* aElement, PRInt32 aStartIndex = 0);
  NS_IMETHOD_(PRBool) RemoveLastElement(const nsISupports* aElement);
  NS_IMETHOD_(void)   Clear(void);

  NS_IMETHOD_(void)   Compact(void);

  NS_IMETHOD_(PRBool) EnumerateForwards(nsISupportsArrayEnumFunc aFunc, void* aData);
  NS_IMETHOD_(PRBool) EnumerateBackwards(nsISupportsArrayEnumFunc aFunc, void* aData);

protected:
  void DeleteArray(void);

  nsISupports** mArray;
  PRInt32 mArraySize;
  PRInt32 mCount;
  nsISupports*  mAutoArray[kAutoArraySize];

private:
  // Copy constructors are not allowed
  SupportsArrayImpl(const nsISupportsArray& other);
};


SupportsArrayImpl::SupportsArrayImpl()
{
  NS_INIT_REFCNT();
  mArray = &(mAutoArray[0]);
  mArraySize = kAutoArraySize;
  mCount = 0;
}

SupportsArrayImpl::~SupportsArrayImpl()
{
  DeleteArray();
}

NS_IMPL_ISUPPORTS(SupportsArrayImpl, kISupportsArrayIID);

void SupportsArrayImpl::DeleteArray(void)
{
  Clear();
  if (mArray != &(mAutoArray[0])) {
    delete[] mArray;
    mArray = &(mAutoArray[0]);
    mArraySize = kAutoArraySize;
  }
}

nsISupportsArray& SupportsArrayImpl::operator=(const nsISupportsArray& aOther)
{
  PRInt32 otherCount = aOther.Count();

  if (otherCount > mArraySize) {
    DeleteArray();
    mArraySize = otherCount;
    mArray = new nsISupports*[mArraySize];
  }
  else {
    Clear();
  }
  mCount = otherCount;
  while (0 <= --otherCount) {
    mArray[otherCount] = aOther.ElementAt(otherCount);
  }
  return *this;
}

PRBool SupportsArrayImpl::Equals(const nsISupportsArray* aOther) const
{
  if (0 != aOther) {
    const SupportsArrayImpl* other = (const SupportsArrayImpl*)aOther;
    if (mCount == other->mCount) {
      PRInt32 index = mCount;
      while (0 <= --index) {
        if (mArray[index] != other->mArray[index]) {
          return PR_FALSE;
        }
      }
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

nsISupports* SupportsArrayImpl::ElementAt(PRInt32 aIndex) const
{
  if ((0 <= aIndex) && (aIndex < mCount)) {
    nsISupports*  element = mArray[aIndex];
    NS_ADDREF(element);
    return element;
  }
  return 0;
}

PRInt32 SupportsArrayImpl::IndexOf(const nsISupports* aPossibleElement, PRInt32 aStartIndex) const
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

PRInt32 SupportsArrayImpl::LastIndexOf(const nsISupports* aPossibleElement) const
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

PRBool SupportsArrayImpl::InsertElementAt(nsISupports* aElement, PRInt32 aIndex)
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
        PRInt32 slide = (mCount - aIndex);
        if (0 < slide) {
          ::memcpy(mArray + aIndex + 1, oldArray + aIndex, slide * sizeof(nsISupports*));
        }
        if (oldArray != &(mAutoArray[0])) {
          delete[] oldArray;
        }
      }
    }
    else {
      PRInt32 slide = (mCount - aIndex);
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

PRBool SupportsArrayImpl::ReplaceElementAt(nsISupports* aElement, PRInt32 aIndex)
{
  if ((0 <= aIndex) && (aIndex < mCount)) {
    NS_ADDREF(aElement);  // addref first in case it's the same object!
    NS_RELEASE(mArray[aIndex]);
    mArray[aIndex] = aElement;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool SupportsArrayImpl::RemoveElementAt(PRInt32 aIndex)
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

PRBool SupportsArrayImpl::RemoveElement(const nsISupports* aElement, PRInt32 aStartIndex)
{
  if ((0 <= aStartIndex) && (aStartIndex < mCount)) {
    nsISupports** ep = mArray;
    nsISupports** end = ep + mCount;
    while (ep < end) {
      if (*ep == aElement) {
        return RemoveElementAt(PRInt32(ep - mArray));
      }
      ep++;
    }
  }
  return PR_FALSE;
}

PRBool SupportsArrayImpl::RemoveLastElement(const nsISupports* aElement)
{
  if (0 < mCount) {
    nsISupports** ep = (mArray + mCount);
    while (mArray <= --ep) {
      if (*ep == aElement) {
        return RemoveElementAt(PRInt32(ep - mArray));
      }
    }
  }
  return PR_FALSE;
}

void SupportsArrayImpl::Clear(void)
{
  while (0 <= --mCount) {
    NS_RELEASE(mArray[mCount]);
  }
  mCount = 0;
}

void SupportsArrayImpl::Compact(void)
{
  if ((mArraySize != mCount) && (kAutoArraySize < mArraySize)) {
    nsISupports** oldArray = mArray;
    PRInt32 oldArraySize = mArraySize;
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

PRBool SupportsArrayImpl::EnumerateForwards(nsISupportsArrayEnumFunc aFunc, void* aData)
{
  PRInt32 index = -1;
  PRBool  running = PR_TRUE;

  while (running && (++index < mCount)) {
    running = (*aFunc)(mArray[index], aData);
  }
  return running;
}

PRBool SupportsArrayImpl::EnumerateBackwards(nsISupportsArrayEnumFunc aFunc, void* aData)
{
  PRInt32 index = mCount;
  PRBool  running = PR_TRUE;

  while (running && (0 <= --index)) {
    running = (*aFunc)(mArray[index], aData);
  }
  return running;
}


NS_COM nsresult
  NS_NewISupportsArray(nsISupportsArray** aInstancePtrResult)
{
  if (aInstancePtrResult == 0) {
    return NS_ERROR_NULL_POINTER;
  }

  SupportsArrayImpl *it = new SupportsArrayImpl();

  if (0 == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kISupportsArrayIID, (void **) aInstancePtrResult);
}

