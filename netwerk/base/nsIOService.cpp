/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 cindent et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "nsIOService.h"
#include "nsIProtocolHandler.h"
#include "nsIFileProtocolHandler.h"
#include "nscore.h"
#include "nsIURI.h"
#include "prprf.h"
#include "nsIErrorService.h"
#include "netCore.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsXPCOM.h"
#include "nsIProxiedProtocolHandler.h"
#include "nsIProxyInfo.h"
#include "nsEscape.h"
#include "nsNetCID.h"
#include "nsCRT.h"
#include "nsSecCheckWrapChannel.h"
#include "nsSimpleNestedURI.h"
#include "nsTArray.h"
#include "nsIConsoleService.h"
#include "nsIUploadChannel2.h"
#include "nsXULAppAPI.h"
#include "nsIScriptSecurityManager.h"
#include "nsIProtocolProxyCallback.h"
#include "nsICancelable.h"
#include "nsINetworkLinkService.h"
#include "nsPISocketTransportService.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "nsURLHelper.h"
#include "nsPIDNSService.h"
#include "nsIProtocolProxyService2.h"
#include "MainThreadUtils.h"
#include "nsIWidget.h"
#include "nsThreadUtils.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/Telemetry.h"
#include "mozilla/net/DNS.h"
#include "CaptivePortalService.h"
#include "ReferrerPolicy.h"

#ifdef MOZ_WIDGET_GONK
#include "nsINetworkManager.h"
#include "nsINetworkInterface.h"
#endif

#if defined(XP_WIN)
#include "nsNativeConnectionHelper.h"
#endif

using namespace mozilla;
using mozilla::net::IsNeckoChild;
using mozilla::net::CaptivePortalService;

#define PORT_PREF_PREFIX           "network.security.ports."
#define PORT_PREF(x)               PORT_PREF_PREFIX x
#define AUTODIAL_PREF              "network.autodial-helper.enabled"
#define MANAGE_OFFLINE_STATUS_PREF "network.manage-offline-status"
#define OFFLINE_MIRRORS_CONNECTIVITY "network.offline-mirrors-connectivity"

// Nb: these have been misnomers since bug 715770 removed the buffer cache.
// "network.segment.count" and "network.segment.size" would be better names,
// but the old names are still used to preserve backward compatibility.
#define NECKO_BUFFER_CACHE_COUNT_PREF "network.buffer.cache.count"
#define NECKO_BUFFER_CACHE_SIZE_PREF  "network.buffer.cache.size"
#define NETWORK_NOTIFY_CHANGED_PREF   "network.notify.changed"
#define NETWORK_CAPTIVE_PORTAL_PREF   "network.captive-portal-service.enabled"

#define MAX_RECURSION_COUNT 50

nsIOService* gIOService = nullptr;
static bool gHasWarnedUploadChannel2;

// A general port blacklist.  Connections to these ports will not be allowed
// unless the protocol overrides.
//
// TODO: I am sure that there are more ports to be added.  
//       This cut is based on the classic mozilla codebase

int16_t gBadPortList[] = { 
  1,    // tcpmux          
  7,    // echo     
  9,    // discard          
  11,   // systat   
  13,   // daytime          
  15,   // netstat  
  17,   // qotd             
  19,   // chargen  
  20,   // ftp-data         
  21,   // ftp-cntl 
  22,   // ssh              
  23,   // telnet   
  25,   // smtp     
  37,   // time     
  42,   // name     
  43,   // nicname  
  53,   // domain  
  77,   // priv-rjs 
  79,   // finger   
  87,   // ttylink  
  95,   // supdup   
  101,  // hostriame
  102,  // iso-tsap 
  103,  // gppitnp  
  104,  // acr-nema 
  109,  // pop2     
  110,  // pop3     
  111,  // sunrpc   
  113,  // auth     
  115,  // sftp     
  117,  // uucp-path
  119,  // nntp     
  123,  // NTP
  135,  // loc-srv / epmap         
  139,  // netbios
  143,  // imap2  
  179,  // BGP
  389,  // ldap        
  465,  // smtp+ssl
  512,  // print / exec          
  513,  // login         
  514,  // shell         
  515,  // printer         
  526,  // tempo         
  530,  // courier        
  531,  // Chat         
  532,  // netnews        
  540,  // uucp       
  556,  // remotefs    
  563,  // nntp+ssl
  587,  //
  601,  //       
  636,  // ldap+ssl
  993,  // imap+ssl
  995,  // pop3+ssl
  2049, // nfs
  4045, // lockd
  6000, // x11        
  0,    // This MUST be zero so that we can populating the array
};

static const char kProfileChangeNetTeardownTopic[] = "profile-change-net-teardown";
static const char kProfileChangeNetRestoreTopic[] = "profile-change-net-restore";
static const char kProfileDoChange[] = "profile-do-change";
static const char kNetworkActiveChanged[] = "network-active-changed";

// Necko buffer defaults
uint32_t   nsIOService::gDefaultSegmentSize = 4096;
uint32_t   nsIOService::gDefaultSegmentCount = 24;

bool nsIOService::sTelemetryEnabled = false;

NS_IMPL_ISUPPORTS(nsAppOfflineInfo, nsIAppOfflineInfo)

////////////////////////////////////////////////////////////////////////////////

nsIOService::nsIOService()
    : mOffline(true)
    , mOfflineForProfileChange(false)
    , mManageLinkStatus(false)
    , mConnectivity(true)
    , mOfflineMirrorsConnectivity(true)
    , mSettingOffline(false)
    , mSetOfflineValue(false)
    , mShutdown(false)
    , mNetworkLinkServiceInitialized(false)
    , mChannelEventSinks(NS_CHANNEL_EVENT_SINK_CATEGORY)
    , mAutoDialEnabled(false)
    , mNetworkNotifyChanged(true)
    , mPreviousWifiState(-1)
    , mLastOfflineStateChange(PR_IntervalNow())
    , mLastConnectivityChange(PR_IntervalNow())
    , mLastNetworkLinkChange(PR_IntervalNow())
{
}

nsresult
nsIOService::Init()
{
    nsresult rv;

    // We need to get references to the DNS service so that we can shut it
    // down later. If we wait until the nsIOService is being shut down,
    // GetService will fail at that point.

    mDNSService = do_GetService(NS_DNSSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
        NS_WARNING("failed to get DNS service");
        return rv;
    }

    // XXX hack until xpidl supports error info directly (bug 13423)
    nsCOMPtr<nsIErrorService> errorService = do_GetService(NS_ERRORSERVICE_CONTRACTID);
    if (errorService) {
        errorService->RegisterErrorStringBundle(NS_ERROR_MODULE_NETWORK, NECKO_MSGS_URL);
    }
    else
        NS_WARNING("failed to get error service");

    InitializeCaptivePortalService();

    // setup our bad port list stuff
    for(int i=0; gBadPortList[i]; i++)
        mRestrictedPortList.AppendElement(gBadPortList[i]);

    // Further modifications to the port list come from prefs
    nsCOMPtr<nsIPrefBranch> prefBranch;
    GetPrefBranch(getter_AddRefs(prefBranch));
    if (prefBranch) {
        prefBranch->AddObserver(PORT_PREF_PREFIX, this, true);
        prefBranch->AddObserver(AUTODIAL_PREF, this, true);
        prefBranch->AddObserver(MANAGE_OFFLINE_STATUS_PREF, this, true);
        prefBranch->AddObserver(NECKO_BUFFER_CACHE_COUNT_PREF, this, true);
        prefBranch->AddObserver(NECKO_BUFFER_CACHE_SIZE_PREF, this, true);
        prefBranch->AddObserver(NETWORK_NOTIFY_CHANGED_PREF, this, true);
        prefBranch->AddObserver(NETWORK_CAPTIVE_PORTAL_PREF, this, true);
        PrefsChanged(prefBranch);
    }
    
    // Register for profile change notifications
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService) {
        observerService->AddObserver(this, kProfileChangeNetTeardownTopic, true);
        observerService->AddObserver(this, kProfileChangeNetRestoreTopic, true);
        observerService->AddObserver(this, kProfileDoChange, true);
        observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, true);
        observerService->AddObserver(this, NS_NETWORK_LINK_TOPIC, true);
        observerService->AddObserver(this, NS_WIDGET_WAKE_OBSERVER_TOPIC, true);
        observerService->AddObserver(this, kNetworkActiveChanged, true);
    }
    else
        NS_WARNING("failed to get observer service");

    Preferences::AddBoolVarCache(&sTelemetryEnabled, "toolkit.telemetry.enabled", false);
    Preferences::AddBoolVarCache(&mOfflineMirrorsConnectivity, OFFLINE_MIRRORS_CONNECTIVITY, true);

    gIOService = this;

    InitializeNetworkLinkService();
    SetOffline(false);

    return NS_OK;
}


