/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/Util.h"

#include "nsProtocolProxyService.h"
#include "nsProxyInfo.h"
#include "nsIClassInfoImpl.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsIProxyAutoConfig.h"
#include "nsIIOService.h"
#include "nsIObserverService.h"
#include "nsIProtocolHandler.h"
#include "nsIProtocolProxyCallback.h"
#include "nsICancelable.h"
#include "nsIDNSService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsReadableUtils.h"
#include "nsThreadUtils.h"
#include "nsString.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsCRT.h"
#include "prnetdb.h"
#include "nsPACMan.h"

//----------------------------------------------------------------------------

using namespace mozilla;

#include "prlog.h"
#if defined(PR_LOGGING)
static PRLogModuleInfo *sLog = PR_NewLogModule("proxy");
#endif
#define LOG(args) PR_LOG(sLog, PR_LOG_DEBUG, args)

//----------------------------------------------------------------------------

#define PROXY_PREF_BRANCH  "network.proxy"
#define PROXY_PREF(x)      PROXY_PREF_BRANCH "." x

#define WPAD_URL "http://wpad/wpad.dat"

//----------------------------------------------------------------------------

// This structure is intended to be allocated on the stack
struct nsProtocolInfo {
    nsCAutoString scheme;
    PRUint32 flags;
    PRInt32 defaultPort;
};

//----------------------------------------------------------------------------

class nsAsyncResolveRequest MOZ_FINAL : public nsIRunnable
                                      , public nsPACManCallback
                                      , public nsICancelable
{
public:
    NS_DECL_ISUPPORTS

    nsAsyncResolveRequest(nsProtocolProxyService *pps, nsIURI *uri,
                          PRUint32 aResolveFlags,
                          nsIProtocolProxyCallback *callback)
        : mStatus(NS_OK)
        , mDispatched(false)
        , mResolveFlags(aResolveFlags)
        , mPPS(pps)
        , mURI(uri)
        , mCallback(callback)
    {
        NS_ASSERTION(mCallback, "null callback");
    }

    void SetResult(nsresult status, nsIProxyInfo *pi)
    {
        mStatus = status;
        mProxyInfo = pi;
    }

    NS_IMETHOD Run()
    {
        if (mCallback)
            DoCallback();
        return NS_OK;
    }

    NS_IMETHOD Cancel(nsresult reason)
    {
        NS_ENSURE_ARG(NS_FAILED(reason));

        // If we've already called DoCallback then, nothing more to do.
        if (!mCallback)
            return NS_OK;

        SetResult(reason, nullptr);
        return DispatchCallback();
    }

    nsresult DispatchCallback()
    {
        if (mDispatched)  // Only need to dispatch once
            return NS_OK;

        nsresult rv = NS_DispatchToCurrentThread(this);
        if (NS_FAILED(rv))
            NS_WARNING("unable to dispatch callback event");
        else {
            mDispatched = true;
            return NS_OK;
        }

        mCallback = nullptr;  // break possible reference cycle
        return rv;
    }

private:

    // Called asynchronously, so we do not need to post another PLEvent
    // before calling DoCallback.
    void OnQueryComplete(nsresult status, const nsCString &pacString)
    {
        // If we've already called DoCallback then, nothing more to do.
        if (!mCallback)
            return;

        // Provided we haven't been canceled...
        if (mStatus == NS_OK) {
            mStatus = status;
            mPACString = pacString;
        }

        // In the cancelation case, we may still have another PLEvent in
        // the queue that wants to call DoCallback.  No need to wait for
        // it, just run the callback now.
        DoCallback();
    }

    void DoCallback()
    {
        // Generate proxy info from the PAC string if appropriate
        if (NS_SUCCEEDED(mStatus) && !mProxyInfo && !mPACString.IsEmpty())
            mPPS->ProcessPACString(mPACString, mResolveFlags,
                                   getter_AddRefs(mProxyInfo));

        // Now apply proxy filters
        if (NS_SUCCEEDED(mStatus)) {
            nsProtocolInfo info;
            mStatus = mPPS->GetProtocolInfo(mURI, &info);
            if (NS_SUCCEEDED(mStatus))
                mPPS->ApplyFilters(mURI, info, mProxyInfo);
            else
                mProxyInfo = nullptr;
        }

        mCallback->OnProxyAvailable(this, mURI, mProxyInfo, mStatus);
        mCallback = nullptr;  // in case the callback holds an owning ref to us
    }

private:

    nsresult  mStatus;
    nsCString mPACString;
    bool      mDispatched;
    PRUint32  mResolveFlags;

    nsRefPtr<nsProtocolProxyService>   mPPS;
    nsCOMPtr<nsIURI>                   mURI;
    nsCOMPtr<nsIProtocolProxyCallback> mCallback;
    nsCOMPtr<nsIProxyInfo>             mProxyInfo;
};

NS_IMPL_ISUPPORTS2(nsAsyncResolveRequest, nsICancelable, nsIRunnable)

//----------------------------------------------------------------------------

#define IS_ASCII_SPACE(_c) ((_c) == ' ' || (_c) == '\t')

//
// apply mask to address (zeros out excluded bits).
//
// NOTE: we do the byte swapping here to minimize overall swapping.
//
static void
proxy_MaskIPv6Addr(PRIPv6Addr &addr, PRUint16 mask_len)
{
    if (mask_len == 128)
        return;

    if (mask_len > 96) {
        addr.pr_s6_addr32[3] = PR_htonl(
                PR_ntohl(addr.pr_s6_addr32[3]) & (~0L << (128 - mask_len)));
    }
    else if (mask_len > 64) {
        addr.pr_s6_addr32[3] = 0;
        addr.pr_s6_addr32[2] = PR_htonl(
                PR_ntohl(addr.pr_s6_addr32[2]) & (~0L << (96 - mask_len)));
    }
    else if (mask_len > 32) {
        addr.pr_s6_addr32[3] = 0;
        addr.pr_s6_addr32[2] = 0;
        addr.pr_s6_addr32[1] = PR_htonl(
                PR_ntohl(addr.pr_s6_addr32[1]) & (~0L << (64 - mask_len)));
    }
    else {
        addr.pr_s6_addr32[3] = 0;
        addr.pr_s6_addr32[2] = 0;
        addr.pr_s6_addr32[1] = 0;
        addr.pr_s6_addr32[0] = PR_htonl(
                PR_ntohl(addr.pr_s6_addr32[0]) & (~0L << (32 - mask_len)));
    }
}

