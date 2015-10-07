/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIdleServiceX_h_
#define nsIdleServiceX_h_

#include "nsIdleService.h"

class nsIdleServiceX : public nsIdleService
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  bool PollIdleTime(uint32_t* aIdleTime) override;

  static already_AddRefed<nsIdleServiceX> GetInstance() 
  {
    RefPtr<nsIdleService> idleService = nsIdleService::GetInstance();
    if (!idleService) {
      idleService = new nsIdleServiceX();
    }
    
    return idleService.forget().downcast<nsIdleServiceX>();
  }
  
protected:
    nsIdleServiceX() { }
    virtual ~nsIdleServiceX() { }
    bool UsePollMode() override;
};

#endif // nsIdleServiceX_h_
