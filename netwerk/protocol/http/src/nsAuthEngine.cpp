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

#include "nsAuthEngine.h"
#include "nsAuth.h"
#include "nsIURI.h"
#include "nsCRT.h"
#include "plstr.h"
#include "nsXPIDLString.h"

nsAuthEngine::nsAuthEngine()
{
    if (NS_FAILED(NS_NewISupportsArray(getter_AddRefs(mAuthList))))
        NS_ERROR("unable to create new auth list");
}

nsAuthEngine::~nsAuthEngine()
{
    mAuthList->Clear();
}

nsresult
nsAuthEngine::Logout()
{
    return mAuthList->Clear();
}
    
nsresult
nsAuthEngine::GetAuthString(nsIURI* i_URI, char** o_AuthString)
{
    nsresult rv = NS_OK;
    if (!i_URI || !o_AuthString)
        return NS_ERROR_NULL_POINTER;
    *o_AuthString = nsnull;
    NS_ASSERTION(mAuthList, "No auth list!");
    // or should we try and make this
    if (!mAuthList) return NS_ERROR_FAILURE;

    nsXPIDLCString host;
    rv = i_URI->GetHost(getter_Copies(host));
    if (NS_FAILED(rv)) return rv;
    PRInt32 port;
    rv = i_URI->GetPort(&port);
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString path;
    rv = i_URI->GetPath(getter_Copies(path));
    if (NS_FAILED(rv)) return rv;

    PRUint32 count=0; 
    (void)mAuthList->Count(&count);
    if (count<=0)
        return NS_OK; // not found
    for (PRUint32 i = count-1; i>=0; --i)
    {
        nsAuth* auth = (nsAuth*)mAuthList->ElementAt(i);
        // perfect match case
        if (auth->uri.get() == i_URI)
        {
            *o_AuthString = nsCRT::strdup(auth->encodedString);
            if (!*o_AuthString) return NS_ERROR_OUT_OF_MEMORY;
            return NS_OK;
        }

        /*
        // TODO
        if (prehost)
        {
            if (0 == PL_strncasecmp(prehost, auth->username, 
                        PL_strlen(auth->username)))
            {
                char* passwordStart = PL_strchr(prehost, ':');
                if (passwordStart)
                {
                    if (0 == PL_strncasecmp(passwordStart+1, 
                                auth->password,
                                PL_strlen(auth->password)))
                    {
                    }
                }
            }
        }
        */
        nsXPIDLCString authHost;
        PRInt32 authPort;
        nsXPIDLCString authPath;
        (void)auth->uri->GetHost(getter_Copies(authHost));
        (void)auth->uri->GetPort(&authPort);
        (void)auth->uri->GetPath(getter_Copies(authPath));
        if ((0 == PL_strncasecmp(authHost, host, 
                        PL_strlen(authHost))) &&
            (port == authPort) &&
            (0 == PL_strncasecmp(authPath, path, 
                        PL_strlen(authPath)))) 
        {
            *o_AuthString = nsCRT::strdup(auth->encodedString);
            if (!*o_AuthString) return NS_ERROR_OUT_OF_MEMORY;
            return NS_OK;
        }
    }
    return rv;
}

nsresult
nsAuthEngine::SetAuthString(nsIURI* i_URI, const char* i_AuthString)
{
    if (!i_URI || !i_AuthString)
        return NS_ERROR_NULL_POINTER;

    NS_ASSERTION(mAuthList, "No authentication list");

    // TODO Extract user/pass info if available
    nsAuth* auth = new nsAuth(i_URI, i_AuthString);
    if (!auth)
        return NS_ERROR_OUT_OF_MEMORY;
    
    nsresult rv = mAuthList->AppendElement(auth) ? NS_OK : NS_ERROR_FAILURE;
    return rv;
}
