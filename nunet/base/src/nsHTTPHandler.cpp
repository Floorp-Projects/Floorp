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

#include "nsHTTPHandler.h"
#include "nsHTTPInstance.h"
#include "nsITimer.h" 
#include "nsIProxy.h"
#include "plstr.h" // For PL_strcasecmp maybe DEBUG only... TODO check

NS_METHOD CreateOrGetHTTPHandler(nsIHTTPHandler* *o_HTTPHandler)
{
    *o_HTTPHandler = nsHTTPHandler::GetInstance();
    return NS_OK;
}

nsHTTPHandler::nsHTTPHandler():
    m_pTimer(nsnull),
    mRefCnt(0)
{
}

nsHTTPHandler::~nsHTTPHandler()
{
    NS_IF_RELEASE(m_pTimer);
}

NS_IMPL_ADDREF(nsHTTPHandler);

nsresult
nsHTTPHandler::GetProtocolInstance(
    nsICoolURL* i_URL, 
    nsIProtocolInstance* *o_Instance)
{
    //Assert that iURL's scheme is HTTP
    //This should only happen in debug checks... TODO
    const char* scheme = 0;
    if (i_URL)
    {
        i_URL->GetScheme(&scheme);
        if (0 == PL_strcasecmp(scheme, "http"))
        {
            //Check to see if an instance already exists in the active list
            // if...
            // else
            // Create one
            nsHTTPInstance* pNewInstance = new nsHTTPInstance(i_URL);
            if (pNewInstance)
            {
                *o_Instance = pNewInstance;
                return NS_OK;
            }
            else
                return NS_ERROR_OUT_OF_MEMORY;
        }
        //return NS_ERROR_MISMATCHED_URL;
    }
    return NS_ERROR_NULL_POINTER;
}

nsresult
nsHTTPHandler::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    *aInstancePtr = NULL;
    
    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

    if (aIID.Equals(nsIProtocolHandler::GetIID())) {
        *aInstancePtr = (void*) ((nsIProtocolHandler*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIHTTPHandler::GetIID())) {
        *aInstancePtr = (void*) ((nsIHTTPHandler*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIProxy::GetIID())) {
        *aInstancePtr = (void*) ((nsIProxy*)this);
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
 
NS_IMPL_RELEASE(nsHTTPHandler);
