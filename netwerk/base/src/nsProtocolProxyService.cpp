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

#include "nsProtocolProxyService.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsIProxyAutoConfig.h"
#include "nsAutoLock.h"

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

static const char PROXY_PREFS[] = "network.proxy";
static PRInt32 PR_CALLBACK ProxyPrefsCallback(const char* pref, void* instance)
{
    nsProtocolProxyService* proxyServ = (nsProtocolProxyService*) instance;
    NS_ASSERTION(proxyServ, "bad instance data");
    if (proxyServ) proxyServ->PrefsChanged(pref);
    return 0;
}


NS_IMPL_THREADSAFE_ISUPPORTS1(nsProtocolProxyService, nsIProtocolProxyService);


nsProtocolProxyService::nsProtocolProxyService():
    mUseProxy(0),
    mArrayLock(PR_NewLock())
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
    nsXPIDLCString tempString;

    if (!pref || !PL_strcmp(pref, "network.proxy.type"))
    {
        PRInt32 type = -1;
        rv = mPrefs->GetIntPref("network.proxy.type",&type);
        if (NS_SUCCEEDED(rv))
            mUseProxy = type; // type == 2 is autoconfig stuff
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.http"))
    {
        mHTTPProxyHost = "";
        rv = mPrefs->CopyCharPref("network.proxy.http", 
                getter_Copies(tempString));
        if (NS_SUCCEEDED(rv) && tempString && *tempString)
            mHTTPProxyHost = nsCRT::strdup(tempString);
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.http_port"))
    {
        mHTTPProxyPort = -1;
        PRInt32 proxyPort = -1;
        rv = mPrefs->GetIntPref("network.proxy.http_port",&proxyPort);
        if (NS_SUCCEEDED(rv) && proxyPort>0) 
            mHTTPProxyPort = proxyPort;
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.ssl"))
    {
        mHTTPSProxyHost = "";
        rv = mPrefs->CopyCharPref("network.proxy.ssl", 
                getter_Copies(tempString));
        if (NS_SUCCEEDED(rv) && tempString && *tempString)
            mHTTPSProxyHost = nsCRT::strdup(tempString);
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.ssl_port"))
    {
        mHTTPSProxyPort = -1;
        PRInt32 proxyPort = -1;
        rv = mPrefs->GetIntPref("network.proxy.ssl_port",&proxyPort);
        if (NS_SUCCEEDED(rv) && proxyPort>0) 
            mHTTPSProxyPort = proxyPort;
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.ftp"))
    {
        mFTPProxyHost = "";
        rv = mPrefs->CopyCharPref("network.proxy.ftp", 
                getter_Copies(tempString));
        if (NS_SUCCEEDED(rv) && tempString && *tempString)
            mFTPProxyHost = nsCRT::strdup(tempString);
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.ftp_port"))
    {
        mFTPProxyPort = -1;
        PRInt32 proxyPort = -1;
        rv = mPrefs->GetIntPref("network.proxy.ftp_port",&proxyPort);
        if (NS_SUCCEEDED(rv) && proxyPort>0) 
            mFTPProxyPort = proxyPort;
    }

    if (!pref || !PL_strcmp(pref, "network.proxy.socks"))
    {
        mSOCKSProxyHost = "";
        rv = mPrefs->CopyCharPref("network.proxy.socks", 
                                  getter_Copies(tempString));
        if (NS_SUCCEEDED(rv) && tempString && *tempString)
            mSOCKSProxyHost = nsCRT::strdup(tempString);
    }
    
    if (!pref || !PL_strcmp(pref, "network.proxy.socks_port"))
    {
        mSOCKSProxyPort = -1;
        PRInt32 proxyPort = -1;
        rv = mPrefs->GetIntPref("network.proxy.socks_port",&proxyPort);
        if (NS_SUCCEEDED(rv) && proxyPort>0) 
            mSOCKSProxyPort = proxyPort;
    }
    
    if (!pref || !PL_strcmp(pref, "network.proxy.no_proxies_on"))
    {
        rv = mPrefs->CopyCharPref("network.proxy.no_proxies_on",
                                  getter_Copies(tempString));
        if (NS_SUCCEEDED(rv) && tempString && *tempString)
            (void)LoadFilters((const char*)tempString);
    }
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
                                     hp->host->GetBuffer(), filter_host_len)))
                return PR_FALSE;
        }
    }
    return PR_TRUE;
}

