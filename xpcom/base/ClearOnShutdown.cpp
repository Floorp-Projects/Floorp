/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ClearOnShutdown.h"

namespace mozilla {
namespace ClearOnShutdown_Internal {

Array<StaticAutoPtr<ShutdownList>,
      static_cast<size_t>(ShutdownPhase::ShutdownPhase_Length)>
    sShutdownObservers;
ShutdownPhase sCurrentClearOnShutdownPhase = ShutdownPhase::NotInShutdown;

void InsertIntoShutdownList(ShutdownObserver* aObserver, ShutdownPhase aPhase) {
  // Adding a ClearOnShutdown for a "past" phase is an error.
  if (PastShutdownPhase(aPhase)) {
    MOZ_ASSERT(false, "ClearOnShutdown for phase that already was cleared");
    aObserver->Shutdown();
    delete aObserver;
    return;
  }

  if (!(sShutdownObservers[static_cast<size_t>(aPhase)])) {
    sShutdownObservers[static_cast<size_t>(aPhase)] = new ShutdownList();
  }
  sShutdownObservers[static_cast<size_t>(aPhase)]->insertBack(aObserver);
}

}  // namespace ClearOnShutdown_Internal

// Called by AdvanceShutdownPhase each time we switch a phase. Will null out
// pointers added by ClearOnShutdown for all phases up to and including aPhase.
void KillClearOnShutdown(ShutdownPhase aPhase) {
  using namespace ClearOnShutdown_Internal;

  MOZ_ASSERT(NS_IsMainThread());
  // Shutdown only goes one direction...
  MOZ_ASSERT(!PastShutdownPhase(aPhase));

  // Set the phase before notifying observers to make sure that they can't run
  // any code which isn't allowed to run after the start of this phase.
  sCurrentClearOnShutdownPhase = aPhase;

  // It's impossible to add an entry for a "past" phase; this is blocked in
  // ClearOnShutdown, but clear them out anyways in case there are phases
  // that weren't passed to KillClearOnShutdown.
  for (size_t phase = static_cast<size_t>(ShutdownPhase::First);
       phase <= static_cast<size_t>(aPhase); phase++) {
    if (sShutdownObservers[static_cast<size_t>(phase)]) {
      while (ShutdownObserver* observer =
                 sShutdownObservers[static_cast<size_t>(phase)]->popLast()) {
        observer->Shutdown();
        delete observer;
      }
      sShutdownObservers[static_cast<size_t>(phase)] = nullptr;
    }
  }
}

}  // namespace mozilla
