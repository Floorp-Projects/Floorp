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
 *   Mike Shaver <shaver@zeroknowledge.com>
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include "nsBasicAuth.h"
#include "plbase64.h"
#include "plstr.h"
#include "nsCRT.h"
#include "prmem.h"
#include "nsString.h"

nsBasicAuth::nsBasicAuth()
{
    NS_INIT_REFCNT();
}

nsBasicAuth::~nsBasicAuth()
{
}

nsresult
nsBasicAuth::Authenticate(nsIURI* i_URI, const char *protocol,
                          const char* iChallenge, const PRUnichar* iUser,
                          const PRUnichar* iPass, nsIPrompt *prompt,
                          char **oResult)
{
    // we only know how to deal with Basic auth for http.
    PRBool isBasicAuth = !PL_strncasecmp(iChallenge, "basic ", 6);
    NS_ASSERTION(isBasicAuth, "nsBasicAuth called for non-Basic auth");
    if (!isBasicAuth)
        return NS_ERROR_INVALID_ARG;
    PRBool isHTTPAuth = !strncmp(protocol, "http", 4);
    NS_ASSERTION(isHTTPAuth, "nsBasicAuth called for non-http auth");
    if (!isHTTPAuth)
        return NS_ERROR_INVALID_ARG;

    if (!oResult || !iUser)
        return NS_ERROR_NULL_POINTER;

    // we work with ASCII around these here parts
    nsCAutoString cUser, cPass;
    cUser.AssignWithConversion(iUser);
    if (iPass) {
        cPass.AssignWithConversion(iPass);
    }
    PRUint32 nbytes = cUser.Length() + 1;
    if (iPass)
        nbytes += cPass.Length() + 1;
    char* tempBuff = (char *)nsMemory::Alloc(nbytes);
    if (!tempBuff)
        return NS_ERROR_OUT_OF_MEMORY;
    strcpy(tempBuff, cUser.GetBuffer());
    if (iPass) {
        strcat(tempBuff, ":");
        strcat(tempBuff, cPass.GetBuffer());
    }

    // <shaver> we use nbytes - 1 here to avoid encoding the trailing
    // NUL
    char *base64Buff = PL_Base64Encode(tempBuff, nbytes - 1, nsnull);
    if (!base64Buff) {
        nsMemory::Free(tempBuff);
        return NS_ERROR_FAILURE; // ??
    }
    nsCAutoString authString("Basic "); // , 6 + strlen(base64Buff));
    authString.Append(base64Buff);
    *oResult = authString.ToNewCString();
    PR_Free(base64Buff);
    nsMemory::Free(tempBuff);
    return NS_OK;
}

nsresult
nsBasicAuth::GetInteractionType(PRUint32 *aType)
{
    *aType = nsIAuthenticator::INTERACTION_STANDARD;
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsBasicAuth, nsIAuthenticator);
