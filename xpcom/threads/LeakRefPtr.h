/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Smart pointer which leaks its owning refcounted object by default. */

#ifndef LeakRefPtr_h
#define LeakRefPtr_h

#include "mozilla/AlreadyAddRefed.h"

namespace mozilla {

/**
 * Instance of this class behaves like a raw pointer which leaks the
 * resource its owning if not explicitly released.
 */
template<class T>
class LeakRefPtr
{
public:
  explicit LeakRefPtr(already_AddRefed<T>&& aPtr)
    : mRawPtr(aPtr.take()) { }

  explicit operator bool() const { return !!mRawPtr; }

  LeakRefPtr<T>& operator=(already_AddRefed<T>&& aPtr)
  {
    mRawPtr = aPtr.take();
    return *this;
  }

  already_AddRefed<T> take()
  {
    T* rawPtr = mRawPtr;
    mRawPtr = nullptr;
    return already_AddRefed<T>(rawPtr);
  }

private:
  T* mRawPtr;
};

} // namespace mozilla

#endif // LeakRefPtr_h
