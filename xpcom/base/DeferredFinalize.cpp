/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DeferredFinalize.h"

#include "mozilla/Assertions.h"
#include "mozilla/CycleCollectedJSRuntime.h"

void
mozilla::DeferredFinalize(nsISupports* aSupports)
{
  CycleCollectedJSRuntime* rt = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(rt, "Should have a CycleCollectedJSRuntime by now");
  rt->DeferredFinalize(aSupports);
}

void
mozilla::DeferredFinalize(DeferredFinalizeAppendFunction aAppendFunc,
                          DeferredFinalizeFunction aFunc,
                          void* aThing)
{
  CycleCollectedJSRuntime* rt = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(rt, "Should have a CycleCollectedJSRuntime by now");
  rt->DeferredFinalize(aAppendFunc, aFunc, aThing);
}
