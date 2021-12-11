/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMArray.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/OperatorNewExtensions.h"
#include "nsQuickSort.h"

#include "nsCOMPtr.h"

// This specialization is private to nsCOMArray.
// It exists solely to automatically zero-out newly created array elements.
template <>
class nsTArrayElementTraits<nsISupports*> {
  typedef nsISupports* E;

 public:
  // Zero out the value
  static inline void Construct(E* aE) {
    new (mozilla::KnownNotNull, static_cast<void*>(aE)) E();
  }
  // Invoke the copy-constructor in place.
  template <class A>
  static inline void Construct(E* aE, const A& aArg) {
    new (mozilla::KnownNotNull, static_cast<void*>(aE)) E(aArg);
  }
  // Construct in place.
  template <class... Args>
  static inline void Emplace(E* aE, Args&&... aArgs) {
    new (mozilla::KnownNotNull, static_cast<void*>(aE))
        E(std::forward<Args>(aArgs)...);
  }
  // Invoke the destructor in place.
  static inline void Destruct(E* aE) { aE->~E(); }
};

static void ReleaseObjects(nsTArray<nsISupports*>& aArray);

// implementations of non-trivial methods in nsCOMArray_base

nsCOMArray_base::nsCOMArray_base(const nsCOMArray_base& aOther) {
  // make sure we do only one allocation
  mArray.SetCapacity(aOther.Count());
  AppendObjects(aOther);
}

nsCOMArray_base::~nsCOMArray_base() { Clear(); }

int32_t nsCOMArray_base::IndexOf(nsISupports* aObject,
                                 uint32_t aStartIndex) const {
  return mArray.IndexOf(aObject, aStartIndex);
}

int32_t nsCOMArray_base::IndexOfObject(nsISupports* aObject) const {
  nsCOMPtr<nsISupports> supports = do_QueryInterface(aObject);
  if (NS_WARN_IF(!supports)) {
    return -1;
  }

  uint32_t i, count;
  int32_t retval = -1;
  count = mArray.Length();
  for (i = 0; i < count; ++i) {
    nsCOMPtr<nsISupports> arrayItem = do_QueryInterface(mArray[i]);
    if (arrayItem == supports) {
      retval = i;
      break;
    }
  }
  return retval;
}

bool nsCOMArray_base::EnumerateForwards(nsBaseArrayEnumFunc aFunc,
                                        void* aData) const {
  for (uint32_t index = 0; index < mArray.Length(); ++index) {
    if (!(*aFunc)(mArray[index], aData)) {
      return false;
    }
  }

  return true;
}

bool nsCOMArray_base::EnumerateBackwards(nsBaseArrayEnumFunc aFunc,
                                         void* aData) const {
  for (uint32_t index = mArray.Length(); index--;) {
    if (!(*aFunc)(mArray[index], aData)) {
      return false;
    }
  }

  return true;
}

int nsCOMArray_base::VoidStarComparator(const void* aElement1,
                                        const void* aElement2, void* aData) {
  auto ctx = static_cast<nsISupportsComparatorContext*>(aData);
  return (*ctx->mComparatorFunc)(*static_cast<nsISupports* const*>(aElement1),
                                 *static_cast<nsISupports* const*>(aElement2),
                                 ctx->mData);
}

void nsCOMArray_base::Sort(nsISupportsComparatorFunc aFunc, void* aData) {
  if (mArray.Length() > 1) {
    nsISupportsComparatorContext ctx = {aFunc, aData};
    NS_QuickSort(mArray.Elements(), mArray.Length(), sizeof(nsISupports*),
                 VoidStarComparator, &ctx);
  }
}

bool nsCOMArray_base::InsertObjectAt(nsISupports* aObject, int32_t aIndex) {
  if ((uint32_t)aIndex > mArray.Length()) {
    return false;
  }

  mArray.InsertElementAt(aIndex, aObject);

  NS_IF_ADDREF(aObject);
  return true;
}

void nsCOMArray_base::InsertElementAt(uint32_t aIndex, nsISupports* aElement) {
  mArray.InsertElementAt(aIndex, aElement);
  NS_IF_ADDREF(aElement);
}

