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

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIPref.h"
#include "nsHashtable.h"
#include "nsXPIDLString.h"
#include "nsIProtocolProxyService.h"

class nsProtocolProxyService : public nsIProtocolProxyService {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLPROXYSERVICE

    nsProtocolProxyService() {
        NS_INIT_REFCNT();
        mUseProxy = PR_FALSE;
        mFilters=0;
    };

    virtual ~nsProtocolProxyService() {
        CRTFREEIF(mFilters);
    };

    NS_IMETHOD Init();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    void PrefsChanged(const char* pref);

protected:
    PRBool CanUseProxy(nsIURI* aURI);

    nsCOMPtr<nsIPref>       mPrefs;
    char                    *mFilters;
    PRBool                  mUseProxy;

    nsXPIDLCString          mHTTPProxyHost;
    PRInt32                 mHTTPProxyPort;

    nsXPIDLCString          mFTPProxyHost;
    PRInt32                 mFTPProxyPort;
};

#endif // __nsprotocolproxyservice___h___

