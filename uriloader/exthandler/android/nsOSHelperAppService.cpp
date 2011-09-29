/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Wu <mwu@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    nsRefPtr<nsMIMEInfoAndroid> mimeInfo;
    *aFound = PR_FALSE;
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
    *aExists = mozilla::AndroidBridge::Bridge()->GetHandlersForURL(aScheme);    
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
