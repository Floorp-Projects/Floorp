/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HoldDropJSObjects.h"

#include "mozilla/Assertions.h"
#include "mozilla/CycleCollectedJSRuntime.h"

namespace mozilla {
namespace cyclecollector {

void
HoldJSObjectsImpl(void* aHolder, nsScriptObjectTracer* aTracer)
{
  CycleCollectedJSRuntime* rt = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(rt, "Should have a CycleCollectedJSRuntime by now");
  rt->AddJSHolder(aHolder, aTracer);
}

void
HoldJSObjectsImpl(nsISupports* aHolder)
{
  nsXPCOMCycleCollectionParticipant* participant = nullptr;
  CallQueryInterface(aHolder, &participant);
  MOZ_ASSERT(participant, "Failed to QI to nsXPCOMCycleCollectionParticipant!");
  MOZ_ASSERT(participant->CheckForRightISupports(aHolder),
             "The result of QIing a JS holder should be the same as ToSupports");

  HoldJSObjectsImpl(aHolder, participant);
}

void
DropJSObjectsImpl(void* aHolder)
{
  CycleCollectedJSRuntime* rt = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(rt, "Should have a CycleCollectedJSRuntime by now");
  rt->RemoveJSHolder(aHolder);
}

void
DropJSObjectsImpl(nsISupports* aHolder)
{
#ifdef DEBUG
  nsXPCOMCycleCollectionParticipant* participant = nullptr;
  CallQueryInterface(aHolder, &participant);
  MOZ_ASSERT(participant, "Failed to QI to nsXPCOMCycleCollectionParticipant!");
  MOZ_ASSERT(participant->CheckForRightISupports(aHolder),
             "The result of QIing a JS holder should be the same as ToSupports");
#endif
  DropJSObjectsImpl(static_cast<void*>(aHolder));
}

} // namespace cyclecollector

#ifdef DEBUG
bool
IsJSHolder(void* aHolder)
{
  CycleCollectedJSRuntime* rt = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(rt, "Should have a CycleCollectedJSRuntime by now");
  return rt->IsJSHolder(aHolder);
}
#endif

} // namespace mozilla
