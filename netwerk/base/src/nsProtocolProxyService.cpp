/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et: */
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

#include "nsProtocolProxyService.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsIProxyAutoConfig.h"
#include "nsAutoLock.h"
#include "nsEventQueueUtils.h"
#include "nsIIOService.h"
#include "nsIProtocolHandler.h"
#include "nsIDNSService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranchInternal.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsNetUtil.h"
#include "nsCRT.h"
#include "prnetdb.h"
#include "nsEventQueueUtils.h"

//----------------------------------------------------------------------------

#include "prlog.h"
#if defined(PR_LOGGING)
static PRLogModuleInfo *sLog = PR_NewLogModule("proxy");
#endif
#define LOG(args) PR_LOG(sLog, PR_LOG_DEBUG, args)

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

//----------------------------------------------------------------------------

class nsProxyInfo : public nsIProxyInfo
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD_(const char*) Host()
    {
        return mHost.get();
    }

    NS_IMETHOD_(PRInt32) Port()
    {
        return mPort;
    }

    NS_IMETHOD_(const char*) Type()
    {
        return mType;
    }

    ~nsProxyInfo()
    {
        NS_IF_RELEASE(mNext);
    }

    nsProxyInfo(const char *type = nsnull)
        : mType(type)
        , mPort(-1)
        , mNext(nsnull)
    {}

    const char  *mType; // pointer to static kProxyType_XYZ value
    nsCString    mHost;
    PRInt32      mPort;
    nsProxyInfo *mNext;
};

#define NS_PROXYINFO_ID \
{ /* ed42f751-825e-4cc2-abeb-3670711a8b85 */         \
    0xed42f751,                                      \
    0x825e,                                          \
    0x4cc2,                                          \
    {0xab, 0xeb, 0x36, 0x70, 0x71, 0x1a, 0x8b, 0x85} \
}
static NS_DEFINE_IID(kProxyInfoID, NS_PROXYINFO_ID);

// These objects will be accessed on the socket transport thread.
NS_IMPL_THREADSAFE_ADDREF(nsProxyInfo)
NS_IMPL_THREADSAFE_RELEASE(nsProxyInfo)

NS_INTERFACE_MAP_BEGIN(nsProxyInfo)
    NS_INTERFACE_MAP_ENTRY(nsIProxyInfo)
    NS_INTERFACE_MAP_ENTRY(nsISupports)
    // see nsProtocolProxyInfo::GetFailoverForProxy
    if (aIID.Equals(kProxyInfoID))
        foundInterface = this;
    else
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS3(nsProtocolProxyService,
                              nsIProtocolProxyService,
                              nsIDNSListener,
                              nsIObserver)

nsProtocolProxyService::nsProtocolProxyService()
    : mUseProxy(0)
    , mHTTPProxyPort(-1)
    , mFTPProxyPort(-1)
    , mGopherProxyPort(-1)
    , mHTTPSProxyPort(-1)
    , mSOCKSProxyPort(-1)
    , mSOCKSProxyVersion(4)
    , mSessionStart(PR_Now())
    , mFailedProxyTimeout(30 * 60) // 30 minute default
{
}

nsProtocolProxyService::~nsProtocolProxyService()
{
    if (mFiltersArray.Count() > 0) {
        mFiltersArray.EnumerateForwards(CleanupFilterArray, nsnull);
        mFiltersArray.Clear();
    }
}

