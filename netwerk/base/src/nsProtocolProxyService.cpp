/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "nsProtocolProxyService.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsIProxyAutoConfig.h"
#include "nsAutoLock.h"
#include "nsIIOService.h"
#include "nsIEventQueueService.h"
#include "nsIProtocolHandler.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsCRT.h"

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

static const char PROXY_PREFS[] = "network.proxy";
static PRInt32 PR_CALLBACK ProxyPrefsCallback(const char* pref, void* instance)
{
    nsProtocolProxyService* proxyServ = (nsProtocolProxyService*) instance;
    NS_ASSERTION(proxyServ, "bad instance data");
    if (proxyServ) proxyServ->PrefsChanged(pref);
    return 0;
}


NS_IMPL_THREADSAFE_ISUPPORTS1(nsProtocolProxyService, nsIProtocolProxyService);
NS_IMPL_THREADSAFE_ISUPPORTS1(nsProtocolProxyService::nsProxyInfo, nsIProxyInfo);


nsProtocolProxyService::nsProtocolProxyService()
    : mArrayLock(PR_NewLock())
    , mUseProxy(0)
    , mHTTPProxyPort(-1)
    , mFTPProxyPort(-1)
    , mGopherProxyPort(-1)
    , mHTTPSProxyPort(-1)
    , mSOCKSProxyPort(-1)
    , mSOCKSProxyVersion(4)
{
}

nsProtocolProxyService::~nsProtocolProxyService()
{
    if(mArrayLock)
        PR_DestroyLock(mArrayLock);

    if (mFiltersArray.Count() > 0) {
        mFiltersArray.EnumerateForwards(
                (nsVoidArrayEnumFunc)this->CleanupFilterArray, nsnull);
        mFiltersArray.Clear();
    }
}

// nsProtocolProxyService methods
NS_IMETHODIMP
nsProtocolProxyService::Init() {
    nsresult rv = NS_OK;

    mPrefs = do_GetService(kPrefServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // register for change callbacks
    rv = mPrefs->RegisterCallback(PROXY_PREFS, ProxyPrefsCallback, (void*)this);
    if (NS_FAILED(rv)) return rv;

    PrefsChanged(nsnull);
    return NS_OK;
}

NS_METHOD
nsProtocolProxyService::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult) {
    nsresult rv;
    if (aOuter) return NS_ERROR_NO_AGGREGATION;

    nsProtocolProxyService* serv = new nsProtocolProxyService();
    if (!serv) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(serv);
    rv = serv->Init();
    if (NS_FAILED(rv)) {
        delete serv;
        return rv;
    }
    rv = serv->QueryInterface(aIID, aResult);
    NS_RELEASE(serv);
    return rv;
}

void
nsProtocolProxyService::LoadStringPref(const char *aPref, nsCString &aResult)
{
    nsXPIDLCString temp;
    nsresult rv;
    
    rv = mPrefs->CopyCharPref(aPref, getter_Copies(temp));
    if (NS_FAILED(rv))
        aResult.Truncate();
    else {
        aResult.Assign(temp);
        // all of our string prefs are hostnames, so we should remove any
        // whitespace characters that the user might have unknowingly entered.
        aResult.StripWhitespace();
    }
}

void
nsProtocolProxyService::LoadIntPref(const char *aPref, PRInt32 &aResult)
{
    PRInt32 temp;
    nsresult rv;

    rv = mPrefs->GetIntPref(aPref, &temp);
    if (NS_FAILED(rv)) 
        aResult = -1;
    else
        aResult = temp;
}

