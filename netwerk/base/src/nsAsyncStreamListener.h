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

#ifndef nsAsyncStreamListener_h__
#define nsAsyncStreamListener_h__

#include "nsIStreamListener.h"
#include "nsCOMPtr.h"
#include "nsIEventQueue.h"
#include "nsIStreamObserver.h"
#include "nsIStreamListener.h"

////////////////////////////////////////////////////////////////////////////////

class nsAsyncStreamObserver : public nsIAsyncStreamObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSIASYNCSTREAMOBSERVER

    // nsAsyncStreamObserver methods:
    nsAsyncStreamObserver() 
        : mStatus(NS_OK) 
    { 
        NS_INIT_REFCNT();
    }
    
    virtual ~nsAsyncStreamObserver();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsISupports* GetReceiver()      { return mReceiver.get(); }
    nsresult GetStatus()            { return mStatus; }
    void SetStatus(nsresult value)  { mStatus = value; }

protected:
    nsCOMPtr<nsIEventQueue>     mEventQueue;
    nsCOMPtr<nsIStreamObserver> mReceiver;
    nsresult                    mStatus;
};

////////////////////////////////////////////////////////////////////////////////

class nsAsyncStreamListener : public nsAsyncStreamObserver,
                              public nsIAsyncStreamListener
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    // nsIStreamListener methods:
    NS_IMETHOD OnStartRequest(nsIChannel* channel,
                              nsISupports* context) 
    { 
        return nsAsyncStreamObserver::OnStartRequest(channel, context); 
    }

    NS_IMETHOD OnStopRequest(nsIChannel* channel,
                             nsISupports* context, 
                             nsresult aStatus,
                             const PRUnichar* aMsg) 
    { 
        return nsAsyncStreamObserver::OnStopRequest(channel, context, aStatus, aMsg); 
    }

    NS_IMETHOD OnDataAvailable(nsIChannel* channel, nsISupports* context,
                               nsIInputStream *aIStream, 
                               PRUint32 aSourceOffset,
                               PRUint32 aLength);

    // nsIAsyncStreamListener methods:
    NS_IMETHOD Init(nsIStreamListener* aListener, nsIEventQueue* aEventQ) {
        return nsAsyncStreamObserver::Init(aListener, aEventQ);
    }

    // nsAsyncStreamListener methods:
    nsAsyncStreamListener() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

};

////////////////////////////////////////////////////////////////////////////////

#endif // nsAsyncStreamListener_h__
