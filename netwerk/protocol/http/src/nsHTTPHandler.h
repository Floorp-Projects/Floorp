/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _nsHTTPHandler_h_
#define _nsHTTPHandler_h_

/* 
    The nsHTTPHandler class is an example implementation of how a 
    pluggable protocol would be written by an external party. As 
    an example this class also uses the Proxy interface.

    Since this is a completely different process boundary, I am 
    keeping this as a singleton. It doesn't have to be that way.

    Currently this is being built with the Netlib dll. But after
    the registration stuff that DP is working on gets completed 
    this will move to the HTTP lib.

    -Gagan Saksena 02/25/99
*/

#include "nsIHTTPProtocolHandler.h"
#include "nsIProxy.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsCRT.h"
#include "nsAuthEngine.h"
#include "nsIProxy.h"
#include "prtime.h"
#include "nsString.h"

//Forward decl.
class nsHashtable;
class nsHTTPChannel;

class nsHTTPHandler : public nsIHTTPProtocolHandler, public nsIProxy 
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    NS_DECL_NSIHTTPPROTOCOLHANDLER
    NS_DECL_NSIPROXY

    nsHTTPHandler();
    nsresult Init();

    /** 
    *   Pull out an existing transport from the list, or if none exists
    *   create one. 
    */
    virtual nsresult RequestTransport(nsIURI *i_Uri, 
                                      nsHTTPChannel* i_Channel, 
                                      PRUint32 bufferSegmentSize,
                                      PRUint32 bufferMaxSize,
                                      nsIChannel** o_pTrans);
    
    /**
    *    Called to create a transport from RequestTransport to accually
    *    make a new channel.
    **/

    virtual nsresult CreateTransport(const char* host, PRInt32 port, 
                                     const char* aPrintHost,
                                     PRUint32 bufferSegmentSize,
                                     PRUint32 bufferMaxSize,
                                     nsIChannel** o_pTrans);
    
    /* Remove this transport from the list. */
    virtual nsresult ReleaseTransport(nsIChannel* i_pTrans);
    virtual nsresult CancelPendingChannel(nsHTTPChannel* aChannel);
    PRTime GetSessionStartTime() { return mSessionStartTime; }
    nsresult FollowRedirects(PRBool bFollow=PR_TRUE);
protected:
    virtual ~nsHTTPHandler();

    // This is the array of connections that the handler thread 
    // maintains to verify unique requests. 
    nsCOMPtr<nsISupportsArray> mConnections;
    nsCOMPtr<nsISupportsArray> mPendingChannelList;
    nsAuthEngine    mAuthEngine;
    nsCOMPtr<nsISupportsArray> mTransportList;
    // Transports that are idle (ready to be used again)
    nsCOMPtr<nsISupportsArray> mIdleTransports;

    char*           mAcceptLanguages;
    PRBool          mDoKeepAlive;
    char*           mProxy;
    PRInt32         mProxyPort;
    PRTime          mSessionStartTime;
    PRBool          mUseProxy; 

    nsresult BuildUserAgent();
    nsCAutoString mAppName;
    nsCAutoString mAppVersion;
    nsCAutoString mVendorName;
    nsCAutoString mVendorVersion;
    nsCAutoString mVendorComment;
    nsCAutoString mAppPlatform;
    nsCAutoString mAppOSCPU;
    nsCAutoString mAppSecurity;
    nsCAutoString mAppLanguage;
    nsCAutoString mAppUserAgent;
    nsCAutoString mAppMisc;
};

#endif /* _nsHTTPHandler_h_ */