void
nsProtocolProxyService::PrefsChanged(const char* pref)
{
    NS_ASSERTION(mPrefs, "No preference service available!");
    if (!mPrefs) return;

    nsresult rv = NS_OK;
    PRBool reloadPAC = PR_FALSE;
    nsXPIDLCString tempString;

    if (!pref || !strcmp(pref, "network.proxy.type")) {
        PRInt32 type = -1;
        rv = mPrefs->GetIntPref("network.proxy.type",&type);
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
                    mPrefs->SetIntPref("network.proxy.type", 0);
            }
            mUseProxy = type; // type == 2 is autoconfig stuff
            reloadPAC = PR_TRUE;
        }
    }

    if (!pref || !strcmp(pref, "network.proxy.http"))
        LoadStringPref("network.proxy.http", mHTTPProxyHost);

    if (!pref || !strcmp(pref, "network.proxy.http_port"))
        LoadIntPref("network.proxy.http_port", mHTTPProxyPort);

    if (!pref || !strcmp(pref, "network.proxy.ssl"))
        LoadStringPref("network.proxy.ssl", mHTTPSProxyHost);

    if (!pref || !strcmp(pref, "network.proxy.ssl_port"))
        LoadIntPref("network.proxy.ssl_port", mHTTPSProxyPort);

    if (!pref || !strcmp(pref, "network.proxy.ftp"))
        LoadStringPref("network.proxy.ftp", mFTPProxyHost);

    if (!pref || !strcmp(pref, "network.proxy.ftp_port"))
        LoadIntPref("network.proxy.ftp_port", mFTPProxyPort);

    if (!pref || !strcmp(pref, "network.proxy.gopher"))
        LoadStringPref("network.proxy.gopher", mGopherProxyHost);

    if (!pref || !strcmp(pref, "network.proxy.gopher_port"))
        LoadIntPref("network.proxy.gopher_port", mGopherProxyPort);

    if (!pref || !strcmp(pref, "network.proxy.socks"))
        LoadStringPref("network.proxy.socks", mSOCKSProxyHost);
    
    if (!pref || !strcmp(pref, "network.proxy.socks_port"))
        LoadIntPref("network.proxy.socks_port", mSOCKSProxyPort);

    if (!pref || !strcmp(pref, "network.proxy.socks_version")) {
        PRInt32 version;
        LoadIntPref("network.proxy.socks_version", version);
        // make sure this preference value remains sane
        if (version == 5)
            mSOCKSProxyVersion = 5;
        else
            mSOCKSProxyVersion = 4;
    }

    if (!pref || !strcmp(pref, "network.proxy.no_proxies_on")) {
        rv = mPrefs->CopyCharPref("network.proxy.no_proxies_on",
                                  getter_Copies(tempString));
        if (NS_SUCCEEDED(rv))
            LoadFilters(tempString.get());
    }

    if ((!pref || !strcmp(pref, "network.proxy.autoconfig_url") || reloadPAC) && (mUseProxy == 2)) {
        rv = mPrefs->CopyCharPref("network.proxy.autoconfig_url", 
                                  getter_Copies(tempString));
        if (NS_SUCCEEDED(rv) && (!reloadPAC || strcmp(tempString.get(), mPACURL.get()))) 
            ConfigureFromPAC(tempString);
    }
}

// this is the main ui thread calling us back, load the pac now
void PR_CALLBACK nsProtocolProxyService::HandlePACLoadEvent(PLEvent* aEvent)
{
    nsresult rv = NS_OK;

    nsProtocolProxyService *pps = 
        (nsProtocolProxyService*) PL_GetEventOwner(aEvent);
    if (!pps) {
        NS_ERROR("HandlePACLoadEvent owner is null");
        return;
    }

    // create pac js component
    pps->mPAC = do_CreateInstance(NS_PROXY_AUTO_CONFIG_CONTRACTID, &rv);
    if (!pps->mPAC || NS_FAILED(rv)) {
        NS_ERROR("Cannot load PAC js component");
        return;
    }

    if (pps->mPACURL.IsEmpty()) {
        NS_ERROR("HandlePACLoadEvent: js PACURL component is empty");
        return;
    }

    nsCOMPtr<nsIIOService> pIOService(do_GetService(kIOServiceCID, &rv));
    if (!pIOService || NS_FAILED(rv)) {
        NS_ERROR("Cannot get IO Service");
        return;
    }

    nsCOMPtr<nsIURI> pURL;
    rv = pIOService->NewURI(pps->mPACURL, nsnull, nsnull, getter_AddRefs(pURL));
    if (NS_FAILED(rv)) {
        NS_ERROR("New URI failed");
        return;
    }
     
    rv = pps->mPAC->LoadPACFromURL(pURL, pIOService);
    if (NS_FAILED(rv)) {
        NS_ERROR("Load PAC failed");
        return;
    }
}