static void
proxy_GetStringPref(nsIPrefBranch *aPrefBranch,
                    const char    *aPref,
                    nsCString     &aResult)
{
    nsXPIDLCString temp;
    nsresult rv = aPrefBranch->GetCharPref(aPref, getter_Copies(temp));
    if (NS_FAILED(rv))
        aResult.Truncate();
    else {
        aResult.Assign(temp);
        // all of our string prefs are hostnames, so we should remove any
        // whitespace characters that the user might have unknowingly entered.
        aResult.StripWhitespace();
    }
}

static void
proxy_GetIntPref(nsIPrefBranch *aPrefBranch,
                 const char    *aPref,
                 PRInt32       &aResult)
{
    PRInt32 temp;
    nsresult rv = aPrefBranch->GetIntPref(aPref, &temp);
    if (NS_FAILED(rv)) 
        aResult = -1;
    else
        aResult = temp;
}

static void
proxy_GetBoolPref(nsIPrefBranch *aPrefBranch,
                 const char    *aPref,
                 bool          &aResult)
{
    bool temp;
    nsresult rv = aPrefBranch->GetBoolPref(aPref, &temp);
    if (NS_FAILED(rv)) 
        aResult = false;
    else
        aResult = temp;
}

//----------------------------------------------------------------------------

static const PRInt32 PROXYCONFIG_DIRECT4X = 3;
static const PRInt32 PROXYCONFIG_COUNT = 6;

NS_IMPL_ADDREF(nsProtocolProxyService)
NS_IMPL_RELEASE(nsProtocolProxyService)
NS_IMPL_CLASSINFO(nsProtocolProxyService, NULL, nsIClassInfo::SINGLETON,
                  NS_PROTOCOLPROXYSERVICE_CID)
NS_IMPL_QUERY_INTERFACE3_CI(nsProtocolProxyService,
                            nsIProtocolProxyService,
                            nsIProtocolProxyService2,
                            nsIObserver)
NS_IMPL_CI_INTERFACE_GETTER2(nsProtocolProxyService,
                             nsIProtocolProxyService,
                             nsIProtocolProxyService2)

nsProtocolProxyService::nsProtocolProxyService()
    : mFilterLocalHosts(false)
    , mFilters(nullptr)
    , mProxyConfig(PROXYCONFIG_DIRECT)
    , mHTTPProxyPort(-1)
    , mFTPProxyPort(-1)
    , mHTTPSProxyPort(-1)
    , mSOCKSProxyPort(-1)
    , mSOCKSProxyVersion(4)
    , mSOCKSProxyRemoteDNS(false)
    , mPACMan(nullptr)
    , mSessionStart(PR_Now())
    , mFailedProxyTimeout(30 * 60) // 30 minute default
{
}

nsProtocolProxyService::~nsProtocolProxyService()
{
    // These should have been cleaned up in our Observe method.
    NS_ASSERTION(mHostFiltersArray.Length() == 0 && mFilters == nullptr &&
                 mPACMan == nullptr, "what happened to xpcom-shutdown?");
}

// nsProtocolProxyService methods
nsresult
nsProtocolProxyService::Init()
{
    mFailedProxies.Init();

    // failure to access prefs is non-fatal
    nsCOMPtr<nsIPrefBranch> prefBranch =
            do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefBranch) {
        // monitor proxy prefs
        prefBranch->AddObserver(PROXY_PREF_BRANCH, this, false);

        // read all prefs
        PrefsChanged(prefBranch, nullptr);
    }

    // register for shutdown notification so we can clean ourselves up properly.
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs)
        obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);

    return NS_OK;
}

NS_IMETHODIMP
nsProtocolProxyService::Observe(nsISupports     *aSubject,
                                const char      *aTopic,
                                const PRUnichar *aData)
{
    if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
        // cleanup
        if (mHostFiltersArray.Length() > 0) {
            mHostFiltersArray.Clear();
        }
        if (mFilters) {
            delete mFilters;
            mFilters = nullptr;
        }
        if (mPACMan) {
            mPACMan->Shutdown();
            mPACMan = nullptr;
        }
    }
    else {
        NS_ASSERTION(strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0,
                     "what is this random observer event?");
        nsCOMPtr<nsIPrefBranch> prefs = do_QueryInterface(aSubject);
        if (prefs)
            PrefsChanged(prefs, NS_LossyConvertUTF16toASCII(aData).get());
    }
    return NS_OK;
}

