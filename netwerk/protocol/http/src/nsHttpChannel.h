/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#ifndef nsHttpChannel_h__
#define nsHttpChannel_h__

#include "nsHttpRequestHead.h"
#include "nsIHttpChannel.h"
#include "nsIHttpEventSink.h"
#include "nsIStreamListener.h"
#include "nsIIOService.h"
#include "nsIURI.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIInputStream.h"
#include "nsIProgressEventSink.h"
#include "nsICachingChannel.h"
#include "nsICacheEntryDescriptor.h"
#include "nsICacheListener.h"
#include "nsITransport.h"
#include "nsIUploadChannel.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsHttpConnection.h"

class nsHttpTransaction;
class nsHttpResponseHead;
class nsIHttpAuthenticator;
class nsIProxyInfo;

//-----------------------------------------------------------------------------
// nsHttpChannel
//-----------------------------------------------------------------------------

class nsHttpChannel : public nsIHttpChannel
                    , public nsIStreamListener
                    , public nsIInterfaceRequestor
                    , public nsIProgressEventSink
                    , public nsICachingChannel
                    , public nsIUploadChannel
                    , public nsICacheListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIHTTPCHANNEL
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIPROGRESSEVENTSINK
    NS_DECL_NSICACHINGCHANNEL
    NS_DECL_NSIUPLOADCHANNEL
    NS_DECL_NSICACHELISTENER

    nsHttpChannel();
    virtual ~nsHttpChannel();

    nsresult Init(nsIURI *uri,
                  PRUint8 capabilities,
                  nsIProxyInfo* proxyInfo);

private:
    nsresult Connect(PRBool firstTime = PR_TRUE);
    nsresult AsyncAbort(nsresult status);
    nsresult AsyncRedirect();
    void     HandleAsyncRedirect();
    nsresult SetupTransaction();
    void     ApplyContentConversions();
    nsresult ProcessResponse();
    nsresult ProcessNormal();
    nsresult ProcessNotModified();
    nsresult ProcessRedirection(PRUint32 httpStatus);
    nsresult ProcessAuthentication(PRUint32 httpStatus);

    // cache specific methods
    nsresult OpenCacheEntry(PRBool *delayed);
    nsresult GenerateCacheKey(nsACString &key);
    nsresult UpdateExpirationTime();
    nsresult CheckCache();
    nsresult ReadFromCache();
    nsresult CloseCacheEntry(nsresult status);
    nsresult InitCacheEntry();
    nsresult FinalizeCacheEntry();
    nsresult InstallCacheListener();

    // auth specific methods
    nsresult GetCredentials(const char *challenges, PRBool proxyAuth, nsAFlatCString &creds);
    nsresult SelectChallenge(const char *challenges, nsAFlatCString &challenge, nsIHttpAuthenticator **); 
    nsresult GetAuthenticator(const char *scheme, nsIHttpAuthenticator **);
    nsresult GetUserPassFromURI(nsAString &user, nsAString &pass);
    nsresult ParseRealm(const char *challenge, nsACString &realm);
    nsresult PromptForUserPass(const char *host, PRInt32 port, PRBool proxyAuth, const char *realm, nsAString &user, nsAString &pass);
    nsresult AddAuthorizationHeaders();
    nsresult GetCurrentPath(char **);

    static void *PR_CALLBACK AsyncRedirect_EventHandlerFunc(PLEvent *);
    static void  PR_CALLBACK AsyncRedirect_EventCleanupFunc(PLEvent *);

private:
    nsCOMPtr<nsIURI>                  mOriginalURI;
    nsCOMPtr<nsIURI>                  mURI;
    nsCOMPtr<nsIStreamListener>       mListener;
    nsCOMPtr<nsISupports>             mListenerContext;
    nsCOMPtr<nsILoadGroup>            mLoadGroup;
    nsCOMPtr<nsISupports>             mOwner;
    nsCOMPtr<nsIInterfaceRequestor>   mCallbacks;
    nsCOMPtr<nsIProgressEventSink>    mProgressSink;
    nsCOMPtr<nsIHttpEventSink>        mHttpEventSink;
    nsCOMPtr<nsIInputStream>          mUploadStream;
    nsCOMPtr<nsIURI>                  mReferrer;
    nsCOMPtr<nsISupports>             mSecurityInfo;

    nsHttpRequestHead                 mRequestHead;
    nsHttpResponseHead               *mResponseHead;

    nsHttpTransaction                *mTransaction;     // hard ref
    nsHttpTransaction                *mPrevTransaction; // hard ref
    nsHttpConnectionInfo             *mConnectionInfo;  // hard ref

    nsXPIDLCString                    mSpec;

    PRUint32                          mLoadFlags;
    PRUint32                          mStatus;
    PRUint8                           mCapabilities;
    PRUint8                           mReferrerType;

    // cache specific data
    nsCOMPtr<nsICacheEntryDescriptor> mCacheEntry;
    nsCOMPtr<nsITransport>            mCacheTransport;
    nsCOMPtr<nsIRequest>              mCacheReadRequest;
    nsHttpResponseHead               *mCachedResponseHead;
    nsCacheAccessMode                 mCacheAccess;
    PRUint32                          mPostID;
    PRUint32                          mRequestTime;

    PRPackedBool                      mIsPending;
    PRPackedBool                      mApplyConversion;
    PRPackedBool                      mTriedCredentialsFromPrehost;
    PRPackedBool                      mFromCacheOnly;
    PRPackedBool                      mCachedContentIsValid;
    PRPackedBool                      mResponseHeadersModified;
};

#endif // nsHttpChannel_h__
