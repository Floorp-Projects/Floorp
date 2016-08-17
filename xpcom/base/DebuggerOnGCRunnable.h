/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DebuggerOnGCRunnable_h
#define mozilla_DebuggerOnGCRunnable_h

#include "nsThreadUtils.h"
#include "js/GCAPI.h"
#include "mozilla/Move.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

// Runnable to fire the SpiderMonkey Debugger API's onGarbageCollection hook.
class DebuggerOnGCRunnable : public CancelableRunnable
{
  JS::dbg::GarbageCollectionEvent::Ptr mGCData;

  explicit DebuggerOnGCRunnable(JS::dbg::GarbageCollectionEvent::Ptr&& aGCData)
    : mGCData(Move(aGCData))
  { }

public:
  static nsresult Enqueue(JSContext* aCx, const JS::GCDescription& aDesc);

  NS_DECL_NSIRUNNABLE
  nsresult Cancel() override;
};

} // namespace mozilla

#endif // ifdef mozilla_dom_DebuggerOnGCRunnable_h
