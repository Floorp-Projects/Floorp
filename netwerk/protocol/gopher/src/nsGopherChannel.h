/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Gopher protocol code.
 *
 * The Initial Developer of the Original Code is Bradley Baetz.
 * Portions created by Bradley Baetz are Copyright (C) 2000 Bradley Baetz.
 * All Rights Reserved.
 *
 * Contributor(s): 
 *  Bradley Baetz <bbaetz@student.usyd.edu.au>
 */

#ifndef nsGopherChannel_h___
#define nsGopherChannel_h___

#include "nsString.h"
#include "nsILoadGroup.h"
#include "nsIInputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsGopherHandler.h"
#include "nsIPrompt.h"
#include "nsIProxy.h"
#include "nsIStreamListener.h"
#include "nsITransport.h"

class nsGopherChannel : public nsIChannel,
                        public nsIProxy,
                        public nsIStreamListener {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIPROXY
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER

    // nsGopherChannel methods:
    nsGopherChannel();
    virtual ~nsGopherChannel();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    nsresult Init(nsIURI* uri);

    nsresult SetProxyChannel(nsIChannel* aChannel);
    nsresult GetUsingProxy(PRBool *aUsingProxy);
protected:
    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
    nsCOMPtr<nsIPrompt>                 mPrompter;
    nsCOMPtr<nsIURI>                    mUrl;
    nsCOMPtr<nsIStreamListener>         mListener;
    PRUint32                            mLoadFlags;
    nsCOMPtr<nsILoadGroup>              mLoadGroup;
    nsCString                           mContentType;
    PRInt32                             mContentLength;
    nsCOMPtr<nsISupports>               mOwner; 
    PRUint32                            mBufferSegmentSize;
    PRUint32                            mBufferMaxSize;
    PRBool                              mActAsObserver;

    nsXPIDLCString                      mHost;
    PRInt32                             mPort;
    char                                mType;
    nsXPIDLCString                      mSelector;
    nsCString                           mRequest;

    nsCOMPtr<nsISupports>               mResponseContext;
    nsCOMPtr<nsITransport>              mTransport;
    nsCOMPtr<nsIRequest>                mTransportRequest;
    nsresult                            mStatus;

    nsCOMPtr<nsIChannel>                mProxyChannel; // a proxy channel
    nsCString                           mProxyHost;
    PRInt32                             mProxyPort;
    nsCString                           mProxyType;
    PRBool                              mProxyTransparent;

    nsresult SendRequest(nsITransport* aTransport);
};

#endif /* nsGopherChannel_h___ */
