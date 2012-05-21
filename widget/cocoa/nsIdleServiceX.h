/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIdleServiceX_h_
#define nsIdleServiceX_h_

#include "nsIdleService.h"

class nsIdleServiceX : public nsIdleService
{
public:
  NS_DECL_ISUPPORTS

  nsIdleServiceX() {}
  virtual ~nsIdleServiceX() {}

  bool PollIdleTime(PRUint32* aIdleTime);

protected:
    bool UsePollMode();
};

#endif // nsIdleServiceX_h_
