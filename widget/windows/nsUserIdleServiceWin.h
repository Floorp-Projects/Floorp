/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUserIdleServiceWin_h__
#define nsUserIdleServiceWin_h__

#include "nsUserIdleService.h"
#include "mozilla/AppShutdown.h"

/* NOTE: Compare of GetTickCount() could overflow.  This corrects for
 * overflow situations.
 ***/
#ifndef SAFE_COMPARE_EVEN_WITH_WRAPPING
#  define SAFE_COMPARE_EVEN_WITH_WRAPPING(A, B) \
    (((int)((long)A - (long)B) & 0xFFFFFFFF))
#endif

class nsUserIdleServiceWin : public nsUserIdleService {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsUserIdleServiceWin, nsUserIdleService)

  bool PollIdleTime(uint32_t* aIdleTime) override;

  static already_AddRefed<nsUserIdleServiceWin> GetInstance() {
    RefPtr<nsUserIdleServiceWin> idleService =
        nsUserIdleService::GetInstance().downcast<nsUserIdleServiceWin>();
    if (!idleService) {
      // Avoid late instantiation or resurrection during shutdown.
      if (mozilla::AppShutdown::IsInOrBeyond(
              mozilla::ShutdownPhase::AppShutdownConfirmed)) {
        return nullptr;
      }
      idleService = new nsUserIdleServiceWin();
    }

    return idleService.forget();
  }

 protected:
  nsUserIdleServiceWin() {}
  virtual ~nsUserIdleServiceWin() {}
};

#endif  // nsUserIdleServiceWin_h__
