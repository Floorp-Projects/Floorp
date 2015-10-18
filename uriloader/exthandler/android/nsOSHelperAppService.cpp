/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsOSHelperAppService.h"
#include "nsMIMEInfoAndroid.h"
#include "AndroidBridge.h"

nsOSHelperAppService::nsOSHelperAppService() : nsExternalHelperAppService()
{
}

nsOSHelperAppService::~nsOSHelperAppService()
{
}

already_AddRefed<nsIMIMEInfo>
nsOSHelperAppService::GetMIMEInfoFromOS(const nsACString& aMIMEType,
                                        const nsACString& aFileExt,
                                        bool* aFound)
{
    RefPtr<nsMIMEInfoAndroid> mimeInfo;
    *aFound = false;
    if (!aMIMEType.IsEmpty())
        *aFound = 
            nsMIMEInfoAndroid::GetMimeInfoForMimeType(aMIMEType, 
                                                      getter_AddRefs(mimeInfo));
    if (!*aFound)
        *aFound =
            nsMIMEInfoAndroid::GetMimeInfoForFileExt(aFileExt, 
                                                     getter_AddRefs(mimeInfo));

    // Code that calls this requires an object regardless if the OS has
    // something for us, so we return the empty object.
    if (!*aFound)
        mimeInfo = new nsMIMEInfoAndroid(aMIMEType);

    return mimeInfo.forget();
}

nsresult
nsOSHelperAppService::OSProtocolHandlerExists(const char* aScheme,
                                              bool* aExists)
{
    *aExists = mozilla::AndroidBridge::Bridge()->GetHandlersForURL(NS_ConvertUTF8toUTF16(aScheme));    
    return NS_OK;
}

nsresult nsOSHelperAppService::GetProtocolHandlerInfoFromOS(const nsACString &aScheme,
                                      bool *found,
                                      nsIHandlerInfo **info)
{
    return nsMIMEInfoAndroid::GetMimeInfoForURL(aScheme, found, info);
}

nsIHandlerApp*
nsOSHelperAppService::CreateAndroidHandlerApp(const nsAString& aName,
                                              const nsAString& aDescription,
                                              const nsAString& aPackageName,
                                              const nsAString& aClassName, 
                                              const nsACString& aMimeType,
                                              const nsAString& aAction)
{
    return new nsAndroidHandlerApp(aName, aDescription, aPackageName,
                                   aClassName, aMimeType, aAction);
}
