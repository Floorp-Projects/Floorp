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
 *   Gagan Saksena <gagan@netscape.com> (original author)
 *   Darin Fisher <darin@netscape.com>
 */

#include "nsAuthEngine.h"
#include "nsAuth.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "plstr.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsNetCID.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

MOZ_DECL_CTOR_COUNTER(nsAuthEngine)

nsAuthEngine::nsAuthEngine()
{
    MOZ_COUNT_CTOR(nsAuthEngine);
    if (NS_FAILED(Init()))
        NS_ERROR("Failed to initialize the auth engine!");
}

nsAuthEngine::~nsAuthEngine()
{
    MOZ_COUNT_DTOR(nsAuthEngine);
    Logout();
}

nsresult
nsAuthEngine::Init()
{
    nsresult rv = NS_OK;
    mIOService = do_GetService(kIOServiceCID, &rv);;
    return rv;
}

nsresult
nsAuthEngine::Logout()
{
    PRInt32 i, count;

    count = mAuthList.Count();
    for (i=0; i<count; ++i)
        delete NS_STATIC_CAST(nsAuth*, mAuthList[i]);
    mAuthList.Clear();
    mAuthList.Compact();

    count = mProxyAuthList.Count();
    for (i=0; i<count; ++i)
        delete NS_STATIC_CAST(nsAuth*, mProxyAuthList[i]);
    mProxyAuthList.Clear();
    mProxyAuthList.Compact();

    return NS_OK;
}
    
nsresult
nsAuthEngine::GetAuthString(nsIURI *aURI, char **aAuthString)
{
    nsCOMPtr<nsIURL> url;

    nsresult rv = NS_OK;
    if (!aURI || !aAuthString)
        return NS_ERROR_NULL_POINTER;
    *aAuthString = nsnull;

    PRInt32 count = mAuthList.Count();
    if (count <= 0)
        return NS_OK; // not found

    nsXPIDLCString host;
    rv = aURI->GetHost(getter_Copies(host));
    if (NS_FAILED(rv)) return rv;
    PRInt32 port;
    rv = aURI->GetPort(&port);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString dir;
    url = do_QueryInterface(aURI);
    if (url)
        rv = url->GetDirectory(getter_Copies(dir));
    else
        rv = aURI->GetPath(getter_Copies(dir));
    if (NS_FAILED(rv)) return rv;

    // just in case there is a prehost in the URL...
    nsXPIDLCString prehost;
    rv = aURI->GetPreHost(getter_Copies(prehost));
    if (NS_FAILED(rv)) return rv;

    // remove everything after the last slash
    // so we're comparing raw dirs
    char *lastSlash = PL_strrchr(dir, '/');
    if (lastSlash) lastSlash[1] = '\0';

    for (PRInt32 i = count-1; i>=0; --i) {
        nsAuth* auth = NS_STATIC_CAST(nsAuth*, mAuthList[i]);

        // perfect match case
        if (auth->uri.get() == aURI) {
            *aAuthString = nsCRT::strdup(auth->encodedString);
            return (*aAuthString) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
        }

        nsXPIDLCString authHost;
        PRInt32 authPort;
        nsXPIDLCString authDir;

        (void)auth->uri->GetHost(getter_Copies(authHost));
        (void)auth->uri->GetPort(&authPort);

        url = do_QueryInterface(auth->uri);
        if (url)
            (void)url->GetDirectory(getter_Copies(authDir));
        else
            (void)auth->uri->GetPath(getter_Copies(authDir));

#if 0 // turn on later...
        // if a prehost was provided compare the usernames (which should) 
        // at least be there... 
        if (prehost)
        {
            if (0 == PL_strncasecmp(prehost, auth->username, 
                        PL_strlen(auth->username)))
            {
                char* passwordStart = PL_strchr(prehost, ':');
                if (passwordStart && auth->password)
                {
                    if (0 != PL_strncasecmp(passwordStart+1, 
                                auth->password,
                                PL_strlen(auth->password)))
                    {
                        // no match since the passwords didn't match...
                        return NS_OK; 
                    }
                }
                else
                    return NS_OK;
            }
            else // no match
                return NS_OK;
        }
#endif

        // remove everything after the last slash
        // so we're comparing raw dirs
        lastSlash = PL_strrchr(authDir, '/');
        if (lastSlash) lastSlash[1] = '\0';

        if ((0 == PL_strncasecmp(authHost, host, PL_strlen(authHost))) &&
            (port == authPort) &&
            (0 == PL_strncasecmp(authDir, dir, PL_strlen(authDir)))) {
            *aAuthString = nsCRT::strdup(auth->encodedString);
            return (*aAuthString) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
        }
    }
    return rv;
}

