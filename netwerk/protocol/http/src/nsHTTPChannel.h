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
 * Original Author: Gagan Saksena <gagan@netscape.com>
 *
 * Contributor(s): 
 */

#ifndef _nsHTTPChannel_h_
#define _nsHTTPChannel_h_

#include "nsIHTTPChannel.h"
#include "nsIChannel.h"
#include "nsITransport.h"
#include "nsHTTPEnums.h"
#include "nsIURI.h"
#include "nsHTTPHandler.h"
#include "nsIEventQueue.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "nsILoadGroup.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIOutputStream.h"
#include "nsHTTPResponseListener.h"
#include "nsIStreamListener.h"
#include "nsIStreamObserver.h"
#include "nsIProxy.h"
#include "nsIPrompt.h"
#include "nsIHTTPEventSink.h"

#ifdef MOZ_NEW_CACHE
#include "nsICachingChannel.h"
#else
#include "nsIStreamAsFile.h"
#include "nsICachedNetData.h"
class nsICachedNetData;
#endif

class nsIFile;

class nsHTTPRequest;
class nsHTTPResponse;

#define LOOPING_REDIRECT_ERROR_URI  "chrome://necko/content/redirect_loop.xul"

// Utility functions- TODO share from nsURLHelpers...
nsresult           
DupString(char* *o_Dest, const char* i_Src);

/* 
    The nsHTTPChannel class is an example implementation of a
    protocol instnce that is active on a per-URL basis.

    Currently this is being built with the Netlib dll. But after
    the registration stuff that DP is working on gets completed 
    this will move to the HTTP lib.

    -Gagan Saksena 02/25/99
*/
class nsHTTPChannel : public nsIHTTPChannel,
                      public nsIInterfaceRequestor,
                      public nsIProgressEventSink,
                      public nsIProxy,
#ifdef MOZ_NEW_CACHE
                      public nsICachingChannel
#else
                      public nsIStreamAsFile
