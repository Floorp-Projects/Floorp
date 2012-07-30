/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_AutoClose_h
#define mozilla_net_AutoClose_h

#include "nsCOMPtr.h"

namespace mozilla { namespace net {

// Like an nsAutoPtr for XPCOM streams (e.g. nsIAsyncInputStream) and other
// refcounted classes that need to have the Close() method called explicitly
// before they are destroyed.
template <typename T>
class AutoClose
{
public:
  AutoClose() { } 
  ~AutoClose(){
    Close();
  }

  operator bool() const
  {
    return mPtr;
  }

  already_AddRefed<T> forget()
  {
    return mPtr.forget();
  }

  void takeOver(nsCOMPtr<T> & rhs)
  {
    Close();
    mPtr = rhs.forget();
  }

  void takeOver(AutoClose<T> & rhs)
  {
    Close();
    mPtr = rhs.mPtr.forget();
  }

  void CloseAndRelease()
  {
    Close();
    mPtr = nullptr;
  }

  T* operator->() const
  {
    return mPtr.operator->();
  }

private:
  void Close()
  {
    if (mPtr) {
      mPtr->Close();
    }
  }

  void operator=(const AutoClose<T> &) MOZ_DELETE;
  AutoClose(const AutoClose<T> &) MOZ_DELETE;

  nsCOMPtr<T> mPtr;
};

} } // namespace mozilla::net

#endif // mozilla_net_AutoClose_h
