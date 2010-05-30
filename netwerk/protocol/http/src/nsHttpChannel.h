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
 *   Christian Biesinger <cbiesinger@web.de>
 *   Daniel Witte <dwitte@mozilla.com>
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

#include "HttpBaseChannel.h"

#include "nsHttpTransaction.h"
#include "nsHttpAuthCache.h"
#include "nsInputStreamPump.h"
#include "nsThreadUtils.h"

#include "nsIHttpEventSink.h"
#include "nsICachingChannel.h"
#include "nsICacheEntryDescriptor.h"
#include "nsICacheListener.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIEncodedChannel.h"
#include "nsIStringEnumerator.h"
#include "nsIPrompt.h"
#include "nsIResumableChannel.h"
#include "nsIProtocolProxyCallback.h"
#include "nsICancelable.h"
#include "nsIProxiedChannel.h"
#include "nsITraceableChannel.h"
#include "nsIAuthPromptCallback.h"

class nsAHttpConnection;
class nsIHttpAuthenticator;

using namespace mozilla::net;

//-----------------------------------------------------------------------------
// nsHttpChannel
//-----------------------------------------------------------------------------

class nsHttpChannel : public HttpBaseChannel
                    , public nsIStreamListener
                    , public nsICachingChannel
                    , public nsICacheListener
                    , public nsIEncodedChannel
                    , public nsITransportEventSink
                    , public nsIResumableChannel
                    , public nsIProtocolProxyCallback
                    , public nsIProxiedChannel
                    , public nsITraceableChannel
                    , public nsIApplicationCacheChannel
                    , public nsIAuthPromptCallback
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSICACHINGCHANNEL
    NS_DECL_NSICACHELISTENER
    NS_DECL_NSIENCODEDCHANNEL
    NS_DECL_NSITRANSPORTEVENTSINK
    NS_DECL_NSIRESUMABLECHANNEL
    NS_DECL_NSIPROTOCOLPROXYCALLBACK
    NS_DECL_NSIPROXIEDCHANNEL
    NS_DECL_NSITRACEABLECHANNEL
    NS_DECL_NSIAPPLICATIONCACHECONTAINER
    NS_DECL_NSIAPPLICATIONCACHECHANNEL
    NS_DECL_NSIAUTHPROMPTCALLBACK

    nsHttpChannel();
    virtual ~nsHttpChannel();

    // Methods HttpBaseChannel didn't implement for us or that we override.
    //
    // nsIRequest
    NS_IMETHOD Cancel(nsresult status);
    NS_IMETHOD Suspend();
    NS_IMETHOD Resume();
    // nsIChannel
    NS_IMETHOD GetSecurityInfo(nsISupports **aSecurityInfo);
    NS_IMETHOD AsyncOpen(nsIStreamListener *listener, nsISupports *aContext);
    // nsIHttpChannelInternal
    NS_IMETHOD SetupFallbackChannel(const char *aFallbackKey);
    // nsISupportsPriority
    NS_IMETHOD SetPriority(PRInt32 value);

public: /* internal necko use only */ 
    typedef void (nsHttpChannel:: *nsAsyncCallback)(void);
    nsHttpResponseHead * GetResponseHead() const { return mResponseHead; }
    void SetRemoteChannel() { mRemoteChannel = 1; }
    void InternalSetUploadStream(nsIInputStream *uploadStream) 
      { mUploadStream = uploadStream; }
    void SetUploadStreamHasHeaders(PRBool hasHeaders) 
      { mUploadStreamHasHeaders = hasHeaders; }

    nsresult SetReferrerInternal(nsIURI *referrer) {
        nsCAutoString spec;
        nsresult rv = referrer->GetAsciiSpec(spec);
        if (NS_FAILED(rv)) return rv;
        mReferrer = referrer;
        mRequestHead.SetHeader(nsHttp::Referer, spec);
        return NS_OK;
    }

