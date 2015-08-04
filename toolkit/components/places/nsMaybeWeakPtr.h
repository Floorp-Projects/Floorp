/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMaybeWeakPtr_h_
#define nsMaybeWeakPtr_h_

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsTArray.h"
#include "nsCycleCollectionNoteChild.h"

// nsMaybeWeakPtr is a helper object to hold a strong-or-weak reference
// to the template class.  It's pretty minimal, but sufficient.

template<class T>
class nsMaybeWeakPtr
{
public:
  MOZ_IMPLICIT nsMaybeWeakPtr(nsISupports *ref) { mPtr = ref; }
  MOZ_IMPLICIT nsMaybeWeakPtr(const nsCOMPtr<nsIWeakReference> &ref) { mPtr = ref; }
  MOZ_IMPLICIT nsMaybeWeakPtr(const nsCOMPtr<T> &ref) { mPtr = ref; }

  bool operator==(const nsMaybeWeakPtr<T> &other) const {
    return mPtr == other.mPtr;
  }

  nsISupports* GetRawValue() const { return mPtr.get(); }

  const nsCOMPtr<T> GetValue() const;

private:
  nsCOMPtr<nsISupports> mPtr;
};

// nsMaybeWeakPtrArray is an array of MaybeWeakPtr objects, that knows how to
// grab a weak reference to a given object if requested.  It only allows a
// given object to appear in the array once.

typedef nsTArray< nsMaybeWeakPtr<nsISupports> > isupports_array_type;
nsresult NS_AppendWeakElementBase(isupports_array_type *aArray,
                                  nsISupports *aElement, bool aWeak);
nsresult NS_RemoveWeakElementBase(isupports_array_type *aArray,
                                  nsISupports *aElement);

template<class T>
class nsMaybeWeakPtrArray : public nsTArray< nsMaybeWeakPtr<T> >
{
public:
  nsresult AppendWeakElement(T *aElement, bool aOwnsWeak)
  {
    return NS_AppendWeakElementBase(
      reinterpret_cast<isupports_array_type*>(this), aElement, aOwnsWeak);
  }

  nsresult RemoveWeakElement(T *aElement)
  {
    return NS_RemoveWeakElementBase(
      reinterpret_cast<isupports_array_type*>(this), aElement);
  }
};

template<class T>
const nsCOMPtr<T>
nsMaybeWeakPtr<T>::GetValue() const
{
  nsresult rv;
  nsCOMPtr<T> ref;
  if (mPtr) {
    rv = mPtr->QueryInterface(NS_GET_TEMPLATE_IID(T), getter_AddRefs(ref));
    if (NS_SUCCEEDED(rv)) {
      return ref;
    }
  }

  nsCOMPtr<nsIWeakReference> weakRef = do_QueryInterface(mPtr);
  if (weakRef) {
    rv = weakRef->QueryReferent(NS_GET_TEMPLATE_IID(T), getter_AddRefs(ref));
    if (NS_SUCCEEDED(rv)) {
      return ref;
    }
  }

  return nullptr;
}

template <typename T>
inline void
ImplCycleCollectionUnlink(nsMaybeWeakPtrArray<T>& aField)
{
  aField.Clear();
}

template <typename E>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsMaybeWeakPtrArray<E>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  aFlags |= CycleCollectionEdgeNameArrayFlag;
  size_t length = aField.Length();
  for (size_t i = 0; i < length; ++i) {
    CycleCollectionNoteChild(aCallback, aField[i].get(), aName, aFlags);
  }
}

// Call a method on each element in the array, but only if the element is
// non-null.

#define ENUMERATE_WEAKARRAY(array, type, method)                           \
  for (uint32_t array_idx = 0; array_idx < array.Length(); ++array_idx) {  \
    const nsCOMPtr<type> &e = array.ElementAt(array_idx).GetValue();       \
    if (e)                                                                 \
      e->method;                                                           \
  }

#endif
