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
#include "nsIInterfaceRequestor.h"
#include "nsCOMPtr.h"

class nsInputStreamChannel : public nsIInputStreamChannel, 
                             public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIINPUTSTREAMCHANNEL
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSISTREAMLISTENER

    nsInputStreamChannel(); 
    virtual ~nsInputStreamChannel();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsIURI>                    mURI;
    char*                               mContentType;
    PRInt32                             mContentLength;
    nsCOMPtr<nsIInputStream>            mInputStream;
    nsCOMPtr<nsILoadGroup>              mLoadGroup;
    nsCOMPtr<nsISupports>               mOwner;
    nsCOMPtr<nsIChannel>                mFileTransport;
    nsCOMPtr<nsIStreamListener>         mRealListener;
    PRUint32                            mBufferSegmentSize;
    PRUint32                            mBufferMaxSize;
    PRUint32                            mLoadAttributes;
    nsresult                            mStatus;
};

#endif // nsInputStreamChannel_h__
