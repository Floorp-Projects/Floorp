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
 * Original Author: Gagan Saksena <gagan@netscape.com>
 *
 * Contributor(s): 
 *   Mike Shaver <shaver@zeroknowledge.com>
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Darin Fisher <darin@netscape.com>
 */

#include "nsBasicAuth.h"
#include "plbase64.h"
#include "plstr.h"
#include "nsCRT.h"
#include "prmem.h"
#include "nsString.h"

nsBasicAuth::nsBasicAuth()
{
    NS_INIT_ISUPPORTS();
}

nsBasicAuth::~nsBasicAuth()
{
}

nsresult
nsBasicAuth::Authenticate(nsIURI *aURI, const char *aProtocol,
                          const char *aChallenge, const PRUnichar *aUser,
                          const PRUnichar *aPass, nsIAuthPrompt *aPrompt,
                          char **aResult)
{
    // we only know how to deal with Basic auth for http.
    PRBool isBasicAuth = !PL_strncasecmp(aChallenge, "basic ", 6);
    NS_ASSERTION(isBasicAuth, "nsBasicAuth called for non-Basic auth");
    if (!isBasicAuth)
        return NS_ERROR_INVALID_ARG;
    PRBool isHTTPAuth = !strncmp(aProtocol, "http", 4);
    NS_ASSERTION(isHTTPAuth, "nsBasicAuth called for non-http auth");
    if (!isHTTPAuth)
        return NS_ERROR_INVALID_ARG;

    if (!aResult || !aUser)
        return NS_ERROR_NULL_POINTER;

    // we work with ASCII around here
    nsCAutoString userpass;
    userpass.AssignWithConversion(aUser);
    if (aPass) {
        userpass.Append(':');
        userpass.AppendWithConversion(aPass);
    }

    char *base64Buff = PL_Base64Encode(userpass.get(),
                                       userpass.Length(),
                                       nsnull);
    
    if (!base64Buff)
        return NS_ERROR_FAILURE; // ??

    nsCAutoString authString("Basic ");
    authString.Append(base64Buff);
    *aResult = authString.ToNewCString();
    PR_Free(base64Buff);
    return NS_OK;
}

nsresult
nsBasicAuth::GetInteractionType(PRUint32 *aType)
{
    *aType = nsIAuthenticator::INTERACTION_STANDARD;
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsBasicAuth, nsIAuthenticator);
