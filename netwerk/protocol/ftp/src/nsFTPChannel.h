/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsIIOService.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsILoadGroup.h"
#include "nsCOMPtr.h"
#include "nsIProtocolHandler.h"
#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIThreadPool.h"
#include "nsFtpConnectionThread.h"
#include "netCore.h"
#include "nsXPIDLString.h"
#include "nsIStreamListener.h"
#include "nsAutoLock.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsIFTPChannel.h"
#include "nsIUploadChannel.h"
#include "nsIProxyInfo.h"

#include "nsICacheService.h"
#include "nsICacheEntryDescriptor.h"
#include "nsICacheListener.h"
#include "nsICacheSession.h"

#define FTP_COMMAND_CHANNEL_SEG_SIZE 64
#define FTP_COMMAND_CHANNEL_MAX_SIZE 512

#define FTP_DATA_CHANNEL_SEG_SIZE (64*1024)
#define FTP_DATA_CHANNEL_MAX_SIZE (512*1024)

#define FTP_CACHE_CONTROL_CONNECTION 1

class nsFTPChannel : public nsIFTPChannel,
                     public nsIUploadChannel,
                     public nsIInterfaceRequestor,
                     public nsIProgressEventSink,
                     public nsIStreamListener, 
                     public nsICacheListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIUPLOADCHANNEL
    NS_DECL_NSIFTPCHANNEL
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIPROGRESSEVENTSINK
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSICACHELISTENER
    
    // nsFTPChannel methods:
    nsFTPChannel();
    virtual ~nsFTPChannel();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    // initializes the channel. 
    nsresult Init(nsIURI* uri,
                  nsIProxyInfo* proxyInfo,
                  nsICacheSession* session);

    nsresult SetupState();
    nsresult GenerateCacheKey(nsACString &cacheKey);
    

protected:
    nsCOMPtr<nsIURI>                mOriginalURI;
    nsCOMPtr<nsIURI>                mURL;
    
    nsCOMPtr<nsIInputStream>        mUploadStream;

    // various callback interfaces
    nsCOMPtr<nsIProgressEventSink>  mEventSink;
    nsCOMPtr<nsIPrompt>             mPrompter;
    nsCOMPtr<nsIFTPEventSink>       mFTPEventSink;
    nsCOMPtr<nsIAuthPrompt>         mAuthPrompter;
    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;

    PRBool                          mIsPending;
    PRUint32                        mLoadFlags;

    PRUint32                        mSourceOffset;
    PRInt32                         mAmount;
    nsCOMPtr<nsILoadGroup>          mLoadGroup;
    nsCString                       mContentType;
    PRInt32                         mContentLength;
    nsCOMPtr<nsISupports>           mOwner;

    nsCOMPtr<nsIStreamListener>     mListener;

    nsFtpState*                     mFTPState;   

    nsXPIDLCString                  mHost;
    PRLock*                         mLock;
    nsCOMPtr<nsISupports>           mUserContext;
    nsresult                        mStatus;
    PRPackedBool                    mCanceled;

    nsCOMPtr<nsIIOService>          mIOService;

    nsCOMPtr<nsICacheSession>         mCacheSession;
    nsCOMPtr<nsICacheEntryDescriptor> mCacheEntry;
    nsCOMPtr<nsIProxyInfo>            mProxyInfo;
};

#endif /* nsFTPChannel_h___ */
