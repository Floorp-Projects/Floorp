/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIWeakReferenceUtils_h__
#define nsIWeakReferenceUtils_h__

#include "nsCOMPtr.h"
#include "nsIWeakReference.h"

typedef nsCOMPtr<nsIWeakReference> nsWeakPtr;

/**
 *
 */

// a type-safe shortcut for calling the |QueryReferent()| member function
// T must inherit from nsIWeakReference, but the cast may be ambiguous.
template<class T, class DestinationType>
inline nsresult
CallQueryReferent(T* aSource, DestinationType** aDestination)
{
  NS_PRECONDITION(aSource, "null parameter");
  NS_PRECONDITION(aDestination, "null parameter");

  return aSource->QueryReferent(NS_GET_TEMPLATE_IID(DestinationType),
                                reinterpret_cast<void**>(aDestination));
}


class nsQueryReferent : public nsCOMPtr_helper
{
public:
  nsQueryReferent(nsIWeakReference* aWeakPtr, nsresult* aError)
    : mWeakPtr(aWeakPtr)
    , mErrorPtr(aError)
  {
  }

  virtual nsresult NS_FASTCALL operator()(const nsIID& aIID, void**) const;

private:
  nsIWeakReference*  mWeakPtr;
  nsresult*          mErrorPtr;
};

inline const nsQueryReferent
do_QueryReferent(nsIWeakReference* aRawPtr, nsresult* aError = 0)
{
  return nsQueryReferent(aRawPtr, aError);
}


/**
 * Deprecated, use |do_GetWeakReference| instead.
 */
extern nsIWeakReference* NS_GetWeakReference(nsISupports*,
                                             nsresult* aResult = 0);

/**
 * |do_GetWeakReference| is a convenience function that bundles up all the work needed
 * to get a weak reference to an arbitrary object, i.e., the |QueryInterface|, test, and
 * call through to |GetWeakReference|, and put it into your |nsCOMPtr|.
 * It is specifically designed to cooperate with |nsCOMPtr| (or |nsWeakPtr|) like so:
 * |nsWeakPtr myWeakPtr = do_GetWeakReference(aPtr);|.
 */
inline already_AddRefed<nsIWeakReference>
do_GetWeakReference(nsISupports* aRawPtr, nsresult* aError = 0)
{
  return dont_AddRef(NS_GetWeakReference(aRawPtr, aError));
}

inline void
do_GetWeakReference(nsIWeakReference* aRawPtr, nsresult* aError = 0)
{
  // This signature exists solely to _stop_ you from doing a bad thing.
  //  Saying |do_GetWeakReference()| on a weak reference itself,
  //  is very likely to be a programmer error.
}

template<class T>
inline void
do_GetWeakReference(already_AddRefed<T>&)
{
  // This signature exists solely to _stop_ you from doing the bad thing.
  //  Saying |do_GetWeakReference()| on a pointer that is not otherwise owned by
  //  someone else is an automatic leak.  See <http://bugzilla.mozilla.org/show_bug.cgi?id=8221>.
}

template<class T>
inline void
do_GetWeakReference(already_AddRefed<T>&, nsresult*)
{
  // This signature exists solely to _stop_ you from doing the bad thing.
  //  Saying |do_GetWeakReference()| on a pointer that is not otherwise owned by
  //  someone else is an automatic leak.  See <http://bugzilla.mozilla.org/show_bug.cgi?id=8221>.
}

#endif
