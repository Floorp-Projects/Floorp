/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set et cin ts=4 sw=4 sts=4: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsHttpChannel_h__
#define nsHttpChannel_h__

#include "nsHttpTransaction.h"
#include "nsHttpRequestHead.h"
#include "nsHttpAuthCache.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsInt64.h"

#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
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
#include "nsIStringEnumerator.h"
#include "nsIOutputStream.h"
#include "nsIAsyncInputStream.h"
#include "nsIInputStreamPump.h"
#include "nsIPrompt.h"
#include "nsIResumableChannel.h"

class nsHttpResponseHead;
class nsAHttpConnection;
class nsIHttpAuthenticator;
class nsIProxyInfo;

//-----------------------------------------------------------------------------
// nsHttpChannel
//-----------------------------------------------------------------------------

class nsHttpChannel : public nsIHttpChannel
                    , public nsIHttpChannelInternal
                    , public nsIStreamListener
                    , public nsICachingChannel
                    , public nsIUploadChannel
                    , public nsICacheListener
                    , public nsIEncodedChannel
                    , public nsITransportEventSink
                    , public nsIResumableChannel
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIHTTPCHANNEL
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSICACHINGCHANNEL
    NS_DECL_NSIUPLOADCHANNEL
    NS_DECL_NSICACHELISTENER
    NS_DECL_NSIENCODEDCHANNEL
    NS_DECL_NSIHTTPCHANNELINTERNAL
    NS_DECL_NSITRANSPORTEVENTSINK
    NS_DECL_NSIRESUMABLECHANNEL

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
    nsresult SetupTransaction();
    void     AddCookiesToRequest();
    void     ApplyContentConversions();
    nsresult CallOnStartRequest();
    nsresult ProcessResponse();
    nsresult ProcessNormal();
    nsresult ProcessNotModified();
    nsresult ProcessRedirection(PRUint32 httpStatus);
    nsresult ProcessAuthentication(PRUint32 httpStatus);
    void     GetCallback(const nsIID &aIID, void **aResult);
    PRBool   ResponseWouldVary();

    // redirection specific methods
    void     HandleAsyncRedirect();
    void     HandleAsyncNotModified();
    nsresult PromptTempRedirect();
    nsresult ProxyFailover();
    nsresult SetupReplacementChannel(nsIURI *, nsIChannel *, PRBool preserveMethod);

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
    nsresult OnDoneReadingPartialCacheEntry(PRBool *streamDone);

    // auth specific methods
    nsresult GenCredsAndSetEntry(nsIHttpAuthenticator *, PRBool proxyAuth, const char *scheme, const char *host, PRInt32 port, const char *dir, const char *realm, const char *challenge, const nsHttpAuthIdentity &ident, nsCOMPtr<nsISupports> &session, char **result);
    nsresult GetCredentials(const char *challenges, PRBool proxyAuth, nsAFlatCString &creds);
    nsresult GetCredentialsForChallenge(const char *challenge, const char *scheme,  PRBool proxyAuth, nsIHttpAuthenticator *auth, nsAFlatCString &creds);
    nsresult GetAuthenticator(const char *challenge, nsCString &scheme, nsIHttpAuthenticator **auth); 
    void     ParseRealm(const char *challenge, nsACString &realm);
    void     GetIdentityFromURI(PRUint32 authFlags, nsHttpAuthIdentity&);
    nsresult PromptForIdentity(const char *scheme, const char *host, PRInt32 port, PRBool proxyAuth, const char *realm, const char *authType, PRUint32 authFlags, nsHttpAuthIdentity &);
    PRBool   ConfirmAuth(const nsString &bundleKey, PRBool doYesNoPrompt);
    void     CheckForSuperfluousAuth();
    void     SetAuthorizationHeader(nsHttpAuthCache *, nsHttpAtom header, const char *scheme, const char *host, PRInt32 port, const char *path, nsHttpAuthIdentity &ident);
    void     AddAuthorizationHeaders();
    nsresult GetCurrentPath(nsACString &);
    void     ClearPasswordManagerEntry(const char *scheme, const char *host, PRInt32 port, const char *realm, const PRUnichar *user);
    nsresult DoAuthRetry(nsAHttpConnection *);

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
    nsCOMPtr<nsIEventQueue>           mEventQ;

    nsHttpRequestHead                 mRequestHead;
    nsHttpResponseHead               *mResponseHead;

    nsCOMPtr<nsIInputStreamPump>      mTransactionPump;
    nsHttpTransaction                *mTransaction;     // hard ref
    nsHttpConnectionInfo             *mConnectionInfo;  // hard ref

    nsCString                         mSpec; // ASCII encoded URL spec

    PRUint32                          mLoadFlags;
    PRUint32                          mStatus;
    nsUint64                          mLogicalOffset;
    PRUint8                           mCaps;

    nsCString                         mContentTypeHint;
    nsCString                         mContentCharsetHint;

    nsCOMPtr<nsISupports>             mProperties;
    
    // cache specific data
    nsCOMPtr<nsICacheEntryDescriptor> mCacheEntry;
    nsCOMPtr<nsIInputStreamPump>      mCachePump;
    nsHttpResponseHead               *mCachedResponseHead;
    nsCacheAccessMode                 mCacheAccess;
    PRUint32                          mPostID;
    PRUint32                          mRequestTime;

    // auth specific data
    nsISupports                      *mAuthContinuationState;
    nsCString                         mAuthType;
    nsHttpAuthIdentity                mIdent;
    nsHttpAuthIdentity                mProxyIdent;

    // Resumable channel specific data
    nsCString                         mEntityID;
    PRUint64                          mStartPos;

    // redirection specific data.
    PRUint8                           mRedirectionLimit;

    // state flags
    PRUint32                          mIsPending                : 1;
    PRUint32                          mApplyConversion          : 1;
    PRUint32                          mAllowPipelining          : 1;
    PRUint32                          mCachedContentIsValid     : 1;
    PRUint32                          mCachedContentIsPartial   : 1;
    PRUint32                          mResponseHeadersModified  : 1;
    PRUint32                          mCanceled                 : 1;
    PRUint32                          mTransactionReplaced      : 1;
    PRUint32                          mUploadStreamHasHeaders   : 1;
    PRUint32                          mAuthRetryPending         : 1;
    PRUint32                          mSuppressDefensiveAuth    : 1;
    PRUint32                          mResuming                 : 1;

    class nsContentEncodings : public nsIUTF8StringEnumerator
    {
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIUTF8STRINGENUMERATOR

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
