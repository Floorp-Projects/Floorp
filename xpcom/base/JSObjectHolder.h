/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_JSObjectHolder_h
#define mozilla_JSObjectHolder_h

#include "js/RootingAPI.h"
#include "nsISupportsImpl.h"

namespace mozilla {

// This class is useful when something on one thread needs to keep alive
// a JS Object from another thread. If they are both on the same thread, the
// owning class should instead be made a cycle collected SCRIPT_HOLDER class.
// This object should only be AddRefed and Released on the same thread as
// mJSObject.
//
// Note that this keeps alive the JS object until it goes away, so be sure not
// to create cycles that keep alive the holder.
//
// JSObjectHolder is ISupports to make it usable with
// NS_ReleaseOnMainThread.
class JSObjectHolder final : public nsISupports {
 public:
  JSObjectHolder(JSContext* aCx, JSObject* aObject) : mJSObject(aCx, aObject) {}

  NS_DECL_ISUPPORTS

  JSObject* GetJSObject() { return mJSObject; }

 private:
  ~JSObjectHolder() = default;

  JS::PersistentRooted<JSObject*> mJSObject;
};

}  // namespace mozilla

#endif  // mozilla_JSObjectHolder_h
