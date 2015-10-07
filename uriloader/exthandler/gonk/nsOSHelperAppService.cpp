/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nsOSHelperAppService.h"
#include "nsMIMEInfoImpl.h"

class nsGonkMIMEInfo : public nsMIMEInfoImpl {
public:
    nsGonkMIMEInfo(const nsACString& aMIMEType) : nsMIMEInfoImpl(aMIMEType) { }

protected:
    virtual nsresult LoadUriInternal(nsIURI *aURI) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }
};

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
    *aFound = false;
    // Even if we return false for aFound, we need to return a working
    // nsIMIMEInfo implementation that will be used by the caller.
    RefPtr<nsGonkMIMEInfo> mimeInfo = new nsGonkMIMEInfo(aMIMEType);
    return mimeInfo.forget();
}

nsresult
nsOSHelperAppService::OSProtocolHandlerExists(const char* aScheme,
                                              bool* aExists)
{
    *aExists = false;
    return NS_OK;
}
