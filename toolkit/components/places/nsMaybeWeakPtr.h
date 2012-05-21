/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMaybeWeakPtr_h_
#define nsMaybeWeakPtr_h_

#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsTArray.h"

// nsMaybeWeakPtr is a helper object to hold a strong-or-weak reference
// to the template class.  It's pretty minimal, but sufficient.

class nsMaybeWeakPtr_base
{
protected:
  // Returns an addref'd pointer to the requested interface
  void* GetValueAs(const nsIID& iid) const;

  nsCOMPtr<nsISupports> mPtr;
};

template<class T>
class nsMaybeWeakPtr : private nsMaybeWeakPtr_base
{
public:
  nsMaybeWeakPtr(nsISupports *ref) { mPtr = ref; }
  nsMaybeWeakPtr(const nsCOMPtr<nsIWeakReference> &ref) { mPtr = ref; }
  nsMaybeWeakPtr(const nsCOMPtr<T> &ref) { mPtr = ref; }

  bool operator==(const nsMaybeWeakPtr<T> &other) const {
    return mPtr == other.mPtr;
  }

  operator const nsCOMPtr<T>() const { return GetValue(); }

protected:
  const nsCOMPtr<T> GetValue() const {
    return nsCOMPtr<T>(dont_AddRef(static_cast<T*>
                                              (GetValueAs(NS_GET_TEMPLATE_IID(T)))));
  }
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

// Call a method on each element in the array, but only if the element is
// non-null.

#define ENUMERATE_WEAKARRAY(array, type, method)                           \
  for (PRUint32 array_idx = 0; array_idx < array.Length(); ++array_idx) {  \
    const nsCOMPtr<type> &e = array.ElementAt(array_idx);                  \
    if (e)                                                                 \
      e->method;                                                           \
  }

#endif
