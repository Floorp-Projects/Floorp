/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsNetService_h__
#define nsNetService_h__

#include "nsINetService.h"

class nsNetService : public nsINetService
{
public:
    NS_DECL_ISUPPORTS

    // nsINetService methods:
    NS_IMETHOD GetProtocolHandler(const char* scheme, nsIProtocolHandler* *result);
    NS_IMETHOD NewConnectionGroup(nsIConnectionGroup* *result);
    NS_IMETHOD MakeAbsoluteUrl(const char* aSpec,
                               nsIUrl* aBaseUrl,
                               char* *result);
    NS_IMETHOD NewUrl(const char* aSpec,
                      nsIUrl* *result,
                      nsIUrl* aBaseUrl=0);
    NS_IMETHOD NewConnection(nsIUrl* url,
                             nsISupports* eventSink,
                             nsIConnectionGroup* group,
                             nsIProtocolConnection* *result);
    NS_IMETHOD HasActiveConnections();

    NS_IMETHOD GetAppCodeName(nsString2& aAppCodeName);
    NS_IMETHOD GetAppVersion(nsString2& aAppVersion);
    NS_IMETHOD GetAppName(nsString2& aAppName);
    NS_IMETHOD GetLanguage(nsString2& aLanguage);
    NS_IMETHOD GetPlatform(nsString2& aPlatform);
    NS_IMETHOD GetUserAgent(nsString2& aUA);

    // nsNetService methods:
    nsNetService();
    virtual ~nsNetService();

    nsresult Init();

protected:
    nsString2   XP_AppName;
    nsString2   XP_AppCodeName;
    nsString2   XP_AppVersion;
    nsString2   XP_AppLanguage;
    nsString2   XP_AppPlatform;
};

#endif // nsNetService_h__