void
nsProtocolProxyService::PrefsChanged(nsIPrefBranch *prefBranch,
                                     const char    *pref)
{
    nsresult rv = NS_OK;
    bool reloadPAC = false;
    nsXPIDLCString tempString;

    if (!pref || !strcmp(pref, PROXY_PREF("type"))) {
        PRInt32 type = -1;
        rv = prefBranch->GetIntPref(PROXY_PREF("type"), &type);
        if (NS_SUCCEEDED(rv)) {
            // bug 115720 - for ns4.x backwards compatibility
            if (type == PROXYCONFIG_DIRECT4X) {
                type = PROXYCONFIG_DIRECT;
                // Reset the type so that the dialog looks correct, and we
                // don't have to handle this case everywhere else
                // I'm paranoid about a loop of some sort - only do this
                // if we're enumerating all prefs, and ignore any error
                if (!pref)
                    prefBranch->SetIntPref(PROXY_PREF("type"), type);
            } else if (type >= PROXYCONFIG_COUNT) {
                LOG(("unknown proxy type: %lu; assuming direct\n", type));
                type = PROXYCONFIG_DIRECT;
            }
            mProxyConfig = type;
            reloadPAC = true;
        }

        if (mProxyConfig == PROXYCONFIG_SYSTEM) {
            mSystemProxySettings = do_GetService(NS_SYSTEMPROXYSETTINGS_CONTRACTID);
            if (!mSystemProxySettings)
                mProxyConfig = PROXYCONFIG_DIRECT;
        } else {
            mSystemProxySettings = nullptr;
        }
    }

    if (!pref || !strcmp(pref, PROXY_PREF("http")))
        proxy_GetStringPref(prefBranch, PROXY_PREF("http"), mHTTPProxyHost);

    if (!pref || !strcmp(pref, PROXY_PREF("http_port")))
        proxy_GetIntPref(prefBranch, PROXY_PREF("http_port"), mHTTPProxyPort);

    if (!pref || !strcmp(pref, PROXY_PREF("ssl")))
        proxy_GetStringPref(prefBranch, PROXY_PREF("ssl"), mHTTPSProxyHost);

    if (!pref || !strcmp(pref, PROXY_PREF("ssl_port")))
        proxy_GetIntPref(prefBranch, PROXY_PREF("ssl_port"), mHTTPSProxyPort);

    if (!pref || !strcmp(pref, PROXY_PREF("ftp")))
        proxy_GetStringPref(prefBranch, PROXY_PREF("ftp"), mFTPProxyHost);

    if (!pref || !strcmp(pref, PROXY_PREF("ftp_port")))
        proxy_GetIntPref(prefBranch, PROXY_PREF("ftp_port"), mFTPProxyPort);

    if (!pref || !strcmp(pref, PROXY_PREF("socks")))
        proxy_GetStringPref(prefBranch, PROXY_PREF("socks"), mSOCKSProxyHost);
    
    if (!pref || !strcmp(pref, PROXY_PREF("socks_port")))
        proxy_GetIntPref(prefBranch, PROXY_PREF("socks_port"), mSOCKSProxyPort);

    if (!pref || !strcmp(pref, PROXY_PREF("socks_version"))) {
        PRInt32 version;
        proxy_GetIntPref(prefBranch, PROXY_PREF("socks_version"), version);
        // make sure this preference value remains sane
        if (version == 5)
            mSOCKSProxyVersion = 5;
        else
            mSOCKSProxyVersion = 4;
    }

    if (!pref || !strcmp(pref, PROXY_PREF("socks_remote_dns")))
        proxy_GetBoolPref(prefBranch, PROXY_PREF("socks_remote_dns"),
                          mSOCKSProxyRemoteDNS);

    if (!pref || !strcmp(pref, PROXY_PREF("failover_timeout")))
        proxy_GetIntPref(prefBranch, PROXY_PREF("failover_timeout"),
                         mFailedProxyTimeout);

    if (!pref || !strcmp(pref, PROXY_PREF("no_proxies_on"))) {
        rv = prefBranch->GetCharPref(PROXY_PREF("no_proxies_on"),
                                     getter_Copies(tempString));
        if (NS_SUCCEEDED(rv))
            LoadHostFilters(tempString.get());
    }

    // We're done if not using something that could give us a PAC URL
    // (PAC, WPAD or System)
    if (mProxyConfig != PROXYCONFIG_PAC && mProxyConfig != PROXYCONFIG_WPAD &&
        mProxyConfig != PROXYCONFIG_SYSTEM)
        return;

    // OK, we need to reload the PAC file if:
    //  1) network.proxy.type changed, or
    //  2) network.proxy.autoconfig_url changed and PAC is configured

    if (!pref || !strcmp(pref, PROXY_PREF("autoconfig_url")))
        reloadPAC = true;

    if (reloadPAC) {
        tempString.Truncate();
        if (mProxyConfig == PROXYCONFIG_PAC) {
            prefBranch->GetCharPref(PROXY_PREF("autoconfig_url"),
                                    getter_Copies(tempString));
        } else if (mProxyConfig == PROXYCONFIG_WPAD) {
            // We diverge from the WPAD spec here in that we don't walk the
            // hosts's FQDN, stripping components until we hit a TLD.  Doing so
            // is dangerous in the face of an incomplete list of TLDs, and TLDs
            // get added over time.  We could consider doing only a single
            // substitution of the first component, if that proves to help
            // compatibility.
            tempString.AssignLiteral(WPAD_URL);
        } else if (mSystemProxySettings) {
            // Get System Proxy settings if available
            mSystemProxySettings->GetPACURI(tempString);
        }
        if (!tempString.IsEmpty())
            ConfigureFromPAC(tempString, false);
    }
}

bool
nsProtocolProxyService::CanUseProxy(nsIURI *aURI, PRInt32 defaultPort) 
{
    if (mHostFiltersArray.Length() == 0)
        return true;

    PRInt32 port;
    nsCAutoString host;
 
    nsresult rv = aURI->GetAsciiHost(host);
    if (NS_FAILED(rv) || host.IsEmpty())
        return false;

    rv = aURI->GetPort(&port);
    if (NS_FAILED(rv))
        return false;
    if (port == -1)
        port = defaultPort;

    PRNetAddr addr;
    bool is_ipaddr = (PR_StringToNetAddr(host.get(), &addr) == PR_SUCCESS);

    PRIPv6Addr ipv6;
    if (is_ipaddr) {
        // convert parsed address to IPv6
        if (addr.raw.family == PR_AF_INET) {
            // convert to IPv4-mapped address
            PR_ConvertIPv4AddrToIPv6(addr.inet.ip, &ipv6);
        }
        else if (addr.raw.family == PR_AF_INET6) {
            // copy the address
            memcpy(&ipv6, &addr.ipv6.ip, sizeof(PRIPv6Addr));
        }
        else {
            NS_WARNING("unknown address family");
            return true; // allow proxying
        }
    }
    
    // Don't use proxy for local hosts (plain hostname, no dots)
    if (!is_ipaddr && mFilterLocalHosts && (kNotFound == host.FindChar('.'))) {
        LOG(("Not using proxy for this local host [%s]!\n", host.get()));
        return false; // don't allow proxying
    }

    PRInt32 index = -1;
    while (++index < PRInt32(mHostFiltersArray.Length())) {
        HostInfo *hinfo = mHostFiltersArray[index];

        if (is_ipaddr != hinfo->is_ipaddr)
            continue;
        if (hinfo->port && hinfo->port != port)
            continue;

        if (is_ipaddr) {
            // generate masked version of target IPv6 address
            PRIPv6Addr masked;
            memcpy(&masked, &ipv6, sizeof(PRIPv6Addr));
            proxy_MaskIPv6Addr(masked, hinfo->ip.mask_len);

            // check for a match
            if (memcmp(&masked, &hinfo->ip.addr, sizeof(PRIPv6Addr)) == 0)
                return false; // proxy disallowed
        }
        else {
            PRUint32 host_len = host.Length();
            PRUint32 filter_host_len = hinfo->name.host_len;

            if (host_len >= filter_host_len) {
                //
                // compare last |filter_host_len| bytes of target hostname.
                //
                const char *host_tail = host.get() + host_len - filter_host_len;
                if (!PL_strncasecmp(host_tail, hinfo->name.host, filter_host_len))
                    return false; // proxy disallowed
            }
        }
    }
    return true;
}

