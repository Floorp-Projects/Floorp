/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsHTTPRequest.h"
#include "nsIURI.h"

nsHTTPRequest::nsHTTPRequest(nsIURI* i_pURI, HTTPMethod i_Method):
    m_pURI(i_pURL),
    m_Method(i_Method)
{
    Build();
}

nsHTTPRequest::~nsHTTPRequest()
{
    if (m_Request)
    {
        delete[] m_Request;
        m_Request = 0;
    }
}

NS_IMPL_ADDREF(nsHTTPRequest);

nsresult
nsHTTPRequest::Build()
{
    return NS_OK;
}

nsresult
nsHTTPRequest::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    *aInstancePtr = NULL;
    
    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

    if (aIID.Equals(nsIHTTPRequest::GetIID())) {
        *aInstancePtr = (void*) ((nsIHTTPRequest*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIHTTPCommonHeaders::GetIID())) {
        *aInstancePtr = (void*) ((nsIHTTPCommonHeaders*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIHeader::GetIID())) {
        *aInstancePtr = (void*) ((nsIHeader*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports*)(nsIProtocolHandler*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}
 
NS_IMPL_RELEASE(nsHTTPRequest);
