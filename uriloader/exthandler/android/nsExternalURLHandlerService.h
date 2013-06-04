/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSEXTERNALURLHANDLERSERVICE_H
#define NSEXTERNALURLHANDLERSERVICE_H

#include "nsIExternalURLHandlerService.h"

// {4BF1F8EF-D947-4BA3-9CD3-8C9A54A63A1C}
#define NS_EXTERNALURLHANDLERSERVICE_CID \
    {0x4bf1f8ef, 0xd947, 0x4ba3, {0x9c, 0xd3, 0x8c, 0x9a, 0x54, 0xa6, 0x3a, 0x1c}}

class nsExternalURLHandlerService MOZ_FINAL
  : public nsIExternalURLHandlerService
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIEXTERNALURLHANDLERSERVICE

    nsExternalURLHandlerService();
private:
    ~nsExternalURLHandlerService();
};

#endif // NSEXTERNALURLHANDLERSERVICE_H
