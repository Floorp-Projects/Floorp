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
 *  Darin Fisher <darin@netscape.com>
 */

#ifndef nsGopherChannel_h__
#define nsGopherChannel_h__

#include "nsGopherHandler.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsCOMPtr.h"

#include "nsILoadGroup.h"
#include "nsIInputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIPrompt.h"
#include "nsIProxy.h"
#include "nsIStreamListener.h"
#include "nsISocketTransport.h"
#include "nsIProgressEventSink.h"
#include "nsIProxyInfo.h"
#include "nsIDirectoryListing.h"
#include "nsIStringBundle.h"
#include "nsIInputStreamPump.h"

class nsGopherChannel : public nsIChannel,
                        public nsIStreamListener,
                        public nsIDirectoryListing,
                        public nsITransportEventSink {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSIDIRECTORYLISTING
    NS_DECL_NSITRANSPORTEVENTSINK

    // nsGopherChannel methods:
    nsGopherChannel();
    virtual ~nsGopherChannel();

    nsresult Init(nsIURI* uri, nsIProxyInfo* proxyInfo);

protected:
    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
    nsCOMPtr<nsIPrompt>                 mPrompter;
    nsCOMPtr<nsIProgressEventSink>      mProgressSink;
    nsCOMPtr<nsIURI>                    mUrl;
    nsCOMPtr<nsIStreamListener>         mListener;
    PRUint32                            mLoadFlags;
    nsCOMPtr<nsILoadGroup>              mLoadGroup;
    nsCString                           mContentType;
    nsCString                           mContentCharset;
    PRInt32                             mContentLength;
    nsCOMPtr<nsISupports>               mOwner; 
    PRUint32                            mListFormat;

    nsXPIDLCString                      mHost;
    PRInt32                             mPort;
    char                                mType;
    nsCString                           mSelector;
    nsCString                           mRequest;

    nsCOMPtr<nsISupports>               mListenerContext;
    nsCOMPtr<nsISocketTransport>        mTransport;
    nsCOMPtr<nsIInputStreamPump>        mPump;
    nsCOMPtr<nsIProxyInfo>              mProxyInfo;
    nsCOMPtr<nsIStringBundle>           mStringBundle;

    nsresult                            mStatus;
    PRBool                              mIsPending;

    nsresult SendRequest();
    nsresult PushStreamConverters(nsIStreamListener *, nsIStreamListener **);
};

#endif // !nsGopherChannel_h__
