/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_EXTERNAL_SHARING_APP_SERVICE_H
#define NS_EXTERNAL_SHARING_APP_SERVICE_H
#include "nsIExternalSharingAppService.h"


#define NS_EXTERNALSHARINGAPPSERVICE_CID                \
  {0x93e2c46e, 0x0011, 0x434b,                          \
    {0x81, 0x2e, 0xb6, 0xf3, 0xa8, 0x1e, 0x2a, 0x58}}

class nsExternalSharingAppService final
  : public nsIExternalSharingAppService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXTERNALSHARINGAPPSERVICE

  nsExternalSharingAppService();

private:
  ~nsExternalSharingAppService();

};

#endif /*NS_EXTERNAL_SHARING_APP_SERVICE_H */