private:
    // AsyncCall may be used to call a member function asynchronously.
    // retval isn't refcounted and is set only when event was successfully
    // posted, the event is returned for the purpose of cancelling when needed
    nsresult AsyncCall(nsAsyncCallback funcPtr,
                       nsRunnableMethod<nsHttpChannel> **retval = nsnull);

    PRBool   RequestIsConditional();
    nsresult Connect(PRBool firstTime = PR_TRUE);
    nsresult AsyncAbort(nsresult status);
    // Send OnStartRequest/OnStopRequest to our listener, if any.
    void     HandleAsyncNotifyListener();
    void     DoNotifyListener();
    nsresult SetupTransaction();
    void     AddCookiesToRequest();
    nsresult ApplyContentConversions();
    nsresult CallOnStartRequest();
    nsresult ProcessResponse();
    nsresult ProcessNormal();
    nsresult ProcessNotModified();
    nsresult ProcessRedirection(PRUint32 httpStatus);
    PRBool   ShouldSSLProxyResponseContinue(PRUint32 httpStatus);
    nsresult ProcessFailedSSLConnect(PRUint32 httpStatus);
    nsresult ProcessAuthentication(PRUint32 httpStatus);
    nsresult ProcessFallback(PRBool *fallingBack);
    PRBool   ResponseWouldVary();

    // redirection specific methods
    void     HandleAsyncRedirect();
    void     HandleAsyncNotModified();
    void     HandleAsyncFallback();
    nsresult PromptTempRedirect();
    nsresult SetupReplacementChannel(nsIURI *, nsIChannel *, PRBool preserveMethod);

    // proxy specific methods
    nsresult ProxyFailover();
    nsresult DoReplaceWithProxy(nsIProxyInfo *);
    void HandleAsyncReplaceWithProxy();
    nsresult ResolveProxy();

    // cache specific methods
    nsresult OpenCacheEntry(PRBool offline, PRBool *delayed);
    nsresult OpenOfflineCacheEntryForWriting();
    nsresult GenerateCacheKey(PRUint32 postID, nsACString &key);
    nsresult UpdateExpirationTime();
    nsresult CheckCache();
    nsresult ShouldUpdateOfflineCacheEntry(PRBool *shouldCacheForOfflineUse);
    nsresult ReadFromCache();
    void     CloseCacheEntry(PRBool doomOnFailure);
    void     CloseOfflineCacheEntry();
    nsresult InitCacheEntry();
    nsresult InitOfflineCacheEntry();
    nsresult AddCacheEntryHeaders(nsICacheEntryDescriptor *entry);
    nsresult StoreAuthorizationMetaData(nsICacheEntryDescriptor *entry);
    nsresult FinalizeCacheEntry();
    nsresult InstallCacheListener(PRUint32 offset = 0);
    nsresult InstallOfflineCacheListener();
    void     MaybeInvalidateCacheEntryForSubsequentGet();
    nsCacheStoragePolicy DetermineStoragePolicy();
    void     AsyncOnExamineCachedResponse();

    // Handle the bogus Content-Encoding Apache sometimes sends
    void ClearBogusContentEncodingIfNeeded();

    // byte range request specific methods
    nsresult SetupByteRangeRequest(PRUint32 partialLen);
    nsresult ProcessPartialContent();
    nsresult OnDoneReadingPartialCacheEntry(PRBool *streamDone);

    // auth specific methods
    nsresult PrepareForAuthentication(PRBool proxyAuth);
    nsresult GenCredsAndSetEntry(nsIHttpAuthenticator *, PRBool proxyAuth, const char *scheme, const char *host, PRInt32 port, const char *dir, const char *realm, const char *challenge, const nsHttpAuthIdentity &ident, nsCOMPtr<nsISupports> &session, char **result);
    nsresult GetAuthenticator(const char *challenge, nsCString &scheme, nsIHttpAuthenticator **auth); 
    void     ParseRealm(const char *challenge, nsACString &realm);
    void     GetIdentityFromURI(PRUint32 authFlags, nsHttpAuthIdentity&);
    /**
     * Following three methods return NS_ERROR_IN_PROGRESS when
     * nsIAuthPrompt2.asyncPromptAuth method is called. This result indicates
     * the user's decision will be gathered in a callback and is not an actual
     * error.
     */
    nsresult GetCredentials(const char *challenges, PRBool proxyAuth, nsAFlatCString &creds);
    nsresult GetCredentialsForChallenge(const char *challenge, const char *scheme,  PRBool proxyAuth, nsIHttpAuthenticator *auth, nsAFlatCString &creds);
    nsresult PromptForIdentity(PRUint32 level, PRBool proxyAuth, const char *realm, const char *authType, PRUint32 authFlags, nsHttpAuthIdentity &);

    PRBool   ConfirmAuth(const nsString &bundleKey, PRBool doYesNoPrompt);
    void     CheckForSuperfluousAuth();
    void     SetAuthorizationHeader(nsHttpAuthCache *, nsHttpAtom header, const char *scheme, const char *host, PRInt32 port, const char *path, nsHttpAuthIdentity &ident);
    void     AddAuthorizationHeaders();
    nsresult GetCurrentPath(nsACString &);
    /**
     * Return all information needed to build authorization information,
     * all paramters except proxyAuth are out parameters. proxyAuth specifies
     * with what authorization we work (WWW or proxy).
     */
    nsresult GetAuthorizationMembers(PRBool proxyAuth, nsCSubstring& scheme, const char*& host, PRInt32& port, nsCSubstring& path, nsHttpAuthIdentity*& ident, nsISupports**& continuationState);
    nsresult DoAuthRetry(nsAHttpConnection *);
    PRBool   MustValidateBasedOnQueryUrl();
    /**
     * Method called to resume suspended transaction after we got credentials
     * from the user. Called from OnAuthAvailable callback or OnAuthCancelled
     * when credentials for next challenge were obtained synchronously.
     */
    nsresult ContinueOnAuthAvailable(const nsCSubstring& creds);

