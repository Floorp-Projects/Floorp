/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsIOService_h__
#define nsIOService_h__

#include "nsIIOService.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsISocketTransportService.h" 
#include "nsIFileTransportService.h" 
#include "nsIDNSService.h" 
#include "nsIProtocolProxyService.h"
#include "nsCOMPtr.h"
#include "nsURLHelper.h"
#include "nsWeakPtr.h"
#include "nsIEventQueueService.h"
#include "nsIURLParser.h"
#include "nsSupportsArray.h"

#define NS_N(x) (sizeof(x)/sizeof(*x))

static const char *gScheme[] = {"chrome", "resource", "file", "http"};

class nsIOService : public nsIIOService
{
public:
    NS_DECL_ISUPPORTS

    // nsIIOService methods:
    NS_DECL_NSIIOSERVICE

    // nsIOService methods:
    nsIOService();
    virtual ~nsIOService();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsresult Init();
    nsresult NewURI(const char* aSpec, nsIURI* aBaseURI,
                    nsIURI* *result, nsIProtocolHandler* *hdlrResult);

protected:
    NS_METHOD GetCachedProtocolHandler(const char *scheme,
                                       nsIProtocolHandler* *hdlrResult,
                                       PRUint32 start=0,
                                       PRUint32 end=0);
    NS_METHOD CacheProtocolHandler(const char *scheme,
                                   nsIProtocolHandler* hdlr);

    NS_METHOD GetCachedURLParser(const char *scheme,
                                 nsIURLParser* *hdlrResult);

    NS_METHOD CacheURLParser(const char *scheme,
                             nsIURLParser* hdlr);

protected:
    PRBool      mOffline;
    nsCOMPtr<nsISocketTransportService> mSocketTransportService;
    nsCOMPtr<nsIFileTransportService>   mFileTransportService;
    nsCOMPtr<nsIDNSService>             mDNSService;
    nsCOMPtr<nsIProtocolProxyService>   mProxyService;
    nsCOMPtr<nsIEventQueueService> mEventQueueService;
    
    // Cached protocol handlers
    nsWeakPtr                  mWeakHandler[NS_N(gScheme)];

    // Cached url handlers
    nsCOMPtr<nsIURLParser>              mDefaultURLParser;
    nsSupportsArray                     mURLParsers;
    nsVoidArray                         mRestrictedPortList;
};

#endif // nsIOService_h__


