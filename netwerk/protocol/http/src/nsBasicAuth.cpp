/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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
	char**	oResult)
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
		nsString authString("Basic ");
		authString += tempBuff;
        // Copy this set into an nsAuth and save to nsAuthList
        // TODO
        *oResult = authString.ToNewCString();
		PR_Free(tempBuff);
        rv = NS_OK;
    }
	return rv;
}

NS_IMETHODIMP
nsBasicAuth::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    *aInstancePtr = NULL;
    
    if (aIID.Equals(NS_GET_IID(nsISupports))) 
	{
        *aInstancePtr = NS_STATIC_CAST(nsISupports*, this);
        NS_ADDREF_THIS();
        return NS_OK;
	}
    return NS_NOINTERFACE;
}
 
NS_IMPL_ADDREF(nsBasicAuth);
NS_IMPL_RELEASE(nsBasicAuth);

