/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsFTPChannel.h"
#include "nsIStreamListener.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsIProxyObjectManager.h"
#include "nsReadableUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIStreamConverterService.h"
#include "nsISocketTransport.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gFTPLog;
#endif /* PR_LOGGING */

////////////// this needs to move to nspr
static inline PRUint32
PRTimeToSeconds(PRTime t_usec)
{
    PRTime usec_per_sec;
    PRUint32 t_sec;
    LL_I2L(usec_per_sec, PR_USEC_PER_SEC);
    LL_DIV(t_usec, t_usec, usec_per_sec);
    LL_L2I(t_sec, t_usec);
    return t_sec;
}

#define NowInSeconds() PRTimeToSeconds(PR_Now())
////////////// end


// There are two transport connections established for an 
// ftp connection. One is used for the command channel , and
// the other for the data channel. The command channel is the first
// connection made and is used to negotiate the second, data, channel.
// The data channel is driven by the command channel and is either
// initiated by the server (PORT command) or by the client (PASV command).
// Client initiation is the most common case and is attempted first.

nsFTPChannel::nsFTPChannel()
    : mIsPending(0),
      mLoadFlags(LOAD_NORMAL),
      mListFormat(FORMAT_HTML),
      mSourceOffset(0),
      mAmount(0),
      mContentLength(-1),
      mFTPState(nsnull),
      mLock(nsnull),
      mStatus(NS_OK),
      mCanceled(PR_FALSE),
      mStartPos(LL_MaxUint())
{
}

nsFTPChannel::~nsFTPChannel()
{
#if defined(PR_LOGGING)
    nsCAutoString spec;
    mURL->GetAsciiSpec(spec);
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("~nsFTPChannel() for %s", spec.get()));
#endif
    NS_IF_RELEASE(mFTPState);
    if (mLock) PR_DestroyLock(mLock);
}

NS_IMPL_ADDREF(nsFTPChannel)
NS_IMPL_RELEASE(nsFTPChannel)

NS_INTERFACE_MAP_BEGIN(nsFTPChannel)
    NS_INTERFACE_MAP_ENTRY(nsIChannel)
    NS_INTERFACE_MAP_ENTRY(nsIFTPChannel)
    NS_INTERFACE_MAP_ENTRY(nsIResumableChannel)
    NS_INTERFACE_MAP_ENTRY(nsIUploadChannel)
    NS_INTERFACE_MAP_ENTRY(nsIRequest)
    NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor) 
    NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)
    NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
    NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
    NS_INTERFACE_MAP_ENTRY(nsICacheListener)
    NS_INTERFACE_MAP_ENTRY(nsIDirectoryListing)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIChannel)
NS_INTERFACE_MAP_END

