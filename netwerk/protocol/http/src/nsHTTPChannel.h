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

#ifndef _nsHTTPChannel_h_
#define _nsHTTPChannel_h_

#include "nsIHTTPChannel.h"
#include "nsIChannel.h"
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
#include "nsICachedNetData.h"
#include "nsIProxy.h"
#include "nsIPrompt.h"
#include "nsIHTTPEventSink.h"
#include "nsIStreamAsFile.h"
class nsIFile;

class nsHTTPRequest;
class nsHTTPResponse;
class nsICachedNetData;

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
                      public nsIStreamAsFile
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
		NS_DECL_NSISTREAMASFILE
		
    // nsHTTPChannel methods:
    nsresult            Authenticate(const char *iChallenge,
                                     PRBool bProxyAuth = PR_FALSE);
    nsresult            Init();
    nsresult            Open(PRBool bIgnoreCache=PR_FALSE);
    nsresult            Redirect(const char *aURL,
                                 nsIChannel **aResult, PRInt32 aStatusCode);

    nsresult            ResponseCompleted(nsIStreamListener *aListener,
                                          nsresult aStatus, const PRUnichar* aStatusArg);

    nsresult            SetResponse(nsHTTPResponse* i_pResp);
    nsresult            GetResponseContext(nsISupports** aContext);
    nsresult            CacheReceivedResponse(nsIStreamListener *aListener,
                                              nsIStreamListener* *aResult);
    nsresult            CacheAbort(PRUint32 statusCode);

    nsresult            OnHeadersAvailable();

    nsresult            FinishedResponseHeaders();

    nsresult            Abort();

    PRUint32            getChannelState ();

    nsresult            ReportProgress(PRUint32 aProgress,
                                       PRUint32 aProgressMax);

    nsresult            BuildNotificationProxies ();

protected:
    nsresult            CheckCache();
    nsresult            ReadFromCache();
    nsresult            ProcessStatusCode();
    nsresult            ProcessRedirection(PRInt32 aStatusCode);
    nsresult            ProcessAuthentication(PRInt32 aStatusCode);
    nsresult            ProcessNotModifiedResponse(nsIStreamListener *aListener);

public:
    nsHTTPResponse*                     mResponse;

    nsHTTPHandler*                      mHandler;
    nsHTTPRequest*                      mRequest;
    nsHTTPResponseListener*             mHTTPServerListener;
    nsCOMPtr<nsISupports>               mResponseContext;
    nsHTTPResponse*                     mCachedResponse;
    nsCOMPtr<nsIProgressEventSink>      mProgressEventSink;
    nsCOMPtr<nsIProgressEventSink>      mRealProgressEventSink;

protected:
    // for PUT/POST cases...
    nsCOMPtr<nsIInputStream>            mRequestStream;
    nsCOMPtr<nsIStreamObserver>         mWriteObserver;

    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsIURI>                    mURI;
    nsCOMPtr<nsIURI>                    mReferrer;
    nsCOMPtr<nsIHTTPEventSink>          mEventSink;
    nsCOMPtr<nsIHTTPEventSink>          mRealEventSink;
    PRBool                              mConnected; 
    HTTPState                           mState;
    nsCOMPtr<nsIPrompt>                 mPrompter;
    nsCOMPtr<nsIPrompt>                 mRealPrompter;
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;

    nsCOMPtr<nsIStreamListener>         mResponseDataListener;
    nsCOMPtr<nsIOutputStream>           mBufOutputStream;

    PRUint32                            mLoadAttributes;

    nsCOMPtr<nsILoadGroup>              mLoadGroup;

    nsCOMPtr<nsISupports>               mOwner;
    
    // Cache-related members
    nsCOMPtr<nsICachedNetData>          mCacheEntry;
    PRBool                              mCachedContentIsAvailable;
    PRBool                              mCachedContentIsValid;
    
    // Called OnHeadersAvailable()
    PRBool                              mFiredOnHeadersAvailable;
    
    // Called mOpenObserver->OnStartRequest
    PRBool                              mFiredOpenOnStartRequest;
    PRBool                              mFiredOpenOnStopRequest;

    // Auth related stuff-
    /* 
       If this is true then we have already tried 
       prehost as a response to the server challenge. 
       And so we need to throw a dialog box!
    */
    PRBool                              mAuthTriedWithPrehost;

    char*                               mProxy;
    PRInt32                             mProxyPort;
    char*                               mProxyType;
    // if the proxy doesn't affect the protocol behavior
    // (such as socks) we want to reverse conditional proxy behavior
    PRBool                              mProxyTransparent;
   
    PRUint32                            mBufferSegmentSize;
    PRUint32                            mBufferMaxSize;
    nsresult                            mStatus;

    nsCOMPtr<nsIChannel>                mCacheTransport;

    PRBool                              mPipeliningAllowed;
    nsHTTPPipelinedRequest*             mPipelinedRequest;
    nsCOMPtr<nsISupports>               mSecurityInfo;
    // Stream as file
    nsCOMPtr<nsISupportsArray>						mStreamAsFileObserverArray;
    PRBool                              mApplyConversion;
    PRBool                              mNotificationProxiesBuilt;
    PRBool                              mOpenInputStreamHasEventQueue;
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
    PRBool                      mProcessing;
};

#endif /* _nsHTTPChannel_h_ */