static const char kProxyType_HTTP[]    = "http";
static const char kProxyType_PROXY[]   = "proxy";
static const char kProxyType_SOCKS[]   = "socks";
static const char kProxyType_SOCKS4[]  = "socks4";
static const char kProxyType_SOCKS5[]  = "socks5";
static const char kProxyType_DIRECT[]  = "direct";
static const char kProxyType_UNKNOWN[] = "unknown";

const char *
nsProtocolProxyService::ExtractProxyInfo(const char *start,
                                         PRUint32 aResolveFlags,
                                         nsProxyInfo **result)
{
    *result = nullptr;
    PRUint32 flags = 0;

    // see BNF in nsIProxyAutoConfig.idl

    // find end of proxy info delimiter
    const char *end = start;
    while (*end && *end != ';') ++end;

    // find end of proxy type delimiter
    const char *sp = start;
    while (sp < end && *sp != ' ' && *sp != '\t') ++sp;

    PRUint32 len = sp - start;
    const char *type = nullptr;
    switch (len) {
    case 5:
        if (PL_strncasecmp(start, kProxyType_PROXY, 5) == 0)
            type = kProxyType_HTTP;
        else if (PL_strncasecmp(start, kProxyType_SOCKS, 5) == 0)
            type = kProxyType_SOCKS4; // assume v4 for 4x compat
        break;
    case 6:
        if (PL_strncasecmp(start, kProxyType_DIRECT, 6) == 0)
            type = kProxyType_DIRECT;
        else if (PL_strncasecmp(start, kProxyType_SOCKS4, 6) == 0)
            type = kProxyType_SOCKS4;
        else if (PL_strncasecmp(start, kProxyType_SOCKS5, 6) == 0)
            // map "SOCKS5" to "socks" to match contract-id of registered
            // SOCKS-v5 socket provider.
            type = kProxyType_SOCKS;
        break;
    }
    if (type) {
        const char *host = nullptr, *hostEnd = nullptr;
        PRInt32 port = -1;

        // If it's a SOCKS5 proxy, do name resolution on the server side.
        // We could use this with SOCKS4a servers too, but they might not
        // support it.
        if (type == kProxyType_SOCKS || mSOCKSProxyRemoteDNS)
            flags |= nsIProxyInfo::TRANSPARENT_PROXY_RESOLVES_HOST;

        // extract host:port
        start = sp;
        while ((*start == ' ' || *start == '\t') && start < end)
            start++;
        if (start < end) {
            host = start;
            hostEnd = strchr(host, ':');
            if (!hostEnd || hostEnd > end) {
                hostEnd = end;
                // no port, so assume default
                if (type == kProxyType_HTTP)
                    port = 80;
                else
                    port = 1080;
            }
            else
                port = atoi(hostEnd + 1);
        }
        nsProxyInfo *pi = new nsProxyInfo;
        if (pi) {
            pi->mType = type;
            pi->mFlags = flags;
            pi->mResolveFlags = aResolveFlags;
            pi->mTimeout = mFailedProxyTimeout;
            // YES, it is ok to specify a null proxy host.
            if (host) {
                pi->mHost.Assign(host, hostEnd - host);
                pi->mPort = port;
            }
            NS_ADDREF(*result = pi);
        }
    }

    while (*end == ';' || *end == ' ' || *end == '\t')
        ++end;
    return end;
}

void
nsProtocolProxyService::GetProxyKey(nsProxyInfo *pi, nsCString &key)
{
    key.AssignASCII(pi->mType);
    if (!pi->mHost.IsEmpty()) {
        key.Append(' ');
        key.Append(pi->mHost);
        key.Append(':');
        key.AppendInt(pi->mPort);
    }
} 

PRUint32
nsProtocolProxyService::SecondsSinceSessionStart()
{
    PRTime now = PR_Now();

    // get time elapsed since session start
    PRInt64 diff;
    LL_SUB(diff, now, mSessionStart);

    // convert microseconds to seconds
    PRTime ups;
    LL_I2L(ups, PR_USEC_PER_SEC);
    LL_DIV(diff, diff, ups);

    // convert to 32 bit value
    PRUint32 dsec;
    LL_L2UI(dsec, diff);

    return dsec;
}

void
nsProtocolProxyService::EnableProxy(nsProxyInfo *pi)
{
    nsCAutoString key;
    GetProxyKey(pi, key);
    mFailedProxies.Remove(key);
}

void
nsProtocolProxyService::DisableProxy(nsProxyInfo *pi)
{
    nsCAutoString key;
    GetProxyKey(pi, key);

    PRUint32 dsec = SecondsSinceSessionStart();

    // Add timeout to interval (this is the time when the proxy can
    // be tried again).
    dsec += pi->mTimeout;

    // NOTE: The classic codebase would increase the timeout value
    //       incrementally each time a subsequent failure occurred.
    //       We could do the same, but it would require that we not
    //       remove proxy entries in IsProxyDisabled or otherwise
    //       change the way we are recording disabled proxies.
    //       Simpler is probably better for now, and at least the
    //       user can tune the timeout setting via preferences.

    LOG(("DisableProxy %s %d\n", key.get(), dsec));

    // If this fails, oh well... means we don't have enough memory
    // to remember the failed proxy.
    mFailedProxies.Put(key, dsec);
}

bool
nsProtocolProxyService::IsProxyDisabled(nsProxyInfo *pi)
{
    nsCAutoString key;
    GetProxyKey(pi, key);

    PRUint32 val;
    if (!mFailedProxies.Get(key, &val))
        return false;

    PRUint32 dsec = SecondsSinceSessionStart();

    // if time passed has exceeded interval, then try proxy again.
    if (dsec > val) {
        mFailedProxies.Remove(key);
        return false;
    }

    return true;
}

