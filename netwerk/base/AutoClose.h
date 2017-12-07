/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_AutoClose_h
#define mozilla_net_AutoClose_h

#include "nsCOMPtr.h"
#include "mozilla/Mutex.h"

namespace mozilla { namespace net {

// Like an nsAutoPtr for XPCOM streams (e.g. nsIAsyncInputStream) and other
// refcounted classes that need to have the Close() method called explicitly
// before they are destroyed.
template <typename T>
class AutoClose
{
public:
  AutoClose() : mMutex("net::AutoClose.mMutex") { }
  ~AutoClose(){
    CloseAndRelease();
  }

  explicit operator bool()
  {
    MutexAutoLock lock(mMutex);
    return mPtr;
  }

  already_AddRefed<T> forget()
  {
    MutexAutoLock lock(mMutex);
    return mPtr.forget();
  }

  void takeOver(nsCOMPtr<T> & rhs)
  {
    already_AddRefed<T> other = rhs.forget();
    TakeOverInternal(&other);
  }

  void CloseAndRelease()
  {
    TakeOverInternal(nullptr);
  }

private:
  void TakeOverInternal(already_AddRefed<T> *aOther)
  {
    nsCOMPtr<T> ptr;
    {
      MutexAutoLock lock(mMutex);
      ptr.swap(mPtr);
      if (aOther) {
        mPtr = *aOther;
      }
    }

    if (ptr) {
      ptr->Close();
    }
  }

  void operator=(const AutoClose<T> &) = delete;
  AutoClose(const AutoClose<T> &) = delete;

  nsCOMPtr<T> mPtr;
  Mutex mMutex;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_AutoClose_h
