/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsQueryObject_h
#define nsQueryObject_h

#include "mozilla/Attributes.h"

#include "nsCOMPtr.h"
#include "nsRefPtr.h"

/*****************************************************************************/

template<class T>
class MOZ_STACK_CLASS nsQueryObject final : public nsCOMPtr_helper
{
public:
  explicit nsQueryObject(T* aRawPtr)
    : mRawPtr(aRawPtr)
  {
  }

  virtual nsresult NS_FASTCALL operator()(const nsIID& aIID,
                                          void** aResult) const
  {
    nsresult status = mRawPtr ? mRawPtr->QueryInterface(aIID, aResult)
                              : NS_ERROR_NULL_POINTER;
    return status;
  }
private:
  T* MOZ_NON_OWNING_REF mRawPtr;
};

template<class T>
class MOZ_STACK_CLASS nsQueryObjectWithError final : public nsCOMPtr_helper
{
public:
  nsQueryObjectWithError(T* aRawPtr, nsresult* aErrorPtr)
    : mRawPtr(aRawPtr), mErrorPtr(aErrorPtr)
  {
  }

  virtual nsresult NS_FASTCALL operator()(const nsIID& aIID,
                                          void** aResult) const
  {
    nsresult status = mRawPtr ? mRawPtr->QueryInterface(aIID, aResult)
                              : NS_ERROR_NULL_POINTER;
    if (mErrorPtr) {
      *mErrorPtr = status;
    }
    return status;
  }
private:
  T* MOZ_NON_OWNING_REF mRawPtr;
  nsresult* mErrorPtr;
};

/*****************************************************************************/

/*****************************************************************************/

template<class T>
inline nsQueryObject<T>
do_QueryObject(T* aRawPtr)
{
  return nsQueryObject<T>(aRawPtr);
}

template<class T>
inline nsQueryObject<T>
do_QueryObject(nsCOMPtr<T>& aRawPtr)
{
  return nsQueryObject<T>(aRawPtr);
}

template<class T>
inline nsQueryObject<T>
do_QueryObject(nsRefPtr<T>& aRawPtr)
{
  return nsQueryObject<T>(aRawPtr);
}

template<class T>
inline nsQueryObjectWithError<T>
do_QueryObject(T* aRawPtr, nsresult* aErrorPtr)
{
  return nsQueryObjectWithError<T>(aRawPtr, aErrorPtr);
}

template<class T>
inline nsQueryObjectWithError<T>
do_QueryObject(nsCOMPtr<T>& aRawPtr, nsresult* aErrorPtr)
{
  return nsQueryObjectWithError<T>(aRawPtr, aErrorPtr);
}

template<class T>
inline nsQueryObjectWithError<T>
do_QueryObject(nsRefPtr<T>& aRawPtr, nsresult* aErrorPtr)
{
  return nsQueryObjectWithError<T>(aRawPtr, aErrorPtr);
}

/*****************************************************************************/

#endif // !defined(nsQueryObject_h)
