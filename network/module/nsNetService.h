/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef net_serv_h___
#define net_serv_h___

#include "nspr.h"
#include "nsIPref.h"
#include "nsINetService.h"
#include "net.h"

class nsINetContainerApplication;
class nsITimer;

class nsNetlibService : public nsINetService {

public:
    NS_DECL_ISUPPORTS

    nsNetlibService(nsINetContainerApplication *aContainer);

    /* Implementation of the nsINetService interface */
    NS_IMETHOD OpenStream(nsIURL *aUrl, nsIStreamListener *aConsumer);
    NS_IMETHOD OpenBlockingStream(nsIURL *aUrl, 
                                  nsIStreamListener *aConsumer,
                                  nsIInputStream **aNewStream);

    NS_IMETHOD GetContainerApplication(nsINetContainerApplication **aContainer);
    nsresult SetContainerApplication(nsINetContainerApplication *aContainer);

    NS_IMETHOD GetCookieString(nsIURL *aURL, nsString& aCookie);
    NS_IMETHOD SetCookieString(nsIURL *aURL, const nsString& aCookie);

protected:
    virtual ~nsNetlibService();

    void SchedulePollingTimer();
    void CleanupPollingTimer(nsITimer* aTimer);
    static void NetPollSocketsCallback(nsITimer* aTimer, void* aClosure);

private:
    void SetupURLStruct(nsIURL *aURL, URL_Struct *aURL_s);
    /* XXX: This is temporary until bamwrap.cpp is removed... */
    void *m_stubContext;
    nsINetContainerApplication *mContainer;
    nsIPref *mPref;

    nsITimer* mPollingTimer;
};


#endif /* net_strm_h___ */
