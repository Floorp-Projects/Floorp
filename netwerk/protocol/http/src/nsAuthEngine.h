/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsAuthEngine_h__
#define nsAuthEngine_h__

#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsIIOService.h"

class nsIURI;

/* 
 * The nsAuthEngine class is the central class for handling
 * authentication lists, checks etc. 
 *
 * There should be a class nsProxyAuthEngine that shares all this...
 * I will move it that way... someday... 
 *
 *      -Gagan Saksena 11/12/99
 */

class nsAuthEngine
{
public:
    nsAuthEngine();
   ~nsAuthEngine();

    nsresult Init();

    // Get an auth string
    nsresult GetAuthString(nsIURI *aURI, char **aAuthString);

    // Set an auth string
    nsresult SetAuthString(nsIURI *aURI, const char *aAuthString);

    // Get a proxy auth string with host/port
    nsresult GetProxyAuthString(const char *host, 
                                PRInt32 port, 
                                char **aAuthString);

    // Set a proxy auth string
    nsresult SetProxyAuthString(const char *host,
                                PRInt32 port,
                                const char *aAuthString);

    // Get an auth string for a particular host/port/realm
    nsresult GetAuthStringForRealm(nsIURI *aURI,
                                   const char *aRealm,
                                   char **aAuthString);

    // Set an auth string for a particular host/port/realm
    nsresult SetAuthStringForRealm(nsIURI *aURI,
                                   const char *aRealm,
                                   const char *aAuthString);

    // Expire all existing auth list entries including proxy auths. 
    nsresult Logout();

protected:

    nsresult SetAuth(nsIURI *aURI, 
                     const char *aAuthString, 
                     const char *aRealm = nsnull,
                     PRBool bProxyAuth = PR_FALSE);

    nsVoidArray mAuthList; 

    // this needs to be a list becuz pac can produce more ...
    nsVoidArray mProxyAuthList; 

    // optimization
    nsCOMPtr<nsIIOService> mIOService;
};

inline nsresult
nsAuthEngine::SetAuthString(nsIURI *aURI, const char *aAuthString)
{
    return SetAuth(aURI, aAuthString);
}

inline nsresult
nsAuthEngine::SetAuthStringForRealm(nsIURI *aURI,
                                    const char *aRealm,
                                    const char*aAuthString)
{
    return SetAuth(aURI, aAuthString, aRealm);
}

#endif