void PR_CALLBACK nsProtocolProxyService::DestroyPACLoadEvent(PLEvent* aEvent)
{
    nsProtocolProxyService *pps = 
        (nsProtocolProxyService*) PL_GetEventOwner(aEvent);
    NS_IF_RELEASE(pps);
    delete aEvent;
}

PRBool
nsProtocolProxyService::CanUseProxy(nsIURI* aURI) 
{
    if (mFiltersArray.Count() == 0)
        return PR_TRUE;

    PRInt32 port;
    nsCAutoString host;
    
    nsresult rv = aURI->GetAsciiHost(host);
    if (NS_FAILED(rv) || host.IsEmpty())
        return PR_FALSE;
    
    rv = aURI->GetPort(&port);
    if (NS_FAILED(rv)) {
        return PR_FALSE;
    }
    
    PRInt32 index = -1;
    int host_len = host.Length();
    int filter_host_len;
    
    while (++index < mFiltersArray.Count()) {
        host_port* hp = (host_port*) mFiltersArray[index];
        
        // only if port doesn't exist or matches
        if (((hp->port == -1) || (hp->port == port)) && hp->host) {
            filter_host_len = hp->host->Length();
            if ((host_len >= filter_host_len) && 
                (0 == PL_strncasecmp(host.get() + host_len - filter_host_len, 
                                     hp->host->get(), filter_host_len)))
                return PR_FALSE;
        }
    }
    return PR_TRUE;
}

// nsIProtocolProxyService
NS_IMETHODIMP
nsProtocolProxyService::ExamineForProxy(nsIURI *aURI, nsIProxyInfo* *aResult) {
    nsresult rv = NS_OK;
    
    NS_ASSERTION(aURI, "need a uri folks.");

    *aResult = nsnull;

    nsCAutoString scheme;
    rv = aURI->GetScheme(scheme);
    if (NS_FAILED(rv)) return rv;

    PRUint32 flags;
    rv = GetProtocolFlags(scheme.get(), &flags);
    if (NS_FAILED(rv)) return rv;

    if (!(flags & nsIProtocolHandler::ALLOWS_PROXY))
        return NS_OK; // Can't proxy this

    // if proxies are enabled and this host:port combo is
    // supposed to use a proxy, check for a proxy.
    if (0 == mUseProxy || (1 == mUseProxy && !CanUseProxy(aURI)))
        return NS_OK;

    // proxy info values
    const char *type = nsnull;
    char *host = nsnull;
    PRInt32 port = -1;
    
    // Proxy auto config magic...
    if (2 == mUseProxy) {
        if (!mPAC) {
            NS_ERROR("ERROR: PAC js component is null, assuming DIRECT");
            return NS_OK; // assume DIRECT connection for now
        }

        nsXPIDLCString rawType; // XXX an enum might make better sense here

        rv = mPAC->ProxyForURL(aURI, &host, &port, getter_Copies(rawType));
        if (NS_SUCCEEDED(rv) && rawType && host) {
            //
            // Accept only known values for the proxy type
            //
            if (PL_strcasecmp(rawType, "http") == 0) {
                if (flags & nsIProtocolHandler::ALLOWS_PROXY_HTTP)
                    type = "http";
            }
            else if (PL_strcasecmp(rawType, "socks") == 0)
                type = "socks";
            else if (PL_strcasecmp(rawType, "socks4") == 0)
                type = "socks4";
        }

        if (type) {
            if (port <= 0)
                port = -1;
            return NewProxyInfo_Internal(type, host, port, aResult);
        }

        // assume errors mean direct - its better than just failing, and
        // the js conosle will have the specific error
        if (host)
            nsMemory::Free(host);
        return NS_OK;
    }

    if (!mHTTPProxyHost.IsEmpty() && mHTTPProxyPort > 0 &&
        scheme.Equals(NS_LITERAL_CSTRING("http"))) {
        host = ToNewCString(mHTTPProxyHost);
        type = "http";
        port = mHTTPProxyPort;
    }
    else if (!mHTTPSProxyHost.IsEmpty() && mHTTPSProxyPort > 0 &&
             scheme.Equals(NS_LITERAL_CSTRING("https"))) {
        host = ToNewCString(mHTTPSProxyHost);
        type = "http";
        port = mHTTPSProxyPort;
    }
    else if (!mFTPProxyHost.IsEmpty() && mFTPProxyPort > 0 &&
             scheme.Equals(NS_LITERAL_CSTRING("ftp"))) {
        host = ToNewCString(mFTPProxyHost);
        type = "http";
        port = mFTPProxyPort;
    }
    else if (!mGopherProxyHost.IsEmpty() && mGopherProxyPort > 0 &&
             scheme.Equals(NS_LITERAL_CSTRING("gopher"))) {
        host = ToNewCString(mGopherProxyHost);
        type = "http";
        port = mGopherProxyPort;
    }
    else if (!mSOCKSProxyHost.IsEmpty() && mSOCKSProxyPort > 0) {
        host = ToNewCString(mSOCKSProxyHost);
        if (mSOCKSProxyVersion == 4) 
            type = "socks4";
        else
            type = "socks";
        port = mSOCKSProxyPort;
    }

    if (type)
        return NewProxyInfo_Internal(type, host, port, aResult);

    return NS_OK;
}