nsresult
nsProtocolProxyService::ConfigureFromPAC(const nsCString &spec,
                                         bool forceReload)
{
    if (!mPACMan) {
        mPACMan = new nsPACMan();
        if (!mPACMan)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIURI> pacURI;
    nsresult rv = NS_NewURI(getter_AddRefs(pacURI), spec);
    if (NS_FAILED(rv))
        return rv;

    if (mPACMan->IsPACURI(pacURI) && !forceReload)
        return NS_OK;

    mFailedProxies.Clear();

    return mPACMan->LoadPACFromURI(pacURI);
}

void
nsProtocolProxyService::ProcessPACString(const nsCString &pacString,
                                         PRUint32 aResolveFlags,
                                         nsIProxyInfo **result)
{
    if (pacString.IsEmpty()) {
        *result = nullptr;
        return;
    }

    const char *proxies = pacString.get();

    nsProxyInfo *pi = nullptr, *first = nullptr, *last = nullptr;
    while (*proxies) {
        proxies = ExtractProxyInfo(proxies, aResolveFlags, &pi);
        if (pi) {
            if (last) {
                NS_ASSERTION(last->mNext == nullptr, "leaking nsProxyInfo");
                last->mNext = pi;
            }
            else
                first = pi;
            last = pi;
        }
    }
    *result = first;
}

// nsIProtocolProxyService2
NS_IMETHODIMP
nsProtocolProxyService::ReloadPAC()
{
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefs)
        return NS_OK;

    PRInt32 type;
    nsresult rv = prefs->GetIntPref(PROXY_PREF("type"), &type);
    if (NS_FAILED(rv))
        return NS_OK;

    nsXPIDLCString pacSpec;
    if (type == PROXYCONFIG_PAC)
        prefs->GetCharPref(PROXY_PREF("autoconfig_url"), getter_Copies(pacSpec));
    else if (type == PROXYCONFIG_WPAD)
        pacSpec.AssignLiteral(WPAD_URL);

    if (!pacSpec.IsEmpty())
        ConfigureFromPAC(pacSpec, true);
    return NS_OK;
}

// nsIProtocolProxyService
NS_IMETHODIMP
nsProtocolProxyService::Resolve(nsIURI *uri, PRUint32 flags,
                                nsIProxyInfo **result)
{
    nsProtocolInfo info;
    nsresult rv = GetProtocolInfo(uri, &info);
    if (NS_FAILED(rv))
        return rv;

    bool usePAC;
    rv = Resolve_Internal(uri, info, flags, &usePAC, result);
    if (NS_FAILED(rv)) {
        LOG(("Resolve_Internal returned rv(0x%08x)\n", rv));
        return rv;
    }

    if (usePAC && mPACMan) {
        NS_ASSERTION(*result == nullptr, "we should not have a result yet");

        // If the caller didn't want us to invoke PAC, then error out.
        if (flags & RESOLVE_NON_BLOCKING)
            return NS_BASE_STREAM_WOULD_BLOCK;

        // Query the PAC file synchronously.
        nsCString pacString;
        rv = mPACMan->GetProxyForURI(uri, pacString);
        if (NS_SUCCEEDED(rv))
            ProcessPACString(pacString, flags, result);
        else if (rv == NS_ERROR_IN_PROGRESS) {
            // Construct a special UNKNOWN proxy entry that informs the caller
            // that the proxy info is yet to be determined.
            rv = NewProxyInfo_Internal(kProxyType_UNKNOWN, EmptyCString(), -1,
                                       0, 0, nullptr, flags, result);
            if (NS_FAILED(rv))
                return rv;
        }
        else
            NS_WARNING("failed querying PAC file; trying DIRECT");
    }

    ApplyFilters(uri, info, result);
    return NS_OK;
}

NS_IMETHODIMP
nsProtocolProxyService::AsyncResolve(nsIURI *uri, PRUint32 flags,
                                     nsIProtocolProxyCallback *callback,
                                     nsICancelable **result)
{
    nsRefPtr<nsAsyncResolveRequest> ctx =
        new nsAsyncResolveRequest(this, uri, flags, callback);
    if (!ctx)
        return NS_ERROR_OUT_OF_MEMORY;

    nsProtocolInfo info;
    nsresult rv = GetProtocolInfo(uri, &info);
    if (NS_FAILED(rv))
        return rv;

    bool usePAC;
    nsCOMPtr<nsIProxyInfo> pi;
    rv = Resolve_Internal(uri, info, flags, &usePAC, getter_AddRefs(pi));
    if (NS_FAILED(rv))
        return rv;

    if (!usePAC || !mPACMan) {
        ApplyFilters(uri, info, pi);

        ctx->SetResult(NS_OK, pi);
        return ctx->DispatchCallback();
    }

    // else kick off a PAC query
    rv = mPACMan->AsyncGetProxyForURI(uri, ctx);
    if (NS_SUCCEEDED(rv)) {
        *result = ctx;
        NS_ADDREF(*result);
    }
    return rv;
}

NS_IMETHODIMP
nsProtocolProxyService::NewProxyInfo(const nsACString &aType,
                                     const nsACString &aHost,
                                     PRInt32 aPort,
                                     PRUint32 aFlags,
                                     PRUint32 aFailoverTimeout,
                                     nsIProxyInfo *aFailoverProxy,
                                     nsIProxyInfo **aResult)
{
    static const char *types[] = {
        kProxyType_HTTP,
        kProxyType_SOCKS,
        kProxyType_SOCKS4,
        kProxyType_DIRECT
    };

    // resolve type; this allows us to avoid copying the type string into each
    // proxy info instance.  we just reference the string literals directly :)
    const char *type = nullptr;
    for (PRUint32 i=0; i<ArrayLength(types); ++i) {
        if (aType.LowerCaseEqualsASCII(types[i])) {
            type = types[i];
            break;
        }
    }
    NS_ENSURE_TRUE(type, NS_ERROR_INVALID_ARG);

    if (aPort <= 0)
        aPort = -1;

    return NewProxyInfo_Internal(type, aHost, aPort, aFlags, aFailoverTimeout,
                                 aFailoverProxy, 0, aResult);
}

