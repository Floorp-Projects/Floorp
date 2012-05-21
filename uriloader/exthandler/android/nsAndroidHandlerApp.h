/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAndroidHandlerApp_h
#define nsAndroidHandlerApp_h

#include "nsMIMEInfoImpl.h"
#include "nsIExternalSharingAppService.h"

class nsAndroidHandlerApp : public nsISharingHandlerApp {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIHANDLERAPP
    NS_DECL_NSISHARINGHANDLERAPP

    public:
    nsAndroidHandlerApp(const nsAString& aName, const nsAString& aDescription,
                        const nsAString& aPackageName, 
                        const nsAString& aClassName, 
                        const nsACString& aMimeType, const nsAString& aAction);
    virtual ~nsAndroidHandlerApp();

private:
    nsString mName;
    nsString mDescription;
    nsCString mMimeType;
    nsString mClassName;
    nsString mPackageName;
    nsString mAction;
};
#endif