nsIOService::~nsIOService()
{
    gIOService = nullptr;
}

nsresult
nsIOService::InitializeCaptivePortalService()
{
    if (XRE_GetProcessType() != GeckoProcessType_Default) {
        // We only initalize a captive portal service in the main process
        return NS_OK;
    }

    mCaptivePortalService = do_GetService(NS_CAPTIVEPORTAL_CID);
    if (mCaptivePortalService) {
        return static_cast<CaptivePortalService*>(mCaptivePortalService.get())->Initialize();
    }

    return NS_OK;
}

nsresult
nsIOService::InitializeSocketTransportService()
{
    nsresult rv = NS_OK;

    if (!mSocketTransportService) {
        mSocketTransportService = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
        if (NS_FAILED(rv)) {
            NS_WARNING("failed to get socket transport service");
        }
    }

    if (mSocketTransportService) {
        rv = mSocketTransportService->Init();
        NS_ASSERTION(NS_SUCCEEDED(rv), "socket transport service init failed");
        mSocketTransportService->SetAutodialEnabled(mAutoDialEnabled);
        mSocketTransportService->SetOffline(false);
    }

    return rv;
}

nsresult
nsIOService::InitializeNetworkLinkService()
{
    nsresult rv = NS_OK;

    if (mNetworkLinkServiceInitialized)
        return rv;

    if (!NS_IsMainThread()) {
        NS_WARNING("Network link service should be created on main thread"); 
        return NS_ERROR_FAILURE; 
    }

    // go into managed mode if we can, and chrome process
    if (XRE_IsParentProcess())
    {
        mNetworkLinkService = do_GetService(NS_NETWORK_LINK_SERVICE_CONTRACTID, &rv);
    }

    if (mNetworkLinkService) {
        mNetworkLinkServiceInitialized = true;
    }

    // After initializing the networkLinkService, query the connectivity state
    OnNetworkLinkEvent(NS_NETWORK_LINK_DATA_UNKNOWN);

    return rv;
}

nsIOService*
nsIOService::GetInstance() {
    if (!gIOService) {
        gIOService = new nsIOService();
        if (!gIOService)
            return nullptr;
        NS_ADDREF(gIOService);

        nsresult rv = gIOService->Init();
        if (NS_FAILED(rv)) {
            NS_RELEASE(gIOService);
            return nullptr;
        }
        return gIOService;
    }
    NS_ADDREF(gIOService);
    return gIOService;
}

NS_IMPL_ISUPPORTS(nsIOService,
                  nsIIOService,
                  nsIIOService2,
                  nsINetUtil,
                  nsISpeculativeConnect,
                  nsIObserver,
                  nsIIOServiceInternal,
                  nsISupportsWeakReference)

////////////////////////////////////////////////////////////////////////////////

nsresult
nsIOService::RecheckCaptivePortalIfLocalRedirect(nsIChannel* newChan)
{
    nsresult rv;

    if (!mCaptivePortalService) {
        return NS_OK;
    }

    nsCOMPtr<nsIURI> uri;
    rv = newChan->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) {
        return rv;
    }

    nsCString host;
    rv = uri->GetHost(host);
    if (NS_FAILED(rv)) {
        return rv;
    }

    PRNetAddr prAddr;
    if (PR_StringToNetAddr(host.BeginReading(), &prAddr) != PR_SUCCESS) {
        // The redirect wasn't to an IP literal, so there's probably no need
        // to trigger the captive portal detection right now. It can wait.
        return NS_OK;
    }

    mozilla::net::NetAddr netAddr;
    PRNetAddrToNetAddr(&prAddr, &netAddr);
    if (IsIPAddrLocal(&netAddr)) {
        // Redirects to local IP addresses are probably captive portals
        mCaptivePortalService->RecheckCaptivePortal();
    }

    return NS_OK;
}

nsresult
nsIOService::AsyncOnChannelRedirect(nsIChannel* oldChan, nsIChannel* newChan,
                                    uint32_t flags,
                                    nsAsyncRedirectVerifyHelper *helper)
{
    // If a redirect to a local network address occurs, then chances are we
    // are in a captive portal, so we trigger a recheck.
    RecheckCaptivePortalIfLocalRedirect(newChan);

    nsCOMPtr<nsIChannelEventSink> sink =
        do_GetService(NS_GLOBAL_CHANNELEVENTSINK_CONTRACTID);
    if (sink) {
        nsresult rv = helper->DelegateOnChannelRedirect(sink, oldChan,
                                                        newChan, flags);
        if (NS_FAILED(rv))
            return rv;
    }

    // Finally, our category
    nsCOMArray<nsIChannelEventSink> entries;
    mChannelEventSinks.GetEntries(entries);
    int32_t len = entries.Count();
    for (int32_t i = 0; i < len; ++i) {
        nsresult rv = helper->DelegateOnChannelRedirect(entries[i], oldChan,
                                                        newChan, flags);
        if (NS_FAILED(rv))
            return rv;
    }
    return NS_OK;
}

nsresult
nsIOService::CacheProtocolHandler(const char *scheme, nsIProtocolHandler *handler)
{
    for (unsigned int i=0; i<NS_N(gScheme); i++)
    {
        if (!nsCRT::strcasecmp(scheme, gScheme[i]))
        {
            nsresult rv;
            NS_ASSERTION(!mWeakHandler[i], "Protocol handler already cached");
            // Make sure the handler supports weak references.
            nsCOMPtr<nsISupportsWeakReference> factoryPtr = do_QueryInterface(handler, &rv);
            if (!factoryPtr)
            {
                // Don't cache handlers that don't support weak reference as
                // there is real danger of a circular reference.
#ifdef DEBUG_dp
                printf("DEBUG: %s protcol handler doesn't support weak ref. Not cached.\n", scheme);
#endif /* DEBUG_dp */
                return NS_ERROR_FAILURE;
            }
            mWeakHandler[i] = do_GetWeakReference(handler);
            return NS_OK;
        }
    }
    return NS_ERROR_FAILURE;
}

nsresult
nsIOService::GetCachedProtocolHandler(const char *scheme, nsIProtocolHandler **result, uint32_t start, uint32_t end)
{
    uint32_t len = end - start - 1;
    for (unsigned int i=0; i<NS_N(gScheme); i++)
    {
        if (!mWeakHandler[i])
            continue;

        // handle unterminated strings
        // start is inclusive, end is exclusive, len = end - start - 1
        if (end ? (!nsCRT::strncasecmp(scheme + start, gScheme[i], len)
                   && gScheme[i][len] == '\0')
                : (!nsCRT::strcasecmp(scheme, gScheme[i])))
        {
            return CallQueryReferent(mWeakHandler[i].get(), result);
        }
    }
    return NS_ERROR_FAILURE;
}
 
NS_IMETHODIMP
nsIOService::GetProtocolHandler(const char* scheme, nsIProtocolHandler* *result)
{
    nsresult rv;

    NS_ENSURE_ARG_POINTER(scheme);
    // XXX we may want to speed this up by introducing our own protocol 
    // scheme -> protocol handler mapping, avoiding the string manipulation
    // and service manager stuff

    rv = GetCachedProtocolHandler(scheme, result);
    if (NS_SUCCEEDED(rv))
        return rv;

    bool externalProtocol = false;
    nsCOMPtr<nsIPrefBranch> prefBranch;
    GetPrefBranch(getter_AddRefs(prefBranch));
    if (prefBranch) {
        nsAutoCString externalProtocolPref("network.protocol-handler.external.");
        externalProtocolPref += scheme;
        rv = prefBranch->GetBoolPref(externalProtocolPref.get(), &externalProtocol);
        if (NS_FAILED(rv)) {
            externalProtocol = false;
        }
    }

    if (!externalProtocol) {
        nsAutoCString contractID(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX);
        contractID += scheme;
        ToLowerCase(contractID);

        rv = CallGetService(contractID.get(), result);
        if (NS_SUCCEEDED(rv)) {
            CacheProtocolHandler(scheme, *result);
            return rv;
        }

#ifdef MOZ_ENABLE_GIO
        // check to see whether GVFS can handle this URI scheme.  if it can
        // create a nsIURI for the "scheme:", then we assume it has support for
        // the requested protocol.  otherwise, we failover to using the default
        // protocol handler.

        rv = CallGetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX"moz-gio",
                            result);
        if (NS_SUCCEEDED(rv)) {
            nsAutoCString spec(scheme);
            spec.Append(':');

            nsIURI *uri;
            rv = (*result)->NewURI(spec, nullptr, nullptr, &uri);
            if (NS_SUCCEEDED(rv)) {
                NS_RELEASE(uri);
                return rv;
            }

            NS_RELEASE(*result);
        }
#endif
    }

    // Okay we don't have a protocol handler to handle this url type, so use
    // the default protocol handler.  This will cause urls to get dispatched
    // out to the OS ('cause we can't do anything with them) when we try to
    // read from a channel created by the default protocol handler.

    rv = CallGetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX"default",
                        result);
    if (NS_FAILED(rv))
        return NS_ERROR_UNKNOWN_PROTOCOL;

    return rv;
}