// nsProtocolProxyService methods
nsresult
nsProtocolProxyService::Init()
{
    if (!mFailedProxies.Init())
        return NS_ERROR_OUT_OF_MEMORY;

    // failure to access prefs is non-fatal
    nsCOMPtr<nsIPrefBranchInternal> prefBranch =
            do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefBranch) {
        // monitor proxy prefs
        prefBranch->AddObserver("network.proxy", this, PR_FALSE);

        // read all prefs
        PrefsChanged(prefBranch, nsnull);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsProtocolProxyService::Observe(nsISupports     *aSubject,
                                const char      *aTopic,
                                const PRUnichar *aData)
{
    nsCOMPtr<nsIPrefBranch> prefs = do_QueryInterface(aSubject);
    if (prefs)
        PrefsChanged(prefs, NS_LossyConvertUTF16toASCII(aData).get());
    return NS_OK;
}

void
nsProtocolProxyService::PrefsChanged(nsIPrefBranch *prefBranch,
                                     const char    *pref)
{
    nsresult rv = NS_OK;
    PRBool reloadPAC = PR_FALSE;
    nsXPIDLCString tempString;

    if (!pref || !strcmp(pref, "network.proxy.type")) {
        PRInt32 type = -1;
        rv = prefBranch->GetIntPref("network.proxy.type",&type);
        if (NS_SUCCEEDED(rv)) {
            // bug 115720 - type 3 is the same as 0 (no proxy),
            // for ns4.x backwards compatability
            if (type == 3) {
                type = 0;
                // Reset the type so that the dialog looks correct, and we
                // don't have to handle this case everywhere else
                // I'm paranoid about a loop of some sort - only do this
                // if we're enumerating all prefs, and ignore any error
                if (!pref)
                    prefBranch->SetIntPref("network.proxy.type", 0);
            }
            mUseProxy = type; // type == 2 is fixed PAC URL, 4 is WPAD
            reloadPAC = PR_TRUE;
        }
    }

    if (!pref || !strcmp(pref, "network.proxy.http"))
        proxy_GetStringPref(prefBranch, "network.proxy.http", mHTTPProxyHost);

    if (!pref || !strcmp(pref, "network.proxy.http_port"))
        proxy_GetIntPref(prefBranch, "network.proxy.http_port", mHTTPProxyPort);

    if (!pref || !strcmp(pref, "network.proxy.ssl"))
        proxy_GetStringPref(prefBranch, "network.proxy.ssl", mHTTPSProxyHost);

    if (!pref || !strcmp(pref, "network.proxy.ssl_port"))
        proxy_GetIntPref(prefBranch, "network.proxy.ssl_port", mHTTPSProxyPort);

    if (!pref || !strcmp(pref, "network.proxy.ftp"))
        proxy_GetStringPref(prefBranch, "network.proxy.ftp", mFTPProxyHost);

    if (!pref || !strcmp(pref, "network.proxy.ftp_port"))
        proxy_GetIntPref(prefBranch, "network.proxy.ftp_port", mFTPProxyPort);

    if (!pref || !strcmp(pref, "network.proxy.gopher"))
        proxy_GetStringPref(prefBranch, "network.proxy.gopher", mGopherProxyHost);

    if (!pref || !strcmp(pref, "network.proxy.gopher_port"))
        proxy_GetIntPref(prefBranch, "network.proxy.gopher_port", mGopherProxyPort);

    if (!pref || !strcmp(pref, "network.proxy.socks"))
        proxy_GetStringPref(prefBranch, "network.proxy.socks", mSOCKSProxyHost);
    
    if (!pref || !strcmp(pref, "network.proxy.socks_port"))
        proxy_GetIntPref(prefBranch, "network.proxy.socks_port", mSOCKSProxyPort);

    if (!pref || !strcmp(pref, "network.proxy.socks_version")) {
        PRInt32 version;
        proxy_GetIntPref(prefBranch, "network.proxy.socks_version", version);
        // make sure this preference value remains sane
        if (version == 5)
            mSOCKSProxyVersion = 5;
        else
            mSOCKSProxyVersion = 4;
    }

    if (!pref || !strcmp(pref, "network.proxy.failover_timeout"))
        proxy_GetIntPref(prefBranch, "network.proxy.failover_timeout", mFailedProxyTimeout);

    if (!pref || !strcmp(pref, "network.proxy.no_proxies_on")) {
        rv = prefBranch->GetCharPref("network.proxy.no_proxies_on",
                                     getter_Copies(tempString));
        if (NS_SUCCEEDED(rv))
            LoadFilters(tempString.get());
    }

    if ((!pref || !strcmp(pref, "network.proxy.autoconfig_url") || reloadPAC) &&
        (mUseProxy == 2)) {
        rv = prefBranch->GetCharPref("network.proxy.autoconfig_url", 
                                     getter_Copies(tempString));
        if (NS_SUCCEEDED(rv) && (!reloadPAC || strcmp(tempString.get(), mPACURI.get()))) 
            ConfigureFromPAC(tempString);
    }

    if ((!pref || reloadPAC) && (mUseProxy == 4))
        ConfigureFromWPAD();
}

// this is the main ui thread calling us back, load the pac now
void* PR_CALLBACK
nsProtocolProxyService::HandlePACLoadEvent(PLEvent* aEvent)
{
    nsProtocolProxyService *pps = 
        (nsProtocolProxyService *) PL_GetEventOwner(aEvent);
    if (!pps) {
        NS_ERROR("HandlePACLoadEvent owner is null");
        return nsnull;
    }

    nsresult rv;

    // create pac js component
    pps->mPAC = do_CreateInstance(NS_PROXYAUTOCONFIG_CONTRACTID, &rv);
    if (!pps->mPAC || NS_FAILED(rv)) {
        NS_ERROR("Cannot load PAC js component");
        return nsnull;
    }

    if (pps->mPACURI.IsEmpty()) {
        NS_ERROR("HandlePACLoadEvent: js PACURL component is empty");
        return nsnull;
    }

    nsCOMPtr<nsIIOService> ios = do_GetIOService(&rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("No IO service");
        return nsnull;
    }

    nsCOMPtr<nsIURI> pURI;
    rv = ios->NewURI(pps->mPACURI, nsnull, nsnull, getter_AddRefs(pURI));
    if (NS_FAILED(rv)) {
        NS_ERROR("New URI failed");
        return nsnull;
    }
     
    rv = pps->mPAC->LoadPACFromURI(pURI, ios);
    if (NS_FAILED(rv)) {
        NS_ERROR("Load PAC failed");
        return nsnull;
    }

    return nsnull;
}

void PR_CALLBACK
nsProtocolProxyService::DestroyPACLoadEvent(PLEvent* aEvent)
{
    nsProtocolProxyService *pps = 
        (nsProtocolProxyService*) PL_GetEventOwner(aEvent);
    NS_IF_RELEASE(pps);
    delete aEvent;
}

PRBool
nsProtocolProxyService::CanUseProxy(nsIURI *aURI, PRInt32 defaultPort) 
{
    if (mFiltersArray.Count() == 0)
        return PR_TRUE;

    PRInt32 port;
    nsCAutoString host;
 
    nsresult rv = aURI->GetAsciiHost(host);
    if (NS_FAILED(rv) || host.IsEmpty())
        return PR_FALSE;

    rv = aURI->GetPort(&port);
    if (NS_FAILED(rv))
        return PR_FALSE;
    if (port == -1)
        port = defaultPort;

    PRNetAddr addr;
    PRBool is_ipaddr = (PR_StringToNetAddr(host.get(), &addr) == PR_SUCCESS);

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
            return PR_TRUE; // allow proxying
        }
    }
    
    PRInt32 index = -1;
    while (++index < mFiltersArray.Count()) {
        HostInfo *hinfo = (HostInfo *) mFiltersArray[index];

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
                return PR_FALSE; // proxy disallowed
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
                    return PR_FALSE; // proxy disallowed
            }
        }
    }
    return PR_TRUE;
}

