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

#include "nsIFTPChannel.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsString2.h"
#include "nsIEventQueue.h"
#include "nsILoadGroup.h"
#include "nsCOMPtr.h"
#include "nsHashtable.h"
#include "nsIProtocolHandler.h"
#include "nsIProgressEventSink.h"
#include "nsIEventSinkGetter.h"
#include "nsIThreadPool.h"
#include "nsIRequest.h"

class nsFTPChannel : public nsIFTPChannel,
                     public nsIStreamListener {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIFTPCHANNEL
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSISTREAMLISTENER

    // nsFTPChannel methods:
    nsFTPChannel();
    virtual ~nsFTPChannel();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    // initializes the channel. creates the FTP connection thread
    // and returns it so the protocol handler can cache it and
    // join() it on shutdown.
    nsresult Init(const char* verb, nsIURI* uri, nsILoadGroup *aGroup,
                  nsIEventSinkGetter* getter, nsIURI* originalURI,
                  nsIProtocolHandler* aHandler, nsIThreadPool* aPool);

protected:
    nsCOMPtr<nsIURI>                mOriginalURI;
    nsCOMPtr<nsIURI>                mURL;
    nsCOMPtr<nsIEventQueue>         mEventQueue;
    nsCOMPtr<nsIProgressEventSink>  mEventSink;
    nsCOMPtr<nsIEventSinkGetter>    mEventSinkGetter;

    PRBool                          mConnected;
    nsCOMPtr<nsIStreamListener>     mListener;
    nsCOMPtr<nsISupports>           mContext;
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
};

#define NS_FTP_SEGMENT_SIZE   (4*1024)
#define NS_FTP_BUFFER_SIZE    (8*1024)


#endif /* nsFTPChannel_h___ */
