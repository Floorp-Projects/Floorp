/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
    { 
        NS_INIT_REFCNT();
    }
    
    virtual ~nsAsyncStreamObserver() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsISupports* GetReceiver()      { return mReceiver.get(); }
    void Clear()      { mReceiver = nsnull; }

protected:
    nsCOMPtr<nsIEventQueue>     mEventQueue;
    nsCOMPtr<nsIStreamObserver> mReceiver;
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
                             nsresult aStatus, const PRUnichar* aStatusArg) 
    { 
        return nsAsyncStreamObserver::OnStopRequest(channel, context, aStatus, aStatusArg); 
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
    nsAsyncStreamListener() {
        MOZ_COUNT_CTOR(nsAsyncStreamListener);
    }

    virtual ~nsAsyncStreamListener() {
        MOZ_COUNT_DTOR(nsAsyncStreamListener);
    }

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

};

////////////////////////////////////////////////////////////////////////////////

#endif // nsAsyncStreamListener_h__
