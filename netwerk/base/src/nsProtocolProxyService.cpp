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

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static const char PROXY_PREFS[] = "network.proxy";
PRInt32 PR_CALLBACK ProxyPrefsCallback(const char* pref, void* instance)
{
    nsProtocolProxyService* proxyServ = (nsProtocolProxyService*) instance;
    NS_ASSERTION(proxyServ, "bad instance data");
    if (proxyServ) proxyServ->PrefsChanged(pref);
    return 0;
}


NS_IMPL_ISUPPORTS1(nsProtocolProxyService, nsIProtocolProxyService);


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
    PRBool bChangeAll = (pref) ? PR_FALSE : PR_TRUE;
    NS_ASSERTION(mPrefs, "No preference service available!");
    if (!mPrefs) return;

    nsresult rv = NS_OK;

    if (bChangeAll || !PL_strcmp(pref, "network.proxy.type"))
    {
        PRInt32 type = -1;
        rv = mPrefs->GetIntPref("network.proxy.type",&type);
        if (NS_SUCCEEDED(rv))
            mUseProxy = (type == 1); // type == 2 is autoconfig stuff
    }

    nsXPIDLCString proxyServer;
    if (bChangeAll || !PL_strcmp(pref, "network.proxy.http"))
    {
        rv = mPrefs->CopyCharPref("network.proxy.http", 
                getter_Copies(proxyServer));
        if (NS_SUCCEEDED(rv) && proxyServer && *proxyServer)
            mHTTPProxyHost = nsCRT::strdup(proxyServer);
    }

    if (bChangeAll || !PL_strcmp(pref, "network.proxy.http_port"))
    {
        PRInt32 proxyPort = -1;
        rv = mPrefs->GetIntPref("network.proxy.http_port",&proxyPort);
        if (NS_SUCCEEDED(rv) && proxyPort>0) 
            mHTTPProxyPort = proxyPort;
    }

    if (bChangeAll || !PL_strcmp(pref, "network.proxy.ftp"))
    {
        rv = mPrefs->CopyCharPref("network.proxy.ftp", 
                getter_Copies(proxyServer));
        if (NS_SUCCEEDED(rv) && proxyServer && *proxyServer)
            mFTPProxyHost = nsCRT::strdup(proxyServer);
    }

    if (bChangeAll || !PL_strcmp(pref, "network.proxy.ftp_port"))
    {
        PRInt32 proxyPort = -1;
        rv = mPrefs->GetIntPref("network.proxy.ftp_port",&proxyPort);
        if (NS_SUCCEEDED(rv) && proxyPort>0) 
            mFTPProxyPort = proxyPort;
    }

    if (bChangeAll || !PL_strcmp(pref, "network.proxy.no_proxies_on"))
    {
        (void)mPrefs->CopyCharPref("network.proxy.no_proxies_on",
                &mFilters);
    }
}

PRBool
nsProtocolProxyService::CanUseProxy(nsIURI* aURI) {
    PRBool rv = PR_TRUE;
    if (!mFilters || !*mFilters) return rv;

    PRInt32 port;
    char *host = nsnull;

    rv = aURI->GetHost(&host);
    if (NS_FAILED(rv) || !host || !*host) return PR_FALSE;

    rv = aURI->GetPort(&port);
    if (NS_FAILED(rv)) {
        nsAllocator::Free(host);
        return PR_FALSE;
    }

    // mFilters is of type "foo bar:8080, baz.com ..."
    char* brk = PL_strpbrk(mFilters, " ,:");
    char* noProxy = mFilters;
    PRInt32 noProxyPort = -1;
    int hostLen = PL_strlen(host);
    char* end = (char*)host+hostLen;
    char* nextbrk = end;
    int matchLen = 0;
    do 
    {
        if (brk)
        {
            if (*brk == ':')
            {
                noProxyPort = atoi(brk+1);
                nextbrk = PL_strpbrk(brk+1, " ,");
                if (!nextbrk)
                    nextbrk = end;
            }
            matchLen = brk-noProxy;
        }
        else
            matchLen = end-noProxy;

        if (matchLen <= hostLen) // match is smaller than host
        {
            if (((noProxyPort == -1) || (noProxyPort == port)) &&
                (0 == PL_strncasecmp(host+hostLen-matchLen, 
                             noProxy, matchLen)))
                {
                    nsCRT::free(host);
                    return PR_FALSE;
                }
        }

        noProxy = nextbrk;
        brk = PL_strpbrk(noProxy, " ,:");
    }
    while (brk);

    nsAllocator::Free(host);
    return rv;
}


// nsIProtocolProxyService
NS_IMETHODIMP
nsProtocolProxyService::ExamineForProxy(nsIURI *aURI, nsIProxy *aProxy) {
    nsresult rv = NS_OK;

    NS_ASSERTION(aURI && aProxy, "need a uri and proxy iface folks.");

    // if proxies are enabled and this host:port combo is
    // supposed to use a proxy, check for a proxy.
    if (!mUseProxy || !CanUseProxy(aURI)) {
        rv = aProxy->SetProxyHost(nsnull);
        if (NS_FAILED(rv)) return rv;
        rv = aProxy->SetProxyPort(-1);
        if (NS_FAILED(rv)) return rv;
        return NS_OK;
    }

    nsXPIDLCString scheme;
    rv = aURI->GetScheme(getter_Copies(scheme));
    if (NS_FAILED(rv)) return rv;

    if (!PL_strcmp(scheme, "http")) {
        rv = aProxy->SetProxyHost(mHTTPProxyHost);
        if (NS_FAILED(rv)) return rv;

        return aProxy->SetProxyPort(mHTTPProxyPort);
    }

    if (!PL_strcmp(scheme, "ftp")) {
        rv = aProxy->SetProxyHost(mFTPProxyHost);
        if (NS_FAILED(rv)) return rv;

        return aProxy->SetProxyPort(mFTPProxyPort);
    }
    return NS_OK;
}

