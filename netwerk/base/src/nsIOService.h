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

#ifndef nsIOService_h__
#define nsIOService_h__

#include "nsIIOService.h"
#include "nsString2.h"

class nsIOService : public nsIIOService
{
public:
    NS_DECL_ISUPPORTS

    // nsIIOService methods:
    NS_IMETHOD GetProtocolHandler(const char *scheme,
                                  nsIProtocolHandler **_retval);
    NS_IMETHOD MakeAbsolute(const char *aRelativeSpec,
                            nsIURI *aBaseURI,
                            char **_retval);
    NS_IMETHOD NewURI(const char *aSpec, nsIURI *aBaseURI,
                      nsIURI **_retval);
    NS_IMETHOD NewChannelFromURI(const char* verb, nsIURI *aURI,
                                 nsIEventSinkGetter *eventSinkGetter,
                                 nsIChannel **_retval);
    NS_IMETHOD NewChannel(const char* verb, const char *aSpec,
                          nsIURI *aBaseURI,
                          nsIEventSinkGetter *eventSinkGetter,
                          nsIChannel **_retval);
    NS_IMETHOD GetAppCodeName(PRUnichar* *aAppCodeName);
    NS_IMETHOD GetAppVersion(PRUnichar* *aAppVersion);
    NS_IMETHOD GetAppName(PRUnichar* *aAppName);
    NS_IMETHOD GetLanguage(PRUnichar* *aLanguage);
    NS_IMETHOD SetLanguage(PRUnichar* aLanguage);
    NS_IMETHOD GetPlatform(PRUnichar* *aPlatform);
    NS_IMETHOD GetUserAgent(PRUnichar* *aUserAgent);
    NS_IMETHOD NewAsyncStreamObserver(nsIStreamObserver *receiver, nsIEventQueue *eventQueue, nsIStreamObserver **_retval);
    NS_IMETHOD NewAsyncStreamListener(nsIStreamListener *receiver, nsIEventQueue *eventQueue, nsIStreamListener **_retval);
    NS_IMETHOD NewSyncStreamListener(nsIInputStream **inStream, nsIBufferOutputStream **outStream, nsIStreamListener **_retval);
    NS_IMETHOD NewChannelFromNativePath(const char *nativePath, nsIFileChannel **_retval);
    NS_IMETHOD NewLoadGroup(nsISupports* outer, nsIStreamObserver* observer, nsILoadGroup* parent, nsILoadGroup **result);
    NS_IMETHOD NewInputStreamChannel(nsIURI* uri, const char *contentType, nsIInputStream *inStr, nsIChannel **result);

    // nsIOService methods:
    nsIOService();
    virtual ~nsIOService();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsresult Init();

protected:
    nsString2   *mAppName;
    nsString2   *mAppCodeName;
    nsString2   *mAppVersion;
    nsString2   *mAppLanguage;
    nsString2   *mAppPlatform;
};

#endif // nsIOService_h__