void nsCOMArray_base::InsertElementAt(uint32_t aIndex,
                                      already_AddRefed<nsISupports> aElement) {
  mArray.InsertElementAt(aIndex, aElement.take());
}

bool nsCOMArray_base::InsertObjectsAt(const nsCOMArray_base& aObjects,
                                      int32_t aIndex) {
  if ((uint32_t)aIndex > mArray.Length()) {
    return false;
  }

  mArray.InsertElementsAt(aIndex, aObjects.mArray);

  // need to addref all these
  uint32_t count = aObjects.Length();
  for (uint32_t i = 0; i < count; ++i) {
    NS_IF_ADDREF(aObjects[i]);
  }

  return true;
}

void nsCOMArray_base::InsertElementsAt(uint32_t aIndex,
                                       const nsCOMArray_base& aElements) {
  mArray.InsertElementsAt(aIndex, aElements.mArray);

  // need to addref all these
  uint32_t count = aElements.Length();
  for (uint32_t i = 0; i < count; ++i) {
    NS_IF_ADDREF(aElements[i]);
  }
}

void nsCOMArray_base::InsertElementsAt(uint32_t aIndex,
                                       nsISupports* const* aElements,
                                       uint32_t aCount) {
  mArray.InsertElementsAt(aIndex, aElements, aCount);

  // need to addref all these
  for (uint32_t i = 0; i < aCount; ++i) {
    NS_IF_ADDREF(aElements[i]);
  }
}

void nsCOMArray_base::ReplaceObjectAt(nsISupports* aObject, int32_t aIndex) {
  mArray.EnsureLengthAtLeast(aIndex + 1);
  nsISupports* oldObject = mArray[aIndex];
  // Make sure to addref first, in case aObject == oldObject
  NS_IF_ADDREF(mArray[aIndex] = aObject);
  NS_IF_RELEASE(oldObject);
}

bool nsCOMArray_base::RemoveObject(nsISupports* aObject) {
  bool result = mArray.RemoveElement(aObject);
  if (result) {
    NS_IF_RELEASE(aObject);
  }
  return result;
}

bool nsCOMArray_base::RemoveObjectAt(int32_t aIndex) {
  if (uint32_t(aIndex) < mArray.Length()) {
    nsISupports* element = mArray[aIndex];

    mArray.RemoveElementAt(aIndex);
    NS_IF_RELEASE(element);
    return true;
  }

  return false;
}

void nsCOMArray_base::RemoveElementAt(uint32_t aIndex) {
  nsISupports* element = mArray[aIndex];
  mArray.RemoveElementAt(aIndex);
  NS_IF_RELEASE(element);
}

bool nsCOMArray_base::RemoveObjectsAt(int32_t aIndex, int32_t aCount) {
  if (uint32_t(aIndex) + uint32_t(aCount) <= mArray.Length()) {
    nsTArray<nsISupports*> elementsToDestroy(aCount);
    elementsToDestroy.AppendElements(mArray.Elements() + aIndex, aCount);
    mArray.RemoveElementsAt(aIndex, aCount);
    ReleaseObjects(elementsToDestroy);
    return true;
  }

  return false;
}

void nsCOMArray_base::RemoveElementsAt(uint32_t aIndex, uint32_t aCount) {
  nsTArray<nsISupports*> elementsToDestroy(aCount);
  elementsToDestroy.AppendElements(mArray.Elements() + aIndex, aCount);
  mArray.RemoveElementsAt(aIndex, aCount);
  ReleaseObjects(elementsToDestroy);
}

// useful for destructors
void ReleaseObjects(nsTArray<nsISupports*>& aArray) {
  for (uint32_t i = 0; i < aArray.Length(); ++i) {
    NS_IF_RELEASE(aArray[i]);
  }
}

void nsCOMArray_base::Clear() {
  nsTArray<nsISupports*> objects = std::move(mArray);
  ReleaseObjects(objects);
}

bool nsCOMArray_base::SetCount(int32_t aNewCount) {
  NS_ASSERTION(aNewCount >= 0, "SetCount(negative index)");
  if (aNewCount < 0) {
    return false;
  }

  int32_t count = mArray.Length();
  if (count > aNewCount) {
    RemoveObjectsAt(aNewCount, mArray.Length() - aNewCount);
  }
  mArray.SetLength(aNewCount);
  return true;
}
