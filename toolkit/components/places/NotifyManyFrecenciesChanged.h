/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_places_NotifyManyFrecenciesChanged_h_
#define mozilla_places_NotifyManyFrecenciesChanged_h_

namespace mozilla {
namespace places {

class NotifyManyFrecenciesChanged final : public Runnable {
 public:
  NotifyManyFrecenciesChanged()
      : Runnable("places::NotifyManyFrecenciesChanged") {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");
    nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
    NS_ENSURE_STATE(navHistory);
    navHistory->NotifyManyFrecenciesChanged();
    return NS_OK;
  }
};

}  // namespace places
}  // namespace mozilla

#endif  // mozilla_places_NotifyManyFrecenciesChanged_h_
