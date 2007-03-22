/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIOService_h__
#define nsIOService_h__

#include "necko-config.h"

#include "nsString.h"
#include "nsIIOService2.h"
#include "nsVoidArray.h"
#include "nsPISocketTransportService.h" 
#include "nsPIDNSService.h" 
#include "nsIProtocolProxyService2.h"
#include "nsCOMPtr.h"
#include "nsURLHelper.h"
#include "nsWeakPtr.h"
#include "nsIURLParser.h"
#include "nsSupportsArray.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsINetUtil.h"
#include "nsIChannelEventSink.h"
#include "nsIContentSniffer.h"
#include "nsCategoryCache.h"
#include "nsINetworkLinkService.h"

#define NS_N(x) (sizeof(x)/sizeof(*x))

#ifdef NECKO_SMALL_BUFFERS
#define NS_NECKO_BUFFER_CACHE_COUNT (10)  // Max holdings: 10 * 2k = 20k
#else
#define NS_NECKO_BUFFER_CACHE_COUNT (24)  // Max holdings: 24 * 4k = 96k
#endif
#define NS_NECKO_15_MINS (15 * 60)

static const char gScheme[][sizeof("resource")] =
    {"chrome", "file", "http", "jar", "resource"};

class nsIPrefBranch;
class nsIPrefBranch2;

class nsIOService : public nsIIOService2
                  , public nsIObserver
                  , public nsINetUtil
                  , public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIIOSERVICE
    NS_DECL_NSIIOSERVICE2
    NS_DECL_NSIOBSERVER
    NS_DECL_NSINETUTIL

    // Gets the singleton instance of the IO Service, creating it as needed
    // Returns nsnull on out of memory or failure to initialize.
    // Returns an addrefed pointer.
    static nsIOService* GetInstance();

    NS_HIDDEN_(nsresult) Init();
    NS_HIDDEN_(nsresult) NewURI(const char* aSpec, nsIURI* aBaseURI,
                                nsIURI* *result,
                                nsIProtocolHandler* *hdlrResult);

    // Called by channels before a redirect happens. This notifies the global
    // redirect observers.
    nsresult OnChannelRedirect(nsIChannel* oldChan, nsIChannel* newChan,
                               PRUint32 flags);

    // Gets the array of registered content sniffers
    const nsCOMArray<nsIContentSniffer>& GetContentSniffers() const {
      return mContentSniffers.GetEntries();
    }

    PRBool IsOffline() { return mOffline; }

private:
    // These shouldn't be called directly:
    // - construct using GetInstance
    // - destroy using Release
    nsIOService() NS_HIDDEN;
    ~nsIOService() NS_HIDDEN;

    NS_HIDDEN_(nsresult) TrackNetworkLinkStatusForOffline();

    NS_HIDDEN_(nsresult) GetCachedProtocolHandler(const char *scheme,
                                                  nsIProtocolHandler* *hdlrResult,
                                                  PRUint32 start=0,
                                                  PRUint32 end=0);
    NS_HIDDEN_(nsresult) CacheProtocolHandler(const char *scheme,
                                              nsIProtocolHandler* hdlr);

    // Prefs wrangling
    NS_HIDDEN_(void) PrefsChanged(nsIPrefBranch *prefs, const char *pref = nsnull);
    NS_HIDDEN_(void) GetPrefBranch(nsIPrefBranch2 **);
    NS_HIDDEN_(void) ParsePortList(nsIPrefBranch *prefBranch, const char *pref, PRBool remove);

private:
    PRPackedBool                         mOffline;
    PRPackedBool                         mOfflineForProfileChange;
    PRPackedBool                         mManageOfflineStatus;
    nsCOMPtr<nsPISocketTransportService> mSocketTransportService;
    nsCOMPtr<nsPIDNSService>             mDNSService;
    nsCOMPtr<nsIProtocolProxyService2>   mProxyService;
    nsCOMPtr<nsINetworkLinkService>      mNetworkLinkService;
    
    // Cached protocol handlers
    nsWeakPtr                            mWeakHandler[NS_N(gScheme)];

    // cached categories
    nsCategoryCache<nsIChannelEventSink> mChannelEventSinks;
    nsCategoryCache<nsIContentSniffer>   mContentSniffers;

    nsVoidArray                          mRestrictedPortList;

public:
    // Necko buffer cache. Used for all default buffer sizes that necko
    // allocates.
    static nsIMemory *gBufferCache;
};

/**
 * Reference to the IO service singleton. May be null.
 */
extern nsIOService* gIOService;

#endif // nsIOService_h__
