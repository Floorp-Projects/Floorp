/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
`` * http://www.mozilla.org/NPL/
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

#include "nsHTTPResponse.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsIHTTPChannel.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsHTTPAtoms.h"

nsHTTPResponse::nsHTTPResponse()
{
    NS_INIT_REFCNT();

    mStatus = 0;
    mServerVersion = HTTP_ONE_ZERO;
}

nsHTTPResponse::~nsHTTPResponse()
{
}

NS_IMPL_ISUPPORTS(nsHTTPResponse, NS_GET_IID(nsISupports))


nsresult nsHTTPResponse::GetContentLength(PRInt32* o_Value)
{
    nsresult rv = NS_OK;

    if (o_Value) {
        PRInt32 err;
        nsXPIDLCString value;
        nsAutoString str;

        mHeaders.GetHeader(nsHTTPAtoms::Content_Length, getter_Copies(value));
        str = value;
        *o_Value = str.ToInteger(&err);
    }
    else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

nsresult nsHTTPResponse::GetStatus(PRUint32* o_Value)
{
    nsresult rv = NS_OK;

    if (o_Value) {
        *o_Value = mStatus;
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

nsresult nsHTTPResponse::GetStatusString(char* *o_String)
{
    nsresult rv = NS_OK;

    if (o_String) {
        *o_String = mStatusString.ToNewCString();
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

nsresult nsHTTPResponse::GetServer(char* *o_String)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// Finally our own methods...

nsresult nsHTTPResponse::SetHeader(nsIAtom* i_Header, const char* i_Value)
{
    return mHeaders.SetHeader(i_Header, i_Value);
}


nsresult nsHTTPResponse::GetHeader(nsIAtom* i_Header, char* *o_Value)
{
    return mHeaders.GetHeader(i_Header, o_Value);
}

nsresult nsHTTPResponse::SetServerVersion(const char* i_Version)
{
    // convert it to HTTP Version
    // TODO
    mServerVersion = HTTP_ONE_ZERO;
    return NS_OK;

}

nsresult nsHTTPResponse::SetStatusString(const char* i_Status)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(mStatusString.Length() == 0, "Overwriting status string!");
    mStatusString = i_Status;

    return rv;
}

nsresult nsHTTPResponse::GetHeaderEnumerator(nsISimpleEnumerator** aResult)
{
    return mHeaders.GetEnumerator(aResult);
}
