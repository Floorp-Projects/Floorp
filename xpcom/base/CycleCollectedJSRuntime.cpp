/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/CycleCollectedJSRuntime.h"
#include "nsCycleCollectionParticipant.h"
#include "nsLayoutStatics.h"

using namespace mozilla;

CycleCollectedJSRuntime::CycleCollectedJSRuntime(uint32_t aMaxbytes,
                                                 JSUseHelperThreads aUseHelperThreads)
  : mJSRuntime(nullptr)
#ifdef DEBUG
  , mObjectToUnlink(nullptr)
#endif
{
  mJSRuntime = JS_NewRuntime(aMaxbytes, aUseHelperThreads);
  if (!mJSRuntime) {
    MOZ_CRASH();
  }

  mJSHolders.Init(512);
}

CycleCollectedJSRuntime::~CycleCollectedJSRuntime()
{
  JS_DestroyRuntime(mJSRuntime);
  mJSRuntime = nullptr;
}


void
CycleCollectedJSRuntime::AddJSHolder(void* aHolder, nsScriptObjectTracer* aTracer)
{
  MOZ_ASSERT(aTracer->Trace, "AddJSHolder needs a non-null Trace function");
  bool wasEmpty = mJSHolders.Count() == 0;
  mJSHolders.Put(aHolder, aTracer);
  if (wasEmpty && mJSHolders.Count() == 1) {
    nsLayoutStatics::AddRef();
  }
}

void
CycleCollectedJSRuntime::RemoveJSHolder(void* aHolder)
{
#ifdef DEBUG
  // Assert that the holder doesn't try to keep any GC things alive.
  // In case of unlinking cycle collector calls AssertNoObjectsToTrace
  // manually because we don't want to check the holder before we are
  // finished unlinking it
  if (aHolder != mObjectToUnlink) {
    AssertNoObjectsToTrace(aHolder);
  }
#endif
  bool hadOne = mJSHolders.Count() == 1;
  mJSHolders.Remove(aHolder);
  if (hadOne && mJSHolders.Count() == 0) {
    nsLayoutStatics::Release();
  }
}

#ifdef DEBUG
bool
CycleCollectedJSRuntime::TestJSHolder(void* aHolder)
{
  return mJSHolders.Get(aHolder, nullptr);
}

static void
AssertNoGcThing(void* aGCThing, const char* aName, void* aClosure)
{
  MOZ_ASSERT(!aGCThing);
}

void
CycleCollectedJSRuntime::AssertNoObjectsToTrace(void* aPossibleJSHolder)
{
  nsScriptObjectTracer* tracer = mJSHolders.Get(aPossibleJSHolder);
  if (tracer && tracer->Trace) {
    tracer->Trace(aPossibleJSHolder, TraceCallbackFunc(AssertNoGcThing), nullptr);
  }
}
#endif
