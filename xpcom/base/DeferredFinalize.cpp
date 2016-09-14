/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DeferredFinalize.h"

#include "mozilla/Assertions.h"
#include "mozilla/CycleCollectedJSContext.h"

void
mozilla::DeferredFinalize(nsISupports* aSupports)
{
  CycleCollectedJSContext* cx = CycleCollectedJSContext::Get();
  MOZ_ASSERT(cx, "Should have a CycleCollectedJSContext by now");
  cx->DeferredFinalize(aSupports);
}

void
mozilla::DeferredFinalize(DeferredFinalizeAppendFunction aAppendFunc,
                          DeferredFinalizeFunction aFunc,
                          void* aThing)
{
  CycleCollectedJSContext* cx = CycleCollectedJSContext::Get();
  MOZ_ASSERT(cx, "Should have a CycleCollectedJSContext by now");
  cx->DeferredFinalize(aAppendFunc, aFunc, aThing);
}