NS_IMETHODIMP
nsIOService::ExtractScheme(const nsACString &inURI, nsACString &scheme)
{
    return net_ExtractURLScheme(inURI, nullptr, nullptr, &scheme);
}

NS_IMETHODIMP 
nsIOService::GetProtocolFlags(const char* scheme, uint32_t *flags)
{
    nsCOMPtr<nsIProtocolHandler> handler;
    nsresult rv = GetProtocolHandler(scheme, getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;

    // We can't call DoGetProtocolFlags here because we don't have a URI. This
    // API is used by (and only used by) extensions, which is why it's still
    // around. Calling this on a scheme with dynamic flags will throw.
    rv = handler->GetProtocolFlags(flags);
    return rv;
}

class AutoIncrement
{
    public:
        explicit AutoIncrement(uint32_t *var) : mVar(var)
        {
            ++*var;
        }
        ~AutoIncrement()
        {
            --*mVar;
        }
    private:
        uint32_t *mVar;
};

nsresult
nsIOService::NewURI(const nsACString &aSpec, const char *aCharset, nsIURI *aBaseURI, nsIURI **result)
{
    NS_ASSERTION(NS_IsMainThread(), "wrong thread");

    static uint32_t recursionCount = 0;
    if (recursionCount >= MAX_RECURSION_COUNT)
        return NS_ERROR_MALFORMED_URI;
    AutoIncrement inc(&recursionCount);

    nsAutoCString scheme;
    nsresult rv = ExtractScheme(aSpec, scheme);
    if (NS_FAILED(rv)) {
        // then aSpec is relative
        if (!aBaseURI)
            return NS_ERROR_MALFORMED_URI;

        rv = aBaseURI->GetScheme(scheme);
        if (NS_FAILED(rv)) return rv;
    }

    // now get the handler for this scheme
    nsCOMPtr<nsIProtocolHandler> handler;
    rv = GetProtocolHandler(scheme.get(), getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;

    return handler->NewURI(aSpec, aCharset, aBaseURI, result);
}


NS_IMETHODIMP 
nsIOService::NewFileURI(nsIFile *file, nsIURI **result)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(file);

    nsCOMPtr<nsIProtocolHandler> handler;

    rv = GetProtocolHandler("file", getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIFileProtocolHandler> fileHandler( do_QueryInterface(handler, &rv) );
    if (NS_FAILED(rv)) return rv;
    
    return fileHandler->NewFileURI(file, result);
}

NS_IMETHODIMP
nsIOService::NewChannelFromURI2(nsIURI* aURI,
                                nsIDOMNode* aLoadingNode,
                                nsIPrincipal* aLoadingPrincipal,
                                nsIPrincipal* aTriggeringPrincipal,
                                uint32_t aSecurityFlags,
                                uint32_t aContentPolicyType,
                                nsIChannel** result)
{
    return NewChannelFromURIWithProxyFlags2(aURI,
                                            nullptr, // aProxyURI
                                            0,       // aProxyFlags
                                            aLoadingNode,
                                            aLoadingPrincipal,
                                            aTriggeringPrincipal,
                                            aSecurityFlags,
                                            aContentPolicyType,
                                            result);
}

NS_IMETHODIMP
nsIOService::NewChannelFromURIWithLoadInfo(nsIURI* aURI,
                                           nsILoadInfo* aLoadInfo,
                                           nsIChannel** result)
{
  NS_ENSURE_ARG_POINTER(aLoadInfo);
  return NewChannelFromURIWithProxyFlagsInternal(aURI,
                                                 nullptr, // aProxyURI
                                                 0,       // aProxyFlags
                                                 aLoadInfo,
                                                 result);
}

/*  ***** DEPRECATED *****
 * please use NewChannelFromURI2 providing the right arguments for:
 *        * aLoadingNode
 *        * aLoadingPrincipal
 *        * aTriggeringPrincipal
 *        * aSecurityFlags
 *        * aContentPolicyType
 *
 * See nsIIoService.idl for a detailed description of those arguments
 */
NS_IMETHODIMP
nsIOService::NewChannelFromURI(nsIURI *aURI, nsIChannel **result)
{
  NS_ASSERTION(false, "Deprecated, use NewChannelFromURI2 providing loadInfo arguments!");
  return NewChannelFromURI2(aURI,
                            nullptr, // aLoadingNode
                            nullptr, // aLoadingPrincipal
                            nullptr, // aTriggeringPrincipal
                            nsILoadInfo::SEC_NORMAL,
                            nsIContentPolicy::TYPE_OTHER,
                            result);
}

nsresult
nsIOService::NewChannelFromURIWithProxyFlagsInternal(nsIURI* aURI,
                                                     nsIURI* aProxyURI,
                                                     uint32_t aProxyFlags,
                                                     nsILoadInfo* aLoadInfo,
                                                     nsIChannel** result)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(aURI);

    nsAutoCString scheme;
    rv = aURI->GetScheme(scheme);
    if (NS_FAILED(rv))
        return rv;

    if (sTelemetryEnabled) {
        nsAutoCString path;
        aURI->GetPath(path);

        bool endsInExcl = StringEndsWith(path, NS_LITERAL_CSTRING("!"));
        int32_t bangSlashPos = path.Find("!/");

        bool hasBangSlash = bangSlashPos != kNotFound;
        bool hasBangDoubleSlash = false;

        if (bangSlashPos != kNotFound) {
            nsDependentCSubstring substr(path, bangSlashPos);
            hasBangDoubleSlash = StringBeginsWith(substr, NS_LITERAL_CSTRING("!//"));
        }

        Telemetry::Accumulate(Telemetry::URL_PATH_ENDS_IN_EXCLAMATION, endsInExcl);
        Telemetry::Accumulate(Telemetry::URL_PATH_CONTAINS_EXCLAMATION_SLASH,
                              hasBangSlash);
        Telemetry::Accumulate(Telemetry::URL_PATH_CONTAINS_EXCLAMATION_DOUBLE_SLASH,
                              hasBangDoubleSlash);
    }

    nsCOMPtr<nsIProtocolHandler> handler;
    rv = GetProtocolHandler(scheme.get(), getter_AddRefs(handler));
    if (NS_FAILED(rv))
        return rv;

    uint32_t protoFlags;
    rv = handler->DoGetProtocolFlags(aURI, &protoFlags);
    if (NS_FAILED(rv))
        return rv;

    // Ideally we are creating new channels by calling NewChannel2 (NewProxiedChannel2).
    // Keep in mind that Addons can implement their own Protocolhandlers, hence
    // NewChannel2() might *not* be implemented.
    // We do not want to break those addons, therefore we first try to create a channel
    // calling NewChannel2(); if that fails:
    // * we fall back to creating a channel by calling NewChannel()
    // * wrap the addon channel
    // * and attach the loadInfo to the channel wrapper
    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsIProxiedProtocolHandler> pph = do_QueryInterface(handler);
    if (pph) {
        rv = pph->NewProxiedChannel2(aURI, nullptr, aProxyFlags, aProxyURI,
                                     aLoadInfo, getter_AddRefs(channel));
        // if calling NewProxiedChannel2() fails we try to fall back to
        // creating a new proxied channel by calling NewProxiedChannel().
        if (NS_FAILED(rv)) {
            rv = pph->NewProxiedChannel(aURI, nullptr, aProxyFlags, aProxyURI,
                                        getter_AddRefs(channel));
            NS_ENSURE_SUCCESS(rv, rv);
            // we have to wrap that channel
            channel = new nsSecCheckWrapChannel(channel, aLoadInfo);
        }
    }
    else {
        rv = handler->NewChannel2(aURI, aLoadInfo, getter_AddRefs(channel));
        // if calling newChannel2() fails we try to fall back to
        // creating a new channel by calling NewChannel().
        if (NS_FAILED(rv)) {
            rv = handler->NewChannel(aURI, getter_AddRefs(channel));
            NS_ENSURE_SUCCESS(rv, rv);
            // we have to wrap that channel
            channel = new nsSecCheckWrapChannel(channel, aLoadInfo);
        }
    }

    // Make sure that all the individual protocolhandlers attach a loadInfo.
    if (aLoadInfo) {
      // make sure we have the same instance of loadInfo on the newly created channel
      nsCOMPtr<nsILoadInfo> loadInfo;
      channel->GetLoadInfo(getter_AddRefs(loadInfo));

      if (aLoadInfo != loadInfo) {
        MOZ_ASSERT(false, "newly created channel must have a loadinfo attached");
        return NS_ERROR_UNEXPECTED;
      }

      // If we're sandboxed, make sure to clear any owner the channel
      // might already have.
      if (loadInfo->GetLoadingSandboxed()) {
        channel->SetOwner(nullptr);
      }
    }

    // Some extensions override the http protocol handler and provide their own
    // implementation. The channels returned from that implementation doesn't
    // seem to always implement the nsIUploadChannel2 interface, presumably
    // because it's a new interface.
    // Eventually we should remove this and simply require that http channels
    // implement the new interface.
    // See bug 529041
    if (!gHasWarnedUploadChannel2 && scheme.EqualsLiteral("http")) {
        nsCOMPtr<nsIUploadChannel2> uploadChannel2 = do_QueryInterface(channel);
        if (!uploadChannel2) {
            nsCOMPtr<nsIConsoleService> consoleService =
                do_GetService(NS_CONSOLESERVICE_CONTRACTID);
            if (consoleService) {
                consoleService->LogStringMessage(NS_LITERAL_STRING(
                    "Http channel implementation doesn't support nsIUploadChannel2. An extension has supplied a non-functional http protocol handler. This will break behavior and in future releases not work at all."
                                                                   ).get());
            }
            gHasWarnedUploadChannel2 = true;
        }
    }

    channel.forget(result);
    return NS_OK;
}

NS_IMETHODIMP
nsIOService::NewChannelFromURIWithProxyFlags2(nsIURI* aURI,
                                              nsIURI* aProxyURI,
                                              uint32_t aProxyFlags,
                                              nsIDOMNode* aLoadingNode,
                                              nsIPrincipal* aLoadingPrincipal,
                                              nsIPrincipal* aTriggeringPrincipal,
                                              uint32_t aSecurityFlags,
                                              uint32_t aContentPolicyType,
                                              nsIChannel** result)
{
    // Ideally all callers of NewChannelFromURIWithProxyFlags2 provide the
    // necessary arguments to create a loadinfo. Keep in mind that addons
    // might still call NewChannelFromURIWithProxyFlags() which forwards
    // its calls to NewChannelFromURIWithProxyFlags2 using *null* values
    // as the arguments for aLoadingNode, aLoadingPrincipal, and also
    // aTriggeringPrincipal.
    // We do not want to break those addons, hence we only create a Loadinfo
    // if 'aLoadingNode' or 'aLoadingPrincipal' are provided. Note, that
    // either aLoadingNode or aLoadingPrincipal is required to succesfully
    // create a LoadInfo object.
    nsCOMPtr<nsILoadInfo> loadInfo;

    if (aLoadingNode || aLoadingPrincipal) {
      nsCOMPtr<nsINode> loadingNode(do_QueryInterface(aLoadingNode));
      loadInfo = new mozilla::LoadInfo(aLoadingPrincipal,
                                       aTriggeringPrincipal,
                                       loadingNode,
                                       aSecurityFlags,
                                       aContentPolicyType);
    }
    NS_ASSERTION(loadInfo, "Please pass security info when creating a channel");
    return NewChannelFromURIWithProxyFlagsInternal(aURI,
                                                   aProxyURI,
                                                   aProxyFlags,
                                                   loadInfo,
                                                   result);
}

/*  ***** DEPRECATED *****
 * please use NewChannelFromURIWithProxyFlags2 providing the right arguments for:
 *        * aLoadingNode
 *        * aLoadingPrincipal
 *        * aTriggeringPrincipal
 *        * aSecurityFlags
 *        * aContentPolicyType
 *
 * See nsIIoService.idl for a detailed description of those arguments
 */
NS_IMETHODIMP
nsIOService::NewChannelFromURIWithProxyFlags(nsIURI *aURI,
                                             nsIURI *aProxyURI,
                                             uint32_t aProxyFlags,
                                             nsIChannel **result)
{
  NS_ASSERTION(false, "Deprecated, use NewChannelFromURIWithProxyFlags2 providing loadInfo arguments!");
  return NewChannelFromURIWithProxyFlags2(aURI,
                                          aProxyURI,
                                          aProxyFlags,
                                          nullptr, // aLoadingNode
                                          nullptr, // aLoadingPrincipal
                                          nullptr, // aTriggeringPrincipal
                                          nsILoadInfo::SEC_NORMAL,
                                          nsIContentPolicy::TYPE_OTHER,
                                          result);
}

NS_IMETHODIMP
nsIOService::NewChannel2(const nsACString& aSpec,
                         const char* aCharset,
                         nsIURI* aBaseURI,
                         nsIDOMNode* aLoadingNode,
                         nsIPrincipal* aLoadingPrincipal,
                         nsIPrincipal* aTriggeringPrincipal,
                         uint32_t aSecurityFlags,
                         uint32_t aContentPolicyType,
                         nsIChannel** result)
{
    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    rv = NewURI(aSpec, aCharset, aBaseURI, getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    return NewChannelFromURI2(uri,
                              aLoadingNode,
                              aLoadingPrincipal,
                              aTriggeringPrincipal,
                              aSecurityFlags,
                              aContentPolicyType,
                              result);
}

/*  ***** DEPRECATED *****
 * please use NewChannel2 providing the right arguments for:
 *        * aLoadingNode
 *        * aLoadingPrincipal
 *        * aTriggeringPrincipal
 *        * aSecurityFlags
 *        * aContentPolicyType
 *
 * See nsIIoService.idl for a detailed description of those arguments
 */
NS_IMETHODIMP
nsIOService::NewChannel(const nsACString &aSpec, const char *aCharset, nsIURI *aBaseURI, nsIChannel **result)
{
  NS_ASSERTION(false, "Deprecated, use NewChannel2 providing loadInfo arguments!");
  return NewChannel2(aSpec,
                     aCharset,
                     aBaseURI,
                     nullptr, // aLoadingNode
                     nullptr, // aLoadingPrincipal
                     nullptr, // aTriggeringPrincipal
                     nsILoadInfo::SEC_NORMAL,
                     nsIContentPolicy::TYPE_OTHER,
                     result);
}

bool
nsIOService::IsLinkUp()
{
    InitializeNetworkLinkService();

    if (!mNetworkLinkService) {
        // We cannot decide, assume the link is up
        return true;
    }

    bool isLinkUp;
    nsresult rv;
    rv = mNetworkLinkService->GetIsLinkUp(&isLinkUp);
    if (NS_FAILED(rv)) {
        return true;
    }

    return isLinkUp;
}

NS_IMETHODIMP
nsIOService::GetOffline(bool *offline)
{
    if (mOfflineMirrorsConnectivity) {
        *offline = mOffline || !mConnectivity;
    } else {
        *offline = mOffline;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsIOService::SetOffline(bool offline)
{
    // When someone wants to go online (!offline) after we got XPCOM shutdown
    // throw ERROR_NOT_AVAILABLE to prevent return to online state.
    if ((mShutdown || mOfflineForProfileChange) && !offline)
        return NS_ERROR_NOT_AVAILABLE;

    // SetOffline() may re-enter while it's shutting down services.
    // If that happens, save the most recent value and it will be
    // processed when the first SetOffline() call is done bringing
    // down the service.
    mSetOfflineValue = offline;
    if (mSettingOffline) {
        return NS_OK;
    }

    mSettingOffline = true;

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();

    NS_ASSERTION(observerService, "The observer service should not be null");

    if (XRE_IsParentProcess()) {
        if (observerService) {
            (void)observerService->NotifyObservers(nullptr,
                NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC, offline ? 
                MOZ_UTF16("true") :
                MOZ_UTF16("false"));
        }
    }

    nsIIOService *subject = static_cast<nsIIOService *>(this);
    while (mSetOfflineValue != mOffline) {
        offline = mSetOfflineValue;

        if (offline && !mOffline) {
            NS_NAMED_LITERAL_STRING(offlineString, NS_IOSERVICE_OFFLINE);
            mOffline = true; // indicate we're trying to shutdown

            // don't care if notifications fail
            if (observerService)
                observerService->NotifyObservers(subject,
                                                 NS_IOSERVICE_GOING_OFFLINE_TOPIC,
                                                 offlineString.get());

            if (mDNSService)
                mDNSService->SetOffline(true);

            if (mSocketTransportService)
                mSocketTransportService->SetOffline(true);

            mLastOfflineStateChange = PR_IntervalNow();
            if (observerService)
                observerService->NotifyObservers(subject,
                                                 NS_IOSERVICE_OFFLINE_STATUS_TOPIC,
                                                 offlineString.get());
        }
        else if (!offline && mOffline) {
            // go online
            if (mDNSService) {
                mDNSService->SetOffline(false);
                DebugOnly<nsresult> rv = mDNSService->Init();
                NS_ASSERTION(NS_SUCCEEDED(rv), "DNS service init failed");
            }
            InitializeSocketTransportService();
            mOffline = false;    // indicate success only AFTER we've
                                    // brought up the services

            // trigger a PAC reload when we come back online
            if (mProxyService)
                mProxyService->ReloadPAC();

            mLastOfflineStateChange = PR_IntervalNow();
            // don't care if notification fails
            // Only send the ONLINE notification if there is connectivity
            if (observerService && mConnectivity) {
                observerService->NotifyObservers(subject,
                                                 NS_IOSERVICE_OFFLINE_STATUS_TOPIC,
                                                 NS_LITERAL_STRING(NS_IOSERVICE_ONLINE).get());
            }
        }
    }

    // Don't notify here, as the above notifications (if used) suffice.
    if ((mShutdown || mOfflineForProfileChange) && mOffline) {
        // be sure to try and shutdown both (even if the first fails)...
        // shutdown dns service first, because it has callbacks for socket transport
        if (mDNSService) {
            DebugOnly<nsresult> rv = mDNSService->Shutdown();
            NS_ASSERTION(NS_SUCCEEDED(rv), "DNS service shutdown failed");
        }
        if (mSocketTransportService) {
            DebugOnly<nsresult> rv = mSocketTransportService->Shutdown();
            NS_ASSERTION(NS_SUCCEEDED(rv), "socket transport service shutdown failed");
        }
    }

    mSettingOffline = false;

    return NS_OK;
}

NS_IMETHODIMP
nsIOService::GetConnectivity(bool *aConnectivity)
{
    *aConnectivity = mConnectivity;
    return NS_OK;
}

NS_IMETHODIMP
nsIOService::SetConnectivity(bool aConnectivity)
{
    // This should only be called from ContentChild to pass the connectivity
    // value from the chrome process to the content process.
    if (XRE_IsParentProcess()) {
        return NS_ERROR_NOT_AVAILABLE;
    }
    return SetConnectivityInternal(aConnectivity);
}


nsresult
nsIOService::SetConnectivityInternal(bool aConnectivity)
{
    if (mConnectivity == aConnectivity) {
        // Nothing to do here.
        return NS_OK;
    }
    mConnectivity = aConnectivity;

    // This is used for PR_Connect PR_Close telemetry so it is important that
    // we have statistic about network change event even if we are offline.
    mLastConnectivityChange = PR_IntervalNow();

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (!observerService) {
        return NS_OK;
    }
    // This notification sends the connectivity to the child processes
    if (XRE_IsParentProcess()) {
        observerService->NotifyObservers(nullptr,
            NS_IPC_IOSERVICE_SET_CONNECTIVITY_TOPIC, aConnectivity ?
            MOZ_UTF16("true") :
            MOZ_UTF16("false"));
    }

    if (mOffline) {
      // We don't need to send any notifications if we're offline
      return NS_OK;
    }

    if (aConnectivity) {
        // If we were previously offline due to connectivity=false,
        // send the ONLINE notification
        observerService->NotifyObservers(
            static_cast<nsIIOService *>(this),
            NS_IOSERVICE_OFFLINE_STATUS_TOPIC,
            NS_LITERAL_STRING(NS_IOSERVICE_ONLINE).get());
    } else {
        // If we were previously online and lost connectivity
        // send the OFFLINE notification
        const nsLiteralString offlineString(MOZ_UTF16(NS_IOSERVICE_OFFLINE));
        observerService->NotifyObservers(static_cast<nsIIOService *>(this),
                                         NS_IOSERVICE_GOING_OFFLINE_TOPIC,
                                         offlineString.get());
        observerService->NotifyObservers(static_cast<nsIIOService *>(this),
                                         NS_IOSERVICE_OFFLINE_STATUS_TOPIC,
                                         offlineString.get());
    }
    return NS_OK;
}

NS_IMETHODIMP
nsIOService::AllowPort(int32_t inPort, const char *scheme, bool *_retval)
{
    int16_t port = inPort;
    if (port == -1) {
        *_retval = true;
        return NS_OK;
    }

    if (port == 0) {
        *_retval = false;
        return NS_OK;
    }
        
    // first check to see if the port is in our blacklist:
    int32_t badPortListCnt = mRestrictedPortList.Length();
    for (int i=0; i<badPortListCnt; i++)
    {
        if (port == mRestrictedPortList[i])
        {
            *_retval = false;

            // check to see if the protocol wants to override
            if (!scheme)
                return NS_OK;
            
            nsCOMPtr<nsIProtocolHandler> handler;
            nsresult rv = GetProtocolHandler(scheme, getter_AddRefs(handler));
            if (NS_FAILED(rv)) return rv;

            // let the protocol handler decide
            return handler->AllowPort(port, scheme, _retval);
        }
    }

    *_retval = true;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

void
nsIOService::PrefsChanged(nsIPrefBranch *prefs, const char *pref)
{
    if (!prefs) return;

    // Look for extra ports to block
    if (!pref || strcmp(pref, PORT_PREF("banned")) == 0)
        ParsePortList(prefs, PORT_PREF("banned"), false);

    // ...as well as previous blocks to remove.
    if (!pref || strcmp(pref, PORT_PREF("banned.override")) == 0)
        ParsePortList(prefs, PORT_PREF("banned.override"), true);

    if (!pref || strcmp(pref, AUTODIAL_PREF) == 0) {
        bool enableAutodial = false;
        nsresult rv = prefs->GetBoolPref(AUTODIAL_PREF, &enableAutodial);
        // If pref not found, default to disabled.
        mAutoDialEnabled = enableAutodial;
        if (NS_SUCCEEDED(rv)) {
            if (mSocketTransportService)
                mSocketTransportService->SetAutodialEnabled(enableAutodial);
        }
    }

    if (!pref || strcmp(pref, MANAGE_OFFLINE_STATUS_PREF) == 0) {
        bool manage;
        if (mNetworkLinkServiceInitialized &&
            NS_SUCCEEDED(prefs->GetBoolPref(MANAGE_OFFLINE_STATUS_PREF,
                                            &manage)))
            SetManageOfflineStatus(manage);
    }

    if (!pref || strcmp(pref, NECKO_BUFFER_CACHE_COUNT_PREF) == 0) {
        int32_t count;
        if (NS_SUCCEEDED(prefs->GetIntPref(NECKO_BUFFER_CACHE_COUNT_PREF,
                                           &count)))
            /* check for bogus values and default if we find such a value */
            if (count > 0)
                gDefaultSegmentCount = count;
    }
    
    if (!pref || strcmp(pref, NECKO_BUFFER_CACHE_SIZE_PREF) == 0) {
        int32_t size;
        if (NS_SUCCEEDED(prefs->GetIntPref(NECKO_BUFFER_CACHE_SIZE_PREF,
                                           &size)))
            /* check for bogus values and default if we find such a value
             * the upper limit here is arbitrary. having a 1mb segment size
             * is pretty crazy.  if you remove this, consider adding some
             * integer rollover test.
             */
            if (size > 0 && size < 1024*1024)
                gDefaultSegmentSize = size;
        NS_WARN_IF_FALSE( (!(size & (size - 1))) , "network segment size is not a power of 2!");
    }

    if (!pref || strcmp(pref, NETWORK_NOTIFY_CHANGED_PREF) == 0) {
        bool allow;
        nsresult rv = prefs->GetBoolPref(NETWORK_NOTIFY_CHANGED_PREF, &allow);
        if (NS_SUCCEEDED(rv)) {
            mNetworkNotifyChanged = allow;
        }
    }

    if (!pref || strcmp(pref, NETWORK_CAPTIVE_PORTAL_PREF) == 0) {
        static int disabledForTest = -1;
        if (disabledForTest == -1) {
            char *s = getenv("MOZ_DISABLE_NONLOCAL_CONNECTIONS");
            if (s) {
                disabledForTest = (strncmp(s, "0", 1) == 0) ? 0 : 1;
            } else {
                disabledForTest = 0;
            }
        }

        bool captivePortalEnabled;
        nsresult rv = prefs->GetBoolPref(NETWORK_CAPTIVE_PORTAL_PREF, &captivePortalEnabled);
        if (NS_SUCCEEDED(rv) && mCaptivePortalService) {
            if (captivePortalEnabled && !disabledForTest) {
                static_cast<CaptivePortalService*>(mCaptivePortalService.get())->Start();
            } else {
                static_cast<CaptivePortalService*>(mCaptivePortalService.get())->Stop();
            }
        }
    }
}

void
nsIOService::ParsePortList(nsIPrefBranch *prefBranch, const char *pref, bool remove)
{
    nsXPIDLCString portList;

    // Get a pref string and chop it up into a list of ports.
    prefBranch->GetCharPref(pref, getter_Copies(portList));
    if (portList) {
        nsTArray<nsCString> portListArray;
        ParseString(portList, ',', portListArray);
        uint32_t index;
        for (index=0; index < portListArray.Length(); index++) {
            portListArray[index].StripWhitespace();
            int32_t portBegin, portEnd;

            if (PR_sscanf(portListArray[index].get(), "%d-%d", &portBegin, &portEnd) == 2) {
               if ((portBegin < 65536) && (portEnd < 65536)) {
                   int32_t curPort;
                   if (remove) {
                        for (curPort=portBegin; curPort <= portEnd; curPort++)
                            mRestrictedPortList.RemoveElement(curPort);
                   } else {
                        for (curPort=portBegin; curPort <= portEnd; curPort++)
                            mRestrictedPortList.AppendElement(curPort);
                   }
               }
            } else {
               nsresult aErrorCode;
               int32_t port = portListArray[index].ToInteger(&aErrorCode);
               if (NS_SUCCEEDED(aErrorCode) && port < 65536) {
                   if (remove)
                       mRestrictedPortList.RemoveElement(port);
                   else
                       mRestrictedPortList.AppendElement(port);
               }
            }

        }
    }
}

void
nsIOService::GetPrefBranch(nsIPrefBranch **result)
{
    *result = nullptr;
    CallGetService(NS_PREFSERVICE_CONTRACTID, result);
}

// This returns true if wifi-only apps should have connectivity.
// Always returns false in the child process (should not depend on this method)
static bool
IsWifiActive()
{
    // We don't need to do this check inside the child process
    if (IsNeckoChild()) {
        return false;
    }
#ifdef MOZ_WIDGET_GONK
    // On B2G we query the network manager for the active interface
    nsCOMPtr<nsINetworkManager> networkManager =
        do_GetService("@mozilla.org/network/manager;1");
    if (!networkManager) {
        return false;
    }
    nsCOMPtr<nsINetworkInfo> activeNetworkInfo;
    networkManager->GetActiveNetworkInfo(getter_AddRefs(activeNetworkInfo));
    if (!activeNetworkInfo) {
        return false;
    }
    int32_t type;
    if (NS_FAILED(activeNetworkInfo->GetType(&type))) {
        return false;
    }
    switch (type) {
    case nsINetworkInfo::NETWORK_TYPE_WIFI:
    case nsINetworkInfo::NETWORK_TYPE_WIFI_P2P:
        return true;
    default:
        return false;
    }
#else
    // On anything else than B2G we return true so than wifi-only
    // apps don't think they are offline.
    return true;
#endif
}

struct EnumeratorParams {
    nsIOService *service;
    int32_t     status;
};

PLDHashOperator
nsIOService::EnumerateWifiAppsChangingState(const unsigned int &aKey,
                                            int32_t aValue,
                                            void *aUserArg)
{
    EnumeratorParams *params = reinterpret_cast<EnumeratorParams*>(aUserArg);
    if (aValue == nsIAppOfflineInfo::WIFI_ONLY) {
        params->service->NotifyAppOfflineStatus(aKey, params->status);
    }
    return PL_DHASH_NEXT;
}

class
nsWakeupNotifier : public nsRunnable
{
public:
    explicit nsWakeupNotifier(nsIIOServiceInternal *ioService)
        :mIOService(ioService)
    { }

    NS_IMETHOD Run()
    {
        return mIOService->NotifyWakeup();
    }

private:
    virtual ~nsWakeupNotifier() { }
    nsCOMPtr<nsIIOServiceInternal> mIOService;
};

NS_IMETHODIMP
nsIOService::NotifyWakeup()
{
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();

    NS_ASSERTION(observerService, "The observer service should not be null");

    if (observerService && mNetworkNotifyChanged) {
        (void)observerService->
            NotifyObservers(nullptr,
                            NS_NETWORK_LINK_TOPIC,
                            MOZ_UTF16(NS_NETWORK_LINK_DATA_CHANGED));
    }

    if (mCaptivePortalService) {
        mCaptivePortalService->RecheckCaptivePortal();
    }

    return NS_OK;
}

// nsIObserver interface
NS_IMETHODIMP
nsIOService::Observe(nsISupports *subject,
                     const char *topic,
                     const char16_t *data)
{
    if (!strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
        nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(subject);
        if (prefBranch)
            PrefsChanged(prefBranch, NS_ConvertUTF16toUTF8(data).get());
    } else if (!strcmp(topic, kProfileChangeNetTeardownTopic)) {
        if (!mOffline) {
            mOfflineForProfileChange = true;
            SetOffline(true);
        }
    } else if (!strcmp(topic, kProfileChangeNetRestoreTopic)) {
        if (mOfflineForProfileChange) {
            mOfflineForProfileChange = false;
            SetOffline(false);
        }
    } else if (!strcmp(topic, kProfileDoChange)) { 
        if (data && NS_LITERAL_STRING("startup").Equals(data)) {
            // Lazy initialization of network link service (see bug 620472)
            InitializeNetworkLinkService();
            // Set up the initilization flag regardless the actuall result.
            // If we fail here, we will fail always on.
            mNetworkLinkServiceInitialized = true;

            // And now reflect the preference setting
            nsCOMPtr<nsIPrefBranch> prefBranch;
            GetPrefBranch(getter_AddRefs(prefBranch));
            PrefsChanged(prefBranch, MANAGE_OFFLINE_STATUS_PREF);
        }
    } else if (!strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
        // Remember we passed XPCOM shutdown notification to prevent any
        // changes of the offline status from now. We must not allow going
        // online after this point.
        mShutdown = true;

        SetOffline(true);

        if (mCaptivePortalService) {
            static_cast<CaptivePortalService*>(mCaptivePortalService.get())->Stop();
            mCaptivePortalService = nullptr;
        }

        // Break circular reference.
        mProxyService = nullptr;
    } else if (!strcmp(topic, NS_NETWORK_LINK_TOPIC)) {
        OnNetworkLinkEvent(NS_ConvertUTF16toUTF8(data).get());
    } else if (!strcmp(topic, NS_WIDGET_WAKE_OBSERVER_TOPIC)) {
        // coming back alive from sleep
        // this indirection brought to you by:
        // https://bugzilla.mozilla.org/show_bug.cgi?id=1152048#c19
        nsCOMPtr<nsIRunnable> wakeupNotifier = new nsWakeupNotifier(this);
        NS_DispatchToMainThread(wakeupNotifier);
    } else if (!strcmp(topic, kNetworkActiveChanged)) {
#ifdef MOZ_WIDGET_GONK
        if (IsNeckoChild()) {
          return NS_OK;
        }
        nsCOMPtr<nsINetworkInfo> interface = do_QueryInterface(subject);
        if (!interface) {
            return NS_ERROR_FAILURE;
        }
        int32_t state;
        if (NS_FAILED(interface->GetState(&state))) {
            return NS_ERROR_FAILURE;
        }

        bool wifiActive = IsWifiActive();
        int32_t newWifiState = wifiActive ?
            nsINetworkInfo::NETWORK_TYPE_WIFI :
            nsINetworkInfo::NETWORK_TYPE_MOBILE;
        if (mPreviousWifiState != newWifiState) {
            // Notify wifi-only apps of their new status
            int32_t status = wifiActive ?
                nsIAppOfflineInfo::ONLINE : nsIAppOfflineInfo::OFFLINE;

            EnumeratorParams params = {this, status};
            mAppsOfflineStatus.EnumerateRead(EnumerateWifiAppsChangingState, &params);
        }

        mPreviousWifiState = newWifiState;
#endif
    }

    return NS_OK;
}

// nsINetUtil interface
NS_IMETHODIMP
nsIOService::ParseResponseContentType(const nsACString &aTypeHeader,
                                      nsACString &aCharset,
                                      bool *aHadCharset,
                                      nsACString &aContentType)
{
    net_ParseContentType(aTypeHeader, aContentType, aCharset, aHadCharset);
    return NS_OK;
}

NS_IMETHODIMP
nsIOService::ProtocolHasFlags(nsIURI   *uri,
                              uint32_t  flags,
                              bool     *result)
{
    NS_ENSURE_ARG(uri);

    *result = false;
    nsAutoCString scheme;
    nsresult rv = uri->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);

    // Grab the protocol flags from the URI.
    uint32_t protocolFlags;
    nsCOMPtr<nsIProtocolHandler> handler;
    rv = GetProtocolHandler(scheme.get(), getter_AddRefs(handler));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = handler->DoGetProtocolFlags(uri, &protocolFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    *result = (protocolFlags & flags) == flags;
    return NS_OK;
}

NS_IMETHODIMP
nsIOService::URIChainHasFlags(nsIURI   *uri,
                              uint32_t  flags,
                              bool     *result)
{
    nsresult rv = ProtocolHasFlags(uri, flags, result);
    NS_ENSURE_SUCCESS(rv, rv);

    if (*result) {
        return rv;
    }

    // Dig deeper into the chain.  Note that this is not a do/while loop to
    // avoid the extra addref/release on |uri| in the common (non-nested) case.
    nsCOMPtr<nsINestedURI> nestedURI = do_QueryInterface(uri);
    while (nestedURI) {
        nsCOMPtr<nsIURI> innerURI;
        rv = nestedURI->GetInnerURI(getter_AddRefs(innerURI));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = ProtocolHasFlags(innerURI, flags, result);

        if (*result) {
            return rv;
        }

        nestedURI = do_QueryInterface(innerURI);
    }

    return rv;
}

NS_IMETHODIMP
nsIOService::ToImmutableURI(nsIURI* uri, nsIURI** result)
{
    if (!uri) {
        *result = nullptr;
        return NS_OK;
    }

    nsresult rv = NS_EnsureSafeToReturn(uri, result);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_TryToSetImmutable(*result);
    return NS_OK;
}

NS_IMETHODIMP
nsIOService::NewSimpleNestedURI(nsIURI* aURI, nsIURI** aResult)
{
    NS_ENSURE_ARG(aURI);

    nsCOMPtr<nsIURI> safeURI;
    nsresult rv = NS_EnsureSafeToReturn(aURI, getter_AddRefs(safeURI));
    NS_ENSURE_SUCCESS(rv, rv);

    NS_IF_ADDREF(*aResult = new nsSimpleNestedURI(safeURI));
    return *aResult ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsIOService::SetManageOfflineStatus(bool aManage)
{
    mManageLinkStatus = aManage;

    // When detection is not activated, the default connectivity state is true.
    if (!mManageLinkStatus) {
        SetConnectivityInternal(true);
        return NS_OK;
    }

    InitializeNetworkLinkService();
    // If the NetworkLinkService is already initialized, it does not call
    // OnNetworkLinkEvent. This is needed, when mManageLinkStatus goes from
    // false to true.
    OnNetworkLinkEvent(NS_NETWORK_LINK_DATA_UNKNOWN);
    return NS_OK;
}

NS_IMETHODIMP
nsIOService::GetManageOfflineStatus(bool* aManage)
{
    *aManage = mManageLinkStatus;
    return NS_OK;
}

// input argument 'data' is already UTF8'ed
nsresult
nsIOService::OnNetworkLinkEvent(const char *data)
{
    if (!mNetworkLinkService)
        return NS_ERROR_FAILURE;

    if (mShutdown)
        return NS_ERROR_NOT_AVAILABLE;

    if (!mManageLinkStatus) {
      return NS_OK;
    }

    if (!strcmp(data, NS_NETWORK_LINK_DATA_DOWN)) {
        // check to make sure this won't collide with Autodial
        if (mSocketTransportService) {
            bool autodialEnabled = false;
            mSocketTransportService->GetAutodialEnabled(&autodialEnabled);
            // If autodialing-on-link-down is enabled, check if the OS auto
            // dial option is set to always autodial. If so, then we are
            // always up for the purposes of offline management.
            if (autodialEnabled) {
                bool isUp = true;
#if defined(XP_WIN)
                // On Windows, we should first check with the OS to see if
                // autodial is enabled.  If it is enabled then we are allowed
                // to manage the offline state.
                isUp = nsNativeConnectionHelper::IsAutodialEnabled();
#endif
                return SetConnectivityInternal(isUp);
            }
        }
    }

    bool isUp = true;
    if (!strcmp(data, NS_NETWORK_LINK_DATA_CHANGED)) {
        mLastNetworkLinkChange = PR_IntervalNow();
        // CHANGED means UP/DOWN didn't change
        return NS_OK;
    } else if (!strcmp(data, NS_NETWORK_LINK_DATA_DOWN)) {
        isUp = false;
    } else if (!strcmp(data, NS_NETWORK_LINK_DATA_UP)) {
        if (mCaptivePortalService) {
            // Interface is up. Triggering a captive portal recheck.
            mCaptivePortalService->RecheckCaptivePortal();
        }
        isUp = true;
    } else if (!strcmp(data, NS_NETWORK_LINK_DATA_UNKNOWN)) {
        nsresult rv = mNetworkLinkService->GetIsLinkUp(&isUp);
        NS_ENSURE_SUCCESS(rv, rv);
    } else {
        NS_WARNING("Unhandled network event!");
        return NS_OK;
    }

    return SetConnectivityInternal(isUp);
}

NS_IMETHODIMP
nsIOService::EscapeString(const nsACString& aString,
                          uint32_t aEscapeType,
                          nsACString& aResult)
{
  NS_ENSURE_ARG_MAX(aEscapeType, 4);

  nsAutoCString stringCopy(aString);
  nsCString result;

  if (!NS_Escape(stringCopy, result, (nsEscapeMask) aEscapeType))
    return NS_ERROR_OUT_OF_MEMORY;

  aResult.Assign(result);

  return NS_OK;
}

NS_IMETHODIMP 
nsIOService::EscapeURL(const nsACString &aStr, 
                       uint32_t aFlags, nsACString &aResult)
{
  aResult.Truncate();
  NS_EscapeURL(aStr.BeginReading(), aStr.Length(), 
               aFlags | esc_AlwaysCopy, aResult);
  return NS_OK;
}

NS_IMETHODIMP 
nsIOService::UnescapeString(const nsACString &aStr, 
                            uint32_t aFlags, nsACString &aResult)
{
  aResult.Truncate();
  NS_UnescapeURL(aStr.BeginReading(), aStr.Length(), 
                 aFlags | esc_AlwaysCopy, aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsIOService::ExtractCharsetFromContentType(const nsACString &aTypeHeader,
                                           nsACString &aCharset,
                                           int32_t *aCharsetStart,
                                           int32_t *aCharsetEnd,
                                           bool *aHadCharset)
{
    nsAutoCString ignored;
    net_ParseContentType(aTypeHeader, ignored, aCharset, aHadCharset,
                         aCharsetStart, aCharsetEnd);
    if (*aHadCharset && *aCharsetStart == *aCharsetEnd) {
        *aHadCharset = false;
    }
    return NS_OK;
}

// parse policyString to policy enum value (see ReferrerPolicy.h)
NS_IMETHODIMP
nsIOService::ParseAttributePolicyString(const nsAString& policyString,
                                                uint32_t *outPolicyEnum)
{
  NS_ENSURE_ARG(outPolicyEnum);
  *outPolicyEnum = (uint32_t)mozilla::net::AttributeReferrerPolicyFromString(policyString);
  return NS_OK;
}

// nsISpeculativeConnect
class IOServiceProxyCallback final : public nsIProtocolProxyCallback
{
    ~IOServiceProxyCallback() {}

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLPROXYCALLBACK

    IOServiceProxyCallback(nsIInterfaceRequestor *aCallbacks,
                           nsIOService *aIOService)
        : mCallbacks(aCallbacks)
        , mIOService(aIOService)
    { }

private:
    nsRefPtr<nsIInterfaceRequestor> mCallbacks;
    nsRefPtr<nsIOService>           mIOService;
};

NS_IMPL_ISUPPORTS(IOServiceProxyCallback, nsIProtocolProxyCallback)

NS_IMETHODIMP
IOServiceProxyCallback::OnProxyAvailable(nsICancelable *request, nsIChannel *channel,
                                         nsIProxyInfo *pi, nsresult status)
{
    // Checking proxy status for speculative connect
    nsAutoCString type;
    if (NS_SUCCEEDED(status) && pi &&
        NS_SUCCEEDED(pi->GetType(type)) &&
        !type.EqualsLiteral("direct")) {
        // proxies dont do speculative connect
        return NS_OK;
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv = channel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) {
        return NS_OK;
    }

    nsAutoCString scheme;
    rv = uri->GetScheme(scheme);
    if (NS_FAILED(rv))
        return NS_OK;

    nsCOMPtr<nsIProtocolHandler> handler;
    rv = mIOService->GetProtocolHandler(scheme.get(),
                                        getter_AddRefs(handler));
    if (NS_FAILED(rv))
        return NS_OK;

    nsCOMPtr<nsISpeculativeConnect> speculativeHandler =
        do_QueryInterface(handler);
    if (!speculativeHandler)
        return NS_OK;

    nsLoadFlags loadFlags = 0;
    channel->GetLoadFlags(&loadFlags);
    if (loadFlags & nsIRequest::LOAD_ANONYMOUS) {
        speculativeHandler->SpeculativeAnonymousConnect(uri, mCallbacks);
    } else {
        speculativeHandler->SpeculativeConnect(uri, mCallbacks);
    }

    return NS_OK;
}

nsresult
nsIOService::SpeculativeConnectInternal(nsIURI *aURI,
                                        nsIInterfaceRequestor *aCallbacks,
                                        bool aAnonymous)
{
    // Check for proxy information. If there is a proxy configured then a
    // speculative connect should not be performed because the potential
    // reward is slim with tcp peers closely located to the browser.
    nsresult rv;
    nsCOMPtr<nsIProtocolProxyService> pps =
            do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIScriptSecurityManager> secMan(
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIPrincipal> systemPrincipal;
    rv = secMan->GetSystemPrincipal(getter_AddRefs(systemPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);

    // dummy channel used to create a TCP connection.
    // we perform security checks on the *real* channel, responsible
    // for any network loads. this real channel just checks the TCP
    // pool if there is an available connection created by the
    // channel we create underneath - hence it's safe to use
    // the systemPrincipal as the loadingPrincipal for this channel.
    nsCOMPtr<nsIChannel> channel;
    rv = NewChannelFromURI2(aURI,
                            nullptr, // aLoadingNode,
                            systemPrincipal,
                            nullptr, //aTriggeringPrincipal,
                            nsILoadInfo::SEC_NORMAL,
                            nsIContentPolicy::TYPE_OTHER,
                            getter_AddRefs(channel));
    NS_ENSURE_SUCCESS(rv, rv);

    if (aAnonymous) {
        nsLoadFlags loadFlags = 0;
        channel->GetLoadFlags(&loadFlags);
        loadFlags |= nsIRequest::LOAD_ANONYMOUS;
        channel->SetLoadFlags(loadFlags);
    }

    nsCOMPtr<nsICancelable> cancelable;
    nsRefPtr<IOServiceProxyCallback> callback =
        new IOServiceProxyCallback(aCallbacks, this);
    nsCOMPtr<nsIProtocolProxyService2> pps2 = do_QueryInterface(pps);
    if (pps2) {
        return pps2->AsyncResolve2(channel, 0, callback, getter_AddRefs(cancelable));
    }
    return pps->AsyncResolve(channel, 0, callback, getter_AddRefs(cancelable));
}

NS_IMETHODIMP
nsIOService::SpeculativeConnect(nsIURI *aURI,
                                nsIInterfaceRequestor *aCallbacks)
{
    return SpeculativeConnectInternal(aURI, aCallbacks, false);
}

NS_IMETHODIMP
nsIOService::SpeculativeAnonymousConnect(nsIURI *aURI,
                                         nsIInterfaceRequestor *aCallbacks)
{
    return SpeculativeConnectInternal(aURI, aCallbacks, true);
}

void
nsIOService::NotifyAppOfflineStatus(uint32_t appId, int32_t state)
{
    MOZ_RELEASE_ASSERT(NS_IsMainThread(),
            "Should be called on the main thread");

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    MOZ_ASSERT(observerService, "The observer service should not be null");

    if (observerService) {
        nsRefPtr<nsAppOfflineInfo> info = new nsAppOfflineInfo(appId, state);
        observerService->NotifyObservers(
            info,
            NS_IOSERVICE_APP_OFFLINE_STATUS_TOPIC,
            MOZ_UTF16("all data in nsIAppOfflineInfo subject argument"));
    }
}

namespace {

class SetAppOfflineMainThread : public nsRunnable
{
public:
    SetAppOfflineMainThread(uint32_t aAppId, int32_t aState)
        : mAppId(aAppId)
        , mState(aState)
    {
    }

    NS_IMETHOD Run()
    {
        MOZ_ASSERT(NS_IsMainThread());
        gIOService->SetAppOfflineInternal(mAppId, mState);
        return NS_OK;
    }
private:
    uint32_t mAppId;
    int32_t mState;
};

} // namespace

NS_IMETHODIMP
nsIOService::SetAppOffline(uint32_t aAppId, int32_t aState)
{
    NS_ENSURE_TRUE(!IsNeckoChild(),
                   NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(aAppId != nsIScriptSecurityManager::NO_APP_ID,
                   NS_ERROR_INVALID_ARG);
    NS_ENSURE_TRUE(aAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID,
                   NS_ERROR_INVALID_ARG);

    if (!NS_IsMainThread()) {
        NS_DispatchToMainThread(new SetAppOfflineMainThread(aAppId, aState));
        return NS_OK;
    }

    SetAppOfflineInternal(aAppId, aState);

    return NS_OK;
}

// This method may be called in both the parent and the child process
// In parent it only gets called in from nsIOService::SetAppOffline
// and SetAppOfflineMainThread::Run
// In the child, it may get called from NeckoChild::RecvAppOfflineStatus
// and TabChild::RecvAppOfflineStatus.
// Note that in the child process, apps should never be in a WIFI_ONLY
// because wifi status is not available on the child
void
nsIOService::SetAppOfflineInternal(uint32_t aAppId, int32_t aState)
{
    MOZ_ASSERT(NS_IsMainThread());
    NS_ENSURE_TRUE_VOID(NS_IsMainThread());

    int32_t state = nsIAppOfflineInfo::ONLINE;
    mAppsOfflineStatus.Get(aAppId, &state);
    if (state == aState) {
        // The app is already in this state. Nothing needs to be done.
        return;
    }

    // wifiActive will always be false in the child process
    // but it will be true in the parent process on Desktop Firefox as it does
    // not have wifi-detection capabilities
    bool wifiActive = IsWifiActive();
    bool offline = (state == nsIAppOfflineInfo::OFFLINE) ||
                   (state == nsIAppOfflineInfo::WIFI_ONLY && !wifiActive);

    switch (aState) {
    case nsIAppOfflineInfo::OFFLINE:
        mAppsOfflineStatus.Put(aAppId, nsIAppOfflineInfo::OFFLINE);
        if (!offline) {
            NotifyAppOfflineStatus(aAppId, nsIAppOfflineInfo::OFFLINE);
        }
        break;
    case nsIAppOfflineInfo::WIFI_ONLY:
        MOZ_RELEASE_ASSERT(!IsNeckoChild());
        mAppsOfflineStatus.Put(aAppId, nsIAppOfflineInfo::WIFI_ONLY);
        if (offline && wifiActive) {
            NotifyAppOfflineStatus(aAppId, nsIAppOfflineInfo::ONLINE);
        } else if (!offline && !wifiActive) {
            NotifyAppOfflineStatus(aAppId, nsIAppOfflineInfo::OFFLINE);
        }
        break;
    case nsIAppOfflineInfo::ONLINE:
        mAppsOfflineStatus.Remove(aAppId);
        if (offline) {
            NotifyAppOfflineStatus(aAppId, nsIAppOfflineInfo::ONLINE);
        }
        break;
    default:
        break;
    }

}

NS_IMETHODIMP
nsIOService::GetAppOfflineState(uint32_t aAppId, int32_t *aResult)
{
    NS_ENSURE_ARG(aResult);

    if (aAppId == NECKO_NO_APP_ID ||
        aAppId == NECKO_UNKNOWN_APP_ID) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    *aResult = nsIAppOfflineInfo::ONLINE;
    mAppsOfflineStatus.Get(aAppId, aResult);

    return NS_OK;
}

NS_IMETHODIMP
nsIOService::IsAppOffline(uint32_t aAppId, bool* aResult)
{
    NS_ENSURE_ARG(aResult);
    *aResult = false;

    if (aAppId == NECKO_NO_APP_ID ||
        aAppId == NECKO_UNKNOWN_APP_ID) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    int32_t state;
    if (mAppsOfflineStatus.Get(aAppId, &state)) {
        switch (state) {
        case nsIAppOfflineInfo::OFFLINE:
            *aResult = true;
            break;
        case nsIAppOfflineInfo::WIFI_ONLY:
            MOZ_RELEASE_ASSERT(!IsNeckoChild());
            *aResult = !IsWifiActive();
            break;
        default:
            // The app is online by default
            break;
        }
    }

    return NS_OK;
}