static const char kProxyType_HTTP[]   = "http";
static const char kProxyType_PROXY[]  = "proxy";
static const char kProxyType_SOCKS[]  = "socks";
static const char kProxyType_SOCKS4[] = "socks4";
static const char kProxyType_SOCKS5[] = "socks5";
static const char kProxyType_DIRECT[] = "direct";

const char *
nsProtocolProxyService::ExtractProxyInfo(const char *start, PRBool permitHttp, nsProxyInfo **result)
{
    *result = nsnull;

    // see BNF in nsIProxyAutoConfig.idl

    // find end of proxy info delimiter
    const char *end = start;
    while (*end && *end != ';') ++end;

    // find end of proxy type delimiter
    const char *sp = start;
    while (sp < end && *sp != ' ' && *sp != '\t') ++sp;

    PRUint32 len = sp - start;
    const char *type = nsnull;
    switch (len) {
    case 5:
        if (permitHttp && PL_strncasecmp(start, kProxyType_PROXY, 5) == 0)
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
        const char *host = nsnull, *hostEnd = nsnull;
        PRInt32 port = -1;
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
    dsec += mFailedProxyTimeout;

    // NOTE: The classic codebase would increase the timeout value
    //       incrementally each time a subsequent failure occured.
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

PRBool
nsProtocolProxyService::IsProxyDisabled(nsProxyInfo *pi)
{
    nsCAutoString key;
    GetProxyKey(pi, key);

    PRUint32 val;
    if (!mFailedProxies.Get(key, &val))
        return PR_FALSE;

    PRUint32 dsec = SecondsSinceSessionStart();

    // if time passed has exceeded interval, then try proxy again.
    if (dsec > val) {
        mFailedProxies.Remove(key);
        return PR_FALSE;
    }

    return PR_TRUE;
}

nsProxyInfo *
nsProtocolProxyService::BuildProxyList(const char *proxies,
                                       PRBool permitHttp,
                                       PRBool pruneDisabledProxies)
{
    nsProxyInfo *pi = nsnull, *first = nsnull, *last = nsnull;
    while (*proxies) {
        proxies = ExtractProxyInfo(proxies, permitHttp, &pi);
        if (pi) {
            if (pruneDisabledProxies && IsProxyDisabled(pi))
                NS_RELEASE(pi);
            else {
                if (last) {
                    NS_ASSERTION(last->mNext == nsnull, "leaking nsProxyInfo");
                    last->mNext = pi;
                }
                else
                    first = pi;
                
                // since we are about to use this proxy, make sure it is not
                // on the disabled proxy list.  we'll add it back to that list
                // if we have to (in GetFailoverForProxy).
                EnableProxy(pi);

                last = pi;
            }
        }
    }
    return first;
}

nsresult
nsProtocolProxyService::ExaminePACForProxy(nsIURI *aURI,
                                           PRUint32 aProtoFlags,
                                           nsIProxyInfo **aResult)
{
    // the following two failure cases can happen if we are queried before the
    // PAC file is loaded.  in these cases, we have no choice but to try a 
    // direct connection and hope for the best.  the user will just have to
    // press reload if the page doesn't load :-(

    if (!mPAC) {
        NS_WARNING("PAC JS component is null; assuming DIRECT...");
        return NS_OK;
    }

    nsCAutoString proxyStr;
    nsresult rv = mPAC->GetProxyForURI(aURI, proxyStr);
    if (NS_FAILED(rv)) {
        NS_WARNING("PAC GetProxyForURI failed; assuming DIRECT...");
        return NS_OK;
    }
    if (proxyStr.IsEmpty()) {
        NS_WARNING("PAC GetProxyForURI returned an empty string; assuming DIRECT...");
        return NS_OK;
    }

    PRBool permitHttp = (aProtoFlags & nsIProtocolHandler::ALLOWS_PROXY_HTTP);

    // result is AddRef'd
    nsProxyInfo *pi = BuildProxyList(proxyStr.get(), permitHttp, PR_TRUE);
    if (pi) {
        //
        // if only DIRECT was specified then return no proxy info, and we're done.
        //
        if (!pi->mNext && pi->mType == kProxyType_DIRECT)
            NS_RELEASE(pi);
    }
    else {
        //
        // if all of the proxy servers were marked invalid, then we'll go ahead
        // and return the full list.  this code works by rebuilding the list
        // without pruning disabled proxies.
        //
        // we could have built the whole list and then removed those that were
        // disabled, but it is less code to build the list twice.
        //
        pi = BuildProxyList(proxyStr.get(), permitHttp, PR_FALSE);
    }

    *aResult = pi;
    return NS_OK;
}

// nsIProtocolProxyService
NS_IMETHODIMP
nsProtocolProxyService::ExamineForProxy(nsIURI *aURI, nsIProxyInfo **aResult)
{
    nsresult rv = NS_OK;
    
    NS_ASSERTION(aURI, "need a uri folks.");

    *aResult = nsnull;

    nsCAutoString scheme;
    rv = aURI->GetScheme(scheme);
    if (NS_FAILED(rv)) return rv;

    PRUint32 flags;
    PRInt32 defaultPort;
    rv = GetProtocolInfo(scheme.get(), flags, defaultPort);
    if (NS_FAILED(rv)) return rv;

    if (!(flags & nsIProtocolHandler::ALLOWS_PROXY))
        return NS_OK; // Can't proxy this

    // if proxies are enabled and this host:port combo is
    // supposed to use a proxy, check for a proxy.
    if (0 == mUseProxy || (1 == mUseProxy && !CanUseProxy(aURI, defaultPort)))
        return NS_OK;
    
    // Proxy auto config magic...
    if (2 == mUseProxy || 4 == mUseProxy)
        return ExaminePACForProxy(aURI, flags, aResult);

    // proxy info values
    const char *type = nsnull;
    const nsACString *host = nsnull;
    PRInt32 port = -1;

    if (!mHTTPProxyHost.IsEmpty() && mHTTPProxyPort > 0 &&
        scheme.EqualsLiteral("http")) {
        host = &mHTTPProxyHost;
        type = kProxyType_HTTP;
        port = mHTTPProxyPort;
    }
    else if (!mHTTPSProxyHost.IsEmpty() && mHTTPSProxyPort > 0 &&
             scheme.EqualsLiteral("https")) {
        host = &mHTTPSProxyHost;
        type = kProxyType_HTTP;
        port = mHTTPSProxyPort;
    }
    else if (!mFTPProxyHost.IsEmpty() && mFTPProxyPort > 0 &&
             scheme.EqualsLiteral("ftp")) {
        host = &mFTPProxyHost;
        type = kProxyType_HTTP;
        port = mFTPProxyPort;
    }
    else if (!mGopherProxyHost.IsEmpty() && mGopherProxyPort > 0 &&
             scheme.EqualsLiteral("gopher")) {
        host = &mGopherProxyHost;
        type = kProxyType_HTTP;
        port = mGopherProxyPort;
    }
    else if (!mSOCKSProxyHost.IsEmpty() && mSOCKSProxyPort > 0) {
        host = &mSOCKSProxyHost;
        if (mSOCKSProxyVersion == 4) 
            type = kProxyType_SOCKS4;
        else
            type = kProxyType_SOCKS;
        port = mSOCKSProxyPort;
    }

    if (type)
        return NewProxyInfo_Internal(type, *host, port, aResult);

    return NS_OK;
}

NS_IMETHODIMP
nsProtocolProxyService::NewProxyInfo(const nsACString &aType,
                                     const nsACString &aHost,
                                     PRInt32 aPort,
                                     nsIProxyInfo **aResult)
{
    static const char *types[] = {
        kProxyType_HTTP,
        kProxyType_SOCKS,
        kProxyType_SOCKS4
    };

    // resolve type; this allows us to avoid copying the type string into each
    // proxy info instance.  we just reference the string literals directly :)
    const char *type = nsnull;
    for (PRUint32 i=0; i<NS_ARRAY_LENGTH(types); ++i) {
        if (aType.LowerCaseEqualsASCII(types[i]) == 0) {
            type = types[i];
            break;
        }
    }
    NS_ENSURE_TRUE(type, NS_ERROR_INVALID_ARG);

    if (aPort <= 0)
        aPort = -1;

    return NewProxyInfo_Internal(type, aHost, aPort, aResult);
}

NS_IMETHODIMP
nsProtocolProxyService::ConfigureFromPAC(const nsACString &aURI)
{
    mPACURI = aURI;
    mFailedProxies.Clear();

    nsresult rv;

    // Now we need to setup a callback from the current thread, in which we
    // will load the PAC file from the specified URI.  Loading it now, causes
    // re-entrancy problems for the XPCOM Service Manager (since we require the
    // IO Service in order to load the PAC URI).

    // get current thread's event queue
    nsCOMPtr<nsIEventQueue> eq = nsnull;
    rv = NS_GetCurrentEventQ(getter_AddRefs(eq));
    if (NS_FAILED(rv) || !eq) {
        NS_ERROR("Failed to get current event queue");
        return rv;
    }

    // create an event
    PLEvent* event = new PLEvent;
    // AddRef this because it is being placed in the PLEvent struct
    // It will be Released when DestroyPACLoadEvent is called
    NS_ADDREF_THIS();
    PL_InitEvent(event, this,
            nsProtocolProxyService::HandlePACLoadEvent,
            nsProtocolProxyService::DestroyPACLoadEvent);

    // post the event into the ui event queue
    rv = eq->PostEvent(event);
    if (NS_FAILED(rv)) {
        NS_ERROR("Failed to post PAC load event to UI EventQueue");
        PL_DestroyEvent(event);
    }
    return rv;
}

NS_IMETHODIMP
nsProtocolProxyService::GetProxyEnabled(PRBool *enabled)
{
    NS_ENSURE_ARG_POINTER(enabled);
    *enabled = (mUseProxy != 0);
    return NS_OK;
}

NS_IMETHODIMP
nsProtocolProxyService::GetFailoverForProxy(nsIProxyInfo  *aProxy,
                                            nsIURI        *aURI,
                                            nsresult       aStatus,
                                            nsIProxyInfo **aResult)
{
    // We only support failover when a PAC file is configured.
    if (mUseProxy != 2)
        return NS_ERROR_NOT_AVAILABLE;

    // Verify that |aProxy| is one of our nsProxyInfo objects.
    nsRefPtr<nsProxyInfo> pi;
    aProxy->QueryInterface(kProxyInfoID, getter_AddRefs(pi));
    if (!pi)
        return NS_ERROR_INVALID_ARG;
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

PRBool PR_CALLBACK
nsProtocolProxyService::CleanupFilterArray(void *aElement, void *aData) 
{
    if (aElement)
        delete (HostInfo *) aElement;

    return PR_TRUE;
}

void
nsProtocolProxyService::LoadFilters(const char *filters)
{
    // check to see the owners flag? /!?/ TODO
    if (mFiltersArray.Count() > 0) {
        mFiltersArray.EnumerateForwards(CleanupFilterArray, nsnull);
        mFiltersArray.Clear();
    }

    if (!filters)
        return; // fail silently...

    //
    // filter  = ( host | domain | ipaddr ["/" mask] ) [":" port] 
    // filters = filter *( "," LWS filter)
    //
    while (*filters) {
        // skip over spaces and ,
        while (*filters && (*filters == ',' || IS_ASCII_SPACE(*filters)))
            filters++;

        const char *starthost = filters;
        const char *endhost = filters + 1; // at least that...
        const char *portLocation = 0; 
        const char *maskLocation = 0;

        //
        // XXX this needs to be fixed to support IPv6 address literals,
        // which in this context will need to be []-escaped.
        //
        while (*endhost && (*endhost != ',' && !IS_ASCII_SPACE(*endhost))) {
            if (*endhost == ':')
                portLocation = endhost;
            else if (*endhost == '/')
                maskLocation = endhost;
            endhost++;
        }

        filters = endhost; // advance iterator up front

        HostInfo *hinfo = new HostInfo();
        if (!hinfo)
            return; // fail silently
        hinfo->port = portLocation ? atoi(portLocation + 1) : 0;

        // locate end of host
        const char *end = maskLocation ? maskLocation :
                          portLocation ? portLocation :
                          endhost;

        nsCAutoString str(starthost, end - starthost);

        PRNetAddr addr;
        if (PR_StringToNetAddr(str.get(), &addr) == PR_SUCCESS) {
            hinfo->is_ipaddr   = PR_TRUE;
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

            hinfo->is_ipaddr = PR_FALSE;
            hinfo->name.host = ToNewCString(Substring(str, startIndex, endIndex));

            if (!hinfo->name.host)
                goto loser;

            hinfo->name.host_len = endIndex - startIndex;
        }

//#define DEBUG_DUMP_FILTERS
#ifdef DEBUG_DUMP_FILTERS
        printf("loaded filter[%u]:\n", mFiltersArray.Count());
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

        mFiltersArray.AppendElement(hinfo);
        hinfo = nsnull;
loser:
        if (hinfo)
            delete hinfo;
    }
}

nsresult
nsProtocolProxyService::GetProtocolInfo(const char *aScheme,
                                        PRUint32 &aFlags,
                                        PRInt32 &defaultPort)
{
    nsresult rv;

    nsCOMPtr<nsIIOService> ios = do_GetIOService(&rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIProtocolHandler> handler;
    rv = ios->GetProtocolHandler(aScheme, getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;

    rv = handler->GetProtocolFlags(&aFlags);
    if (NS_FAILED(rv)) return rv;

    return handler->GetDefaultPort(&defaultPort);
}

nsresult
nsProtocolProxyService::NewProxyInfo_Internal(const char *aType,
                                              const nsACString &aHost,
                                              PRInt32 aPort,
                                              nsIProxyInfo **aResult)
{
    nsProxyInfo *proxyInfo = new nsProxyInfo();
    if (!proxyInfo)
        return NS_ERROR_OUT_OF_MEMORY;

    proxyInfo->mType = aType;
    proxyInfo->mHost = aHost;
    proxyInfo->mPort = aPort;

    NS_ADDREF(*aResult = proxyInfo);
    return NS_OK;
}

void
nsProtocolProxyService::ConfigureFromWPAD()
{
    nsCOMPtr<nsIDNSService> dnsService = do_GetService(NS_DNSSERVICE_CONTRACTID);
    if (!dnsService) {
        NS_ERROR("Can't run WPAD: no DNS service?");
        return;
    }
    
    // Asynchronously resolve "wpad", configuring from PAC if we find one (in our
    // OnLookupComplete method).
    //
    // We diverge from the WPAD spec here in that we don't walk the hosts's
    // FQDN, stripping components until we hit a TLD.  Doing so is dangerous in
    // the face of an incomplete list of TLDs, and TLDs get added over time.  We
    // could consider doing only a single substitution of the first component,
    // if that proves to help compatibility.

    nsCOMPtr<nsIEventQueue> curQ;
    nsresult rv = NS_GetCurrentEventQ(getter_AddRefs(curQ));
    if (NS_FAILED(rv)) {
        NS_ERROR("Can't run WPAD: can't get event queue for async resolve!");
        return;
    }
    
    nsCOMPtr<nsIDNSRequest> request;
    rv = dnsService->AsyncResolve(NS_LITERAL_CSTRING("wpad"), PR_TRUE, this, curQ,
                                  getter_AddRefs(request));
    if (NS_FAILED(rv)) {
        NS_ERROR("Can't run WPAD: AsyncResolve failed");
    }
}

NS_IMETHODIMP
nsProtocolProxyService::OnLookupComplete(nsIDNSRequest *aRequest,
                                         nsIDNSRecord *aRecord,
                                         nsresult aStatus)
{
    if (aRecord)
        ConfigureFromPAC(NS_LITERAL_CSTRING("http://wpad/wpad.dat"));
    return NS_OK;
}
