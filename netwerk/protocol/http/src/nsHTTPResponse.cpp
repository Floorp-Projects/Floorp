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

nsHTTPResponse::nsHTTPResponse(nsIInputStream* i_InputStream):
    m_pStatusString(nsnull)
{
    NS_INIT_REFCNT();

    m_Status = 0;
    m_ServerVersion = HTTP_ONE_ZERO;

    m_pInputStream = i_InputStream;
    NS_IF_ADDREF(m_pInputStream);
}

nsHTTPResponse::~nsHTTPResponse()
{
    NS_IF_RELEASE(m_pInputStream);

    if (m_pStatusString) {
        nsCRT::free(m_pStatusString);
        m_pStatusString = nsnull;
    }

}

NS_IMPL_ISUPPORTS(nsHTTPResponse, nsCOMTypeInfo<nsISupports>::GetIID())


nsresult nsHTTPResponse::GetContentLength(PRInt32* o_Value)
{
    if (o_Value) {
        PRInt32 err;
        nsXPIDLCString value;
        nsAutoString str;

	    mHeaders.GetHeader(nsHTTPAtoms::Content_Length, getter_Copies(value));
        str = value;
        *o_Value = str.ToInteger(&err);
        return NS_OK;
    }
    else 
        return NS_ERROR_NULL_POINTER;
    return NS_OK;
}

nsresult nsHTTPResponse::GetStatus(PRUint32* o_Value)
{
    if (o_Value)
        *o_Value = m_Status;
    else 
        return NS_ERROR_NULL_POINTER;
    return NS_OK;
}

nsresult nsHTTPResponse::GetStatusString(char* *o_String)
{
    if (o_String)
        *o_String = m_pStatusString;
    return NS_OK;
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
    m_ServerVersion = HTTP_ONE_ZERO;
    return NS_OK;

}

nsresult nsHTTPResponse::SetStatusString(const char* i_Status)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(!m_pStatusString, "Overwriting status string!");
    m_pStatusString = nsCRT::strdup(i_Status);
    if (!m_pStatusString) {
      rv = NS_ERROR_FAILURE;
    }

    return rv;
}

nsresult nsHTTPResponse::GetInputStream(nsIInputStream* *o_Stream)
{
    *o_Stream = m_pInputStream;
    return NS_OK;
}

nsresult nsHTTPResponse::GetHeaderEnumerator(nsISimpleEnumerator** aResult)
{
    return mHeaders.GetEnumerator(aResult);
}
