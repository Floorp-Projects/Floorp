/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:cin:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsOSHelperAppService.h"

nsOSHelperAppService::nsOSHelperAppService() : nsExternalHelperAppService() {}

nsOSHelperAppService::~nsOSHelperAppService() {}

nsresult nsOSHelperAppService::OSProtocolHandlerExists(const char* aProtocolScheme,
                                                       bool* aHandlerExists) {
  *aHandlerExists = false;
  return NS_OK;
}

NS_IMETHODIMP
nsOSHelperAppService::GetApplicationDescription(const nsACString& aScheme, nsAString& _retval) {
  return NS_ERROR_NOT_AVAILABLE;
}

nsresult nsOSHelperAppService::GetFileTokenForPath(const char16_t* aPlatformAppPath,
                                                   nsIFile** aFile) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsOSHelperAppService::GetFromTypeAndExtension(const nsACString& aType, const nsACString& aFileExt,
                                              nsIMIMEInfo** aMIMEInfo) {
  return nsExternalHelperAppService::GetFromTypeAndExtension(aType, aFileExt, aMIMEInfo);
}

NS_IMETHODIMP nsOSHelperAppService::GetMIMEInfoFromOS(const nsACString& aMIMEType,
                                                      const nsACString& aFileExt, bool* aFound,
                                                      nsIMIMEInfo** aMIMEInfo) {
  *aMIMEInfo = nullptr;
  *aFound = false;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsOSHelperAppService::GetProtocolHandlerInfoFromOS(const char* aScheme, bool* found,
                                                   nsIHandlerInfo** _retval) {
  *found = false;
  return NS_OK;
}
