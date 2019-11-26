/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsOSHelperAppService.h"
#include "nsMIMEInfoAndroid.h"
#include "AndroidBridge.h"

nsOSHelperAppService::nsOSHelperAppService() : nsExternalHelperAppService() {}

nsOSHelperAppService::~nsOSHelperAppService() {}

nsresult nsOSHelperAppService::GetMIMEInfoFromOS(const nsACString& aMIMEType,
                                                 const nsACString& aFileExt,
                                                 bool* aFound,
                                                 nsIMIMEInfo** aMIMEInfo) {
  RefPtr<nsMIMEInfoAndroid> mimeInfo;
  *aFound = false;
  if (!aMIMEType.IsEmpty())
    *aFound = nsMIMEInfoAndroid::GetMimeInfoForMimeType(
        aMIMEType, getter_AddRefs(mimeInfo));
  if (!*aFound)
    *aFound = nsMIMEInfoAndroid::GetMimeInfoForFileExt(
        aFileExt, getter_AddRefs(mimeInfo));

  // Code that calls this requires an object regardless if the OS has
  // something for us, so we return the empty object.
  if (!*aFound) mimeInfo = new nsMIMEInfoAndroid(aMIMEType);

  mimeInfo.forget(aMIMEInfo);
  return NS_OK;
}

nsresult nsOSHelperAppService::OSProtocolHandlerExists(const char* aScheme,
                                                       bool* aExists) {
  // We don't want to get protocol handlers from the OS in GV; the app
  // should take care of that in NavigationDelegate.onLoadRequest().
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsOSHelperAppService::GetApplicationDescription(const nsACString& aScheme,
                                                nsAString& _retval) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsOSHelperAppService::IsCurrentAppOSDefaultForProtocol(
    const nsACString& aScheme, bool* _retval) {
  return NS_ERROR_NOT_AVAILABLE;
}

nsresult nsOSHelperAppService::GetProtocolHandlerInfoFromOS(
    const nsACString& aScheme, bool* found, nsIHandlerInfo** info) {
  // We don't want to get protocol handlers from the OS in GV; the app
  // should take care of that in NavigationDelegate.onLoadRequest().
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsIHandlerApp* nsOSHelperAppService::CreateAndroidHandlerApp(
    const nsAString& aName, const nsAString& aDescription,
    const nsAString& aPackageName, const nsAString& aClassName,
    const nsACString& aMimeType, const nsAString& aAction) {
  return new nsAndroidHandlerApp(aName, aDescription, aPackageName, aClassName,
                                 aMimeType, aAction);
}
