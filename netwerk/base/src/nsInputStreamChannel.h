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

#ifndef nsInputStreamChannel_h__
#define nsInputStreamChannel_h__

#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "nsCRT.h"
#include "nsILoadGroup.h"
#include "nsIStreamListener.h"
#include "nsIStreamProvider.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamIO.h"

class nsInputStreamIO : public nsIInputStreamIO
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMIO
    NS_DECL_NSIINPUTSTREAMIO

    nsInputStreamIO();
    virtual ~nsInputStreamIO();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
    char*                               mName;
    nsCOMPtr<nsIInputStream>            mInputStream;
    char*                               mContentType;
    PRInt32                             mContentLength;
    nsresult                            mStatus;
};

////////////////////////////////////////////////////////////////////////////////

class nsStreamIOChannel : public nsIStreamIOChannel, 
                          public nsIStreamListener,
                          public nsIStreamProvider
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSISTREAMIOCHANNEL
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISTREAMPROVIDER

    nsStreamIOChannel(); 
    virtual ~nsStreamIOChannel();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
    nsIStreamListener* GetListener() { return (nsIStreamListener*)mUserObserver.get(); }
    void SetListener(nsIStreamListener* listener) { mUserObserver = listener; }
    nsIStreamProvider* GetProvider() { return (nsIStreamProvider*)mUserObserver.get(); }
    void SetProvider(nsIStreamProvider* provider) { mUserObserver = provider; }

protected:
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsIURI>                    mURI;
    char*                               mContentType;
    PRInt32                             mContentLength;
    nsCOMPtr<nsIStreamIO>               mStreamIO;
    nsCOMPtr<nsILoadGroup>              mLoadGroup;
    nsCOMPtr<nsISupports>               mOwner;
    nsCOMPtr<nsIChannel>                mFileTransport;
    nsCOMPtr<nsIStreamObserver>         mUserObserver;
    PRUint32                            mBufferSegmentSize;
    PRUint32                            mBufferMaxSize;
    PRUint32                            mLoadAttributes;
    nsresult                            mStatus;
};

#endif // nsInputStreamChannel_h__
