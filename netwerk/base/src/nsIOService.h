/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIObserver.h"
#include "nsWeakReference.h"

#define NS_N(x) (sizeof(x)/sizeof(*x))

static const char *gScheme[] = {"chrome", "resource", "file", "http"};

class nsIPrefBranch;

class nsIOService : public nsIIOService
                  , public nsIObserver
                  , public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS

    // nsIIOService methods:
    NS_DECL_NSIIOSERVICE

    // nsIObserver methods:
    NS_DECL_NSIOBSERVER

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

    // Prefs wrangling
    void PrefsChanged(nsIPrefBranch *prefs, const char *pref = nsnull);
    void GetPrefBranch(nsIPrefBranch **);
    void ParsePortList(nsIPrefBranch *prefBranch, const char *pref, PRBool remove);

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
    nsAutoVoidArray                     mURLParsers;
    nsVoidArray                         mRestrictedPortList;
};

#endif // nsIOService_h__


