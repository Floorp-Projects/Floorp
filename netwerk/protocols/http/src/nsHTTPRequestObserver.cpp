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

#include "nsIString.h"
#include "nsHTTPRequestObserver.h"
#include "nsHTTPResponseListener.h"
#include "nsITransport.h"

nsHTTPRequestObserver::nsHTTPRequestObserver(nsITransport* i_pTransport) :
    m_pTransport(i_pTransport)
{
    NS_INIT_REFCNT();
    NS_ADDREF(m_pTransport);
}

nsHTTPRequestObserver::~nsHTTPRequestObserver()
{
    if (m_pTransport)
        NS_RELEASE(m_pTransport);
}

NS_IMPL_ISUPPORTS(nsHTTPRequestObserver,nsIStreamObserver::GetIID());

NS_IMETHODIMP
nsHTTPRequestObserver::OnStartBinding(nsISupports* /*i_pContext*/)
{
    //TODO globally replace printf with trace calls. 
    //printf("nsHTTPRequestObserver::OnStartBinding...\n");
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPRequestObserver::OnStopBinding(nsISupports* i_pContext,
                                 nsresult iStatus,
                                 nsIString* i_pMsg)
{
    //printf("nsHTTPRequestObserver::OnStopBinding...\n");
    // if we could write successfully... 
    if (NS_SUCCEEDED(iStatus)) 
    {
        //Prepare to receive the response!
        nsHTTPResponseListener* pListener = new nsHTTPResponseListener();
        m_pTransport->AsyncRead(nsnull, nsnull /*gEventQ */, pListener);
        //TODO check this portion here...
        return pListener ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }

    /*
        Somewhere here we need to send a message up the event sink 
        that we successfully (or not) have sent request to the 
        server. TODO
    */
    return iStatus;
}
