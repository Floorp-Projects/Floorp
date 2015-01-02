/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSupportsArray_h__
#define nsSupportsArray_h__

//#define DEBUG_SUPPORTSARRAY 1

#include "nsISupportsArray.h"
#include "mozilla/Attributes.h"

static const uint32_t kAutoArraySize = 8;

class nsSupportsArray MOZ_FINAL : public nsISupportsArray
{
  ~nsSupportsArray(void); // nonvirtual since we're not subclassed

public:
  nsSupportsArray(void);
  static nsresult Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

  NS_DECL_THREADSAFE_ISUPPORTS

  NS_DECL_NSISERIALIZABLE

  // nsICollection methods:
  NS_IMETHOD Count(uint32_t* aResult) MOZ_OVERRIDE
  {
    *aResult = mCount;
    return NS_OK;
  }
  NS_IMETHOD GetElementAt(uint32_t aIndex, nsISupports** aResult) MOZ_OVERRIDE;
  NS_IMETHOD QueryElementAt(uint32_t aIndex, const nsIID& aIID, void** aResult) MOZ_OVERRIDE
  {
    if (aIndex < mCount) {
      nsISupports* element = mArray[aIndex];
      if (element) {
        return element->QueryInterface(aIID, aResult);
      }
    }
    return NS_ERROR_FAILURE;
  }
  NS_IMETHOD SetElementAt(uint32_t aIndex, nsISupports* aValue) MOZ_OVERRIDE
  {
    return ReplaceElementAt(aValue, aIndex) ? NS_OK : NS_ERROR_FAILURE;
  }
  NS_IMETHOD AppendElement(nsISupports* aElement) MOZ_OVERRIDE
  {
    // XXX Invalid cast of bool to nsresult (bug 778110)
    return (nsresult)InsertElementAt(aElement, mCount)/* ? NS_OK : NS_ERROR_FAILURE*/;
  }
  // XXX this is badly named - should be RemoveFirstElement
  NS_IMETHOD RemoveElement(nsISupports* aElement) MOZ_OVERRIDE;
  NS_IMETHOD_(bool) MoveElement(int32_t aFrom, int32_t aTo) MOZ_OVERRIDE;
  NS_IMETHOD Enumerate(nsIEnumerator** aResult) MOZ_OVERRIDE;
  NS_IMETHOD Clear(void) MOZ_OVERRIDE;

  // nsISupportsArray methods:
  NS_IMETHOD_(bool) Equals(const nsISupportsArray* aOther) MOZ_OVERRIDE;

  NS_IMETHOD_(int32_t) IndexOf(const nsISupports* aPossibleElement) MOZ_OVERRIDE;
  NS_IMETHOD_(int32_t) IndexOfStartingAt(const nsISupports* aPossibleElement,
                                         uint32_t aStartIndex = 0) MOZ_OVERRIDE;
  NS_IMETHOD_(int32_t) LastIndexOf(const nsISupports* aPossibleElement) MOZ_OVERRIDE;

  NS_IMETHOD GetIndexOf(nsISupports* aPossibleElement, int32_t* aResult) MOZ_OVERRIDE
  {
    *aResult = IndexOf(aPossibleElement);
    return NS_OK;
  }

  NS_IMETHOD GetIndexOfStartingAt(nsISupports* aPossibleElement,
                                  uint32_t aStartIndex, int32_t* aResult) MOZ_OVERRIDE
  {
    *aResult = IndexOfStartingAt(aPossibleElement, aStartIndex);
    return NS_OK;
  }

  NS_IMETHOD GetLastIndexOf(nsISupports* aPossibleElement, int32_t* aResult) MOZ_OVERRIDE
  {
    *aResult = LastIndexOf(aPossibleElement);
    return NS_OK;
  }

  NS_IMETHOD_(bool) InsertElementAt(nsISupports* aElement, uint32_t aIndex) MOZ_OVERRIDE;

  NS_IMETHOD_(bool) ReplaceElementAt(nsISupports* aElement, uint32_t aIndex) MOZ_OVERRIDE;

  NS_IMETHOD_(bool) RemoveElementAt(uint32_t aIndex) MOZ_OVERRIDE
  {
    return RemoveElementsAt(aIndex, 1);
  }
  NS_IMETHOD_(bool) RemoveLastElement(const nsISupports* aElement) MOZ_OVERRIDE;

  NS_IMETHOD DeleteLastElement(nsISupports* aElement) MOZ_OVERRIDE
  {
    return (RemoveLastElement(aElement) ? NS_OK : NS_ERROR_FAILURE);
  }

  NS_IMETHOD DeleteElementAt(uint32_t aIndex) MOZ_OVERRIDE
  {
    return (RemoveElementAt(aIndex) ? NS_OK : NS_ERROR_FAILURE);
  }

  NS_IMETHOD_(bool) AppendElements(nsISupportsArray* aElements) MOZ_OVERRIDE
  {
    return InsertElementsAt(aElements, mCount);
  }

  NS_IMETHOD Compact(void) MOZ_OVERRIDE;

  NS_IMETHOD Clone(nsISupportsArray** aResult) MOZ_OVERRIDE;

  NS_IMETHOD_(bool) InsertElementsAt(nsISupportsArray* aOther,
                                     uint32_t aIndex) MOZ_OVERRIDE;

  NS_IMETHOD_(bool) RemoveElementsAt(uint32_t aIndex, uint32_t aCount) MOZ_OVERRIDE;

  NS_IMETHOD_(bool) SizeTo(int32_t aSize) MOZ_OVERRIDE;
protected:
  void DeleteArray(void);

  void GrowArrayBy(int32_t aGrowBy);

  nsISupports** mArray;
  uint32_t mArraySize;
  uint32_t mCount;
  nsISupports*  mAutoArray[kAutoArraySize];
#if DEBUG_SUPPORTSARRAY
  uint32_t mMaxCount;
  uint32_t mMaxSize;
#endif

private:
  // Copy constructors are not allowed
  explicit nsSupportsArray(const nsISupportsArray& aOther);
};

#endif // nsSupportsArray_h__
