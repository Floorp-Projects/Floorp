/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_EXTERNAL_SHARING_APP_SERVICE_H
#define NS_EXTERNAL_SHARING_APP_SERVICE_H

#include "nsIExternalSharingAppService.h"
#include "nsAutoPtr.h"

class  ShareUiInterface;

#define NS_EXTERNALSHARINGAPPSERVICE_CID \
  {0xea782c90, 0xc6ec, 0x11df, \
  {0xbd, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66, 0x74}}

class nsExternalSharingAppService : public nsIExternalSharingAppService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXTERNALSHARINGAPPSERVICE

  nsExternalSharingAppService();

private:
  ~nsExternalSharingAppService();

protected:
  nsAutoPtr<ShareUiInterface> mShareUi;
};
#endif
