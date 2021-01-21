/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_places_NotifyRankingChanged_h_
#define mozilla_places_NotifyRankingChanged_h_

#include "mozilla/dom/PlacesObservers.h"
#include "mozilla/dom/PlacesRanking.h"

using namespace mozilla::dom;

namespace mozilla {
namespace places {

class NotifyRankingChanged final : public Runnable {
 public:
  NotifyRankingChanged() : Runnable("places::NotifyRankingChanged") {}

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is marked
  // MOZ_CAN_RUN_SCRIPT.  See bug 1535398.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");

    RefPtr<PlacesRanking> event = new PlacesRanking();
    Sequence<OwningNonNull<PlacesEvent>> events;
    bool success = !!events.AppendElement(event.forget(), fallible);
    MOZ_RELEASE_ASSERT(success);
    PlacesObservers::NotifyListeners(events);

    return NS_OK;
  }
};

}  // namespace places
}  // namespace mozilla

#endif  // mozilla_places_NotifyRankingChanged_h_
