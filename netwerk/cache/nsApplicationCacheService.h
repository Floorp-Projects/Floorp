/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsApplicationCacheService_h_
#define _nsApplicationCacheService_h_

#include "nsIApplicationCacheService.h"
#include "mozilla/Attributes.h"

class nsCacheService;

class nsApplicationCacheService MOZ_FINAL : public nsIApplicationCacheService
{
public:
    nsApplicationCacheService();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIAPPLICATIONCACHESERVICE

    static void AppClearDataObserverInit();

private:
    nsRefPtr<nsCacheService> mCacheService;
};

#endif // _nsApplicationCacheService_h_