nsresult
nsAuthEngine::SetAuth(nsIURI* aURI, 
        const char* aAuthString, 
        const char* aRealm,
        PRBool bProxyAuth)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(aURI);

    if (!mIOService)
        return NS_ERROR_FAILURE; // Init didn't make ioservice?

    nsVoidArray& list = bProxyAuth ? mProxyAuthList : mAuthList;

    //cleanup case
    if (!aAuthString) {
        PRInt32 count = mAuthList.Count(); 
        if (count<=0)
            return NS_OK; // not found
        for (PRInt32 i = count-1; i>=0; --i) {
            nsAuth* auth = NS_STATIC_CAST(nsAuth*, mAuthList[i]);
            // perfect match case
            if (auth->uri.get() == aURI) {
                rv = list.RemoveElementAt(i) ? NS_OK : NS_ERROR_FAILURE;
                delete auth;
                return rv;
            }
            // other wacky cases too TODO
        }
    }

    // TODO Extract user/pass info if available
    char *unescaped_AuthString = nsnull;
    rv = mIOService->Unescape(aAuthString, &unescaped_AuthString);
    if (NS_FAILED(rv)) {
        CRTFREEIF(unescaped_AuthString);
        return rv;
    }
    nsAuth* auth = new nsAuth(aURI, unescaped_AuthString, nsnull, nsnull, aRealm);
    CRTFREEIF(unescaped_AuthString);
    if (!auth)
        return NS_ERROR_OUT_OF_MEMORY;
    
    // We have to replace elements with earliar matching...TODO
    return (list.AppendElement(auth) ? NS_OK : NS_ERROR_FAILURE);
}


nsresult
nsAuthEngine::GetProxyAuthString(const char* aHost, PRInt32 aPort, char **aAuthString) 
{
    nsresult rv = NS_OK;
    if (!aAuthString)
        return NS_ERROR_NULL_POINTER;
    *aAuthString = nsnull;

    PRInt32 count = mProxyAuthList.Count();
    if (count <= 0)
        return NS_OK; // not found...

    for (PRInt32 i = count-1; i>=0; --i) {
        nsAuth* auth = NS_STATIC_CAST(nsAuth*, mProxyAuthList[i]);

        nsXPIDLCString authHost;
        PRInt32 authPort;

        (void)auth->uri->GetHost(getter_Copies(authHost));
        (void)auth->uri->GetPort(&authPort);

        if ((0 == PL_strncasecmp(authHost, aHost, PL_strlen(authHost))) &&
            (aPort == authPort)) {
            *aAuthString = nsCRT::strdup(auth->encodedString);
            return (*aAuthString) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
        }
    }
    return rv;
}

nsresult
nsAuthEngine::SetProxyAuthString(const char* host, PRInt32 port, const char* aAuthString)
{
    nsresult rv;
    nsCAutoString spec("http://");
    nsCOMPtr<nsIURI> uri;

    spec.Append(host);
    spec.Append(':');
    spec.AppendInt(port);

    if (!mIOService)
        return NS_ERROR_FAILURE; // Init didn't make ioservice?

    rv = mIOService->NewURI(spec.get(), nsnull, getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    return SetAuth(uri, aAuthString, nsnull, PR_TRUE);
}

nsresult
nsAuthEngine::GetAuthStringForRealm(nsIURI* aURI, const char* aRealm, char** aAuthString)
{
    nsresult rv = NS_OK;
    if (!aAuthString)
        return NS_ERROR_NULL_POINTER;
    if (!aRealm)
        return NS_ERROR_NULL_POINTER;
    *aAuthString = nsnull;

    PRInt32 count = mAuthList.Count();
    if (count <= 0)
        return NS_OK; // not found

    nsXPIDLCString host;
    rv = aURI->GetHost(getter_Copies(host));
    if (NS_FAILED(rv)) return rv;

    PRInt32 port;
    rv = aURI->GetPort(&port);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = count-1; i>=0; --i) {
        nsAuth* auth = NS_STATIC_CAST(nsAuth*, mAuthList[i]);
        
        nsXPIDLCString authHost;
        PRInt32 authPort;

        (void)auth->uri->GetHost(getter_Copies(authHost));
        (void)auth->uri->GetPort(&authPort);

        if ((0 == nsCRT::strcasecmp(authHost, host)) &&
            (0 == nsCRT::strcasecmp(auth->realm, aRealm)) &&
            (port == authPort)) {
            *aAuthString = nsCRT::strdup(auth->encodedString);
            return (!*aAuthString) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
        }
    }
    return NS_OK; // not found
}
