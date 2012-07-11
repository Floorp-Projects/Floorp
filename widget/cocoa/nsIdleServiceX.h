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

  bool PollIdleTime(PRUint32* aIdleTime);

  static already_AddRefed<nsIdleServiceX> GetInstance() 
  {
    nsIdleServiceX* idleService = 
      static_cast<nsIdleServiceX*>(nsIdleService::GetInstance().get());
    if (!idleService) {
      idleService = new nsIdleServiceX();
      NS_ADDREF(idleService);
    }
    
    return idleService;
  }
  
protected:
    nsIdleServiceX() { }
    virtual ~nsIdleServiceX() { }
    bool UsePollMode();
};

#endif // nsIdleServiceX_h_
