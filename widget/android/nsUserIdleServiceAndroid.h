/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUserIdleServiceAndroid_h__
#define nsUserIdleServiceAndroid_h__

#include "nsUserIdleService.h"
#include "mozilla/AppShutdown.h"

class nsUserIdleServiceAndroid : public nsUserIdleService {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsUserIdleServiceAndroid,
                                       nsUserIdleService)

  bool PollIdleTime(uint32_t* aIdleTime) override;

  static already_AddRefed<nsUserIdleServiceAndroid> GetInstance() {
    RefPtr<nsUserIdleService> idleService = nsUserIdleService::GetInstance();
    if (!idleService) {
      // Avoid late instantiation or resurrection during shutdown.
      if (mozilla::AppShutdown::IsInOrBeyond(
              mozilla::ShutdownPhase::AppShutdownConfirmed)) {
        return nullptr;
      }
      idleService = new nsUserIdleServiceAndroid();
    }

    return idleService.forget().downcast<nsUserIdleServiceAndroid>();
  }

 protected:
  nsUserIdleServiceAndroid() {}
  virtual ~nsUserIdleServiceAndroid() {}
};

#endif  // nsUserIdleServiceAndroid_h__
