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
#include "nsNetCID.h"
#include "nsIIOService.h"
#include "nsIEventQueueService.h"
#include "nsIProtocolHandler.h"

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


nsProtocolProxyService::nsProtocolProxyService():
    mArrayLock(PR_NewLock()),
    mUseProxy(0),
    mPAC(nsnull)

{
    NS_INIT_REFCNT();
}

nsProtocolProxyService::~nsProtocolProxyService()
{
    if(mArrayLock)
        PR_DestroyLock(mArrayLock);

    if (mFiltersArray.Count() > 0) 
    {
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
nsProtocolProxyService::PrefsChanged(const char* pref) {
    NS_ASSERTION(mPrefs, "No preference service available!");
    if (!mPrefs) return;

    nsresult rv = NS_OK;
    PRBool reloadPAC = PR_FALSE;
    nsXPIDLCString tempString;

    if (!pref || !PL_strcmp(pref, "network.proxy.type"))
    {
        PRInt32 type = -1;
        rv = mPrefs->GetIntPref("network.proxy.type",&type);
        if (NS_SUCCEEDED(rv)) {
            mUseProxy = type; // type == 2 is autoconfig stuff
            reloadPAC = PR_TRUE;
        }
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.http"))
    {
        rv = mPrefs->CopyCharPref("network.proxy.http", 
                getter_Copies(mHTTPProxyHost));
        if (!NS_SUCCEEDED(rv))
            mHTTPProxyHost.Adopt(nsCRT::strdup(""));
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.http_port"))
    {
        mHTTPProxyPort = -1;
        PRInt32 proxyPort;
        rv = mPrefs->GetIntPref("network.proxy.http_port",&proxyPort);
        if (NS_SUCCEEDED(rv)) 
            mHTTPProxyPort = proxyPort;
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.ssl"))
    {
        rv = mPrefs->CopyCharPref("network.proxy.ssl", 
                getter_Copies(mHTTPSProxyHost));
        if (!NS_SUCCEEDED(rv))
            mHTTPSProxyHost.Adopt(nsCRT::strdup(""));
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.ssl_port"))
    {
        mHTTPSProxyPort = -1;
        PRInt32 proxyPort;
        rv = mPrefs->GetIntPref("network.proxy.ssl_port",&proxyPort);
        if (NS_SUCCEEDED(rv)) 
            mHTTPSProxyPort = proxyPort;
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.ftp"))
    {
        rv = mPrefs->CopyCharPref("network.proxy.ftp", 
                getter_Copies(mFTPProxyHost));
        if (!NS_SUCCEEDED(rv))
            mFTPProxyHost.Adopt(nsCRT::strdup(""));
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.ftp_port"))
    {
        mFTPProxyPort = -1;
        PRInt32 proxyPort;
        rv = mPrefs->GetIntPref("network.proxy.ftp_port",&proxyPort);
        if (NS_SUCCEEDED(rv)) 
            mFTPProxyPort = proxyPort;
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.gopher"))
    {
        rv = mPrefs->CopyCharPref("network.proxy.gopher", 
                                      getter_Copies(mGopherProxyHost));
        if (!NS_SUCCEEDED(rv) || !mGopherProxyHost)
            mGopherProxyHost.Adopt(nsCRT::strdup(""));
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.gopher_port"))
    {
        mGopherProxyPort = -1;
        PRInt32 proxyPort = -1;
        rv = mPrefs->GetIntPref("network.proxy.gopher_port",&proxyPort);
        if (NS_SUCCEEDED(rv) && proxyPort>0) 
            mGopherProxyPort = proxyPort;
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.socks"))
    {
        rv = mPrefs->CopyCharPref("network.proxy.socks", 
                                  getter_Copies(mSOCKSProxyHost));
        if (!NS_SUCCEEDED(rv))
            mSOCKSProxyHost.Adopt(nsCRT::strdup(""));
    }
    
    if (!pref || !PL_strcmp(pref, "network.proxy.socks_port"))
    {
        mSOCKSProxyPort = -1;
        PRInt32 proxyPort;
        rv = mPrefs->GetIntPref("network.proxy.socks_port",&proxyPort);
        if (NS_SUCCEEDED(rv)) 
            mSOCKSProxyPort = proxyPort;
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.socks_version"))
    {
        mSOCKSProxyVersion = -1; 
        PRInt32 SOCKSVersion;
        rv = mPrefs->GetIntPref("network.proxy.socks_version",&SOCKSVersion);
        if (NS_SUCCEEDED(rv)) 
            mSOCKSProxyVersion = SOCKSVersion;
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.no_proxies_on"))
    {
        rv = mPrefs->CopyCharPref("network.proxy.no_proxies_on",
                                  getter_Copies(tempString));
        if (NS_SUCCEEDED(rv))
            (void)LoadFilters((const char*)tempString);
    }

    if ((!pref || !PL_strcmp(pref, "network.proxy.autoconfig_url") || reloadPAC) && 
        (mUseProxy == 2))
    {
        rv = mPrefs->CopyCharPref("network.proxy.autoconfig_url", 
                                  getter_Copies(tempString));
        if (NS_SUCCEEDED(rv) && (!reloadPAC || PL_strcmp(tempString, mPACURL))) 
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
    if (!pps->mPAC) {
        NS_ERROR("HandlePACLoadEvent: js PAC component is null");
        return;
    }

    if (!pps->mPACURL) {
        NS_ERROR("HandlePACLoadEvent: js PACURL component is null");
        return;
    }

    nsCOMPtr<nsIIOService> pIOService(do_GetService(kIOServiceCID, &rv));
    if (!pIOService || NS_FAILED(rv)) {
        NS_ERROR("Cannot get IO Service");
        return;
    }

    nsCOMPtr<nsIURI> pURL;
    rv = pIOService->NewURI((const char*) pps->mPACURL, nsnull, 
                            getter_AddRefs(pURL));
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
    nsXPIDLCString host;
    
    nsresult rv = aURI->GetHost(getter_Copies(host));
    if (NS_FAILED(rv) || !host || !*host) 
        return PR_FALSE;
    
    rv = aURI->GetPort(&port);
    if (NS_FAILED(rv)) {
        return PR_FALSE;
    }
    
    PRInt32 index = -1;
    int host_len = PL_strlen(host);
    int filter_host_len;
    
    while (++index < mFiltersArray.Count()) 
    {
        host_port* hp = (host_port*) mFiltersArray[index];
        
        // only if port doesn't exist or matches
        if (((hp->port == -1) || (hp->port == port)) &&
            hp->host)
        {
            filter_host_len = hp->host->Length();
            if ((host_len >= filter_host_len) && 
                (0 == PL_strncasecmp(host + host_len - filter_host_len, 
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

    nsCOMPtr<nsIIOService> ios = do_GetService(kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString scheme;
    rv = aURI->GetScheme(getter_Copies(scheme));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIProtocolHandler> handler;
    rv = ios->GetProtocolHandler(scheme.get(), getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;

    PRUint32 flags;
    rv = handler->GetProtocolFlags(&flags);
    if (NS_FAILED(rv)) return rv;

    if (!(flags & nsIProtocolHandler::ALLOWS_PROXY))
        return NS_OK; // Can't proxy this

    // if proxies are enabled and this host:port combo is
    // supposed to use a proxy, check for a proxy.
    if ((0 == mUseProxy) || 
        ((1 == mUseProxy) && !CanUseProxy(aURI))) {
        return NS_OK;
    }

    nsProxyInfo* proxyInfo = nsnull;
    NS_NEWXPCOM(proxyInfo, nsProxyInfo);
    if (!proxyInfo)
        return NS_ERROR_OUT_OF_MEMORY;
    
    // Proxy auto config magic...
    if (2 == mUseProxy)
    {
        if (!mPAC) {
            NS_ERROR("ERROR: PAC js component is null");
            delete proxyInfo;
            return NS_ERROR_NULL_POINTER;
        }

         rv = mPAC->ProxyForURL(aURI, 
                                &proxyInfo->mHost,
                                &proxyInfo->mPort, 
                                &proxyInfo->mType);
         if (NS_FAILED(rv) || !proxyInfo->Type() ||               // If: it didn't work
             !PL_strcasecmp("direct", proxyInfo->Type()) ||       // OR we're meant to go direct
             (PL_strcasecmp("http", proxyInfo->Type()) != 0 &&    // OR we're an http proxy...
                !(flags & nsIProtocolHandler::ALLOWS_PROXY_HTTP))) { // ... but we can't proxy with http
             delete proxyInfo;                                    // don't proxy this
         } else {
             if (proxyInfo->Port() <= 0)
                proxyInfo->mPort = -1;
             NS_ADDREF(*aResult = proxyInfo);
         }
         return rv;
    }
    
    // Nothing below here returns failure
    NS_ADDREF(*aResult = proxyInfo);

    PRBool isScheme = PR_FALSE;

    if (mHTTPProxyHost.get()[0] && mHTTPProxyPort > 0 &&
        NS_SUCCEEDED(aURI->SchemeIs("http", &isScheme)) && isScheme) {
        proxyInfo->mHost = PL_strdup(mHTTPProxyHost);
        proxyInfo->mType = PL_strdup("http");
        proxyInfo->mPort = mHTTPProxyPort;
        return NS_OK;
    }
    
    if (mHTTPSProxyHost.get()[0] && mHTTPSProxyPort > 0 &&
        NS_SUCCEEDED(aURI->SchemeIs("https", &isScheme)) && isScheme) {
        proxyInfo->mHost = PL_strdup(mHTTPSProxyHost);
        proxyInfo->mType = PL_strdup("http");
        proxyInfo->mPort = mHTTPSProxyPort;
        return NS_OK;
    }
    
    if (mFTPProxyHost.get()[0] && mFTPProxyPort > 0 &&
        NS_SUCCEEDED(aURI->SchemeIs("ftp", &isScheme)) && isScheme) {
        proxyInfo->mHost = PL_strdup(mFTPProxyHost);
        proxyInfo->mType = PL_strdup("http");
        proxyInfo->mPort = mFTPProxyPort;
        return NS_OK;
    }

    if (mGopherProxyHost.get()[0] && mGopherProxyPort > 0 &&
        NS_SUCCEEDED(aURI->SchemeIs("gopher", &isScheme)) && isScheme) {
        proxyInfo->mHost = PL_strdup(mGopherProxyHost);
        proxyInfo->mType = PL_strdup("http");
        proxyInfo->mPort = mGopherProxyPort;
        return NS_OK;
    }
    
    if (mSOCKSProxyHost.get()[0] && mSOCKSProxyPort > 0 &&
        mSOCKSProxyVersion == 4) {
        proxyInfo->mHost = PL_strdup(mSOCKSProxyHost);
        proxyInfo->mPort = mSOCKSProxyPort;
        proxyInfo->mType = PL_strdup("socks4");
        return NS_OK;
    }

    if (mSOCKSProxyHost.get()[0] && mSOCKSProxyPort > 0 &&
        mSOCKSProxyVersion == 5) {
        proxyInfo->mHost = PL_strdup(mSOCKSProxyHost);
        proxyInfo->mPort = mSOCKSProxyPort;
        proxyInfo->mType = PL_strdup("socks");
        return NS_OK;
    }

    NS_RELEASE(*aResult); // Will call destructor
    
    return NS_OK;
}

NS_IMETHODIMP
nsProtocolProxyService::NewProxyInfo(const char* type, const char* host,
                                     PRInt32 port, nsIProxyInfo* *result)
{
    nsProxyInfo* proxyInfo = nsnull;
    NS_NEWXPCOM(proxyInfo, nsProxyInfo);
    if (!proxyInfo)
        return NS_ERROR_OUT_OF_MEMORY;

    if (type)
        proxyInfo->mType = nsCRT::strdup(type);

    if (host)
        proxyInfo->mHost = nsCRT::strdup(host);

    proxyInfo->mPort = port;

    *result = proxyInfo;
    NS_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP
nsProtocolProxyService::ConfigureFromPAC(const char *url)
{
    nsresult rv = NS_OK;
    mPACURL.Adopt(nsCRT::strdup(url));

    // create pac js component
    mPAC = do_CreateInstance(NS_PROXY_AUTO_CONFIG_CONTRACTID, &rv);
    if (!mPAC || NS_FAILED(rv)) {
        NS_ERROR("Cannot load PAC js component");
        return rv;
    }

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
    while (++index < mFiltersArray.Count())
    {
        host_port* hp = (host_port*) mFiltersArray[index];
        if ((hp && hp->host) &&
            (iPort == hp->port) && 
            (0 == PL_strcasecmp((const char*)hp->host, iHost))) 
        {
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
    if (aElement) 
    {
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
    if (mFiltersArray.Count() > 0) 
    {
        mFiltersArray.EnumerateForwards(
            (nsVoidArrayEnumFunc)this->CleanupFilterArray, nsnull);
        mFiltersArray.Clear();
    }

    if (!filters)
        return ;//fail silently...

    char* np = (char*)filters;
    while (*np)
    {
        // skip over spaces and ,
        while (*np && (*np == ',' || nsCRT::IsAsciiSpace(*np)))
            np++;

        char* endproxy = np+1; // at least that...
        char* portLocation = 0; 
        PRInt32 nport = 0; // no proxy port
        while (*endproxy && (*endproxy != ',' && 
                    !nsCRT::IsAsciiSpace(*endproxy)))
        {
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
