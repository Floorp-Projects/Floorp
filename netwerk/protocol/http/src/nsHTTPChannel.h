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
#include "nsICapabilities.h"
#include "nsIHttpEventSink.h"
#include "nsILoadGroup.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsHTTPRequest;
class nsHTTPResponse;
/* 
    The nsHTTPChannel class is an example implementation of a
    protocol instnce that is active on a per-URL basis.

    Currently this is being built with the Netlib dll. But after
    the registration stuff that DP is working on gets completed 
    this will move to the HTTP lib.

    -Gagan Saksena 02/25/99
*/
class nsHTTPChannel : public nsIHTTPChannel
{

public:

    // Constructors and Destructor
    nsHTTPChannel(nsIURI* i_URL, 
                  const char* verb,
                  nsIURI* originalURI,
                  nsHTTPHandler* i_Handler);

    virtual ~nsHTTPChannel();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIHTTPCHANNEL

    // nsHTTPChannel methods:
    nsresult            Authenticate(const char *iChallenge,
                                 nsIChannel **oChannel);
    nsresult            Init(nsILoadGroup *aGroup);
    nsresult            Open();
    nsresult            Redirect(const char *aURL,
                                 nsIChannel **aResult);
    nsresult            ResponseCompleted(nsIChannel* aTransport, 
                                          nsresult aStatus,
                                          const PRUnichar* aMsg);
    nsresult            SetResponse(nsHTTPResponse* i_pResp);
    nsresult            GetResponseContext(nsISupports** aContext);
    nsresult            SetContentLength(PRInt32 aContentLength);
    nsresult            SetContentType(const char* aContentType);
    nsresult            SetCharset(const char *aCharset);

    nsresult            OnHeadersAvailable();

    // Set if this channel is using proxy to connect
    nsresult            SetUsingProxy(PRBool i_UsingProxy);

protected:
    nsCOMPtr<nsIURI>            mOriginalURI;
    nsCOMPtr<nsIURI>            mURI;
    PRBool                      mConnected; 
    nsHTTPHandler*              mHandler;
    HTTPState                   mState;

    nsCString                   mVerb;
    nsCOMPtr<nsIHTTPEventSink>  mEventSink;
    nsCOMPtr<nsICapabilities>   mCallbacks;

    nsHTTPRequest*              mRequest;
    nsHTTPResponse*             mResponse;
    nsIStreamListener*          mResponseDataListener;

    PRUint32                    mLoadAttributes;

    nsCOMPtr<nsISupports>       mResponseContext;
    nsCOMPtr<nsILoadGroup>      mLoadGroup;

    PRInt32                     mContentLength;
    nsCString                   mContentType;
    nsCString                   mCharset;
    nsCOMPtr<nsISupports>       mOwner;
	// Auth related stuff-
	/* 
		If this is true then we have already tried 
		prehost as a response to the server challenge. 
		And so we need to throw a dialog box!
	*/
	PRBool 						mAuthTriedWithPrehost;
    PRBool                      mUsingProxy;
};

#endif /* _nsHTTPChannel_h_ */
