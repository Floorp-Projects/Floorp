/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsNetService_h___
#define nsNetService_h___

#include "nsString.h"
#include "nspr.h"
#include "nsIPref.h"
#include "nsITransport.h"
#include "nsIEventQueueService.h"
#include "nsINetService.h"
#include "nsNetThread.h"
#include "nsHashtable.h"
#include "net.h"

class nsITimer;

// The chrome registry interface
class nsIChromeRegistry;

class nsNetlibService : public nsINetService {

public:
    NS_DECL_ISUPPORTS

    nsNetlibService();

    /* Implementation of the nsINetService interface */
    NS_IMETHOD OpenStream(nsIURI *aUrl, 
                          nsIStreamListener *aConsumer);
    NS_IMETHOD OpenBlockingStream(nsIURI *aUrl, 
                                  nsIStreamListener *aConsumer,
                                  nsIInputStream **aNewStream);
    
    NS_IMETHOD InterruptStream(nsIURI* aURL);

    NS_IMETHOD GetCookieString(nsIURI *aURL, nsString& aCookie);
    NS_IMETHOD SetCookieString(nsIURI *aURL, const nsString& aCookie);

    NS_IMETHOD Cookie_DisplayCookieInfoAsHTML();
    NS_IMETHOD Cookie_CookieViewerReturn(nsAutoString results);
    NS_IMETHOD Cookie_GetCookieListForViewer(nsString& aCookieList);
    NS_IMETHOD Cookie_GetPermissionListForViewer(nsString& aPermissionList);

    NS_IMETHOD GetProxyHTTP(nsString& aProxyHTTP);
    NS_IMETHOD SetProxyHTTP(nsString& aProxyHTTP);
    NS_IMETHOD GetHTTPOneOne(PRBool& aOneOne);
    NS_IMETHOD SetHTTPOneOne(PRBool aSendOneOne);

    NS_IMETHOD GetAppCodeName(nsString& aAppCodeName);
    NS_IMETHOD GetAppVersion(nsString& aAppVersion);
    NS_IMETHOD GetAppName(nsString& aAppName);
    NS_IMETHOD GetLanguage(nsString& aLanguage);
    NS_IMETHOD GetPlatform(nsString& aPlatform);
    NS_IMETHOD GetUserAgent(nsString& aUA);
    NS_IMETHOD SetCustomUserAgent(nsString aCustom);
    NS_IMETHOD RegisterProtocol(const nsString& aName,
                                nsIProtocolURLFactory* aProtocolURLFactory,
                                nsIProtocol* aProtocol);
    NS_IMETHOD UnregisterProtocol(const nsString& aName);
    NS_IMETHOD GetProtocol(const nsString& aName, 
                           nsIProtocolURLFactory* *aProtocolURLFactory,
                           nsIProtocol* *aProtocol);
    NS_IMETHOD CreateURL(nsIURI* *aURL, 
                         const nsString& aSpec,
                         const nsIURI* aContextURL = nsnull,
                         nsISupports* aContainer = nsnull,
                         nsILoadGroup* aGroup = nsnull);
    NS_IMETHOD AreThereActiveConnections(void);

	NS_IMETHOD CreateSocketTransport(nsITransport **aTransport, PRUint32 aPortToUse, const char * aHostName);

	NS_IMETHOD CreateFileSocketTransport(nsITransport **aTransport, const char * aFileName);

protected:
    virtual ~nsNetlibService();

    nsresult StartNetlibThread(void);
    nsresult StopNetlibThread(void);

    void SchedulePollingTimer();
    void CleanupPollingTimer(nsITimer* aTimer);
    static void NetPollSocketsCallback(nsITimer* aTimer, void* aClosure);

#if defined(NETLIB_THREAD)
    static void NetlibThreadMain(void *aParam);
#endif /* NETLIB_THREAD */

    // Chrome Registry static variables
    static nsIChromeRegistry* gChromeRegistry;
    static int gRefCnt;

private:
    void SetupURLStruct(nsIURI *aURL, URL_Struct *aURL_s);
    /* XXX: This is temporary until bamwrap.cpp is removed... */
    void *m_stubContext;
    nsIPref *mPref;

    nsITimer* mPollingTimer;

    nsNetlibThread* mNetlibThread;
    nsIEventQueueService* mEventQService;

    nsHashtable* mProtocols;
};

extern "C" void net_ReleaseContext(MWContext *context);


#endif /* nsNetService_h___ */
