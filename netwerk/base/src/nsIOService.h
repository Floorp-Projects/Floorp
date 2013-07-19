/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIOService_h__
#define nsIOService_h__

#include "necko-config.h"

#include "nsString.h"
#include "nsIIOService2.h"
#include "nsTArray.h"
#include "nsPISocketTransportService.h" 
#include "nsPIDNSService.h" 
#include "nsIProtocolProxyService2.h"
#include "nsCOMPtr.h"
#include "nsURLHelper.h"
#include "nsWeakPtr.h"
#include "nsIURLParser.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsINetUtil.h"
#include "nsIChannelEventSink.h"
#include "nsIContentSniffer.h"
#include "nsCategoryCache.h"
#include "nsINetworkLinkService.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "nsISpeculativeConnect.h"
#include "mozilla/Attributes.h"

#define NS_N(x) (sizeof(x)/sizeof(*x))

// We don't want to expose this observer topic.
// Intended internal use only for remoting offline/inline events.
// See Bug 552829
#define NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC "ipc:network:set-offline"

static const char gScheme[][sizeof("resource")] =
    {"chrome", "file", "http", "jar", "resource"};

class nsIPrefBranch;

class nsIOService MOZ_FINAL : public nsIIOService2
                            , public nsIObserver
                            , public nsINetUtil
                            , public nsISpeculativeConnect
                            , public nsSupportsWeakReference
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIIOSERVICE
    NS_DECL_NSIIOSERVICE2
    NS_DECL_NSIOBSERVER
    NS_DECL_NSINETUTIL
    NS_DECL_NSISPECULATIVECONNECT

    // Gets the singleton instance of the IO Service, creating it as needed
    // Returns nullptr on out of memory or failure to initialize.
    // Returns an addrefed pointer.
    static nsIOService* GetInstance();

    NS_HIDDEN_(nsresult) Init();
    NS_HIDDEN_(nsresult) NewURI(const char* aSpec, nsIURI* aBaseURI,
                                nsIURI* *result,
                                nsIProtocolHandler* *hdlrResult);

    // Called by channels before a redirect happens. This notifies the global
    // redirect observers.
    nsresult AsyncOnChannelRedirect(nsIChannel* oldChan, nsIChannel* newChan,
                                    uint32_t flags,
                                    nsAsyncRedirectVerifyHelper *helper);

    bool IsOffline() { return mOffline; }
    bool IsLinkUp();

    bool IsComingOnline() const {
      return mOffline && mSettingOffline && !mSetOfflineValue;
    }

private:
    // These shouldn't be called directly:
    // - construct using GetInstance
    // - destroy using Release
    nsIOService() NS_HIDDEN;
    ~nsIOService() NS_HIDDEN;

    NS_HIDDEN_(nsresult) TrackNetworkLinkStatusForOffline();

    NS_HIDDEN_(nsresult) GetCachedProtocolHandler(const char *scheme,
                                                  nsIProtocolHandler* *hdlrResult,
                                                  uint32_t start=0,
                                                  uint32_t end=0);
    NS_HIDDEN_(nsresult) CacheProtocolHandler(const char *scheme,
                                              nsIProtocolHandler* hdlr);

    // Prefs wrangling
    NS_HIDDEN_(void) PrefsChanged(nsIPrefBranch *prefs, const char *pref = nullptr);
    NS_HIDDEN_(void) GetPrefBranch(nsIPrefBranch **);
    NS_HIDDEN_(void) ParsePortList(nsIPrefBranch *prefBranch, const char *pref, bool remove);

    nsresult InitializeSocketTransportService();
    nsresult InitializeNetworkLinkService();

    // consolidated helper function
    void LookupProxyInfo(nsIURI *aURI, nsIURI *aProxyURI, uint32_t aProxyFlags,
                         nsCString *aScheme, nsIProxyInfo **outPI);

private:
    bool                                 mOffline;
    bool                                 mOfflineForProfileChange;
    bool                                 mManageOfflineStatus;

    // Used to handle SetOffline() reentrancy.  See the comment in
    // SetOffline() for more details.
    bool                                 mSettingOffline;
    bool                                 mSetOfflineValue;

    bool                                 mShutdown;

    nsCOMPtr<nsPISocketTransportService> mSocketTransportService;
    nsCOMPtr<nsPIDNSService>             mDNSService;
    nsCOMPtr<nsIProtocolProxyService2>   mProxyService;
    nsCOMPtr<nsINetworkLinkService>      mNetworkLinkService;
    bool                                 mNetworkLinkServiceInitialized;

    // Cached protocol handlers
    nsWeakPtr                            mWeakHandler[NS_N(gScheme)];

    // cached categories
    nsCategoryCache<nsIChannelEventSink> mChannelEventSinks;

    nsTArray<int32_t>                    mRestrictedPortList;

    bool                                 mAutoDialEnabled;
public:
    // Used for all default buffer sizes that necko allocates.
    static uint32_t   gDefaultSegmentSize;
    static uint32_t   gDefaultSegmentCount;
};

/**
 * Reference to the IO service singleton. May be null.
 */
extern nsIOService* gIOService;

#endif // nsIOService_h__
