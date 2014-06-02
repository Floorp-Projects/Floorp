/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsOSHelperAppService_h
#define nsOSHelperAppService_h

#include "nsCExternalHandlerService.h"
#include "nsExternalHelperAppService.h"

class nsOSHelperAppService : public nsExternalHelperAppService
{
public:
    nsOSHelperAppService();
    virtual ~nsOSHelperAppService();

    virtual already_AddRefed<nsIMIMEInfo>
    GetMIMEInfoFromOS(const nsACString& aMIMEType,
                      const nsACString& aFileExt,
                      bool* aFound);

    virtual nsresult
    OSProtocolHandlerExists(const char* aScheme,
                            bool* aExists);

    NS_IMETHOD GetProtocolHandlerInfoFromOS(const nsACString &aScheme,
                                            bool *found,
                                            nsIHandlerInfo **_retval);

    static nsIHandlerApp*
    CreateAndroidHandlerApp(const nsAString& aName,
                            const nsAString& aDescription,
                            const nsAString& aPackageName,
                            const nsAString& aClassName, 
                            const nsACString& aMimeType,
                            const nsAString& aAction = EmptyString());
};

#endif /* nsOSHelperAppService_h */
