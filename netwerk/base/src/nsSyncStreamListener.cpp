/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsIStreamListener.h"
#include "nsIByteBufferInputStream.h"
#include "nsIString.h"
#include "nsCRT.h"

////////////////////////////////////////////////////////////////////////////////

class nsSyncStreamListener : public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS

    // nsIStreamListener methods:
    NS_IMETHOD OnStartBinding(nsISupports* context);
    NS_IMETHOD OnDataAvailable(nsISupports* context,
                               nsIInputStream *aIStream, 
                               PRUint32 aLength);
    NS_IMETHOD OnStopBinding(nsISupports* context,
                             nsresult aStatus,
                             nsIString* aMsg);

    // nsSyncStreamListener methods:
    nsSyncStreamListener()
        : mOutputStream(nsnull) {
        NS_INIT_REFCNT();
    }
    virtual ~nsSyncStreamListener();

    nsresult Init(nsIInputStream* *result);

protected:
    nsIByteBufferOutputStream*  mOutputStream;
};

////////////////////////////////////////////////////////////////////////////////

nsresult 
nsSyncStreamListener::Init(nsIInputStream* *result)
{
    nsresult rv;
    nsIInputStream* in;
    nsIOutputStream* out;

    rv = NS_NewPipe(&in, &out);
    if (NS_FAILED(rv)) return rv;

    rv = out->QueryInterface(nsIByteBufferOutputStream::GetIID(), (void**)&mOutputStream);
    NS_RELEASE(out);
    if (NS_FAILED(rv)) {
        NS_RELEASE(in);
        return rv;
    }
    *result = in;
    return NS_OK;
}

nsSyncStreamListener::~nsSyncStreamListener()
{
    NS_IF_RELEASE(mOutputStream);
}

NS_IMPL_ADDREF(nsSyncStreamListener);
NS_IMPL_RELEASE(nsSyncStreamListener);

NS_IMETHODIMP
nsSyncStreamListener::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(nsIStreamListener::GetIID()) ||
        aIID.Equals(nsIStreamObserver::GetIID()) ||
        aIID.Equals(nsISupports::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsIStreamListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE; 
}

NS_IMETHODIMP 
nsSyncStreamListener::OnStartBinding(nsISupports* context)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsSyncStreamListener::OnDataAvailable(nsISupports* context,
                                      nsIInputStream *aIStream, 
                                      PRUint32 aLength)
{
    nsresult rv;
    PRUint32 amt;
    PRInt32 count = (PRInt32)aLength;
    while (count > 0) {   // this should only go around once since the output stream is blocking
        rv = mOutputStream->Write(aIStream, &amt);
        if (NS_FAILED(rv)) return rv;
        count -= amt;
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsSyncStreamListener::OnStopBinding(nsISupports* context,
                                    nsresult aStatus,
                                    nsIString* aMsg)
{
    // XXX what do we do with the status and error message?
    return mOutputStream->Close();
}

////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewSyncStreamListener(nsIStreamListener* *listener,
                         nsIInputStream* *inStream)
{
    nsSyncStreamListener* l = new nsSyncStreamListener();
    if (l == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = l->Init(inStream);
    if (NS_FAILED(rv)) {
        delete l;
        return rv;
    }

    NS_ADDREF(l);
    *listener = l;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