nsresult
nsFTPChannel::Init(nsIURI* uri, nsIProxyInfo* proxyInfo, nsICacheSession* session)
{
    nsresult rv = NS_OK;

    // setup channel state
    mURL = uri;
    mProxyInfo = proxyInfo;

    rv = mURL->GetAsciiHost(mHost);
    if (NS_FAILED(rv)) return rv;

    if (!mLock) {
        mLock = PR_NewLock();
        if (!mLock) return NS_ERROR_OUT_OF_MEMORY;
    }

    mIOService = do_GetIOService(&rv);
    if (NS_FAILED(rv)) return rv;

    mCacheSession = session;
    
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

// The FTP channel doesn't maintain any connection state. Nor does it
// interpret the protocol. The FTP connection thread is responsible for
// these things and thus, the FTP channel simply calls through to the 
// FTP connection thread using an xpcom proxy object to make the
// cross thread call.

NS_IMETHODIMP
nsFTPChannel::GetName(nsACString &result)
{
    return mURL->GetSpec(result);
}

NS_IMETHODIMP
nsFTPChannel::IsPending(PRBool *result) {
    *result = mIsPending;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetStatus(nsresult *status)
{
    *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::Cancel(nsresult status) {

    PR_LOG(gFTPLog, 
           PR_LOG_DEBUG, 
           ("nsFTPChannel::Cancel() called [this=%x, status=%x, mCanceled=%d]\n", 
            this, status, mCanceled));

    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
    nsAutoLock lock(mLock);
    
    if (mCanceled) 
        return NS_OK;

    mCanceled = PR_TRUE;

    mStatus = status;

    if (mFTPState) 
        (void)mFTPState->Cancel(status);

    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::Suspend(void) {

    PR_LOG(gFTPLog, PR_LOG_DEBUG, 
           ("nsFTPChannel::Suspend() called [this=%x]\n", this));

    nsAutoLock lock(mLock);
    if (mFTPState) {
        return mFTPState->Suspend();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::Resume(void) {

    PR_LOG(gFTPLog, PR_LOG_DEBUG, 
           ("nsFTPChannel::Resume() called [this=%x]\n", this));

    nsAutoLock lock(mLock);
    if (mFTPState) {
        return mFTPState->Resume();
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
nsFTPChannel::GetOriginalURI(nsIURI* *aURL)
{
    *aURL = mOriginalURI ? mOriginalURI : mURL;
    NS_ADDREF(*aURL);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetOriginalURI(nsIURI* aURL)
{
    mOriginalURI = aURL;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetURI(nsIURI* *aURL)
{
    *aURL = mURL;
    NS_ADDREF(*aURL);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::Open(nsIInputStream **result)
{
    return NS_ImplementChannelOpen(this, result);
}

nsresult
nsFTPChannel::GenerateCacheKey(nsACString &cacheKey)
{
    cacheKey.SetLength(0);
    
    nsCAutoString spec;
    mURL->GetAsciiSpec(spec);

    // Strip any trailing #ref from the URL before using it as the key
    const char *p = strchr(spec.get(), '#');
    if (p)
        cacheKey.Append(Substring(spec, 0, p - spec.get()));
    else
        cacheKey.Append(spec);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
    nsresult rv = AsyncOpenAt(listener, ctxt, mStartPos, mEntityID);
    // mEntityID no longer needed, clear it to avoid returning a wrong entity
    // id when someone asks us
    mEntityID = nsnull;
    return rv;
}

NS_IMETHODIMP
nsFTPChannel::ResumeAt(PRUint64 aStartPos, nsIResumableEntityID* aEntityID)
{
    mEntityID = aEntityID;
    mStartPos = aStartPos;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetEntityID(nsIResumableEntityID **entityID)
{
    *entityID = mEntityID;
    NS_IF_ADDREF(*entityID);
    return NS_OK;
}

nsresult
nsFTPChannel::AsyncOpenAt(nsIStreamListener *listener, nsISupports *ctxt,
                          PRUint64 startPos, nsIResumableEntityID* entityID)
{
    PRInt32 port;
    nsresult rv = mURL->GetPort(&port);
    if (NS_FAILED(rv))
        return rv;
 
    rv = NS_CheckPortSafety(port, "ftp", mIOService);
    if (NS_FAILED(rv))
        return rv;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::AsyncOpen() called\n"));

    mListener = listener;
    mUserContext = ctxt;

    // Add this request to the load group
    if (mLoadGroup) {
        rv = mLoadGroup->AddRequest(this, nsnull);
        if (NS_FAILED(rv)) return rv;
    }
    PRBool offline;

    // If we're starting from the beginning, then its OK to use the cache,
    // because the entire file must be there (the cache doesn't support
    // partial entries yet)
    // Note that ftp doesn't store metadata, so disable caching if there was
    // an entityID. Storing this metadata isn't worth it until we can
    // get partial data out of the cache anyway...
    if (mCacheSession && !mUploadStream && !entityID &&
        (startPos==0 || startPos==PRUint32(-1))) {
        mIOService->GetOffline(&offline);

        // Set the desired cache access mode accordingly...
        nsCacheAccessMode accessRequested;
        if (offline) {
            // Since we are offline, we can only read from the cache.
            accessRequested = nsICache::ACCESS_READ;
        }
        else if (mLoadFlags & LOAD_BYPASS_CACHE)
            accessRequested = nsICache::ACCESS_WRITE; // replace cache entry
        else
            accessRequested = nsICache::ACCESS_READ_WRITE; // normal browsing
        
        nsCAutoString cacheKey;
        GenerateCacheKey(cacheKey);

        rv = mCacheSession->AsyncOpenCacheEntry(cacheKey.get(),
                                                accessRequested,
                                                this);
        if (NS_SUCCEEDED(rv)) return rv;
        
        // If we failed to use the cache, try without
        PR_LOG(gFTPLog, PR_LOG_DEBUG,
               ("Opening cache entry failed [rv=%x]", rv));
    }
    
    return SetupState(startPos, entityID);
}

nsresult 
nsFTPChannel::SetupState(PRUint32 startPos, nsIResumableEntityID* entityID)
{
    if (!mFTPState) {
        NS_NEWXPCOM(mFTPState, nsFtpState);
        if (!mFTPState) return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(mFTPState);
    }
    nsresult rv = mFTPState->Init(this, 
                                  mPrompter, 
                                  mAuthPrompter, 
                                  mFTPEventSink, 
                                  mCacheEntry,
                                  mProxyInfo,
                                  startPos,
                                  entityID);
    if (NS_FAILED(rv)) return rv;

    (void) mFTPState->SetWriteStream(mUploadStream);

    rv = mFTPState->Connect();
    if (NS_FAILED(rv)) return rv;

    mIsPending = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetLoadFlags(PRUint32 *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetLoadFlags(PRUint32 aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

// FTP does not provide a file typing mechanism. We fallback to file
// extension mapping.

NS_IMETHODIMP
nsFTPChannel::GetContentType(nsACString &aContentType)
{
    nsAutoLock lock(mLock);

    if (mContentType.IsEmpty()) {
        aContentType = NS_LITERAL_CSTRING(UNKNOWN_CONTENT_TYPE);
    } else {
        aContentType = mContentType;
    }

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::GetContentType() returned %s\n", PromiseFlatCString(aContentType).get()));
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetContentType(const nsACString &aContentType)
{
    nsAutoLock lock(mLock);
    NS_ParseContentType(aContentType, mContentType, mContentCharset);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetContentCharset(nsACString &aContentCharset)
{
    aContentCharset = mContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetContentCharset(const nsACString &aContentCharset)
{
    mContentCharset = aContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetContentLength(PRInt32 *aContentLength)
{
    nsAutoLock lock(mLock);
    *aContentLength = mContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetContentLength(PRInt32 aContentLength)
{
    nsAutoLock lock(mLock);
    mContentLength = aContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetOwner(nsISupports* *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetOwner(nsISupports* aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
    NS_IF_ADDREF(*aNotificationCallbacks = mCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
    mCallbacks = aNotificationCallbacks;

    if (mCallbacks) {

        // nsIProgressEventSink
        nsCOMPtr<nsIProgressEventSink> sink;
        (void)mCallbacks->GetInterface(NS_GET_IID(nsIProgressEventSink), 
                                       getter_AddRefs(sink));

        if (sink)
            NS_GetProxyForObject(NS_CURRENT_EVENTQ, 
                                 NS_GET_IID(nsIProgressEventSink), 
                                 sink, 
                                 PROXY_ASYNC | PROXY_ALWAYS, 
                                 getter_AddRefs(mEventSink));

        
        // nsIFTPEventSink
        nsCOMPtr<nsIFTPEventSink> ftpSink;
        (void)mCallbacks->GetInterface(NS_GET_IID(nsIFTPEventSink),
                                       getter_AddRefs(ftpSink));
        
        if (ftpSink)
            NS_GetProxyForObject(NS_CURRENT_EVENTQ, 
                                 NS_GET_IID(nsIFTPEventSink), 
                                 sink, 
                                 PROXY_ASYNC | PROXY_ALWAYS, 
                                 getter_AddRefs(mFTPEventSink));        

        // nsIPrompt
        nsCOMPtr<nsIPrompt> prompt;
        (void)mCallbacks->GetInterface(NS_GET_IID(nsIPrompt),
                                       getter_AddRefs(prompt));

        NS_ASSERTION ( prompt, "Channel doesn't have a prompt!!!" );

        if (prompt)
            NS_GetProxyForObject(NS_CURRENT_EVENTQ, 
                                 NS_GET_IID(nsIPrompt), 
                                 prompt, 
                                 PROXY_SYNC, 
                                 getter_AddRefs(mPrompter));

        // nsIAuthPrompt
        nsCOMPtr<nsIAuthPrompt> aPrompt;
        (void)mCallbacks->GetInterface(NS_GET_IID(nsIAuthPrompt),
                                       getter_AddRefs(aPrompt));

        if (aPrompt)
            NS_GetProxyForObject(NS_CURRENT_EVENTQ, 
                                 NS_GET_IID(nsIAuthPrompt), 
                                 aPrompt, 
                                 PROXY_SYNC, 
                                 getter_AddRefs(mAuthPrompter));

    }
    return NS_OK;
}

NS_IMETHODIMP 
nsFTPChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

// nsIInterfaceRequestor method
NS_IMETHODIMP
nsFTPChannel::GetInterface(const nsIID &anIID, void **aResult ) {
    // capture the progress event sink stuff. pass the rest through.
    if (anIID.Equals(NS_GET_IID(nsIProgressEventSink))) {
        *aResult = NS_STATIC_CAST(nsIProgressEventSink*, this);
        NS_ADDREF(this);
        return NS_OK;
    } else {
        return mCallbacks ? mCallbacks->GetInterface(anIID, aResult) : NS_ERROR_NO_INTERFACE;
    }
}

// nsIProgressEventSink methods
NS_IMETHODIMP
nsFTPChannel::OnStatus(nsIRequest *request, nsISupports *aContext,
                       nsresult aStatus, const PRUnichar* aStatusArg)
{
    if (aStatus == NS_NET_STATUS_CONNECTED_TO)
    {
        // The state machine needs to know that the data connection
        // was successfully started so that it can issue data commands
        // securely.
        if (mFTPState)
            mFTPState->DataConnectionEstablished();
        else
            NS_ERROR("ftp state is null.");
    }

    if (!mEventSink || (mLoadFlags & LOAD_BACKGROUND) || !mIsPending || NS_FAILED(mStatus))
        return NS_OK;

    return mEventSink->OnStatus(this, mUserContext, aStatus,
                                NS_ConvertASCIItoUCS2(mHost).get());
}

NS_IMETHODIMP
nsFTPChannel::OnProgress(nsIRequest *request, nsISupports* aContext,
                         PRUint32 aProgress, PRUint32 aProgressMax)
{
    if (!mEventSink || (mLoadFlags & LOAD_BACKGROUND) || !mIsPending)
        return NS_OK;

    return mEventSink->OnProgress(this, mUserContext, 
                                  aProgress, aProgressMax);
}


// nsIRequestObserver methods.
NS_IMETHODIMP
nsFTPChannel::OnStopRequest(nsIRequest *request, nsISupports* aContext,
                            nsresult aStatus)
{
    PR_LOG(gFTPLog, 
           PR_LOG_DEBUG, 
           ("nsFTPChannel::OnStopRequest() called [this=%x, request=%x, aStatus=%x]\n", 
            this, request, aStatus));

    nsresult rv = NS_OK;
    
    if (NS_SUCCEEDED(mStatus))
        mStatus = aStatus;

    if (mListener) {
        (void) mListener->OnStopRequest(this, mUserContext, mStatus);
    }
    if (mLoadGroup) {
        (void) mLoadGroup->RemoveRequest(this, nsnull, mStatus);
    }
    
    if (mCacheEntry) {
        if (NS_SUCCEEDED(mStatus)) {
            (void) mCacheEntry->SetExpirationTime( NowInSeconds() + 900 ); // valid for 15 minutes.
            (void) mCacheEntry->MarkValid();
	}
        else {
            (void) mCacheEntry->Doom();
        }
        mCacheEntry->Close();
        mCacheEntry = 0;
    }

    if (mUploadStream)
        mUploadStream->Close();

    if (mFTPState) {
        mFTPState->DataConnectionComplete();
        NS_RELEASE(mFTPState);
    }
    mIsPending = PR_FALSE;
    return rv;
}

NS_IMETHODIMP
nsFTPChannel::OnStartRequest(nsIRequest *request, nsISupports *aContext) 
{
    PR_LOG(gFTPLog, 
           PR_LOG_DEBUG, 
           ("nsFTPChannel::OnStartRequest() called [this=%x, request=%x]\n", 
            this, request));
   
    if (NS_SUCCEEDED(mStatus))
        request->GetStatus(&mStatus);
    
    nsCOMPtr<nsIResumableChannel> resumable = do_QueryInterface(request);
    if (resumable)
        resumable->GetEntityID(getter_AddRefs(mEntityID));
    
    nsresult rv = NS_OK;
    if (mListener) {
        if (mContentType.IsEmpty()) {
            // Time to sniff!
            nsCOMPtr<nsIStreamConverterService> serv =
                do_GetService("@mozilla.org/streamConverters;1", &rv);
            if (NS_SUCCEEDED(rv)) {
                NS_ConvertASCIItoUCS2 from(UNKNOWN_CONTENT_TYPE);
                nsCOMPtr<nsIStreamListener> converter;
                rv = serv->AsyncConvertData(from.get(),
                                            NS_LITERAL_STRING("*/*").get(),
                                            mListener,
                                            mUserContext,
                                            getter_AddRefs(converter));
                if (NS_SUCCEEDED(rv)) {
                    mListener = converter;
                }
            }
        }
        
        rv = mListener->OnStartRequest(this, mUserContext);
        if (NS_FAILED(rv)) return rv;
    }
    return rv;
}


// nsIStreamListener method
NS_IMETHODIMP
nsFTPChannel::OnDataAvailable(nsIRequest *request, nsISupports* aContext,
                               nsIInputStream *aInputStream, PRUint32 aSourceOffset,
                               PRUint32 aLength) {
    return mListener->OnDataAvailable(this, mUserContext, aInputStream, aSourceOffset, aLength);
}

//-----------------------------------------------------------------------------
// nsFTPChannel::nsICacheListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFTPChannel::OnCacheEntryAvailable(nsICacheEntryDescriptor *entry,
                                     nsCacheAccessMode access,
                                     nsresult status)
{
    nsresult rv;
    
    if (mCanceled) {
        NS_ASSERTION(NS_FAILED(mStatus), "Must be canceled with a failure status code");
        OnStartRequest(NS_STATIC_CAST(nsIRequest*, this), nsnull);
        OnStopRequest(NS_STATIC_CAST(nsIRequest*, this), nsnull,  mStatus);
        return mStatus;
    }

    if (NS_SUCCEEDED(status)) {
        mCacheEntry = entry;
    }
    
    rv = SetupState(PRUint32(-1),nsnull);

    if (NS_FAILED(rv)) {
        Cancel(rv);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetUploadStream(nsIInputStream *stream, const nsACString &contentType, PRInt32 contentLength)
{
    mUploadStream = stream;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetUploadStream(nsIInputStream **stream)
{
    NS_ENSURE_ARG_POINTER(stream);
    *stream = mUploadStream;
    NS_IF_ADDREF(*stream);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetListFormat(PRUint32 format)
{
    // Convert the pref value
    if (format == FORMAT_PREF) {
        format = FORMAT_HTML; // default
        nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
        if (prefs) {
            PRInt32 sFormat;
            if (NS_SUCCEEDED(prefs->GetIntPref("network.dir.format", &sFormat)))
                format = sFormat;
        }
    }
    if (format != FORMAT_RAW &&
        format != FORMAT_HTML &&
        format != FORMAT_HTTP_INDEX) {
        NS_WARNING("invalid directory format");
        return NS_ERROR_FAILURE;
    }
    mListFormat = format;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetListFormat(PRUint32 *format)
{
    *format = mListFormat;
    return NS_OK;
}