#endif
{

public:

    // Constructors and Destructor
    nsHTTPChannel(nsIURI* i_URL, nsHTTPHandler* i_Handler);

    virtual ~nsHTTPChannel();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIHTTPCHANNEL
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIPROGRESSEVENTSINK
    NS_DECL_NSIPROXY
#ifdef MOZ_NEW_CACHE
    NS_DECL_NSICACHINGCHANNEL
#else
	NS_DECL_NSISTREAMASFILE
#endif
		
    // nsHTTPChannel methods:
    nsresult            Authenticate(const char *iChallenge,
                                     PRBool bProxyAuth = PR_FALSE);
    nsresult            Init();
    nsresult            Begin(PRBool bIgnoreCache=PR_FALSE);
    nsresult            Redirect(const char *aURL,
                                 nsIChannel **aResult, PRInt32 aStatusCode);

    nsresult            ResponseCompleted(nsIStreamListener *aListener,
                                          nsresult aStatus, const PRUnichar* aStatusArg);

    nsresult            SetResponse(nsHTTPResponse* i_pResp);
    nsresult            GetResponseContext(nsISupports** aContext);

    nsresult            OnHeadersAvailable();

    nsresult            FinishedResponseHeaders();

    nsresult            Abort();

    PRUint32            getChannelState();

    nsresult            ReportProgress(PRUint32 aProgress,
                                       PRUint32 aProgressMax);

    nsresult            BuildNotificationProxies ();

protected:

    nsresult            CacheReceivedResponse(nsIStreamListener *aListener,
                                              nsIStreamListener* *aResult);
    nsresult            CacheAbort(PRUint32 statusCode);

    nsresult            CheckCache();
    nsresult            ReadFromCache();

    nsresult            ProcessStatusCode();
    nsresult            ProcessRedirection(PRInt32 aStatusCode);
    nsresult            ProcessAuthentication(PRInt32 aStatusCode);
    nsresult            ProcessNotModifiedResponse(nsIStreamListener *aListener);

public:
    nsISupports        *ResponseContext() { return mResponseContext; }
    void                SetHTTPServerListener(nsHTTPResponseListener *l) { mHTTPServerListener = l; }
    PRBool              HasCachedResponse() { return mCachedResponse != 0; }

protected:
    nsHTTPHandler                      *mHandler;

    HTTPState                           mState;
    nsresult                            mStatus;

    nsHTTPRequest                      *mRequest;
    nsHTTPResponse                     *mResponse;
    nsHTTPResponse                     *mCachedResponse;

    nsHTTPResponseListener             *mHTTPServerListener;

    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsIURI>                    mURI;
    nsCOMPtr<nsIURI>                    mReferrer;

    // Various event sinks
    nsCOMPtr<nsIHTTPEventSink>          mEventSink;
    nsCOMPtr<nsIHTTPEventSink>          mRealEventSink;
    nsCOMPtr<nsIPrompt>                 mPrompter;
    nsCOMPtr<nsIPrompt>                 mRealPrompter;
    nsCOMPtr<nsIProgressEventSink>      mProgressEventSink;
    nsCOMPtr<nsIProgressEventSink>      mRealProgressEventSink;
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;

    PRUint32                            mLoadAttributes;
    nsCOMPtr<nsILoadGroup>              mLoadGroup;

    // nsIPrincipal
    nsCOMPtr<nsISupports>               mOwner;

    // SSL security info (copied from the transport)
    nsCOMPtr<nsISupports>               mSecurityInfo;

    // Listener passed into AsyncOpen()
    nsCOMPtr<nsIStreamListener>         mResponseDataListener;
    nsCOMPtr<nsISupports>               mResponseContext;

    // Pipe output stream used by Open()
    nsCOMPtr<nsIOutputStream>           mBufOutputStream;

    // for PUT/POST cases...
    nsCOMPtr<nsIInputStream>            mRequestStream;
    
    // Cache stuff
#ifdef MOZ_NEW_CACHE
    nsCOMPtr<nsICacheSession>           mCacheSession;
    nsCOMPtr<nsICacheEntryDescriptor>   mCacheEntry;
    nsCOMPtr<nsITransport>              mCacheTransport;
#else
    nsCOMPtr<nsICachedNetData>          mCacheEntry;
    nsCOMPtr<nsIChannel>                mCacheChannel;
    nsCOMPtr<nsISupportsArray>          mStreamAsFileObserverArray;
#endif

    // The HTTP authentication realm (may be NULL)
    char                               *mAuthRealm;

    // Proxy stuff
    char                               *mProxyType;
    char                               *mProxy;
    PRInt32                             mProxyPort;
   
    // Pipelining stuff
    nsHTTPPipelinedRequest             *mPipelinedRequest;
    PRPackedBool                        mPipeliningAllowed;

    // True if connected to a server
    PRPackedBool                        mConnected; 

    // If this is true, then data should be decoded as we stream
    // it to our listener
    PRPackedBool                        mApplyConversion;

    // If this is true, then we do not have to spawn a new thread
    // to service Open()
    PRPackedBool                        mOpenHasEventQueue;
     
    // Cache-related flags
    PRPackedBool                        mCachedContentIsAvailable;
    PRPackedBool                        mCachedContentIsValid;
    
    // Called OnHeadersAvailable()
    PRPackedBool                        mFiredOnHeadersAvailable;
    
    // If this is true then we have already tried 
    // prehost as a response to the server challenge. 
    // And so we need to throw a dialog box!
    PRPackedBool                        mAuthTriedWithPrehost;

    // if the proxy doesn't affect the protocol behavior
    // (such as socks) we want to reverse conditional proxy behavior
    PRPackedBool                        mProxyTransparent;
};

#include "nsIRunnable.h"

class nsSyncHelper : public nsIRunnable,
                     public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISTREAMOBSERVER

    nsSyncHelper();
    virtual ~nsSyncHelper() {};

    nsresult Init (nsIChannel *aChannel, nsIStreamListener *aListener);

protected:
    nsCOMPtr<nsIChannel>        mChannel;
    nsCOMPtr<nsIStreamListener> mListener;
    PRPackedBool                mProcessing;
};

#endif /* _nsHTTPChannel_h_ */