NS_IMETHODIMP
nsProtocolProxyService::GetFailoverForProxy(nsIProxyInfo  *aProxy,
                                            nsIURI        *aURI,
                                            nsresult       aStatus,
                                            nsIProxyInfo **aResult)
{
    // We only support failover when a PAC file is configured, either
    // directly or via system settings
    if (mProxyConfig != PROXYCONFIG_PAC && mProxyConfig != PROXYCONFIG_WPAD &&
        mProxyConfig != PROXYCONFIG_SYSTEM)
        return NS_ERROR_NOT_AVAILABLE;

    // Verify that |aProxy| is one of our nsProxyInfo objects.
    nsCOMPtr<nsProxyInfo> pi = do_QueryInterface(aProxy);
    NS_ENSURE_ARG(pi);
    // OK, the QI checked out.  We can proceed.

    // Remember that this proxy is down.
    DisableProxy(pi);

    // NOTE: At this point, we might want to prompt the user if we have
    //       not already tried going DIRECT.  This is something that the
    //       classic codebase supported; however, IE6 does not prompt.

    if (!pi->mNext)
        return NS_ERROR_NOT_AVAILABLE;

    LOG(("PAC failover from %s %s:%d to %s %s:%d\n",
        pi->mType, pi->mHost.get(), pi->mPort,
        pi->mNext->mType, pi->mNext->mHost.get(), pi->mNext->mPort));

    NS_ADDREF(*aResult = pi->mNext);
    return NS_OK;
}

NS_IMETHODIMP
nsProtocolProxyService::RegisterFilter(nsIProtocolProxyFilter *filter,
                                       PRUint32 position)
{
    UnregisterFilter(filter);  // remove this filter if we already have it

    FilterLink *link = new FilterLink(position, filter);
    if (!link)
        return NS_ERROR_OUT_OF_MEMORY;

    if (!mFilters) {
        mFilters = link;
        return NS_OK;
    }

    // insert into mFilters in sorted order
    FilterLink *last = nullptr;
    for (FilterLink *iter = mFilters; iter; iter = iter->next) {
        if (position < iter->position) {
            if (last) {
                link->next = last->next;
                last->next = link;
            }
            else {
                link->next = mFilters;
                mFilters = link;
            }
            return NS_OK;
        }
        last = iter;
    }
    // our position is equal to or greater than the last link in the list
    last->next = link;
    return NS_OK;
}

NS_IMETHODIMP
nsProtocolProxyService::UnregisterFilter(nsIProtocolProxyFilter *filter)
{
    // QI to nsISupports so we can safely test object identity.
    nsCOMPtr<nsISupports> givenObject = do_QueryInterface(filter);

    FilterLink *last = nullptr;
    for (FilterLink *iter = mFilters; iter; iter = iter->next) {
        nsCOMPtr<nsISupports> object = do_QueryInterface(iter->filter);
        if (object == givenObject) {
            if (last)
                last->next = iter->next;
            else
                mFilters = iter->next;
            iter->next = nullptr;
            delete iter;
            return NS_OK;
        }
        last = iter;
    }

    // No need to throw an exception in this case.
    return NS_OK;
}

NS_IMETHODIMP
nsProtocolProxyService::GetProxyConfigType(PRUint32* aProxyConfigType)
{
  *aProxyConfigType = mProxyConfig;
  return NS_OK;
}

void
nsProtocolProxyService::LoadHostFilters(const char *filters)
{
    // check to see the owners flag? /!?/ TODO
    if (mHostFiltersArray.Length() > 0) {
        mHostFiltersArray.Clear();
    }

    if (!filters)
        return; // fail silently...

    //
    // filter  = ( host | domain | ipaddr ["/" mask] ) [":" port] 
    // filters = filter *( "," LWS filter)
    //
    // Reset mFilterLocalHosts - will be set to true if "<local>" is in pref string
    mFilterLocalHosts = false;
    while (*filters) {
        // skip over spaces and ,
        while (*filters && (*filters == ',' || IS_ASCII_SPACE(*filters)))
            filters++;

        const char *starthost = filters;
        const char *endhost = filters + 1; // at least that...
        const char *portLocation = 0; 
        const char *maskLocation = 0;

        while (*endhost && (*endhost != ',' && !IS_ASCII_SPACE(*endhost))) {
            if (*endhost == ':')
                portLocation = endhost;
            else if (*endhost == '/')
                maskLocation = endhost;
            else if (*endhost == ']') // IPv6 address literals
                portLocation = 0;
            endhost++;
        }

        filters = endhost; // advance iterator up front

        // locate end of host
        const char *end = maskLocation ? maskLocation :
                          portLocation ? portLocation :
                          endhost;

        nsCAutoString str(starthost, end - starthost);

        // If the current host filter is "<local>", then all local (i.e.
        // no dots in the hostname) hosts should bypass the proxy
        if (str.EqualsIgnoreCase("<local>")) {
            mFilterLocalHosts = true;
            LOG(("loaded filter for local hosts "
                 "(plain host names, no dots)\n"));
            // Continue to next host filter;
            continue;
        }

        // For all other host filters, create HostInfo object and add to list
        HostInfo *hinfo = new HostInfo();
        hinfo->port = portLocation ? atoi(portLocation + 1) : 0;

        PRNetAddr addr;
        if (PR_StringToNetAddr(str.get(), &addr) == PR_SUCCESS) {
            hinfo->is_ipaddr   = true;
            hinfo->ip.family   = PR_AF_INET6; // we always store address as IPv6
            hinfo->ip.mask_len = maskLocation ? atoi(maskLocation + 1) : 128;

            if (hinfo->ip.mask_len == 0) {
                NS_WARNING("invalid mask");
                goto loser;
            }

            if (addr.raw.family == PR_AF_INET) {
                // convert to IPv4-mapped address
                PR_ConvertIPv4AddrToIPv6(addr.inet.ip, &hinfo->ip.addr);
                // adjust mask_len accordingly
                if (hinfo->ip.mask_len <= 32)
                    hinfo->ip.mask_len += 96;
            }
            else if (addr.raw.family == PR_AF_INET6) {
                // copy the address
                memcpy(&hinfo->ip.addr, &addr.ipv6.ip, sizeof(PRIPv6Addr));
            }
            else {
                NS_WARNING("unknown address family");
                goto loser;
            }

            // apply mask to IPv6 address
            proxy_MaskIPv6Addr(hinfo->ip.addr, hinfo->ip.mask_len);
        }
        else {
            PRUint32 startIndex, endIndex;
            if (str.First() == '*')
                startIndex = 1; // *.domain -> .domain
            else
                startIndex = 0;
            endIndex = (portLocation ? portLocation : endhost) - starthost;

            hinfo->is_ipaddr = false;
            hinfo->name.host = ToNewCString(Substring(str, startIndex, endIndex));

            if (!hinfo->name.host)
                goto loser;

            hinfo->name.host_len = endIndex - startIndex;
        }

//#define DEBUG_DUMP_FILTERS
#ifdef DEBUG_DUMP_FILTERS
        printf("loaded filter[%u]:\n", mHostFiltersArray.Length());
        printf("  is_ipaddr = %u\n", hinfo->is_ipaddr);
        printf("  port = %u\n", hinfo->port);
        if (hinfo->is_ipaddr) {
            printf("  ip.family = %x\n", hinfo->ip.family);
            printf("  ip.mask_len = %u\n", hinfo->ip.mask_len);

            PRNetAddr netAddr;
            PR_SetNetAddr(PR_IpAddrNull, PR_AF_INET6, 0, &netAddr);
            memcpy(&netAddr.ipv6.ip, &hinfo->ip.addr, sizeof(hinfo->ip.addr));

            char buf[256];
            PR_NetAddrToString(&netAddr, buf, sizeof(buf));

            printf("  ip.addr = %s\n", buf);
        }
        else {
            printf("  name.host = %s\n", hinfo->name.host);
        }
#endif

        mHostFiltersArray.AppendElement(hinfo);
        hinfo = nullptr;
loser:
        delete hinfo;
    }
}

