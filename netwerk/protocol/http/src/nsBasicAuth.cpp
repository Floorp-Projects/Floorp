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
nsBasicAuth::Authenticate(nsIURI* i_URI, 
    const char* iChallenge, 
    const char* iUserPass,
    char**  oResult)
{
    nsresult rv = NS_ERROR_FAILURE;
    // Assert that iChallenge starts with Basic. TODO
    // Then do the conversion
    if (oResult && iUserPass)
    {
        char* tempBuff = nsnull;
        tempBuff = PL_Base64Encode(iUserPass, 0, tempBuff); 
        if (!tempBuff)
            return NS_ERROR_FAILURE; // ??
        
          // STRING USE WARNING: perhaps |authString| should be an |nsCAutoString|? -- scc
        nsString authString; authString.AssignWithConversion("Basic ");
        authString.AppendWithConversion(tempBuff);
        *oResult = authString.ToNewCString();
        PR_Free(tempBuff);
        rv = NS_OK;
    }
    return rv;
}

NS_IMPL_ADDREF(nsBasicAuth);
NS_IMPL_RELEASE(nsBasicAuth);

NS_IMPL_QUERY_INTERFACE0(nsBasicAuth);
