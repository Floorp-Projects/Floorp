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

// ftp implementation header

#ifndef nsFTPChannel_h___
#define nsFTPChannel_h___

#include "nsPIFTPChannel.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsString2.h"
#include "nsIEventQueue.h"
#include "nsILoadGroup.h"
#include "nsCOMPtr.h"
#include "nsHashtable.h"
#include "nsIProtocolHandler.h"
#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIThreadPool.h"
#include "nsIRequest.h"
#include "nsAutoLock.h"
#include "nsFtpConnectionThread.h"
#include "netCore.h"
#include "nsIProgressEventSink.h"

class nsFTPChannel : public nsPIFTPChannel,
                     public nsIInterfaceRequestor,
                     public nsIProgressEventSink {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSPIFTPCHANNEL
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIPROGRESSEVENTSINK

    // nsFTPChannel methods:
    nsFTPChannel();
    virtual ~nsFTPChannel();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    // initializes the channel. creates the FTP connection thread
    // and returns it so the protocol handler can cache it and
    // join() it on shutdown.
    nsresult Init(const char* verb, 
                  nsIURI* uri, 
                  nsILoadGroup* aLoadGroup,
                  nsIInterfaceRequestor* notificationCallbacks, 
                  nsLoadFlags loadAttributes, 
                  nsIURI* originalURI,
                  PRUint32 bufferSegmentSize,
                  PRUint32 bufferMaxSize,
                  nsIProtocolHandler* aHandler, 
                  nsIThreadPool* aPool);

protected:
    nsCOMPtr<nsIURI>                mOriginalURI;
    nsCOMPtr<nsIURI>                mURL;
    nsCOMPtr<nsIProgressEventSink>  mEventSink;
    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;

    PRBool                          mConnected;
    PRBool                          mAsyncOpen; // was AsyncOpen called
    PRUint32                        mLoadAttributes;

    PRUint32                        mSourceOffset;
    PRInt32                         mAmount;
    nsCOMPtr<nsILoadGroup>          mLoadGroup;
    nsAutoString                    mContentType;
    PRInt32                         mContentLength;
    nsCOMPtr<nsISupports>           mOwner;

    nsCOMPtr<nsIRequest>            mThreadRequest;
    nsCOMPtr<nsIRequest>            mProxiedThreadRequest;
    nsCOMPtr<nsIProtocolHandler>    mHandler;
    nsCOMPtr<nsIThreadPool>         mPool; // the thread pool we want to use to fire off connections.
    nsFtpConnectionThread           *mConnThread; // the raw pointer to the thread object.
    PRUint32                        mBufferSegmentSize;
    PRUint32                        mBufferMaxSize;
};

#endif /* nsFTPChannel_h___ */
