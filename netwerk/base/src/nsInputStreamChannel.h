/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsInputStreamChannel_h__
#define nsInputStreamChannel_h__

#include "nsIStreamIOChannel.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "nsCRT.h"
#include "nsILoadGroup.h"
#include "nsIStreamListener.h"
#include "nsIStreamProvider.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsIStreamIO.h"
#include "nsITransport.h"

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
                          public nsIStreamProvider,
                          public nsIProgressEventSink,
                          public nsIInterfaceRequestor
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSISTREAMIOCHANNEL
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISTREAMPROVIDER
    NS_DECL_NSIPROGRESSEVENTSINK
    NS_DECL_NSIINTERFACEREQUESTOR

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
    nsCOMPtr<nsIProgressEventSink>      mProgressSink;
    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsIURI>                    mURI;
    char*                               mContentType;
    PRInt32                             mContentLength;
    nsCOMPtr<nsIStreamIO>               mStreamIO;
    nsCOMPtr<nsILoadGroup>              mLoadGroup;
    nsCOMPtr<nsISupports>               mOwner;
    nsCOMPtr<nsITransport>              mFileTransport;
    nsCOMPtr<nsIRequest>                mRequest;
    nsCOMPtr<nsIRequestObserver>        mUserObserver;
    PRUint32                            mBufferSegmentSize;
    PRUint32                            mBufferMaxSize;
    PRUint32                            mLoadFlags;
    nsresult                            mStatus;
};

#endif // nsInputStreamChannel_h__
