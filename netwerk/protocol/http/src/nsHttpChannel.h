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
#include "nsIHttpHeaderVisitor.h"
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
#include "nsIEncodedChannel.h"
#include "nsITransport.h"
#include "nsIUploadChannel.h"
#include "nsISimpleEnumerator.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsHttpConnection.h"

#include "nsIHttpChannelInternal.h"

class nsHttpTransaction;
class nsHttpResponseHead;
class nsHttpAuthCache;
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
                    , public nsIEncodedChannel
                    , public nsIHttpChannelInternal
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
    NS_DECL_NSIENCODEDCHANNEL
    NS_DECL_NSIHTTPCHANNELINTERNAL

    nsHttpChannel();
    virtual ~nsHttpChannel();

    nsresult Init(nsIURI *uri,
                  PRUint8 capabilities,
                  nsIProxyInfo* proxyInfo);

public: /* internal; workaround lame compilers */ 
    typedef void (nsHttpChannel:: *nsAsyncCallback)(void);

private:
    //
    // AsyncCall may be used to call a member function asynchronously.
    //
    struct nsAsyncCallEvent : PLEvent
    {
        nsAsyncCallback mFuncPtr;
    };

    nsresult AsyncCall(nsAsyncCallback funcPtr);

    nsresult Connect(PRBool firstTime = PR_TRUE);
    nsresult AsyncAbort(nsresult status);
    void     HandleAsyncRedirect();
    void     HandleAsyncNotModified();
    nsresult SetupTransaction();
    void     ApplyContentConversions();
    nsresult CallOnStartRequest();
    nsresult ProcessResponse();
    nsresult ProcessNormal();
    nsresult ProcessNotModified();
    nsresult ProcessRedirection(PRUint32 httpStatus);
    nsresult ProcessAuthentication(PRUint32 httpStatus);

    // cache specific methods
    nsresult OpenCacheEntry(PRBool offline, PRBool *delayed);
    nsresult GenerateCacheKey(nsACString &key);
    nsresult UpdateExpirationTime();
    nsresult CheckCache();
    nsresult ReadFromCache();
    nsresult CloseCacheEntry(nsresult status);
    nsresult InitCacheEntry();
    nsresult StoreAuthorizationMetaData();
    nsresult FinalizeCacheEntry();
    nsresult InstallCacheListener(PRUint32 offset = 0);

    // byte range request specific methods
    nsresult SetupByteRangeRequest(PRUint32 partialLen);
    nsresult ProcessPartialContent();
    nsresult BufferPartialContent(nsIInputStream *, PRUint32 count);
    nsresult OnDoneReadingPartialCacheEntry(PRBool *streamDone);

    // auth specific methods
    nsresult GetCredentials(const char *challenges, PRBool proxyAuth, nsAFlatCString &creds);
    nsresult SelectChallenge(const char *challenges, nsAFlatCString &challenge, nsIHttpAuthenticator **); 
    nsresult GetAuthenticator(const char *scheme, nsIHttpAuthenticator **);
    void     GetUserPassFromURI(PRUnichar **user, PRUnichar **pass);
    void     ParseRealm(const char *challenge, nsACString &realm);
    nsresult PromptForUserPass(const char *host, PRInt32 port, PRBool proxyAuth, const char *realm, PRUnichar **user, PRUnichar **pass);
    void     SetAuthorizationHeader(nsHttpAuthCache *, nsHttpAtom header, const char *host, PRInt32 port, const char *path, PRUnichar **user, PRUnichar **pass);
    void     AddAuthorizationHeaders();
    nsresult GetCurrentPath(nsACString &);
    void     ClearPasswordManagerEntry(const char *host, PRInt32 port, const char *realm, const PRUnichar *user);

    static void *PR_CALLBACK AsyncCall_EventHandlerFunc(PLEvent *);
    static void  PR_CALLBACK AsyncCall_EventCleanupFunc(PLEvent *);

private:
    nsCOMPtr<nsIURI>                  mOriginalURI;
    nsCOMPtr<nsIURI>                  mURI;
    nsCOMPtr<nsIURI>                  mDocumentURI;
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

    nsCString                         mSpec; // ASCII encoded URL spec

    PRUint32                          mLoadFlags;
    PRUint32                          mStatus;
    PRUint32                          mLogicalOffset;
    PRUint8                           mCapabilities;

    // cache specific data
    nsCOMPtr<nsICacheEntryDescriptor> mCacheEntry;
    nsCOMPtr<nsITransport>            mCacheTransport;
    nsCOMPtr<nsIRequest>              mCacheReadRequest;
    nsHttpResponseHead               *mCachedResponseHead;
    nsCacheAccessMode                 mCacheAccess;
    PRUint32                          mPostID;
    PRUint32                          mRequestTime;

    // byte-range specific data
    nsCOMPtr<nsIInputStream>          mBufferIn;
    nsCOMPtr<nsIOutputStream>         mBufferOut;

    // auth specific data
    nsXPIDLString                     mUser;
    nsXPIDLString                     mPass;
    nsXPIDLString                     mProxyUser;
    nsXPIDLString                     mProxyPass;

    // redirection specific data.
    PRUint8                           mRedirectionLimit;

    PRPackedBool                      mIsPending;
    PRPackedBool                      mApplyConversion;
    PRPackedBool                      mAllowPipelining;
    PRPackedBool                      mCachedContentIsValid;
    PRPackedBool                      mCachedContentIsPartial;
    PRPackedBool                      mResponseHeadersModified;
    PRPackedBool                      mCanceled;
    PRPackedBool                      mUploadStreamHasHeaders;

    class nsContentEncodings : public nsISimpleEnumerator
    {
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSISIMPLEENUMERATOR

        nsContentEncodings(nsIHttpChannel* aChannel, const char* aEncodingHeader);
        virtual ~nsContentEncodings();
        
    private:
        nsresult PrepareForNext(void);
        
        // We do not own the buffer.  The channel owns it.
        const char* mEncodingHeader;
        const char* mCurStart;  // points to start of current header
        const char* mCurEnd;  // points to end of current header
        
        // Hold a ref to our channel so that it can't go away and take the
        // header with it.
        nsCOMPtr<nsIHttpChannel> mChannel;
        
        PRPackedBool mReady;
    };
};

#endif // nsHttpChannel_h__