NS_IMETHODIMP
nsProtocolProxyService::NewProxyInfo(const char *aType,
                                     const char *aHost,
                                     PRInt32 aPort,
                                     nsIProxyInfo **aResult)
{
    const char *type = nsnull;

    // canonicalize type
    if (PL_strcasecmp(aType, "http"))
        type = "http";
    else if (PL_strcasecmp(aType, "socks"))
        type = "socks";
    else if (PL_strcasecmp(aType, "socks4"))
        type = "socks4";
    else
        return NS_ERROR_INVALID_ARG;

    if (aPort <= 0)
        aPort = -1;

    return NewProxyInfo_Internal(type, nsCRT::strdup(aHost), aPort, aResult);
}

NS_IMETHODIMP
nsProtocolProxyService::ConfigureFromPAC(const char *url)
{
    nsresult rv = NS_OK;
    mPACURL.Assign(url);

    /* now we need to setup a callback from the main ui thread
       in which we will load the pac file from the specified
       url. loading it now, in the current thread results in a
       browser crash */

    // get event queue service
    nsCOMPtr<nsIEventQueueService> eqs = 
        do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID);
    if (!eqs) {
        NS_ERROR("Failed to get EventQueue service");
        return rv;
    }

    // get ui thread's event queue
    nsCOMPtr<nsIEventQueue> eq = nsnull;
    rv = eqs->GetThreadEventQueue(NS_UI_THREAD, getter_AddRefs(eq));
    if (NS_FAILED(rv) || !eqs) {
        NS_ERROR("Failed to get UI EventQueue");
        return rv;
    }

    // create an event
    PLEvent* event = new PLEvent;
    // AddRef this because it is being placed in the PLEvent struct
    // It will be Released when DestroyPACLoadEvent is called
    NS_ADDREF_THIS();
    PL_InitEvent(event, 
            this,
            (PLHandleEventProc) 
            nsProtocolProxyService::HandlePACLoadEvent,
            (PLDestroyEventProc) 
            nsProtocolProxyService::DestroyPACLoadEvent);

    // post the event into the ui event queue
    if (eq->PostEvent(event) == PR_FAILURE) {
        NS_ERROR("Failed to post PAC load event to UI EventQueue");
        NS_RELEASE_THIS();
        delete event;
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsProtocolProxyService::GetProxyEnabled(PRBool* o_Enabled)
{
    if (!o_Enabled)
        return NS_ERROR_NULL_POINTER;
    *o_Enabled = mUseProxy;
    return NS_OK;
}

NS_IMETHODIMP
nsProtocolProxyService::AddNoProxyFor(const char* iHost, PRInt32 iPort)
{
    if (!iHost)
        return NS_ERROR_NULL_POINTER;

    host_port* hp = new host_port();
    if (!hp)
        return NS_ERROR_OUT_OF_MEMORY;
    hp->host = new nsCString(iHost);
    if (!hp->host) {
        delete hp;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    hp->port = iPort;
    
    nsAutoLock lock(mArrayLock);
    return (mFiltersArray.AppendElement(hp)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsProtocolProxyService::RemoveNoProxyFor(const char* iHost, PRInt32 iPort)
{
    if (!iHost)
        return NS_ERROR_NULL_POINTER;
    
    nsAutoLock lock(mArrayLock);

    if (mFiltersArray.Count()==0)
        return NS_ERROR_FAILURE;

    PRInt32 index = -1;
    while (++index < mFiltersArray.Count()) {
        host_port* hp = (host_port*) mFiltersArray[index];
        if ((hp && hp->host) && (iPort == hp->port) && 
            (0 == PL_strcasecmp((const char*)hp->host, iHost))) {
            delete hp->host;
            delete hp;
            mFiltersArray.RemoveElementAt(index);
            return NS_OK;
        }
    }
    return NS_ERROR_FAILURE; // not found
}

PRBool 
nsProtocolProxyService::CleanupFilterArray(void* aElement, void* aData) 
{
    if (aElement) {
        host_port* hp = (host_port*)aElement;
        delete hp->host;
        delete hp;
    }
    return PR_TRUE;
}

void
nsProtocolProxyService::LoadFilters(const char* filters)
{
    host_port* hp;
    // check to see the owners flag? /!?/ TODO
    if (mFiltersArray.Count() > 0) {
        mFiltersArray.EnumerateForwards(
            (nsVoidArrayEnumFunc)this->CleanupFilterArray, nsnull);
        mFiltersArray.Clear();
    }

    if (!filters)
        return ;//fail silently...

    char* np = (char*)filters;
    while (*np) {
        // skip over spaces and ,
        while (*np && (*np == ',' || nsCRT::IsAsciiSpace(*np)))
            np++;

        char* endproxy = np+1; // at least that...
        char* portLocation = 0; 
        PRInt32 nport = 0; // no proxy port
        while (*endproxy && (*endproxy != ',' && !nsCRT::IsAsciiSpace(*endproxy))) {
            if (*endproxy == ':')
                portLocation = endproxy;
            endproxy++;
        }
        if (portLocation)
            nport = atoi(portLocation+1);

        hp = new host_port();
        if (!hp)
            return; // fail silently
        hp->host = new nsCString(np, endproxy-np);
        if (!hp->host) {
            delete hp;
            return;
        }
        hp->port = nport>0 ? nport : -1;

        mFiltersArray.AppendElement(hp);
        np = endproxy;
    }
}

nsresult
nsProtocolProxyService::GetProtocolFlags(const char *aScheme, PRUint32 *aFlags)
{
    nsresult rv;

    if (!mIOService) {
        mIOService = do_GetIOService(&rv);
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsIProtocolHandler> handler;
    rv = mIOService->GetProtocolHandler(aScheme, getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;

    return handler->GetProtocolFlags(aFlags);
}

nsresult
nsProtocolProxyService::NewProxyInfo_Internal(const char *aType,
                                              char *aHost,
                                              PRInt32 aPort,
                                              nsIProxyInfo **aResult)
{
    nsProxyInfo *proxyInfo = nsnull;
    NS_NEWXPCOM(proxyInfo, nsProxyInfo);
    if (!proxyInfo)
        return NS_ERROR_OUT_OF_MEMORY;

    proxyInfo->mType = aType;
    proxyInfo->mHost = aHost;
    proxyInfo->mPort = aPort;

    NS_ADDREF(*aResult = proxyInfo);
    return NS_OK;
}
