/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
 
#ifndef __nsprotocolproxyservice___h___
#define __nsprotocolproxyservice___h___

#include "plevent.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIPref.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsIProtocolProxyService.h"
#include "nsIProxyAutoConfig.h"
#include "nsIProxyInfo.h"
#include "prmem.h"

class nsProtocolProxyService : public nsIProtocolProxyService {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLPROXYSERVICE

    nsProtocolProxyService();
    virtual ~nsProtocolProxyService();

    NS_IMETHOD Init();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    void PrefsChanged(const char* pref);

    class nsProxyInfo : public nsIProxyInfo {
        NS_DECL_ISUPPORTS

        NS_IMETHOD_(const char*) Host() {
            return mHost;
        }

        NS_IMETHOD_(PRInt32) Port() {
            return mPort;
        }

        NS_IMETHOD_(const char*) Type() {
            return mType;
        }

        virtual ~nsProxyInfo() {
            PR_FREEIF(mHost);
            PR_FREEIF(mType);
        }

        nsProxyInfo() : mType(nsnull), mHost(nsnull), mPort(-1) {
            NS_INIT_REFCNT();
        }

        char* mType;
        char* mHost;
        PRInt32 mPort;
    };

protected:

    void           LoadFilters(const char* filters);
    static PRBool  CleanupFilterArray(void* aElement, void* aData);

    // simplified array of filters defined by this struct
    struct host_port {
        nsCString*  host;
        PRInt32     port;
    };

    PRLock                  *mArrayLock;
    nsVoidArray             mFiltersArray;

    PRBool CanUseProxy(nsIURI* aURI);

    nsCOMPtr<nsIPref>       mPrefs;
    PRUint16                mUseProxy;

    nsXPIDLCString          mHTTPProxyHost;
    PRInt32                 mHTTPProxyPort;

    nsXPIDLCString          mFTPProxyHost;
    PRInt32                 mFTPProxyPort;

    nsXPIDLCString          mGopherProxyHost;
    PRInt32                 mGopherProxyPort;

    nsXPIDLCString          mHTTPSProxyHost;
    PRInt32                 mHTTPSProxyPort;
    
    nsXPIDLCString          mSOCKSProxyHost;
    PRInt32                 mSOCKSProxyPort;
    PRInt32                 mSOCKSProxyVersion;

    nsCOMPtr<nsIProxyAutoConfig> mPAC;
    nsXPIDLCString          mPACURL;

    static void PR_CALLBACK HandlePACLoadEvent(PLEvent* aEvent);
    static void PR_CALLBACK DestroyPACLoadEvent(PLEvent* aEvent);
};

#endif // __nsprotocolproxyservice___h___

