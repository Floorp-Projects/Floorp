/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUserIdleServiceX_h_
#define nsUserIdleServiceX_h_

#include "nsUserIdleService.h"

class nsUserIdleServiceX : public nsUserIdleService {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsUserIdleServiceX, nsUserIdleService)

  bool PollIdleTime(uint32_t* aIdleTime) override;

  static already_AddRefed<nsUserIdleServiceX> GetInstance() {
    RefPtr<nsUserIdleService> idleService = nsUserIdleService::GetInstance();
    if (!idleService) {
      idleService = new nsUserIdleServiceX();
    }

    return idleService.forget().downcast<nsUserIdleServiceX>();
  }

 protected:
  nsUserIdleServiceX() {}
  virtual ~nsUserIdleServiceX() {}
  bool UsePollMode() override;
};

#endif  // nsUserIdleServiceX_h_