nsresult
nsProtocolProxyService::GetProtocolInfo(nsIURI *uri, nsProtocolInfo *info)
{
    nsresult rv;

    rv = uri->GetScheme(info->scheme);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIIOService> ios = do_GetIOService(&rv);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIProtocolHandler> handler;
    rv = ios->GetProtocolHandler(info->scheme.get(), getter_AddRefs(handler));
    if (NS_FAILED(rv))
        return rv;

    rv = handler->GetProtocolFlags(&info->flags);
    if (NS_FAILED(rv))
        return rv;

    rv = handler->GetDefaultPort(&info->defaultPort);
    return rv;
}

nsresult
nsProtocolProxyService::NewProxyInfo_Internal(const char *aType,
                                              const nsACString &aHost,
                                              PRInt32 aPort,
                                              PRUint32 aFlags,
                                              PRUint32 aFailoverTimeout,
                                              nsIProxyInfo *aFailoverProxy,
                                              PRUint32 aResolveFlags,
                                              nsIProxyInfo **aResult)
{
    nsCOMPtr<nsProxyInfo> failover;
    if (aFailoverProxy) {
        failover = do_QueryInterface(aFailoverProxy);
        NS_ENSURE_ARG(failover);
    }

    nsProxyInfo *proxyInfo = new nsProxyInfo();
    if (!proxyInfo)
        return NS_ERROR_OUT_OF_MEMORY;

    proxyInfo->mType = aType;
    proxyInfo->mHost = aHost;
    proxyInfo->mPort = aPort;
    proxyInfo->mFlags = aFlags;
    proxyInfo->mResolveFlags = aResolveFlags;
    proxyInfo->mTimeout = aFailoverTimeout == PR_UINT32_MAX
        ? mFailedProxyTimeout : aFailoverTimeout;
    failover.swap(proxyInfo->mNext);

    NS_ADDREF(*aResult = proxyInfo);
    return NS_OK;
}

