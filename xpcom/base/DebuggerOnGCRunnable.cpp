/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebuggerOnGCRunnable.h"

#include <utility>

#include "js/Debug.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/SchedulerGroup.h"

namespace mozilla {

/* static */
nsresult DebuggerOnGCRunnable::Enqueue(JSContext* aCx,
                                       const JS::GCDescription& aDesc) {
  auto gcEvent = aDesc.toGCEvent(aCx);
  if (!gcEvent) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  RefPtr<DebuggerOnGCRunnable> runOnGC =
      new DebuggerOnGCRunnable(std::move(gcEvent));
  return NS_DispatchToCurrentThread(runOnGC);
}

NS_IMETHODIMP
DebuggerOnGCRunnable::Run() {
  dom::AutoJSAPI jsapi;
  jsapi.Init();
  if (!JS::dbg::FireOnGarbageCollectionHook(jsapi.cx(), std::move(mGCData))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult DebuggerOnGCRunnable::Cancel() {
  mGCData = nullptr;
  return NS_OK;
}

}  // namespace mozilla
