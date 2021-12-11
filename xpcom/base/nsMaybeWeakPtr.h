/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMaybeWeakPtr_h_
#define nsMaybeWeakPtr_h_

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIWeakReferenceUtils.h"
#include "nsTArray.h"
#include "nsCycleCollectionNoteChild.h"

// nsMaybeWeakPtr is a helper object to hold a strong-or-weak reference
// to the template class.  It's pretty minimal, but sufficient.

template <class T>
class nsMaybeWeakPtr {
 public:
  nsMaybeWeakPtr() = default;
  MOZ_IMPLICIT nsMaybeWeakPtr(T* aRef) : mPtr(aRef), mWeak(false) {}
  MOZ_IMPLICIT nsMaybeWeakPtr(const nsCOMPtr<nsIWeakReference>& aRef)
      : mPtr(aRef), mWeak(true) {}

  nsMaybeWeakPtr<T>& operator=(T* aRef) {
    mPtr = aRef;
    mWeak = false;
    return *this;
  }

  nsMaybeWeakPtr<T>& operator=(const nsCOMPtr<nsIWeakReference>& aRef) {
    mPtr = aRef;
    mWeak = true;
    return *this;
  }

  bool operator==(const nsMaybeWeakPtr<T>& other) const {
    return mPtr == other.mPtr;
  }

  nsISupports* GetRawValue() const { return mPtr.get(); }
  bool IsWeak() const { return mWeak; }

  const nsCOMPtr<T> GetValue() const;

 private:
  nsCOMPtr<nsISupports> mPtr;
  bool mWeak;
};

// nsMaybeWeakPtrArray is an array of MaybeWeakPtr objects, that knows how to
// grab a weak reference to a given object if requested.  It only allows a
// given object to appear in the array once.

template <class T>
class nsMaybeWeakPtrArray : public CopyableTArray<nsMaybeWeakPtr<T>> {
  typedef nsTArray<nsMaybeWeakPtr<T>> MaybeWeakArray;

  nsresult SetMaybeWeakPtr(nsMaybeWeakPtr<T>& aRef, T* aElement,
                           bool aOwnsWeak) {
    nsresult rv = NS_OK;

    if (aOwnsWeak) {
      aRef = do_GetWeakReference(aElement, &rv);
    } else {
      aRef = aElement;
    }

    return rv;
  }

 public:
  nsresult AppendWeakElement(T* aElement, bool aOwnsWeak) {
    nsMaybeWeakPtr<T> ref;
    MOZ_TRY(SetMaybeWeakPtr(ref, aElement, aOwnsWeak));

    MaybeWeakArray::AppendElement(ref);
    return NS_OK;
  }

  nsresult AppendWeakElementUnlessExists(T* aElement, bool aOwnsWeak) {
    nsMaybeWeakPtr<T> ref;
    MOZ_TRY(SetMaybeWeakPtr(ref, aElement, aOwnsWeak));

    if (MaybeWeakArray::Contains(ref)) {
      return NS_ERROR_INVALID_ARG;
    }

    MaybeWeakArray::AppendElement(ref);
    return NS_OK;
  }

  nsresult RemoveWeakElement(T* aElement) {
    if (MaybeWeakArray::RemoveElement(aElement)) {
      return NS_OK;
    }

    // Don't use do_GetWeakReference; it should only be called if we know
    // the object supports weak references.
    nsCOMPtr<nsISupportsWeakReference> supWeakRef = do_QueryInterface(aElement);
    if (!supWeakRef) {
      return NS_ERROR_INVALID_ARG;
    }

    nsCOMPtr<nsIWeakReference> weakRef;
    nsresult rv = supWeakRef->GetWeakReference(getter_AddRefs(weakRef));
    NS_ENSURE_SUCCESS(rv, rv);

    if (MaybeWeakArray::RemoveElement(weakRef)) {
      return NS_OK;
    }

    return NS_ERROR_INVALID_ARG;
  }
};

template <class T>
const nsCOMPtr<T> nsMaybeWeakPtr<T>::GetValue() const {
  if (!mPtr) {
    return nullptr;
  }

  nsCOMPtr<T> ref;
  nsresult rv;

  if (mWeak) {
    nsCOMPtr<nsIWeakReference> weakRef = do_QueryInterface(mPtr);
    if (weakRef) {
      ref = do_QueryReferent(weakRef, &rv);
      if (NS_SUCCEEDED(rv)) {
        return ref;
      }
    }
  } else {
    ref = do_QueryInterface(mPtr, &rv);
    if (NS_SUCCEEDED(rv)) {
      return ref;
    }
  }

  return nullptr;
}

template <typename T>
inline void ImplCycleCollectionUnlink(nsMaybeWeakPtrArray<T>& aField) {
  aField.Clear();
}

template <typename E>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    nsMaybeWeakPtrArray<E>& aField, const char* aName, uint32_t aFlags = 0) {
  aFlags |= CycleCollectionEdgeNameArrayFlag;
  size_t length = aField.Length();
  for (size_t i = 0; i < length; ++i) {
    CycleCollectionNoteChild(aCallback, aField[i].GetRawValue(), aName, aFlags);
  }
}

// Call a method on each element in the array, but only if the element is
// non-null.

#define ENUMERATE_WEAKARRAY(array, type, method)                          \
  for (uint32_t array_idx = 0; array_idx < array.Length(); ++array_idx) { \
    const nsCOMPtr<type>& e = array.ElementAt(array_idx).GetValue();      \
    if (e) e->method;                                                     \
  }

#endif