nsresult
nsProtocolProxyService::Resolve_Internal(nsIURI *uri,
                                         const nsProtocolInfo &info,
                                         PRUint32 flags,
                                         bool *usePAC,
                                         nsIProxyInfo **result)
{
    NS_ENSURE_ARG_POINTER(uri);

    *usePAC = false;
    *result = nullptr;

    if (!(info.flags & nsIProtocolHandler::ALLOWS_PROXY))
        return NS_OK;  // Can't proxy this (filters may not override)

    if (mSystemProxySettings) {
        nsCAutoString PACURI;
        if (NS_FAILED(mSystemProxySettings->GetPACURI(PACURI)) ||
            PACURI.IsEmpty()) {
            nsCAutoString proxy;
            nsresult rv = mSystemProxySettings->GetProxyForURI(uri, proxy);
            if (NS_SUCCEEDED(rv)) {
                ProcessPACString(proxy, flags, result);
                return NS_OK;
            }
            // no proxy, stop search
            return NS_OK;
        }

        // See bug #586908.
        // Avoid endless loop if |uri| is the current PAC-URI. Returning OK
        // here means that we will not use a proxy for this connection.
        if (mPACMan && mPACMan->IsPACURI(uri))
            return NS_OK;

        // Switch to new PAC file if that setting has changed. If the setting
        // hasn't changed, ConfigureFromPAC will exit early.
        nsresult rv = ConfigureFromPAC(PACURI, false);
        if (NS_FAILED(rv))
            return rv;
    }

    // if proxies are enabled and this host:port combo is supposed to use a
    // proxy, check for a proxy.
    if (mProxyConfig == PROXYCONFIG_DIRECT ||
        (mProxyConfig == PROXYCONFIG_MANUAL &&
         !CanUseProxy(uri, info.defaultPort)))
        return NS_OK;

    // Proxy auto config magic...
    if (mProxyConfig == PROXYCONFIG_PAC || mProxyConfig == PROXYCONFIG_WPAD ||
        mProxyConfig == PROXYCONFIG_SYSTEM) {
        // Do not query PAC now.
        *usePAC = true;
        return NS_OK;
    }

    // proxy info values
    const char *type = nullptr;
    const nsACString *host = nullptr;
    PRInt32 port = -1;

    PRUint32 proxyFlags = 0;

    if ((flags & RESOLVE_PREFER_SOCKS_PROXY) &&
        !mSOCKSProxyHost.IsEmpty() && mSOCKSProxyPort > 0) {
      host = &mSOCKSProxyHost;
      if (mSOCKSProxyVersion == 4) 
          type = kProxyType_SOCKS4;
      else
          type = kProxyType_SOCKS;
      port = mSOCKSProxyPort;
      if (mSOCKSProxyRemoteDNS)
          proxyFlags |= nsIProxyInfo::TRANSPARENT_PROXY_RESOLVES_HOST;
    }
    else if ((flags & RESOLVE_PREFER_HTTPS_PROXY) &&
             !mHTTPSProxyHost.IsEmpty() && mHTTPSProxyPort > 0) {
        host = &mHTTPSProxyHost;
        type = kProxyType_HTTP;
        port = mHTTPSProxyPort;
    }
    else if (!mHTTPProxyHost.IsEmpty() && mHTTPProxyPort > 0 &&
             ((flags & RESOLVE_IGNORE_URI_SCHEME) ||
              info.scheme.EqualsLiteral("http"))) {
        host = &mHTTPProxyHost;
        type = kProxyType_HTTP;
        port = mHTTPProxyPort;
    }
    else if (!mHTTPSProxyHost.IsEmpty() && mHTTPSProxyPort > 0 &&
             !(flags & RESOLVE_IGNORE_URI_SCHEME) &&
             info.scheme.EqualsLiteral("https")) {
        host = &mHTTPSProxyHost;
        type = kProxyType_HTTP;
        port = mHTTPSProxyPort;
    }
    else if (!mFTPProxyHost.IsEmpty() && mFTPProxyPort > 0 &&
             !(flags & RESOLVE_IGNORE_URI_SCHEME) &&
             info.scheme.EqualsLiteral("ftp")) {
        host = &mFTPProxyHost;
        type = kProxyType_HTTP;
        port = mFTPProxyPort;
    }
    else if (!mSOCKSProxyHost.IsEmpty() && mSOCKSProxyPort > 0) {
        host = &mSOCKSProxyHost;
        if (mSOCKSProxyVersion == 4) 
            type = kProxyType_SOCKS4;
        else
            type = kProxyType_SOCKS;
        port = mSOCKSProxyPort;
        if (mSOCKSProxyRemoteDNS)
            proxyFlags |= nsIProxyInfo::TRANSPARENT_PROXY_RESOLVES_HOST;
    }

    if (type) {
        nsresult rv = NewProxyInfo_Internal(type, *host, port, proxyFlags,
                                            PR_UINT32_MAX, nullptr, flags,
                                            result);
        if (NS_FAILED(rv))
            return rv;
    }

    return NS_OK;
}

void
nsProtocolProxyService::ApplyFilters(nsIURI *uri, const nsProtocolInfo &info,
                                     nsIProxyInfo **list)
{
    if (!(info.flags & nsIProtocolHandler::ALLOWS_PROXY))
        return;

    // We prune the proxy list prior to invoking each filter.  This may be
    // somewhat inefficient, but it seems like a good idea since we want each
    // filter to "see" a valid proxy list.

    nsresult rv;
    nsCOMPtr<nsIProxyInfo> result;

    for (FilterLink *iter = mFilters; iter; iter = iter->next) {
        PruneProxyInfo(info, list);

        rv = iter->filter->ApplyFilter(this, uri, *list,
                                       getter_AddRefs(result));
        if (NS_FAILED(rv))
            continue;
        result.swap(*list);
    }

    PruneProxyInfo(info, list);
}

void
nsProtocolProxyService::PruneProxyInfo(const nsProtocolInfo &info,
                                       nsIProxyInfo **list)
{
    if (!*list)
        return;
    nsProxyInfo *head = nullptr;
    CallQueryInterface(*list, &head);
    if (!head) {
        NS_NOTREACHED("nsIProxyInfo must QI to nsProxyInfo");
        return;
    }
    NS_RELEASE(*list);

    // Pruning of disabled proxies works like this:
    //   - If all proxies are disabled, return the full list
    //   - Otherwise, remove the disabled proxies.
    //
    // Pruning of disallowed proxies works like this:
    //   - If the protocol handler disallows the proxy, then we disallow it.

    // Start by removing all disallowed proxies if required:
    if (!(info.flags & nsIProtocolHandler::ALLOWS_PROXY_HTTP)) {
        nsProxyInfo *last = nullptr, *iter = head; 
        while (iter) {
            if (iter->Type() == kProxyType_HTTP) {
                // reject!
                if (last)
                    last->mNext = iter->mNext;
                else
                    head = iter->mNext;
                nsProxyInfo *next = iter->mNext;
                iter->mNext = nullptr;
                iter->Release();
                iter = next;
            } else {
                last = iter;
                iter = iter->mNext;
            }
        }
        if (!head)
            return;
    }

    // Now, scan to see if all remaining proxies are disabled.  If so, then
    // we'll just bail and return them all.  Otherwise, we'll go and prune the
    // disabled ones.
    
    bool allDisabled = true;

    nsProxyInfo *iter;
    for (iter = head; iter; iter = iter->mNext) {
        if (!IsProxyDisabled(iter)) {
            allDisabled = false;
            break;
        }
    }

    if (allDisabled)
        LOG(("All proxies are disabled, so trying all again"));
    else {
        // remove any disabled proxies.
        nsProxyInfo *last = nullptr; 
        for (iter = head; iter; ) {
            if (IsProxyDisabled(iter)) {
                // reject!
                nsProxyInfo *reject = iter;

                iter = iter->mNext;
                if (last)
                    last->mNext = iter;
                else
                    head = iter;

                reject->mNext = nullptr;
                NS_RELEASE(reject);
                continue;
            }

            // since we are about to use this proxy, make sure it is not on
            // the disabled proxy list.  we'll add it back to that list if
            // we have to (in GetFailoverForProxy).
            //
            // XXX(darin): It might be better to do this as a final pass.
            //
            EnableProxy(iter);

            last = iter;
            iter = iter->mNext;
        }
    }

    // if only DIRECT was specified then return no proxy info, and we're done.
    if (head && !head->mNext && head->mType == kProxyType_DIRECT)
        NS_RELEASE(head);

    *list = head;  // Transfer ownership
}