private:
    nsCOMPtr<nsISupports>             mSecurityInfo;
    nsCOMPtr<nsICancelable>           mProxyRequest;

    nsRefPtr<nsInputStreamPump>       mTransactionPump;
    nsRefPtr<nsHttpTransaction>       mTransaction;

    PRUint64                          mLogicalOffset;
    nsCString                         mUserSetCookieHeader;

    // cache specific data
    nsCOMPtr<nsICacheEntryDescriptor> mCacheEntry;
    nsRefPtr<nsInputStreamPump>       mCachePump;
    nsAutoPtr<nsHttpResponseHead>     mCachedResponseHead;
    nsCacheAccessMode                 mCacheAccess;
    PRUint32                          mPostID;
    PRUint32                          mRequestTime;

    nsCOMPtr<nsICacheEntryDescriptor> mOfflineCacheEntry;
    nsCacheAccessMode                 mOfflineCacheAccess;
    nsCString                         mOfflineCacheClientID;

    nsCOMPtr<nsIApplicationCache>     mApplicationCache;

    // auth specific data
    nsISupports                      *mProxyAuthContinuationState;
    nsCString                         mProxyAuthType;
    nsISupports                      *mAuthContinuationState;
    nsCString                         mAuthType;
    nsHttpAuthIdentity                mIdent;
    nsHttpAuthIdentity                mProxyIdent;

    // Reference to the prompt wating in prompt queue. The channel is
    // responsible to call its cancel method when user in any way cancels
    // this request.
    nsCOMPtr<nsICancelable>           mAsyncPromptAuthCancelable;
    // Saved in GetCredentials when prompt is asynchronous, the first challenge
    // we obtained from the server with 401/407 response, will be processed in
    // OnAuthAvailable callback.
    nsCString                         mCurrentChallenge;
    // Saved in GetCredentials when prompt is asynchronous, remaning challenges
    // we have to process when user cancels the auth dialog for the current
    // challenge.
    nsCString                         mRemainingChallenges;

    // Resumable channel specific data
    nsCString                         mEntityID;
    PRUint64                          mStartPos;

    // Function pointer that can be set to indicate that we got suspended while
    // waiting on an AsyncCall.  When we get resumed we should AsyncCall this
    // function.
    nsAsyncCallback                   mPendingAsyncCallOnResume;

    // Proxy info to replace with
    nsCOMPtr<nsIProxyInfo>            mTargetProxyInfo;

    // Suspend counter.  This is used if someone tries to suspend/resume us
    // before we have either a cache pump or a transaction pump.
    PRUint32                          mSuspendCount;

    // If the channel is associated with a cache, and the URI matched
    // a fallback namespace, this will hold the key for the fallback
    // cache entry.
    nsCString                         mFallbackKey;

    // state flags
    PRUint32                          mApplyConversion          : 1;
    PRUint32                          mCachedContentIsValid     : 1;
    PRUint32                          mCachedContentIsPartial   : 1;
    PRUint32                          mCanceled                 : 1;
    PRUint32                          mTransactionReplaced      : 1;
    PRUint32                          mAuthRetryPending         : 1;
    // True when we need to authenticate to proxy, i.e. when we get 407
    // response. Used in OnAuthAvailable and OnAuthCancelled callbacks.
    PRUint32                          mProxyAuth                : 1;
    PRUint32                          mTriedProxyAuth           : 1;
    PRUint32                          mTriedHostAuth            : 1;
    PRUint32                          mSuppressDefensiveAuth    : 1;
    PRUint32                          mResuming                 : 1;
    PRUint32                          mInitedCacheEntry         : 1;
    PRUint32                          mCacheForOfflineUse       : 1;
    // True if mCacheForOfflineUse was set because we were caching
    // opportunistically.
    PRUint32                          mCachingOpportunistically : 1;
    // True if we are loading a fallback cache entry from the
    // application cache.
    PRUint32                          mFallbackChannel          : 1;
    PRUint32                          mInheritApplicationCache  : 1;
    PRUint32                          mChooseApplicationCache   : 1;
    PRUint32                          mLoadedFromApplicationCache : 1;
    PRUint32                          mTracingEnabled           : 1;
    // True if consumer added its own If-None-Match or If-Modified-Since
    // headers. In such a case we must not override them in the cache code
    // and also we want to pass possible 304 code response through.
    PRUint32                          mCustomConditionalRequest : 1;
    // True iff this channel is servicing a remote HttpChannelChild
    PRUint32                          mRemoteChannel : 1;

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