// nsIProtocolProxyService
NS_IMETHODIMP
nsProtocolProxyService::ExamineForProxy(nsIURI *aURI, nsIProxy *aProxy) {
    nsresult rv = NS_OK;
    
    NS_ASSERTION(aURI && aProxy, "need a uri and proxy iface folks.");
    
    // if proxies are enabled and this host:port combo is
    // supposed to use a proxy, check for a proxy.
    if ((0 == mUseProxy) || 
        ((1 == mUseProxy) && !CanUseProxy(aURI))) {
        rv = aProxy->SetProxyHost(nsnull);
        if (NS_FAILED(rv)) return rv;
        rv = aProxy->SetProxyPort(-1);
        if (NS_FAILED(rv)) return rv;
        return NS_OK;
    }
    
    // Proxy auto config magic...
    if (2 == mUseProxy)
    {
        // later on put this in a member variable TODO
        nsCOMPtr<nsIProxyAutoConfig> pac = 
            do_CreateInstance(NS_PROXY_AUTO_CONFIG_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv))
        {
            nsXPIDLCString p_host;
            nsXPIDLCString p_type;
            PRInt32 p_port;
            if (NS_SUCCEEDED(rv = pac->ProxyForURL(aURI, getter_Copies(p_host),
                                               &p_port, getter_Copies(p_type))))
            {
                if (NS_FAILED(rv = aProxy->SetProxyHost(p_host))) 
                    return rv;
                if (NS_FAILED(rv = aProxy->SetProxyPort(p_port)))
                    return rv;
                if (NS_FAILED(rv = aProxy->SetProxyType(p_type)))
                    return rv;
                return NS_OK;
            }
        }
        return rv;
    }
    
    nsXPIDLCString scheme;
    rv = aURI->GetScheme(getter_Copies(scheme));
    if (NS_FAILED(rv)) return rv;
    
    if (mFTPProxyHost.get()[0] && mFTPProxyPort > 0 &&
        !PL_strcasecmp(scheme, "ftp")) {
        rv = aProxy->SetProxyHost(mFTPProxyHost);
        if (NS_FAILED(rv)) return rv;
        aProxy->SetProxyType("http");
        return aProxy->SetProxyPort(mFTPProxyPort);
    }
    
    if (mHTTPProxyHost.get()[0] && mHTTPProxyPort > 0 &&
        !PL_strcasecmp(scheme, "http")) {
        rv = aProxy->SetProxyHost(mHTTPProxyHost);
        if (NS_FAILED(rv)) return rv;
        aProxy->SetProxyType("http");
        return aProxy->SetProxyPort(mHTTPProxyPort);
    }
    
    if (mHTTPSProxyHost.get()[0] && mHTTPSProxyPort > 0 &&
        !PL_strcasecmp(scheme, "https")) {
        rv = aProxy->SetProxyHost(mHTTPSProxyHost);
        if (NS_FAILED(rv)) return rv;
        aProxy->SetProxyType("http");
        return aProxy->SetProxyPort(mHTTPSProxyPort);
    }
    
    if (mSOCKSProxyHost.get()[0] && mSOCKSProxyPort > 0) {
        rv = aProxy->SetProxyHost(mSOCKSProxyHost);
        if (NS_FAILED(rv)) return rv;
        aProxy->SetProxyType("socks");
        return aProxy->SetProxyPort(mSOCKSProxyPort);
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
        if (!hp->host)
            return;
        hp->port = nport>0 ? nport : -1;

        mFiltersArray.AppendElement(hp);
        np = endproxy;
    }
}

